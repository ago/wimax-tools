/*
 * Linux WiMax
 * Messaging interface implementation
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
 * \defgroup the_messaging_interface The message interface
 *
 * This is a payload agnostic message interface for communication
 * between the WiMAX kernel drivers and user space applications.
 *
 * It writes messages (wimax_msg_write()) by sending them to the WiMAX
 * kernel stack, which passes it to the driver using the \e
 * wimax_dev->op_msg_from_user() call.
 *
 * To write messages to the driver:
 *
 * @code
 *  wimaxll_msg_write(wmx, PIPE_NAME, buf, buf_size);
 * @endcode
 *
 * where \a buf points to where the message is stored. \e PIPE_NAME
 * can be NULL. It is passed verbatim to the receiver.
 *
 * To wait for a message from the driver:
 *
 * @code
 *  void *msg;
 *  ...
 *  size = wimaxll_msg_read(wmx, PIPE_NAME, &msg);
 * @endcode
 *
 * Note this call is synchronous and blocking, and won't timeout. You
 * can put it on a thread to emulate asynchrony (see \ref
 * multithreading), but still it is quite difficult to integrate it in
 * an event loop. Read on for mainloop integration options.
 *
 * In \e msg you get a pointer to a dynamically allocated (by \e
 * libwimaxll) area with the message payload. When the application is
 * done processing the message, call:
 *
 * @code
 *  wimaxll_msg_free(msg);
 * @endcode
 *
 * All functions return negative \a errno codes on error.
 *
 * To integrate message reception into a mainloop, \ref callbacks
 * "callbacks" and select() should be used. The file descriptor
 * associated to the default \e message \e pipe can be obtained with
 * wimaxll_recv_fd(). When there is activity on the file descriptor,
 * wimaxll_recv() should be called:
 *
 * \code
 * wimax_recv(wmx);
 * \endcode
 *
 * this will, as explained in \ref receiving, for each received
 * notification, execute its callback.
 *
 * The callback for reception of messages from the WiMAX kernel stack
 * can be set with wimaxll_set_cb_msg_to_user()). For detailed
 * information on the message reception callback, see the definition
 * of \ref wimaxll_msg_to_user_cb_f.
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
 * WIMAX_GNL_MSG_FROM_USER: policy specification
 *
 * \ingroup the_messaging_interface
 * \internal
 *
 * Authoritative reference for this is at the kernel code,
 * drivers/net/wimax/op-msg.c.
 */
static
struct nla_policy wimaxll_gnl_msg_from_user_policy[WIMAX_GNL_ATTR_MAX + 1] = {
	[WIMAX_GNL_MSG_DATA] = {
		.type = NLA_UNSPEC,
	},
	[WIMAX_GNL_MSG_IFIDX] = {
		.type = NLA_U32,
	},
	[WIMAX_GNL_MSG_PIPE_NAME] = {
		.type = NLA_STRING,
	},
};


/**
 * Callback to process an WIMAX_GNL_OP_MSG_TO_USER from the kernel
 *
 * \internal
 * \ingroup the_messaging_interface
 *
 * \param wmx WiMAX device handle
 * \param mch Pointer to \c struct wimaxll_mc_handle
 * \param msg Pointer to netlink message
 * \return 0 if ok, < 0 errno code on error
 *
 * wimaxll_recv() calls libnl's nl_recvmsgs() to receive messages;
 * when a valid message is received, wimax_gnl__cb() that selects a
 * callback to run for each type of message and it will call this
 * function to actually do it. If no message handling callback is set,
 * this is not called.
 *
 * This "netlink" callback will just de-marshall the arguments and
 * call the callback set by the user with wimaxll_set_cb_msg_to_user().
 */
int wimaxll_gnl_handle_msg_to_user(struct wimaxll_handle *wmx,
				   struct nl_msg *msg)
{
	size_t size;
	ssize_t result;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *gnl_hdr;
	struct nlattr *tb[WIMAX_GNL_ATTR_MAX+1];
	const char *pipe_name;
	unsigned dest_ifidx;
	void *data;

	d_fnstart(7, wmx, "(wmx %p msg %p)\n", wmx, msg);
	nl_hdr = nlmsg_hdr(msg);
	gnl_hdr = nlmsg_data(nl_hdr);

	assert(gnl_hdr->cmd == WIMAX_GNL_OP_MSG_TO_USER);

	/* Parse the attributes */
	result = genlmsg_parse(nl_hdr, 0, tb, WIMAX_GNL_ATTR_MAX,
			       wimaxll_gnl_msg_from_user_policy);
	if (result < 0) {
		wimaxll_msg(wmx, "E: %s: genlmsg_parse() failed: %zd\n",
			  __func__, result);
		goto error_parse;
	}
	/* Find if the message is for the interface wmx represents */
	if (tb[WIMAX_GNL_MSG_IFIDX] == NULL) {
		wimaxll_msg(wmx, "E: %s: cannot find IFIDX attribute\n",
			    __func__);
		result = -EINVAL;
		goto error_no_attrs;
	}
	dest_ifidx = nla_get_u32(tb[WIMAX_GNL_MSG_IFIDX]);
	if (wmx->ifidx > 0 && wmx->ifidx != dest_ifidx) {
		result = -ENODEV;
		goto error_no_attrs;
	}
	/* Extract marshalled arguments */
	if (tb[WIMAX_GNL_MSG_DATA] == NULL) {
		wimaxll_msg(wmx, "E: %s: cannot find MSG_DATA attribute\n",
			  __func__);
		result = -ENXIO;
		goto error_no_attrs;

	}
	size = nla_len(tb[WIMAX_GNL_MSG_DATA]);
	data = nla_data(tb[WIMAX_GNL_MSG_DATA]);

	if (tb[WIMAX_GNL_MSG_PIPE_NAME])
		pipe_name = nla_get_string(tb[WIMAX_GNL_MSG_PIPE_NAME]);
	else
		pipe_name = NULL;

	d_printf(1, wmx, "D: CRX genlmsghdr cmd %u version %u\n",
		 gnl_hdr->cmd, gnl_hdr->version);
	d_printf(1, wmx, "D: CRX msg from kernel %zu bytes pipe %s\n",
		 size, pipe_name);
	d_dump(2, wmx, data, size);

	/* If this is an "any" handle, set the wmx->ifidx to the
	 * received one so the callback can now where did the thing
	 * come from. Will be restored.
	 */
	if (wmx->ifidx == 0) {
		wmx->ifidx = dest_ifidx;
		dest_ifidx = 0;
	}
	/* Now execute the callback for handling msg-to-user */
	result = wmx->msg_to_user_cb(wmx, wmx->msg_to_user_priv,
				     pipe_name, data, size);
	wmx->ifidx = dest_ifidx;
error_no_attrs:
error_parse:
	d_fnend(7, wmx, "(wmx %p msg %p) = %zd\n", wmx, msg, result);
	return result;
}


struct wimaxll_cb_msg_to_user_context {
	struct wimaxll_cb_ctx ctx;
	void *data;
};


/*
 * Default handling of messages
 *
 * When someone calls wimaxll_msg_read() those functions set this
 * default callback, which will just copy the data to a buffer and
 * pass that pointer to the caller along with the size.
 */
static
int wimaxll_msg_read_cb(struct wimaxll_handle *wmx,
			void *_ctx,
			const char *pipe_name,
			const void *data, size_t data_size)
{
	int result;
	struct wimaxll_cb_ctx *ctx = _ctx;
	struct wimaxll_cb_msg_to_user_context *mtu_ctx =
		wimaxll_container_of(
			ctx, struct wimaxll_cb_msg_to_user_context, ctx);
	const char *dst_pipe_name = mtu_ctx->data;
	int pipe_match;

	d_fnstart(3, wmx, "(wmx %p ctx %p pipe_name %s data %p size %zd)\n",
		  wmx, ctx, pipe_name, data, data_size);

	result = -EBUSY;
	if (mtu_ctx->ctx.result != -EINPROGRESS)
		goto out;
	/*
	 * Is there a match in requested pipes? (dst_pipe_name ==
	 * WIMAX_PIPE_ANY), * messages for any pipe work.
	 *
	 * This way of checking makes it kind of easier to read...if
	 * the user requests messages from the default pipe (pipe_name
	 * == NULL), we want only those. Sucks strcmp doesn't take
	 * NULLs :)
	 */
	d_printf(3, wmx, "dst_pipe_name %s\n", dst_pipe_name);

	if (dst_pipe_name == WIMAX_PIPE_ANY)
		pipe_match = 1;
	else if (dst_pipe_name == NULL && pipe_name == NULL)
		pipe_match = 1;
	else if ((dst_pipe_name == NULL && pipe_name != NULL)
		 || (dst_pipe_name != NULL && pipe_name == NULL))
		pipe_match = 0;
	else if (strcmp(dst_pipe_name, pipe_name) == 0)
		pipe_match = 1;
	else
		pipe_match = 0;

	result = -EINPROGRESS;
	if (pipe_match == 0)	/* Not addressed to us */
		goto out;

	mtu_ctx->data = malloc(data_size);
	if (mtu_ctx->data) {
		memcpy(mtu_ctx->data, data, data_size);
		ctx->result = data_size;
	} else
		ctx->result = -ENOMEM;
	/* Now tell wimaxll_recv() [the callback dispatcher] to stop
	 * and return control to the caller */
	result = -EBUSY;
out:
	d_fnend(3, wmx, "(wmx %p ctx %p pipe_name %s data %p size %zd) = %d\n",
		wmx, ctx, pipe_name, data, data_size, result);
	return result;
}


/**
 * Read a message from any WiMAX kernel-user pipe
 *
 * \param wmx WiMAX device handle
 * \param pipe_name Name of the pipe for which we want to read a
 *     message. If NULL, only messages from the default pipe (without
 *     pipe name) will be received. To receive messages from any pipe,
 *     use pipe WIMAX_PIPE_ANY.
 * \param buf Somewhere where to store the pointer to the message data.
 * \return If successful, a positive (and \c *buf set) or zero size of
 *     the message; on error, a negative \a errno code (\c buf
 *     n/a).
 *
 * Returns a message allocated in \c *buf as sent by the kernel via
 * the indicated pipe. The message is allocated by the
 * library and owned by the caller. When done, it has to be freed with
 * wimaxll_msg_free() to release the space allocated to it.
 *
 * \note This is a blocking call.
 *
 * \ingroup the_messaging_interface
 *
 * \internal
 *
 * We use the data pointer of the context structure to pass the
 * pipe_name to the callback. This is ok, as we process only one
 * message and then return.
 */
ssize_t wimaxll_msg_read(struct wimaxll_handle *wmx,
			 const char *pipe_name, void **buf)
{
	ssize_t result;
	struct wimaxll_cb_msg_to_user_context mtu_ctx = {
		.ctx = WIMAXLL_CB_CTX_INIT(wmx),
		.data = (void *) pipe_name,
	};
	wimaxll_msg_to_user_cb_f prev_cb = NULL;
	void *prev_priv = NULL;

	d_fnstart(3, wmx, "(wmx %p pipe_name %s, buf %p)\n",
		  wmx, pipe_name, buf);
	wimaxll_get_cb_msg_to_user(wmx, &prev_cb, &prev_priv);
	wimaxll_set_cb_msg_to_user(wmx, wimaxll_msg_read_cb,
				   &mtu_ctx.ctx);
	do {
		/* Loop until we get a message in the desired pipe */
		result = wimaxll_recv(wmx);
		d_printf(3, wmx, "I: mtu_ctx.result %zd result %zd\n",
			 mtu_ctx.ctx.result, result);
	} while (result >= 0 && mtu_ctx.ctx.result == -EINPROGRESS);
	if (result >= 0) {
		*buf = mtu_ctx.data;
		result = mtu_ctx.ctx.result;
	}
	wimaxll_set_cb_msg_to_user(wmx, prev_cb, prev_priv);
	d_fnend(3, wmx, "(wmx %p pipe_name %s buf %p) = %zd\n",
		wmx, pipe_name, buf, result);
	return result;
}


/**
 * Free a message received with wimaxll_msg_read()
 *
 * \param msg message pointer returned by wimaxll_msg_read().
 *
 * \ingroup the_messaging_interface
 */
void wimaxll_msg_free(void *msg)
{
	d_fnstart(3, NULL, "(msg %p)\n", msg);
	free(msg);
	d_fnend(3, NULL, "(msg %p) = void\n", msg);
}


/**
 * Send a driver-specific message to a WiMAX device
 *
 * \param wmx wimax device descriptor
 * \param pipe_name Name of the pipe for which to send the message;
 *     NULL means adding no destination pipe.
 * \param buf Pointer to the message.
 * \param size size of the message.
 * \return 0 if ok < 0 errno code on error. On error it is assumed
 *     the message wasn't delivered.
 *
 * Sends a data buffer down to the kernel driver. The format of the
 * message is driver specific.
 *
 * \note This is a blocking call
 *
 * \ingroup the_messaging_interface
 */
ssize_t wimaxll_msg_write(struct wimaxll_handle *wmx,
			  const char *pipe_name,
			  const void *buf, size_t size)
{
	ssize_t result;
	struct nl_msg *nl_msg;
	void *msg;

	d_fnstart(3, wmx, "(wmx %p buf %p size %zu)\n", wmx, buf, size);
	result = -EBADF;
	if (wmx->ifidx == 0)
		goto error_not_any;
	nl_msg = nlmsg_alloc();
	if (!nl_msg) {
		result = errno;
		wimaxll_msg(wmx, "E: cannot allocate generic netlink "
			  "message: %m\n");
		goto error_msg_alloc;
	}
	msg = genlmsg_put(nl_msg, NL_AUTO_PID, NL_AUTO_SEQ,
			  wimaxll_family_id(wmx), 0, 0,
			  WIMAX_GNL_OP_MSG_FROM_USER, WIMAX_GNL_VERSION);
	if (!msg) {
		result = errno;
		wimaxll_msg(wmx, "E: %s: error preparing message: %zd\n",
			  __func__, result);
		goto error_msg_prep;
	}

	nla_put_u32(nl_msg, WIMAX_GNL_MSG_IFIDX, (__u32) wmx->ifidx);
	if (pipe_name != NULL)
		nla_put_string(nl_msg, WIMAX_GNL_MSG_PIPE_NAME, pipe_name);
	nla_put(nl_msg, WIMAX_GNL_MSG_DATA, size, buf);

	d_printf(5, wmx, "D: CTX nl + genl header:\n");
	d_dump(5, wmx, nlmsg_hdr(nl_msg),
	       sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr));
	d_printf(5, wmx, "D: CTX wimax message:\n");
	d_dump(5, wmx, buf, size);

	result = nl_send_auto_complete(wmx->nlh_tx, nl_msg);
	if (result < 0) {
		wimaxll_msg(wmx, "E: error sending message: %zd\n", result);
		goto error_msg_send;
	}

	result = wimaxll_wait_for_ack(wmx);	/* Get the ACK from netlink */
	if (result < 0)
		wimaxll_msg(wmx, "E: %s: generic netlink ack failed: %zd\n",
			  __func__, result);
error_msg_send:
error_msg_prep:
	nlmsg_free(nl_msg);
error_msg_alloc:
error_not_any:
	d_fnend(3, wmx, "(wmx %p buf %p size %zu) = %zd\n",
		wmx, buf, size, result);
	return result;
}


/**
 * Get the callback and priv pointer for a MSG_TO_USER message
 *
 * Get the callback and private pointer that will be called by
 * wimaxll_recv() when a MSG_TO_USER is received over generic netlink.
 *
 * \param wmx WiMAX handle.
 * \param cb Where to store the current callback function.
 * \param priv Where to store the private data pointer passed to the
 *     callback.
 *
 * \ingroup the_messaging_interface_group
 */
void wimaxll_get_cb_msg_to_user(
	struct wimaxll_handle *wmx, wimaxll_msg_to_user_cb_f *cb,
	void **priv)
{
	*cb = wmx->msg_to_user_cb;
	*priv = wmx->msg_to_user_priv;
}


/**
 * Set the callback and priv pointer for a MSG_TO_USER message
 *
 * Set the callback and private pointer that will be called by
 * wimaxll_recv() when a MSG_TO_USER is received over generic netlink.
 *
 * \param wmx WiMAX handle.
 * \param cb Callback function to set
 * \param priv Private data pointer to pass to the callback
 *     function (wrap a \a struct wimaxll_cb_ctx in your context
 *     struct and pass a pointer to it; then use wimaxll_container_of()
 *     to extract it back).
 *
 * \ingroup the_messaging_interface_group
 */
void wimaxll_set_cb_msg_to_user(
	struct wimaxll_handle *wmx, wimaxll_msg_to_user_cb_f cb,
	void *priv)
{
	wmx->msg_to_user_cb = cb;
	wmx->msg_to_user_priv = priv;
}
