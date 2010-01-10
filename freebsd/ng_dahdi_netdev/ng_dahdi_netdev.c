/*
 * Copyright (c) 2010 Max Khon <fjoe@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/callout.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/kernel.h>

#include <machine/stdarg.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_ether.h>

#include "ng_dahdi_netdev.h"

#if __FreeBSD_version < 800000
struct ng_node *ng_name2noderef(struct ng_node *node, const char *name);
#endif

static struct mtx netdev_mtx;
MTX_SYSINIT(netdev_mtx, &netdev_mtx, "DAHDI netdevice lock", 0);

MALLOC_DEFINE(M_DAHDI_NETDEV, "dahdi netdev", "DAHDI netdev data structures");

static SLIST_HEAD(, notifier_block) notifier_blocks =
	SLIST_HEAD_INITIALIZER(notifier_blocks);

static SLIST_HEAD(, packet_type) packet_types =
	SLIST_HEAD_INITIALIZER(packet_types);

int shutting_down = 0;

static void
rlprintf(int pps, const char *fmt, ...)
	__printflike(2, 3);

static void
rlprintf(int pps, const char *fmt, ...)
{
	va_list ap;
	static struct timeval last_printf;
	static int count;

	if (ppsratecheck(&last_printf, &count, pps)) {
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

/**
 * Register notifier
 */
int
register_netdevice_notifier(struct notifier_block *block)
{
	mtx_lock(&netdev_mtx);
	if (shutting_down)
		return (ENXIO);
	SLIST_INSERT_HEAD(&notifier_blocks, block, next);
	mtx_unlock(&netdev_mtx);
	return (0);
}

/**
 * Unregister notifier
 */
int
unregister_netdevice_notifier(struct notifier_block *block)
{
	mtx_lock(&netdev_mtx);
	SLIST_REMOVE(&notifier_blocks, block, notifier_block, next);
	mtx_unlock(&netdev_mtx);
	return (0);
}

/**
 * Notify netdevice
 */
static void
netdevice_notify(struct net_device *netdev, unsigned long event)
{
	struct notifier_block *block;

	printf("dahdi_netdev(%s): interface %s\n",
	    NG_NODE_NAME(netdev->node),
	    event == NETDEV_UP ? "up" : "down");
	SLIST_FOREACH(block, &notifier_blocks, next) {
		block->notifier_call(block, event, netdev);
	}
}

/**
 * Add packet type handler
 */
void
dev_add_pack(struct packet_type *ptype)
{
	mtx_lock(&netdev_mtx);
	if (shutting_down)
		return;
	SLIST_INSERT_HEAD(&packet_types, ptype, next);
	mtx_unlock(&netdev_mtx);
}

/**
 * Remove packet type handler
 */
void
dev_remove_pack(struct packet_type *ptype)
{
	mtx_lock(&netdev_mtx);
	SLIST_REMOVE(&packet_types, ptype, packet_type, next);
	mtx_unlock(&netdev_mtx);
}

static int ng_dahdi_netdev_mod_event(module_t mod, int event, void *data);
static ng_rcvmsg_t ng_dahdi_netdev_rcvmsg;
static ng_shutdown_t ng_dahdi_netdev_shutdown;
static ng_newhook_t ng_dahdi_netdev_newhook;
static ng_disconnect_t ng_dahdi_netdev_disconnect;
static ng_rcvdata_t ng_dahdi_netdev_rcvdata;

static struct ng_type ng_dahdi_netdev_typestruct = {
	.version =	NG_ABI_VERSION,
	.name =		"ng_dahdi_netdev",
	.mod_event =	ng_dahdi_netdev_mod_event,
	.rcvmsg =	ng_dahdi_netdev_rcvmsg,
	.shutdown =	ng_dahdi_netdev_shutdown,
	.newhook =	ng_dahdi_netdev_newhook,
	.rcvdata =	ng_dahdi_netdev_rcvdata,
	.disconnect =	ng_dahdi_netdev_disconnect,
};
NETGRAPH_INIT(dahdi_netdev, &ng_dahdi_netdev_typestruct);
MODULE_VERSION(ng_dahdi_netdev, 1);

/**
 * Get network device by name
 */
struct net_device *
dev_get_by_name(const char *devname)
{
	struct ng_node *ether_node = NULL, *node = NULL;
	struct net_device *netdev = NULL;
	struct ng_mesg *msg;
	char node_name[IFNAMSIZ + 8];
	int error;
	struct ngm_connect *nc;

	/* check if DAHDI netgraph node for that device already exists */
	snprintf(node_name, sizeof(node_name), "dahdi@%s", devname);
	node = ng_name2noderef(NULL, node_name);
	if (node != NULL) {
		netdev = NG_NODE_PRIVATE(node);
		printf("dahdi_netdev(%s): existing netgraph node ether %*D\n",
		    NG_NODE_NAME(node), (int) sizeof(netdev->dev_addr), netdev->dev_addr, ":");
		NG_NODE_UNREF(node);
		return (netdev);
	}

	/* create new network device */
	netdev = malloc(sizeof(*netdev), M_DAHDI_NETDEV, M_NOWAIT | M_ZERO);
	if (netdev == NULL) {
		printf("dahdi_netdev(%s): can not create netdevice\n",
		    node_name);
		goto error;
	}
	strlcpy(netdev->name, devname, sizeof(netdev->name));

	/* create new DAHDI netgraph node */
	if (ng_make_node_common(&ng_dahdi_netdev_typestruct, &node) != 0) {
		printf("dahdi_netdev(%s): can not create netgraph node\n",
		    node_name);
		goto error;
	}
	netdev->node = node;
	NG_NODE_SET_PRIVATE(node, netdev);
	if (ng_name_node(node, node_name) != 0) {
		printf("dahdi_netdev(%s): can not set netgraph node name\n",
		    node_name);
		goto error;
	}

	/* get reference to ethernet device ng node */
	ether_node = ng_name2noderef(NULL, devname);
	if (ether_node == NULL) {
		printf("dahdi_netdev(%s): no netgraph node for %s\n",
		    NG_NODE_NAME(node), netdev->name);
		goto error;
	}

	/* get ethernet address */
	NG_MKMESSAGE(msg, NGM_ETHER_COOKIE, NGM_ETHER_GET_ENADDR, 0, M_NOWAIT);
	if (msg == NULL) {
		printf("dahdi_netdev(%s): can not allocate NGM_ETHER_GET_ENADDR message\n",
		    NG_NODE_NAME(node));
		return (0);
	}
	NG_SEND_MSG_ID(error, node, msg, NG_NODE_ID(ether_node), NG_NODE_ID(node));
	if (error) {
		printf("dahdi_netdev(%s): NGM_ETHER_GET_ENADDR: error %d\n",
		    NG_NODE_NAME(node), error);
		return (0);
	}
	NG_NODE_UNREF(ether_node);
	ether_node = NULL;

	/* connect to ether "orphans" hook */
	NG_MKMESSAGE(msg, NGM_GENERIC_COOKIE, NGM_CONNECT,
	    sizeof(*nc), M_NOWAIT);
	if (msg == NULL) {
		printf("dahdi_netdev(%s): can not allocate NGM_CONNECT message\n",
		    NG_NODE_NAME(node));
		goto error;
	}
	nc = (struct ngm_connect *) msg->data;
	snprintf(nc->path, sizeof(nc->path), "%s:", devname);
	strlcpy(nc->ourhook, "upper", sizeof(nc->ourhook));
	strlcpy(nc->peerhook, NG_ETHER_HOOK_ORPHAN, sizeof(nc->peerhook));
	NG_SEND_MSG_ID(error, node, msg, NG_NODE_ID(node), NG_NODE_ID(node));
	if (error) {
		printf("dahdi_netdev(%s): NGM_CONNECT(%s:%s <-> %s): error %d\n",
		    NG_NODE_NAME(node), devname, NG_ETHER_HOOK_ORPHAN, "upper", error);
		goto error;
	}

	printf("dahdi_netdev(%s): new netgraph node\n",
	    NG_NODE_NAME(node));
	return (netdev);

error:
	if (netdev != NULL)
		free(netdev, M_DAHDI_NETDEV);
	if (node != NULL)
		NG_NODE_UNREF(node);
	if (ether_node != NULL)
		NG_NODE_UNREF(ether_node);
	return (NULL);
}

/**
 * Release network device
 */
void
dev_put(struct net_device *netdev)
{
	if (netdev == NULL)
		return;

	NG_NODE_REALLY_DIE(netdev->node);	/* Force real removal of node */
	ng_rmnode_self(netdev->node);		/* remove all netgraph parts */
}

/**
 * Transmit raw ethernet frame
 *
 * Takes ownership of passed mbuf.
 */
void
dev_xmit(struct net_device *netdev, struct mbuf *m)
{
	int error;

	if (netdev->upper == NULL)
		return;
	NG_SEND_DATA_ONLY(error, netdev->upper, m);
}

static int
ng_dahdi_netdev_attach(void)
{
	/* intentionally left empty */
	return (0);
}

static int
ng_dahdi_netdev_detach(void)
{
	mtx_lock(&netdev_mtx);

	shutting_down = 1;

	if (!SLIST_EMPTY(&notifier_blocks)) {
		printf("%s: notifier block list is not empty\n", __FUNCTION__);
		return (EBUSY);
	}

	if (!SLIST_EMPTY(&packet_types)) {
		printf("%s: packet type list is not empty\n", __FUNCTION__);
		return (EBUSY);
	}

	mtx_unlock(&netdev_mtx);

	return (0);
}

static int
ng_dahdi_netdev_mod_event(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		return ng_dahdi_netdev_attach();

	case MOD_UNLOAD:
		return ng_dahdi_netdev_detach();
	}

	return (EOPNOTSUPP);
}

/**
 * Receive an incoming control message
 */
static int
ng_dahdi_netdev_rcvmsg(struct ng_node *node, struct ng_item *item, struct ng_hook *lasthook)
{
	struct net_device *netdev = NG_NODE_PRIVATE(node);
	struct ng_mesg *msg, *resp = NULL;
	int error = 0;

	NGI_GET_MSG(item, msg);
	switch (msg->header.typecookie) {
	case NGM_ETHER_COOKIE:
		switch (msg->header.cmd) {
		case NGM_ETHER_GET_ENADDR:
			bcopy(msg->data, netdev->dev_addr, sizeof(netdev->dev_addr));
			printf("dahdi_netdev(%s): ether %*D\n",
			    NG_NODE_NAME(node),
			    (int) sizeof(netdev->dev_addr), netdev->dev_addr, ":");
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case NGM_FLOW_COOKIE:
		switch (msg->header.cmd) {
		case NGM_LINK_IS_UP:
		case NGM_LINK_IS_DOWN:
			netdevice_notify(netdev,
			    msg->header.cmd == NGM_LINK_IS_UP ?  NETDEV_UP : NETDEV_DOWN);
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
ng_dahdi_netdev_shutdown(struct ng_node *node)
{
	if (node->nd_flags & NGF_REALLY_DIE) {
		struct net_device *netdev = NG_NODE_PRIVATE(node);

		printf("dahdi_netdev(%s): destroying netgraph node\n",
		    NG_NODE_NAME(node));
		netdevice_notify(netdev, NETDEV_DOWN);
		NG_NODE_SET_PRIVATE(node, NULL);
		NG_NODE_UNREF(node);

		free(netdev, M_DAHDI_NETDEV);
		return (0);
	}

	NG_NODE_REVIVE(node);		/* Tell ng_rmnode we are persistent */
	return (0);
}

/*
 * Check for attaching a new hook.
 */
static int
ng_dahdi_netdev_newhook(struct ng_node *node, struct ng_hook *hook, const char *name)
{
	struct net_device *netdev = NG_NODE_PRIVATE(node);
	struct ng_hook **hookptr;

	if (strcmp(name, "upper") == 0) {
		hookptr = &netdev->upper;
	} else {
		printf("dahdi_netdev(%s): unsupported hook %s\n",
		    NG_NODE_NAME(node), name);
		return (EINVAL);
	}

	if (*hookptr != NULL) {
		printf("dahdi_netdev(%s): %s hook is already connected\n",
		    NG_NODE_NAME(node), name);
		return (EISCONN);
	}

	*hookptr = hook;
	return (0);
}

/*
 * Hook disconnection.
 */
static int
ng_dahdi_netdev_disconnect(struct ng_hook *hook)
{
	struct ng_node *node = NG_HOOK_NODE(hook);
	struct net_device *netdev = NG_NODE_PRIVATE(node);

	if (hook == netdev->upper) {
		netdev->upper = NULL;
	} else {
		panic("dahdi_netdev(%s): %s: weird hook", NG_NODE_NAME(node), __func__);
	}

	return (0);
}

/**
 * Receive data
 */
static int
ng_dahdi_netdev_rcvdata(struct ng_hook *hook, struct ng_item *item)
{
	struct ng_node *node = NG_HOOK_NODE(hook);
	struct net_device *netdev = NG_NODE_PRIVATE(node);
	struct mbuf *m;
	struct ether_header eh;
	struct packet_type *ptype;
	unsigned char *msg = NULL;
	int msglen;

	/* get mbuf */
	NGI_GET_M(item, m);
	NG_FREE_ITEM(item);

	/*
	 * get ethernet header
	 * do copy because of possible m_pullup later
	 */
	msglen = m_length(m, NULL);
	if (msglen < sizeof(eh))
		return (EINVAL);
	m_copydata(m, 0, sizeof(eh), (caddr_t) &eh);

	/* pass it down to packet type handlers */
	SLIST_FOREACH(ptype, &packet_types, next) {
		if (ptype->type == eh.ether_type) {
			if (msg == NULL) {
				/* skip ethernet header */
				msglen -= sizeof(eh);
				msg = malloc(msglen, M_DAHDI_NETDEV, M_NOWAIT);
				if (msg == NULL) {
					rlprintf(10, "dahdi_netdev(%s): malloc failed\n",
					    NG_NODE_NAME(node));
					return (ENOMEM);
				}
				m_copydata(m, sizeof(eh), msglen, msg);
			}
			ptype->func(netdev, &eh, msg, msglen);
		}
	}

	/* free memory */
	NG_FREE_M(m);
	if (msg != NULL)
		free(msg, M_DAHDI_NETDEV);
	return (0);
}
