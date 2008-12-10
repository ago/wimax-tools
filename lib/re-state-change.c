/*
 * Linux WiMax
 * State Change Report
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
 */
/**
 * \defgroup state_change_group Tracking state changes
 *
 * When the WiMAX devices change state, the kernel sends \e state \e
 * change notification.
 *
 * An application can simply block a thread waiting for state changes
 * using the following convenience function:
 *
 * @code
 * result = wimaxll_wait_for_state_change(wmx, &old_state, &new_state);
 * @endcode
 *
 * However, in most cases, applications will want to integrate into
 * main loops and use the callback mechanism.
 *
 * For that, they just need to set a callback for the state change
 * notification:
 *
 *
 * @code
 * wimaxll_set_cb_state_change(wmx, my_state_change_callback, context_pointer);
 * @endcode
 *
 * and then wait for notifications to be available (see \ref receiving
 * "receiving with select()"). When data is available and wimax_recv()
 * is called to process it, the callback will be executed for each
 * state change notification.
 *
 * Applications can query the current callback set for the state
 * change notifications with wimaxll_get_cb_state_change().
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
 * WIMAX_GNL_RE_STATE_CHANGE: policy specification
 *
 * \internal
 *
 * Authoritative reference for this is at the kernel code,
 * drivers/net/wimax/stack.c.
 */
static
struct nla_policy wimaxll_gnl_re_state_change_policy[WIMAX_GNL_ATTR_MAX + 1] = {
	[WIMAX_GNL_STCH_STATE_OLD] = { .type = NLA_U8 },
	[WIMAX_GNL_STCH_STATE_NEW] = { .type = NLA_U8 },
};


/**
 * Callback to process an WIMAX_GNL_RE_STATE_CHANGE from the kernel
 *
 * \internal
 *
 * \param wmx WiMAX device handle
 * \param mch WiMAX multicast group handle
 * \param msg Pointer to netlink message
 * \return \c enum nl_cb_action
 *
 * wimaxll_mc_rx_read() calls libnl's nl_recvmsgs() to receive messages;
 * when a valid message is received, it goes into a loop that selects
 * a callback to run for each type of message and it will call this
 * function.
 *
 * This just expects a _RE_STATE_CHANGE message, whose payload is what
 * has to be passed to the caller. We just extract the data and call
 * the callback defined in the handle.
 */
int wimaxll_gnl_handle_state_change(struct wimaxll_handle *wmx,
				    struct nl_msg *msg)
{
	ssize_t result;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *gnl_hdr;
	struct nlattr *tb[WIMAX_GNL_ATTR_MAX+1];
	struct wimaxll_gnl_cb_context *ctx = wmx->state_change_context;
	enum wimax_st old_state, new_state;

	d_fnstart(7, wmx, "(msg %p)\n", msg);
	nl_hdr = nlmsg_hdr(msg);
	gnl_hdr = nlmsg_data(nl_hdr);

	assert(gnl_hdr->cmd == WIMAX_GNL_RE_STATE_CHANGE);

	/* Parse the attributes */
	result = genlmsg_parse(nl_hdr, 0, tb, WIMAX_GNL_ATTR_MAX,
			       wimaxll_gnl_re_state_change_policy);
	if (result < 0) {
		wimaxll_msg(wmx, "E: %s: genlmsg_parse() failed: %d\n",
			  __func__, result);
		wimaxll_cb_maybe_set_result(ctx, result);
		result = NL_SKIP;
		goto error_parse;
	}
	if (tb[WIMAX_GNL_STCH_STATE_OLD] == NULL) {
		wimaxll_msg(wmx, "E: %s: cannot find STCH_STATE_OLD "
			    "attribute\n", __func__);
		wimaxll_cb_maybe_set_result(ctx, -ENXIO);
		result = NL_SKIP;
		goto error_no_attrs;

	}
	old_state = nla_get_u8(tb[WIMAX_GNL_STCH_STATE_OLD]);

	if (tb[WIMAX_GNL_STCH_STATE_NEW] == NULL) {
		wimaxll_msg(wmx, "E: %s: cannot find STCH_STATE_NEW "
			    "attribute\n", __func__);
		wimaxll_cb_maybe_set_result(ctx, -ENXIO);
		result = NL_SKIP;
		goto error_no_attrs;

	}
	new_state = nla_get_u8(tb[WIMAX_GNL_STCH_STATE_NEW]);

	d_printf(1, wmx, "D: CRX re_state_change old %u new %u\n",
		 old_state, new_state);

	/* Now execute the callback for handling re-state-change; if
	 * it doesn't update the context's result code, we'll do. */
	result = wmx->state_change_cb(wmx, ctx, old_state, new_state);
	wimaxll_cb_maybe_set_result(ctx, result);
	if (result == -EBUSY)
		result = NL_STOP;
	else
		result = NL_OK;
error_no_attrs:
error_parse:
	d_fnend(7, wmx, "(msg %p ctx %p) = %zd\n", msg, ctx, result);
	return result;
}


/*
 * Context for the default callback we use in
 * wimaxll_wait_for_state_change()
 */
struct wimaxll_state_change_context {
	struct wimaxll_gnl_cb_context ctx;
	enum wimax_st *old_state, *new_state;
	int set:1;
};


/**
 * Get the callback and priv pointer for a WIMAX_GNL_RE_STATE_CHANGE message
 *
 * \param wmx WiMAX handle.
 * \param cb Where to store the current callback function.
 * \param context Where to store the private data pointer passed to the
 *     callback.
 *
 * \ingroup state_change_group
 */
void wimaxll_get_cb_state_change(struct wimaxll_handle *wmx,
			       wimaxll_state_change_cb_f *cb,
			       struct wimaxll_gnl_cb_context **context)
{
	*cb = wmx->state_change_cb;
	*context = wmx->state_change_context;
}


/**
 * Set the callback and priv pointer for a WIMAX_GNL_RE_STATE_CHANGE message
 *
 * \param wmx WiMAX handle.
 * \param cb Callback function to set
 * \param context Private data pointer to pass to the callback function.
 *
 * \ingroup state_change_group
 */
void wimaxll_set_cb_state_change(struct wimaxll_handle *wmx,
			       wimaxll_state_change_cb_f cb,
			       struct wimaxll_gnl_cb_context *context)
{
	wmx->state_change_cb = cb;
	wmx->state_change_context = context;
}


/*
 * Default callback we use in wimaxll_wait_for_state_change()
 */
static
int wimaxll_cb_state_change(struct wimaxll_handle *wmx,
			    struct wimaxll_gnl_cb_context *ctx,
			    enum wimax_st old_state,
			    enum wimax_st new_state)
{
	struct wimaxll_state_change_context *stch_ctx =
		wimaxll_container_of(ctx, struct wimaxll_state_change_context,
				     ctx);

	if (stch_ctx->set)
		return -EBUSY;
	*stch_ctx->old_state = old_state;
	*stch_ctx->new_state = new_state;
	stch_ctx->set = 1;
	return 0;
}


/**
 * Wait for an state change notification from the kernel
 *
 * \param wmx WiMAX device handle
 * \param old_state Pointer to where to store the previous state
 * \param new_state Pointer to where to store the new state
 * \return If successful, 0 and the values pointed to by the \a
 *     old_state and \a new_state arguments are valid; on error, a
 *     negative \a errno code and the state pointers contain no valid
 *     information.
 *
 * Waits for the WiMAX device to change state and reports said state
 * change.
 *
 * Internally, this function uses wimax_recv() , which means that on
 * reception (from the kernel) of notifications other than state
 * change, any callbacks that are set for them will be executed.
 *
 * \note This is a blocking call.
 *
 * \note This function cannot be run in parallel with other code that
 *     modifies the \e state \e change callbacks for this same handle.
 *
 * \ingroup state_change_group
 */
ssize_t wimaxll_wait_for_state_change(struct wimaxll_handle *wmx,
				      enum wimax_st *old_state,
				      enum wimax_st *new_state)
{
	ssize_t result;
	wimaxll_state_change_cb_f prev_cb = NULL;
	struct wimaxll_gnl_cb_context *prev_ctx = NULL;
	struct wimaxll_state_change_context ctx = {
		.ctx = WIMAXLL_GNL_CB_CONTEXT_INIT(wmx),
		.old_state = old_state,
		.new_state = new_state,
		.set = 0,
	};

	d_fnstart(3, wmx, "(wmx %p old_state %p new_state %p)\n",
		  wmx, old_state, new_state);
	wimaxll_get_cb_state_change(wmx, &prev_cb, &prev_ctx);
	wimaxll_set_cb_state_change(wmx, wimaxll_cb_state_change, &ctx.ctx);
	result = wimaxll_recv(wmx);
	/* the callback filled out *old_state and *new_state if ok */
	wimaxll_set_cb_state_change(wmx, prev_cb, prev_ctx);
	d_fnend(3, wmx, "(wmx %p old_state %p [%u] new_state %p [%u])\n",
		wmx, old_state, *old_state, new_state, *new_state);
	return result;
}
