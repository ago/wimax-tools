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
 * This interfaces builds on the \e kernel-to-user unidirectional \ref
 * the_pipe_interface_group "pipe interface". It writes data to the default
 * \e message pipe by sending it to the WiMAX kernel stack, which
 * passes it to the driver using the \e wimax_dev->op_msg_from_user()
 * call.
 *
 * Only the default \e message pipe is bidirectional; WiMAX kernel
 * drivers receive messages sent with wimax_msg_write().
 *
 * \note The wimaxll_msg_fd() and wimaxll_msg_read() functions operate
 * on the default \e message pipe, being convenience functions for
 * wimaxll_pipe_fd() and wimaxll_pipe_msg_read().
 *
 * To wait for a message from the driver:
 *
 * @code
 *  void *msg;
 *  ...
 *  size = wimaxll_msg_read(wmx, &msg);
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
 * To write messages to the driver:
 *
 * @code
 *  wimaxll_msg_write(wmx, buf, buf_size);
 * @endcode
 *
 * where \a buf points to where the message is stored.
 *
 * \note Messages can be written to the driver \e only over the
 *     default \e message pipe. Thus, no wimax_pipe_msg_write()
 *     function is available.
 *
 * All functions return negative \a errno codes on error.
 *
 * To integrate message reception into a mainloop, \ref callbacks
 * "callbacks" and select() should be used. The file descriptor
 * associated to the default \e message \e pipe can be obtained with
 * wimaxll_msg_fd(). When there is activity on the file descriptor,
 * wimaxll_pipe_read() should be called on the default pipe:
 *
 * \code
 * wimax_pipe_read(wmx, wimax_msg_pipe_id(wmx));
 * \endcode
 *
 * this will, as explained in \ref receiving, for each received
 * notification, execute its callback.
 *
 * The callback for reception of messages from the WiMAX kernel stack
 * can be set with wimaxll_pipe_set_cb_msg_to_user() (using as \e
 * pipe_id the value returned by wimax_msg_pipe_id()). For detailed
 * information on the message reception callback, see the definition
 * of \ref wimaxll_msg_to_user_cb_f.
 *
 * The kernel WiMAX stack allows drivers to create any number of pipes
 * on which to send information (messages) to user space. This
 * interface provides means to read those messages, which are mostly
 * device specific.
 *
 * This is a lower level interface than \ref the_messaging_interface
 * "the messaging interface"; however, it operates similarly.
 *
 * @code
 * void *msg;
 * ...
 * handle = wimaxll_pipe_open(wmx, "PIPENAME");
 * ...
 * wimaxll_pipe_msg_read(wmx, handle, &msg);
 * ...
 * wimaxll_msg_free(msg);
 * ...
 * wimaxll_pipe_close(wmx, handle);
 * @endcode
 *
 * More information about the details of this interface can be found
 * \ref the_pipe_interface_group "here".
 *
 * \note These pipes are not bidirectional.
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
 *
 */
static
struct nla_policy wimaxll_gnl_msg_from_user_policy[WIMAX_GNL_ATTR_MAX + 1] = {
	[WIMAX_GNL_MSG_DATA] = {
		.type = NLA_UNSPEC,
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
 * \return \c enum nl_cb_action
 *
 * wimaxll_mc_rx_read() calls libnl's nl_recvmsgs() to receive messages;
 * when a valid message is received, it goes into a loop that selects
 * a callback to run for each type of message and it will call this
 * function.
 *
 * This just expects a _MSG_TO_USER message, whose payload is what
 * has to be passed to the caller. Because nl_recvmsgs() will free the
 * message data, a new buffer has to be allocated and copied (a patch
 * has been merged already to future versions of libnl that helps in
 * this).
 *
 * It stores the buffer and size (or result in case of error) in the
 * context passed in \e mch->msg_to_user_context.
 */
int wimaxll_gnl_handle_msg_to_user(struct wimaxll_handle *wmx,
				 struct wimaxll_mc_handle *mch,
				 struct nl_msg *msg)
{
	size_t size;
	ssize_t result;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *gnl_hdr;
	struct nlattr *tb[WIMAX_GNL_ATTR_MAX+1];
	struct wimaxll_gnl_cb_context *ctx = mch->msg_to_user_context;
	void *data;

	d_fnstart(7, wmx, "(wmx %p mch %p msg %p)\n", wmx, mch, msg);
	nl_hdr = nlmsg_hdr(msg);
	gnl_hdr = nlmsg_data(nl_hdr);

	assert(gnl_hdr->cmd == WIMAX_GNL_OP_MSG_TO_USER);

	/* Parse the attributes */
	result = genlmsg_parse(nl_hdr, 0, tb, WIMAX_GNL_ATTR_MAX,
			       wimaxll_gnl_msg_from_user_policy);
	if (result < 0) {
		wimaxll_msg(wmx, "E: %s: genlmsg_parse() failed: %d\n",
			  __func__, result);
		wimaxll_cb_context_set_result(ctx, result);
		result = NL_SKIP;
		goto error_parse;
	}
	if (tb[WIMAX_GNL_MSG_DATA] == NULL) {
		wimaxll_msg(wmx, "E: %s: cannot find MSG_DATA attribute\n",
			  __func__);
		wimaxll_cb_context_set_result(ctx, -ENXIO);
		result = NL_SKIP;
		goto error_no_attrs;

	}
	wimaxll_cb_context_set_result(ctx, 0);

	size = nla_len(tb[WIMAX_GNL_MSG_DATA]);
	data = nla_data(tb[WIMAX_GNL_MSG_DATA]);

	d_printf(1, wmx, "D: CRX genlmsghdr cmd %u version %u\n",
		 gnl_hdr->cmd, gnl_hdr->version);
	d_printf(1, wmx, "D: CRX msg from kernel %u bytes\n", size);
	d_dump(2, wmx, data, size);

	/* This was set by whoever called nl_recmvsgs (or
	 * wimaxll_mc_rx_read() or wimaxll_pipe_read()) */
	if (mch->msg_to_user_cb(wmx, ctx, data, size) == -EBUSY)
		result = NL_STOP;
	else
		result = NL_OK;
error_no_attrs:
error_parse:
	d_fnend(7, wmx, "(wmx %p mch %p msg %p) = %d\n", wmx, mch, msg, result);
	return result;
}


struct wimaxll_cb_msg_to_user_context {
	struct wimaxll_gnl_cb_context ctx;
	void *data;
};


/*
 * Default handling of messages
 *
 * When someone calls wimaxll_msg_read() or wimaxll_pipe_msg_read(), those
 * functions set this default callback, which will just copy the data
 * to a buffer and pass that pointer to the caller along with the size.
 */
static
int wimaxll_cb_msg_to_user(struct wimaxll_handle *wmx,
			 struct wimaxll_gnl_cb_context *ctx,
			 const char *data, size_t data_size)
{
	struct wimaxll_cb_msg_to_user_context *mtu_ctx =
		wimaxll_container_of(
			ctx, struct wimaxll_cb_msg_to_user_context, ctx);

	if (mtu_ctx->data)
		return -EBUSY;
	mtu_ctx->data = malloc(data_size);
	if (mtu_ctx->data) {
		memcpy(mtu_ctx->data, data, data_size);
		ctx->result = data_size;
	} else
		ctx->result = -ENOMEM;
	return 0;
}


/**
 * Read a message from any WiMAX kernel-user pipe
 *
 * \param wmx WiMAX device handle
 * \param pipe_id Pipe to read from (as returned by
 *     wimaxll_pipe_open()). To use the default pipe, indicate
 *     use wimax_msg_pipe_id().
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
 */
ssize_t wimaxll_pipe_msg_read(struct wimaxll_handle *wmx, unsigned pipe_id,
			    void **buf)
{
	ssize_t result;
	struct wimaxll_cb_msg_to_user_context mtu_ctx = {
		.ctx = WIMAXLL_GNL_CB_CONTEXT_INIT(wmx),
		.data = NULL,
	};
	wimaxll_msg_to_user_cb_f prev_cb = NULL;
	struct wimaxll_gnl_cb_context *prev_priv = NULL;

	d_fnstart(3, wmx, "(wmx %p buf %p)\n", wmx, buf);
	wimaxll_pipe_get_cb_msg_to_user(wmx, pipe_id, &prev_cb, &prev_priv);
	wimaxll_pipe_set_cb_msg_to_user(wmx, pipe_id,
				      wimaxll_cb_msg_to_user, &mtu_ctx.ctx);
	result = wimaxll_pipe_read(wmx, pipe_id);
	if (result >= 0) {
		*buf = mtu_ctx.data;
		result = mtu_ctx.ctx.result;
	}
	wimaxll_pipe_set_cb_msg_to_user(wmx, pipe_id, prev_cb, prev_priv);
	d_fnend(3, wmx, "(wmx %p buf %p) = %zd\n", wmx, buf, result);
	return result;
}


/**
 * Free a message received with wimaxll_pipe_msg_read() or
 * wimaxll_msg_read()
 *
 * \param msg message pointer returned by wimaxll_pipe_msg_read() or
 *     wimaxll_msg_read().
 *
 * \note this function is the same as wimaxll_msg_free()
 *
 * \ingroup the_messaging_interface
 */
void wimaxll_pipe_msg_free(void *msg)
{
	d_fnstart(3, NULL, "(msg %p)\n", msg);
	free(msg);
	d_fnend(3, NULL, "(msg %p) = void\n", msg);
}


/**
 * Return the file descriptor associated to the default \e message pipe
 *
 * \param wmx WiMAX device handle
 * \return file descriptor associated to the messaging group, that can
 *     be fed to functions like select().
 *
 * This allows to select() on the file descriptor, which will block
 * until a message is available, that then can be read with
 * wimaxll_pipe_read().
 *
 * \ingroup the_messaging_interface
 */
int wimaxll_msg_fd(struct wimaxll_handle *wmx)
{
	return wimaxll_mc_rx_fd(wmx, wmx->mc_msg);
}


/**
 * Read a message from the WiMAX default \e message pipe.
 *
 * \param wmx WiMAX device handle
 * \param buf Somewhere where to store the pointer to the message data.
 * \return If successful, a positive (and \c *buf set) or zero size of
 *     the message; on error, a negative \a errno code (\c buf
 *     n/a).
 *
 * Returns a message allocated in \c *buf as sent by the kernel via
 * the default \e message pipe. The message is allocated by the
 * library and owned by the caller. When done, it has to be freed with
 * wimaxll_msg_free() to release the space allocated to it.
 *
 * \note This is a blocking call.
 *
 * \ingroup the_messaging_interface
 */
ssize_t wimaxll_msg_read(struct wimaxll_handle *wmx, void **buf)
{
	return wimaxll_pipe_msg_read(wmx, wmx->mc_msg, buf);
}


/**
 * Free a message received with wimaxll_pipe_msg_read() or
 * wimaxll_msg_read()
 *
 * \param msg message pointer returned by wimaxll_pipe_msg_read() or
 *     wimaxll_msg_read().
 *
 * \note this function is the same as wimaxll_pipe_msg_free()
 *
 * \ingroup the_messaging_interface
 */
void wimaxll_msg_free(void *msg)
{
	wimaxll_pipe_msg_free(msg);
}


/**
 * Send a driver-specific message to a WiMAX device
 *
 * \param wmx wimax device descriptor
 * \param buf Pointer to the wimax message.
 * \param size size of the message.
 *
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
			  const void *buf, size_t size)
{
	ssize_t result;
	struct nl_msg *nl_msg;
	void *msg;

	d_fnstart(3, wmx, "(wmx %p buf %p size %zu)\n", wmx, buf, size);
	nl_msg = nlmsg_new();
	if (nl_msg == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: cannot allocate generic netlink "
			  "message: %m\n");
		goto error_msg_alloc;
	}
	msg = genlmsg_put(nl_msg, NL_AUTO_PID, NL_AUTO_SEQ,
			  wimaxll_family_id(wmx), 0, 0,
			  WIMAX_GNL_OP_MSG_FROM_USER, WIMAX_GNL_VERSION);
	if (msg == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: %s: error preparing message: %d\n",
			  __func__, result);
		goto error_msg_prep;
	}

	nla_put(nl_msg, WIMAX_GNL_MSG_DATA, size, buf);

	d_printf(5, wmx, "D: CTX nl + genl header:\n");
	d_dump(5, wmx, nlmsg_hdr(nl_msg),
	       sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr));
	d_printf(5, wmx, "D: CTX wimax message:\n");
	d_dump(5, wmx, buf, size);

	result = nl_send_auto_complete(wmx->nlh_tx, nl_msg);
	if (result < 0) {
		wimaxll_msg(wmx, "E: error sending message: %d\n", result);
		goto error_msg_send;
	}

	result = wimaxll_wait_for_ack(wmx);	/* Get the ACK from netlink */
	if (result < 0)
		wimaxll_msg(wmx, "E: %s: generic netlink ack failed: %d\n",
			  __func__, result);
error_msg_send:
error_msg_prep:
	nlmsg_free(nl_msg);
error_msg_alloc:
	d_fnend(3, wmx, "(wmx %p buf %p size %zu) = %zd\n",
		wmx, buf, size, result);
	return result;
}


/**
 * Return the pipe ID for the messaging interface
 *
 * @param wmx WiMAX device descriptor
 * @return Pipe id of the messaging interface, that can be used with
 *     the wimaxll_pipe_*() functions.
 *
 * \ingroup the_messaging_interface_group
 */
unsigned wimaxll_msg_pipe_id(struct wimaxll_handle *wmx)
{
	return wmx->mc_msg;
}


/**
 * Get the callback and priv pointer for a MSG_TO_USER message
 *
 * \param wmx WiMAX handle.
 * \param pipe_id Pipe on which to listen for the message [as returned
 *     by wimaxll_pipe_open()].
 * \param cb Where to store the current callback function.
 * \param context Where to store the private data pointer passed to the
 *     callback.
 *
 * \ingroup the_messaging_interface_group
 */
void wimaxll_pipe_get_cb_msg_to_user(
	struct wimaxll_handle *wmx, unsigned pipe_id,
	wimaxll_msg_to_user_cb_f *cb, struct wimaxll_gnl_cb_context **context)
{
	struct wimaxll_mc_handle *mch;

	mch = __wimaxll_get_mc_handle(wmx, pipe_id);
	if (mch != NULL) {
		*cb = mch->msg_to_user_cb;
		*context = mch->msg_to_user_context;
	}
}


/**
 * Set the callback and priv pointer for a MSG_TO_USER message
 *
 * \param wmx WiMAX handle.
 * \param pipe_id Pipe on which to listen for the message [as returned
 *     by wimaxll_pipe_open()].
 * \param cb Callback function to set
 * \param context Private data pointer to pass to the callback
 *     function (wrap a \a struct wimaxll_gnl_cb_context in your context
 *     struct and pass a pointer to it; then use wimaxll_container_of()
 *     to extract it back).
 *
 * \ingroup the_messaging_interface_group
 */
void wimaxll_pipe_set_cb_msg_to_user(
	struct wimaxll_handle *wmx, unsigned pipe_id,
	wimaxll_msg_to_user_cb_f cb, struct wimaxll_gnl_cb_context *context)
{
	struct wimaxll_mc_handle *mch;

	mch = __wimaxll_get_mc_handle(wmx, pipe_id);
	if (mch != NULL) {
		mch->msg_to_user_cb = cb;
		mch->msg_to_user_context = context;
	}
}


void wimax_msg_fd() __attribute__ ((weak, alias("wimaxll_msg_fd")));
void wimax_msg_read() __attribute__ ((weak, alias("wimaxll_msg_read")));
void wimax_msg_write() __attribute__ ((weak, alias("wimaxll_msg_write")));
void wimax_msg_free() __attribute__ ((weak, alias("wimaxll_msg_free")));
void wimax_msg_pipe_id() __attribute__ ((weak, alias("wimaxll_msg_pipe_id")));
void wimax_pipe_msg_read()
	__attribute__ ((weak, alias("wimaxll_pipe_msg_read")));
void wimax_pipe_msg_free()
	__attribute__ ((weak, alias("wimaxll_pipe_msg_free")));
void wimax_pipe_get_cb_msg_to_user()
	__attribute__ ((weak, alias("wimaxll_pipe_get_cb_msg_to_user")));
void wimax_pipe_set_cb_msg_to_user()
	__attribute__ ((weak, alias("wimaxll_pipe_set_cb_msg_to_user")));
