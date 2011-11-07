/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Max Khon.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/syscallsubr.h>
#include <sys/taskqueue.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>

#include <netinet/in.h>
#include <netgraph/ng_cisco.h>
#include <netgraph/ng_iface.h>

#include <dahdi/kernel.h>

#include "ng_dahdi_iface.h"

#define module_printk(level, fmt, args...) printk(level "%s: " fmt, THIS_MODULE->name, ## args)

#if __FreeBSD_version < 800000
struct ng_node *ng_name2noderef(struct ng_node *node, const char *name);
#endif

#define DAHDI_IFACE_HOOK_UPPER "upper"

static ng_rcvmsg_t ng_dahdi_iface_rcvmsg;
static ng_shutdown_t ng_dahdi_iface_shutdown;
static ng_newhook_t ng_dahdi_iface_newhook;
static ng_disconnect_t ng_dahdi_iface_disconnect;
static ng_rcvdata_t ng_dahdi_iface_rcvdata;

static struct ng_type ng_dahdi_iface_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		"ng_dahdi_iface",
	.rcvmsg =	ng_dahdi_iface_rcvmsg,
	.shutdown =	ng_dahdi_iface_shutdown,
	.newhook =	ng_dahdi_iface_newhook,
	.rcvdata =	ng_dahdi_iface_rcvdata,
	.disconnect =	ng_dahdi_iface_disconnect,
};
NETGRAPH_INIT(dahdi_iface, &ng_dahdi_iface_typestruct);

static void dahdi_iface_rx_task(void *context, int pending);

/**
 * iface struct
 */
struct dahdi_iface {
	struct dahdi_chan *chan;	/**< dahdi master channel associated with the iface */
	struct taskqueue *rx_taskqueue;	/**< rx task queue */
	struct task rx_task;		/**< rx task */
	struct ng_node *node;		/**< our netgraph node */
	struct ng_hook *upper;		/**< our upper hook */
	char path[64];			/**< iface node path */
};

/**
 * Create iface struct
 */
static struct dahdi_iface *
dahdi_iface_alloc(struct dahdi_chan *chan)
{
	struct dahdi_iface *iface;

	iface = malloc(sizeof(*iface), M_DAHDI, M_WAITOK | M_ZERO);
	iface->chan = chan;
	iface->rx_taskqueue = taskqueue_create_fast("dahdi_iface_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &iface->rx_taskqueue);
	taskqueue_start_threads(&iface->rx_taskqueue, 1, PI_NET, "%s taskq", chan->name);
	TASK_INIT(&iface->rx_task, 0, dahdi_iface_rx_task, chan);
	return iface;
}

/**
 * Free iface struct
 */
static void
dahdi_iface_free(struct dahdi_iface *iface)
{
	taskqueue_free(iface->rx_taskqueue);
	free(iface, M_DAHDI);
}

/**
 * Ensure that specified netgraph type is available
 */
static int
ng_ensure_type(const char *type)
{
	int error;
	int fileid;
	char filename[NG_TYPESIZ + 3];

	if (ng_findtype(type) != NULL)
		return (0);

	/* Not found, try to load it as a loadable module. */
	snprintf(filename, sizeof(filename), "ng_%s", type);
	error = kern_kldload(curthread, filename, &fileid);
	if (error != 0)
		return (-1);

	/* See if type has been loaded successfully. */
	if (ng_findtype(type) == NULL) {
		(void)kern_kldunload(curthread, fileid, LINKER_UNLOAD_NORMAL);
		return (-1);
	}

	return (0);
}

/**
 * Connect hooks
 */
static void
dahdi_iface_connect_node_path(struct ng_node *node, const char *ourpath,
    const char *path, const char *ourhook, const char *peerhook)
{
	int error;
	struct ng_mesg *msg;
	struct ngm_connect *nc;

	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NGM_CONNECT, sizeof(*nc), M_WAITOK);
	if (msg == NULL) {
		printf("dahdi_iface(%s): Error: can not allocate NGM_CONNECT message (ignored)\n",
		    NG_NODE_NAME(node));
		return;
	}
	nc = (struct ngm_connect *) msg->data;
	strlcpy(nc->path, path, sizeof(nc->path));
	strlcpy(nc->ourhook, ourhook, sizeof(nc->ourhook));
	strlcpy(nc->peerhook, peerhook, sizeof(nc->peerhook));
	NG_SEND_MSG_PATH(error, node, msg, __DECONST(char *, ourpath), NG_NODE_ID(node));
	if (error) {
		printf("dahdi_iface(%s): Error: NGM_CONNECT(%s<->%s): error %d (ignored)\n",
		    NG_NODE_NAME(node), ourhook, peerhook, error);
		return;
	}
}

/**
 * Shutdown node specified by path
 */
static void
dahdi_iface_shutdown_node_path(struct ng_node *node, const char *path)
{
	int error;
	struct ng_mesg *msg;

	if (path[0] == '\0')
		return;

	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NGM_SHUTDOWN, 0, M_WAITOK);
	NG_SEND_MSG_PATH(error, node, msg, __DECONST(char *, path), NG_NODE_ID(node));
	if (error) {
		printf("dahdi_iface(%s): Error: NGM_SHUTDOWN: error %d (ignored)\n",
		    NG_NODE_NAME(node), error);
		return;
	}
}

/**
 * Create a netgraph node and connect it to ng_iface instance
 *
 * @return 0 on success, -1 on error
 */
int
dahdi_iface_create(struct dahdi_chan *chan)
{
	struct dahdi_iface *iface = NULL;
	struct ng_node *node;
	struct ng_mesg *msg;
	char node_name[64];
	int error;
	struct ngm_mkpeer *ngm_mkpeer;

	/* check if DAHDI netgraph node for that device already exists */
	snprintf(node_name, sizeof(node_name), "dahdi@%s", chan->name);
	node = ng_name2noderef(NULL, node_name);
	if (node != NULL) {
		printf("dahdi_iface(%s): existing netgraph node\n", NG_NODE_NAME(node));
		NG_NODE_UNREF(node);
		return (0);
	}

	/* create new network device */
	iface = dahdi_iface_alloc(chan);
	if (iface == NULL) {
		printf("dahdi_iface(%s): Error: Failed to create iface struct\n",
		    node_name);
		return (0);
	}
	chan->iface = iface;

	/* create new DAHDI netgraph node */
	if (ng_make_node_common(&ng_dahdi_iface_typestruct, &node) != 0) {
		printf("dahdi_iface(%s): Error: Failed to create netgraph node\n",
		    node_name);
		goto error;
	}
	iface->node = node;
	NG_NODE_SET_PRIVATE(node, iface);
	if (ng_name_node(node, node_name) != 0) {
		printf("dahdi_iface(%s): Error: Failed to set netgraph node name\n",
		    node_name);
		goto error;
	}

	/* create HDLC encapsulation layer peer node */
	if (ng_ensure_type(NG_CISCO_NODE_TYPE) < 0) {
		printf("dahdi_iface(%s): Error: Failed to load %s netgraph type\n",
		    NG_NODE_NAME(node), NG_CISCO_NODE_TYPE);
		goto error;
	}

	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NGM_MKPEER, sizeof(*ngm_mkpeer), M_WAITOK);
	ngm_mkpeer = (struct ngm_mkpeer *) msg->data;
	strlcpy(ngm_mkpeer->type, NG_CISCO_NODE_TYPE, sizeof(ngm_mkpeer->type));
	strlcpy(ngm_mkpeer->ourhook, DAHDI_IFACE_HOOK_UPPER, sizeof(ngm_mkpeer->ourhook));
	strlcpy(ngm_mkpeer->peerhook, NG_CISCO_HOOK_DOWNSTREAM, sizeof(ngm_mkpeer->peerhook));
	NG_SEND_MSG_ID(error, node, msg, NG_NODE_ID(node), NG_NODE_ID(node));
	if (error) {
		printf("dahdi_iface(%s): Error: NGM_MKPEER: error %d (%s)\n",
		    NG_NODE_NAME(node), error, NG_CISCO_NODE_TYPE);
		goto error;
	}

	/* create network iface peer node */
	if (ng_ensure_type(NG_IFACE_NODE_TYPE) < 0) {
		printf("dahdi_iface(%s): Error: Failed to load %s netgraph type\n",
		    NG_NODE_NAME(node), NG_IFACE_NODE_TYPE);
		goto error;
	}

	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NGM_MKPEER, sizeof(*ngm_mkpeer), M_WAITOK);
	ngm_mkpeer = (struct ngm_mkpeer *) msg->data;
	strlcpy(ngm_mkpeer->type, NG_IFACE_NODE_TYPE, sizeof(ngm_mkpeer->type));
	strlcpy(ngm_mkpeer->ourhook, NG_CISCO_HOOK_INET, sizeof(ngm_mkpeer->ourhook));
	strlcpy(ngm_mkpeer->peerhook, NG_IFACE_HOOK_INET, sizeof(ngm_mkpeer->peerhook));
	NG_SEND_MSG_PATH(error, node, msg, DAHDI_IFACE_HOOK_UPPER, NG_NODE_ID(node));
	if (error) {
		printf("dahdi_iface(%s): Error: NGM_MKPEER: error %d (%s)\n",
		    NG_NODE_NAME(node), error, NG_IFACE_NODE_TYPE);
		goto error;
	}
	snprintf(iface->path, sizeof(iface->path), "%s.%s",
	    DAHDI_IFACE_HOOK_UPPER, NG_CISCO_HOOK_INET);

	/* connect other hooks */
	dahdi_iface_connect_node_path(node, DAHDI_IFACE_HOOK_UPPER,
	    NG_CISCO_HOOK_INET, NG_CISCO_HOOK_INET6, NG_IFACE_HOOK_INET6);
	dahdi_iface_connect_node_path(node, DAHDI_IFACE_HOOK_UPPER,
	    NG_CISCO_HOOK_INET, NG_CISCO_HOOK_APPLETALK, NG_IFACE_HOOK_ATALK);
	dahdi_iface_connect_node_path(node, DAHDI_IFACE_HOOK_UPPER,
	    NG_CISCO_HOOK_INET, NG_CISCO_HOOK_IPX, NG_IFACE_HOOK_IPX);

	/* get iface name */
	NG_MKMESSAGE(msg, NGM_IFACE_COOKIE, NGM_IFACE_GET_IFNAME, 0, M_WAITOK);
	NG_SEND_MSG_PATH(error, node, msg, iface->path, NG_NODE_ID(node));
	if (error) {
		printf("dahdi_iface(%s): Error: NGM_MKPEER: error %d (%s)\n",
		    NG_NODE_NAME(node), error, NG_IFACE_NODE_TYPE);
		goto error;
	}

	printf("dahdi_iface(%s): new netgraph node\n",
	    NG_NODE_NAME(node));

	/* setup channel */
	if (dahdi_net_chan_init(chan, DAHDI_DEFAULT_NUM_BUFS * 8)) {
		printf("dahdi_iface(%s): Error: Failed to initialize channel\n",
		    NG_NODE_NAME(node));
		goto error;
	}

	return (0);

error:
	if (iface != NULL) {
		if (iface->node != NULL) {
			dahdi_iface_shutdown_node_path(iface->node, iface->path);
			NG_NODE_UNREF(iface->node);
			iface->node = NULL;
		}

		dahdi_iface_free(iface);
		chan->iface = NULL;
	}
	return (-1);
}

/**
 * Destroy a netgraph node and ng_iface instance associated with it
 */
void
dahdi_iface_destroy(struct dahdi_chan *chan)
{
	struct dahdi_iface *iface;

	if ((iface = chan->iface) == NULL || iface->node == NULL)
		return;

	/* shutdown HDLC encapsulation layer peer node */
	if (iface->upper != NULL) {
		dahdi_iface_shutdown_node_path(iface->node, iface->path);
		dahdi_iface_shutdown_node_path(iface->node, DAHDI_IFACE_HOOK_UPPER);
	}

	NG_NODE_REALLY_DIE(iface->node);	/* Force real removal of node */
	ng_rmnode_self(iface->node);

	dahdi_net_chan_destroy(chan);
	chan->iface = NULL;
	chan->flags &= ~DAHDI_FLAG_NETDEV;
}

/**
 * Enqueues a task to receive the data frame from the synchronous line
 *
 * It is not possible to send the received data frame from dahdi_receive()
 * context because it can be run in the filter thread context and mbuf
 * allocation is not possible because of that.
 */
void
dahdi_iface_rx(struct dahdi_chan *chan)
{
	struct dahdi_iface *iface;
	int oldreadbuf;

	if ((iface = chan->iface) == NULL)
		return;

	/* switch buffers */
	if ((oldreadbuf = chan->inreadbuf) >= 0) {
		chan->inreadbuf = (chan->inreadbuf + 1) % chan->numbufs;
		if (chan->inreadbuf == chan->outreadbuf)
			chan->inreadbuf = -1;		/* no more buffers to receive to */
		if (chan->outreadbuf < 0)
			chan->outreadbuf = oldreadbuf;	/* new buffer to read from */
	}

	taskqueue_enqueue_fast(iface->rx_taskqueue, &iface->rx_task);
}

/**
 * Receive data frame from the synchronous line
 *
 * Receives data frame from the synchronous line and sends it up to the upstream.
 */
static void
dahdi_iface_rx_task(void *context, int pending)
{
	struct dahdi_chan *chan = context;
	struct dahdi_iface *iface;
	unsigned long flags;
	int oldreadbuf;

	if ((iface = chan->iface) == NULL)
		return;

	spin_lock_irqsave(&chan->lock, flags);
	while ((oldreadbuf = chan->outreadbuf) >= 0) {
		struct mbuf *m = NULL;

		/* read frame */
		if (iface->upper != NULL && chan->readn[chan->outreadbuf] > 1) {

			/* Drop the FCS */
			chan->readn[chan->outreadbuf] -= 2;

			MGETHDR(m, M_NOWAIT, MT_DATA);
			if (m != NULL) {
				if (chan->readn[chan->outreadbuf] >= MINCLSIZE) {
					MCLGET(m, M_NOWAIT);
				}

				/* copy data */
				m_append(m, chan->readn[chan->outreadbuf], chan->readbuf[chan->outreadbuf]);
			}
		}

		/* switch buffers */
		chan->readn[chan->outreadbuf] = 0;
		chan->readidx[chan->outreadbuf] = 0;
		chan->outreadbuf = (chan->outreadbuf + 1) % chan->numbufs;
		if (chan->outreadbuf == chan->inreadbuf)
			chan->outreadbuf = -1;		/* no more buffers to read from */
		if (chan->inreadbuf < 0)
			chan->inreadbuf = oldreadbuf;	/* new buffer to read to */

		if (m != NULL) {
			int error;

			spin_unlock_irqrestore(&chan->lock, flags);
			NG_SEND_DATA_ONLY(error, iface->upper, m);
			spin_lock_irqsave(&chan->lock, flags);
		}
	}
	spin_unlock_irqrestore(&chan->lock, flags);
}

/**
 * Abort receiving a data frame
 */
void
dahdi_iface_abort(struct dahdi_chan *chan, int event)
{
	/* nothing to do */
#if 0
	module_printk(KERN_DEBUG, "%s: event %d\n", __func__, event);
#endif
}

/**
 * Wake up transmitter
 */
void
dahdi_iface_wakeup_tx(struct dahdi_chan *chan)
{
	/* XXX not implemented */
}

/**
 * Receive an incoming control message
 */
static int
ng_dahdi_iface_rcvmsg(struct ng_node *node, struct ng_item *item, struct ng_hook *lasthook)
{
	/* struct dahdi_iface *iface = NG_NODE_PRIVATE(node); */
	struct ng_mesg *msg, *resp = NULL;
	int error = 0;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_IFACE_COOKIE:
		switch (msg->header.cmd) {
		case NGM_IFACE_GET_IFNAME:
			printf("dahdi_iface(%s): interface %s\n",
			    NG_NODE_NAME(node), msg->data);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	NG_RESPOND_MSG(error, node, item, resp);
	NG_FREE_MSG(msg);
	return (error);
}

/**
 * Shutdown node
 *
 * Reset the node but does not remove it unless the REALLY_DIE flag is set.
 */
static int
ng_dahdi_iface_shutdown(struct ng_node *node)
{
	struct dahdi_iface *iface = NG_NODE_PRIVATE(node);

	if (node->nd_flags & NGF_REALLY_DIE) {
		/* destroy the node itself */
		printf("dahdi_iface(%s): destroying netgraph node\n",
		    NG_NODE_NAME(node));
		NG_NODE_SET_PRIVATE(node, NULL);
		NG_NODE_UNREF(node);

		/* destroy the iface */
		dahdi_iface_free(iface);
		return (0);
	}

	NG_NODE_REVIVE(node);		/* Tell ng_rmnode we are persistent */
	return (0);
}

/*
 * Check for attaching a new hook
 */
static int
ng_dahdi_iface_newhook(struct ng_node *node, struct ng_hook *hook, const char *name)
{
	struct dahdi_iface *iface = NG_NODE_PRIVATE(node);
	struct ng_hook **hookptr;

	if (strcmp(name, DAHDI_IFACE_HOOK_UPPER) == 0) {
		hookptr = &iface->upper;
	} else {
		printf("dahdi_iface(%s): unsupported hook %s\n",
		    NG_NODE_NAME(iface->node), name);
		return (EINVAL);
	}

	if (*hookptr != NULL) {
		printf("dahdi_iface(%s): %s hook is already connected\n",
		    NG_NODE_NAME(iface->node), name);
		return (EISCONN);
	}

	*hookptr = hook;
	return (0);
}

/*
 * Hook disconnection
 */
static int
ng_dahdi_iface_disconnect(struct ng_hook *hook)
{
	struct ng_node *node = NG_HOOK_NODE(hook);
	struct dahdi_iface *iface = NG_NODE_PRIVATE(node);

	if (hook == iface->upper) {
		iface->upper = NULL;
	} else {
		panic("dahdi_iface(%s): %s: weird hook", NG_NODE_NAME(iface->node), __func__);
	}

	return (0);
}

/**
 * Receive data
 *
 * Receives data frame from the upstream and sends it down to the synchronous line.
 */
static int
ng_dahdi_iface_rcvdata(struct ng_hook *hook, struct ng_item *item)
{
	struct ng_node *node = NG_HOOK_NODE(hook);
	struct dahdi_iface *iface = NG_NODE_PRIVATE(node);
	struct dahdi_chan *ss = iface->chan;
	struct mbuf *m;
	int retval = 0;
	unsigned long flags;
	unsigned char *data;
	int data_len;

	/* get mbuf */
	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);
	data_len = m_length(m, NULL);

	/* see if we have any buffers */
	spin_lock_irqsave(&ss->lock, flags);
	if (data_len > ss->blocksize - 2) {
		printf("dahdi_iface(%s): mbuf is too large (%d > %d)",
		    NG_NODE_NAME(iface->node), data_len, ss->blocksize - 2);
		/* stats->tx_dropped++ */
		retval = EINVAL;
		goto out;
	}
	if (ss->inwritebuf < 0) {
		/* no space */
		retval = ENOBUFS;
		goto out;
	}

	/* we have a place to put this packet */
	data = ss->writebuf[ss->inwritebuf];
	m_copydata(m, 0, data_len, data);
	ss->writen[ss->inwritebuf] = data_len;
	dahdi_net_chan_xmit(ss);

out:
	spin_unlock_irqrestore(&ss->lock, flags);

	/* free memory */
	NG_FREE_M(m);
	return (retval);
}
