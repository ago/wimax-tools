/*
 * Linux WiMax
 * Shared/common routines
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * These are a set of facilities used by the implementation of the
 * different ops in this library.
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <linux/types.h>
#include <netlink/msg.h>
#include <netlink/genl/genl.h>
#include <wimaxll.h>
#include "internal.h"
#define D_LOCAL 0
#include "debug.h"


/**
 * Netlink callback to process netlink callback errors
 *
 * \internal
 *
 * \param nla Source netlink address
 * \param nlerr Netlink error descritor
 * \param _ctx Pointer to (\a struct wimaxll_cb_ctx)
 *
 * \return 'enum nl_cb_action', NL_OK if there is no error, NL_STOP on
 *     error and _mch->result possibly updated.
 *
 * While reading from netlink and processing with callbacks (using
 * nl_recvmsgs()), we use this for the callback 'state machine' to
 * store the result of an error message from the kernel.
 */
int wimaxll_gnl_error_cb(struct sockaddr_nl *nla, struct nlmsgerr *nlerr,
			 void *_ctx)
{
	struct wimaxll_cb_ctx *ctx = _ctx;
	struct wimaxll_handle *wmx = ctx->wmx;

	d_fnstart(3, wmx, "(nla %p nlnerr %p [%d] ctx %p)\n",
		  nla, nlerr, nlerr->error, _ctx);
	wimaxll_cb_maybe_set_result(ctx, nlerr->error);
	ctx->msg_done = 1;
	d_fnend(3, wmx, "(nla %p nlnerr %p [%d] ctx %p) = NL_STOP\n",
		nla, nlerr, nlerr->error, _ctx);
	return NL_STOP;
}


/**
 * Netlink callback to process an ack message and pass the 'error' code
 *
 * \internal
 *
 * Process a netlink ack message and extract the error code, which is
 * placed in the context passed as argument for the calling function
 * to use.
 *
 * We use this so that ACKers in the kernel can pass a simple error
 * code (integer) in the ACK that netlink sends, without having to
 * send an extra message.
 *
 * Complementary to wimaxll_gnl_error_cb().
 *
 * Frontend to this is wimaxll_wait_for_ack()
 */
int wimaxll_gnl_ack_cb(struct nl_msg *msg, void *_ctx)
{
	int result;
	struct nlmsghdr *nl_hdr;
	struct nlmsgerr *nl_err;
	size_t size = nlmsg_datalen(nlmsg_hdr(msg));
	struct wimaxll_cb_ctx *ctx = _ctx;

	d_fnstart(7, NULL, "(msg %p ctx %p)\n", msg, _ctx);
	nl_hdr = nlmsg_hdr(msg);
	size = nlmsg_datalen(nl_hdr);
	nl_err = nlmsg_data(nl_hdr);

	if (size < sizeof(*nl_err)) {
		wimaxll_msg(NULL, "E: netlink ack: buffer too small "
			  "(%zu vs %zu expected)\n",
			  size, sizeof(*nl_hdr) + sizeof(*nl_err));
		result = -EIO;
		goto error_ack_short;
	}
	d_printf(4, NULL, "netlink ack: nlmsghdr len %u type %u flags 0x%04x "
		 "seq 0x%x pid %u\n", nl_hdr->nlmsg_len, nl_hdr->nlmsg_type,
		 nl_hdr->nlmsg_flags, nl_hdr->nlmsg_seq, nl_hdr->nlmsg_pid);
	if (nl_hdr->nlmsg_type != NLMSG_ERROR) {
		wimaxll_msg(NULL, "E: netlink ack: message is not an ack but "
			  "type %u\n", nl_hdr->nlmsg_type);
		result = -EBADE;
		goto error_bad_type;
	}
	d_printf(3, NULL, "netlink ack: nlmsgerr error %d for "
		 "nlmsghdr len %u type %u flags 0x%04x seq 0x%x pid %u\n",
		 nl_err->error,
		 nl_err->msg.nlmsg_len, nl_err->msg.nlmsg_type,
		 nl_err->msg.nlmsg_flags, nl_err->msg.nlmsg_seq,
		 nl_err->msg.nlmsg_pid);
	wimaxll_cb_maybe_set_result(ctx, nl_err->error);
	if (nl_err->error < 0)
		d_printf(2, NULL, "D: netlink ack: received netlink error %d\n",
			  nl_err->error);
	ctx->msg_done = 1;
error_ack_short:
error_bad_type:
	d_fnend(7, NULL, "(msg %p ctx %p) = NL_STOP\n", msg, _ctx);
	return NL_STOP;
}


/**
 * Wait for a netlink ACK and pass on the result code it passed
 *
 * \internal
 *
 * \param wmx WiMAX device handle
 * \return error code passed by the kernel in the nlmsgerr structure
 *     that contained the ACK.
 *
 * Similar to nl_wait_for_ack(), but returns the value in
 * nlmsgerr->error, so it can be used by the kernel to return simple
 * error codes.
 */
int wimaxll_wait_for_ack(struct wimaxll_handle *wmx)
{
	int result;
	struct nl_cb *cb;
	struct wimaxll_cb_ctx ctx;

	ctx.wmx = wmx;
	ctx.result = -EINPROGRESS;
	ctx.msg_done = 0;

	cb = nl_socket_get_cb(wmx->nlh_tx);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wimaxll_gnl_ack_cb, &ctx);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, wimaxll_gnl_error_cb, &ctx);
	do
		result = nl_recvmsgs(wmx->nlh_tx, cb);
	while (ctx.msg_done == 0 && result >= 0);
	result = ctx.result;
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_put(cb);
	if (result < 0)
		return result;
	else
		return ctx.result;
}


/**
 * Deliver \e libwimaxll diagnostics messages to \e stderr
 *
 * \deprecated { use wimaxll_vlmsg_cb }
 * 
 * \param fmt printf-like format
 * \param vargs variable-argument list as created by
 *     stdargs.h:va_list() that will be formatted according to \e
 *     fmt.
 *
 * Default diagnostics printing function.
 *
 * \ingroup diagnostics_group
 */
void wimaxll_vmsg_stderr(const char *fmt, va_list vargs)
{
	vfprintf(stderr, fmt, vargs);
}


/**
 * Return the name of a the system's WiMAX interface associated to an
 * open handle
 *
 * \param wmx WiMAX device handle
 * \return Interface name (only valid while the handle is open)
 *
 * Note that if this is an \e any interface (open for all devices),
 * this will be empty.
 * 
 * \ingroup device_management
 */
const char *wimaxll_ifname(const struct wimaxll_handle *wmx)
{
	return wmx->name;
}


/**
 * Return the interface index of the system's WiMAX interface
 * associated to an open handle
 *
 * \param wmx WiMAX device handle
 * \return Interface index
 *
 * Note that if this is an \e any interface (open for all devices),
 * this will vary. When not processing a callback, it will be
 * zero. When processing a callback, this call will return the
 * interface for which the callback was executed.
 *
 * \ingroup device_management
 */
unsigned wimaxll_ifidx(const struct wimaxll_handle *wmx)
{
	return wmx->ifidx;
}


/**
 * Set the private data associated to a WiMAX device handle
 *
 * @param wmx WiMAX device handle
 *
 * @param priv Private data pointer to associate.
 *
 * @ingroup device_management
 */
void wimaxll_priv_set(struct wimaxll_handle *wmx, void *priv)
{
	wmx->priv = priv;
}


/**
 * Return the private data associated to a WiMAX device handle
 *
 * @param wmx WiMAX device handle
 *
 * @returns pointer to priv data as set by wimaxll_priv_set() or
 *     wimaxll_open_name() or wimaxll_open_ifindex().
 *
 * @ingroup device_management
 */
void * wimaxll_priv_get(struct wimaxll_handle *wmx)
{
	return wmx->priv;
}
