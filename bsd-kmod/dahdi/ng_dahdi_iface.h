/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Max Khon under sponsorship from UniqueSec HB.
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

#ifndef _NG_DAHDI_IFACE_H_
#define _NG_DAHDI_IFACE_H_

/**
 * Create a netgraph node and connect it to ng_iface instance
 *
 * @return 0 on success, -1 on error
 */
int dahdi_iface_create(struct dahdi_chan *chan);

/**
 * Destroy a netgraph node and ng_iface instance associated with it
 */
void dahdi_iface_destroy(struct dahdi_chan *chan);

/**
 * Process data frame received from the synchronous line
 */
void dahdi_iface_rx(struct dahdi_chan *chan);

/**
 * Abort receiving a data frame
 */
void dahdi_iface_abort(struct dahdi_chan *chan, int event);

/**
 * Wake up transmitter
 */
void dahdi_iface_wakeup_tx(struct dahdi_chan *chan);

#endif /* _NG_DAHDI_IFACE_H_ */
