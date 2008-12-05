/*
 * Linux WiMax
 * Framework for reading from multicast groups
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
 * @defgroup mc_rx Reading form Generic Netlink multicast groups
 *
 * The WiMAX stack sends asynchronous traffic (notifications and
 * messages) to user space through Generic Netlink multicast groups;
 * thus, when reading that traffic from the kernel, libwimaxll
 * actually reads from a generic netlink multicast group.
 *
 * This allows the kernel to send a single notification that can be
 * received by an undetermined (and unbound) number of listeners. As
 * well, this also allows a very flexible way to multiplex different
 * channels without affecting all the listeners.
 *
 * What is called a \e pipe is mapped over one of these multicast
 * groups.
 *
 * \b Example:
 *
 * If a driver wants to send tracing information to an application in
 * user space for analysis, it can create a new \e pipe (Generic
 * Netlink multicast group) and send it over there.
 *
 * The application can listen to it and its traffic volume won't
 * affect other applications listening to other events coming from the
 * same device. Some of these other applications could not be ready
 * ready to cope with such a high traffic.
 *
 * If the same model were implemented just using different netlink
 * messages, all applications listening to events from the driver
 * would be awakened every time any kind of message were sent, even if
 * they do not need to listen to some of those messages.
 *
 * \warning This is a \b very \b low level interface that is for
 *     internal use.
 *
 * \warning If you have to use it in an application, it probably means
 *     something is wrong.
 *
 * \warning You might want to use higher level messaging interfaces,
 *     such as the \ref the_pipe_interface_group "the pipe interface"
 *     or the \ref the_messaging_interface "the messaging interface".
 *
 * \section usage Usage
 *
 * The functions provided by this interface are almost identical than
 * those of the \ref the_pipe_interface_group "pipe interface". The
 * main difference is that wimaxll_mc_rx_read() operates at a lower
 * level.
 *
 * \code
 * int mc_handle;
 * ssize_t bytes;
 * ...
 * mc_handle = wimaxll_mc_rx_open(wimaxll_handle, "name");
 * ...
 * bytes = wimaxll_mc_rx_read(wimaxll_handle, mc_handle);
 * ...
 * wimaxll_mc_rx_close(wimaxll_handle, mc_handle);
 * \endcode
 *
 * \a my_callback is a function that will be called for every valid
 * message received from the kernel on a single call to
 * wimaxll_mc_rx_read().
 *
 * Internally, each \e open pipe/multicast-group contains the list of
 * callbacks for each known message. This is used a look up table for
 * executing them on reception.
 *
 * \section roadmap Roadmap
 *
 * \code
 *
 * wimaxll_mc_rx_open()
 *   wimaxll_mc_idx_by_name()
 *
 * wimaxll_mc_rx_read()
 *   nl_recvmsgs()
 *      wimaxll_seq_check_cb()
 *      wimaxll_gnl_error_cb()
 *      wimaxll_gnl_cb()
 *        wimaxll_gnl_handle_state_change()
 *        wimaxll_gnl_handle_msg_to_user()
 *	  wimaxll_mch_maybe_set_result()
 *
 * wimaxll_mc_rx_fd()
 *   __wimaxll_mc_handle()
 * wimaxll_mc_rx_close()
 * \endcode
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


static
/**
 * Lookup the index for a named multicast group
 *
 * \param wmx WiMAX device handle
 * \param name Name of the multicast group to lookup.
 * \return On success, non-zero positive index for the multicast
 *     group; on error, negative errno code.
 *
 * Look up the index of the named multicast group in the cache
 * obtained at wimaxll_open() time.
 *
 * \internal
 * \ingroup mc_rx
 * \fn int wimaxll_mc_idx_by_name(struct wimaxll_handle *wmx, const char *name)
 */
int wimaxll_mc_idx_by_name(struct wimaxll_handle *wmx, const char *name)
{
	unsigned cnt;
	for (cnt = 0; cnt < WIMAXLL_MC_MAX; cnt++)
		if (!strcmp(wmx->gnl_mc[cnt].name, name))
			return cnt;
	return -EPROTONOSUPPORT;
}


/*
 * Netlink callback for (disabled) sequence check
 *
 * When reading from multicast groups ignore the sequence check, as
 * they are events (as indicated by the netlink documentation; see the
 * documentation on nl_disable_sequence_check(), for example here:
 * http://people.suug.ch/~tgr/libnl/doc-1.1/
 * group__socket.html#g0ff2f43147e3a4547f7109578b3ca422).
 *
 * We need to do this \e manually, as we are using a new callback set
 * group and thus the libnl defaults set by
 * nl_disable_sequence_check() don't apply.
 */
static
int wimaxll_seq_check_cb(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}


static
/**
 * Callback to process a (succesful) message coming from generic
 * netlink
 *
 * \internal
 *
 * Called by nl_recvmsgs() when a valid message is received. We
 * multiplex and handle messages that are known to the library. If the
 * message is unknown, do nothing other than setting -ENODATA.
 *
 * When reading from a pipe with wimaxll_pipe_read(), -ENODATA is
 * considered a retryable error -- effectively, the message is
 * skipped.
 *
 * \fn int wimaxll_gnl_cb(struct nl_msg *msg, void *_mch)
 */
int wimaxll_gnl_cb(struct nl_msg *msg, void *_mch)
{
	ssize_t result;
	struct wimaxll_mc_handle *mch = _mch;
	struct wimaxll_handle *wmx = mch->wmx;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *gnl_hdr;

	d_fnstart(7, wmx, "(msg %p mch %p)\n", msg, mch);
	nl_hdr = nlmsg_hdr(msg);
	gnl_hdr = nlmsg_data(nl_hdr);

	switch (gnl_hdr->cmd) {
	case WIMAX_GNL_OP_MSG_TO_USER:
		if (mch->msg_to_user_cb)
			result = wimaxll_gnl_handle_msg_to_user(wmx, mch, msg);
		else
			goto out_no_handler;
		break;
	case WIMAX_GNL_RE_STATE_CHANGE:
		if (mch->state_change_cb)
			result = wimaxll_gnl_handle_state_change(wmx, mch, msg);
		else
			goto out_no_handler;
		break;
	default:
		goto error_unknown_msg;
	}
	wimaxll_mch_maybe_set_result(mch, 0);
	d_fnend(7, wmx, "(msg %p mch %p) = %zd\n", msg, mch, result);
	return result;

error_unknown_msg:
	d_printf(1, wmx, "E: %s: received unknown gnl message %d\n",
		 __func__, gnl_hdr->cmd);
out_no_handler:
	wimaxll_mch_maybe_set_result(mch, -ENODATA);
	result = NL_SKIP;
	d_fnend(7, wmx, "(msg %p mch %p) = %zd\n", msg, mch, result);
	return result;
}


/**
 * Open a handle for reception from a multicast group
 *
 * \param wmx WiMAX device handle
 * \param mc_name Name of the multicast group that has to be opened
 *
 * \return If successful, a non-negative handle number (\e { the
 *     multicast group descriptor}), to be given to other functions
 *     for actual operation. In case of error, a negative errno code.
 *
 * Allocates a handle to use for reception of data on from a single
 * multicast group.
 *
 * Only one handle may be opened at the same time to each multicast
 * group.
 *
 * \ingroup mc_rx
 */
int wimaxll_mc_rx_open(struct wimaxll_handle *wmx,
		       const char *mc_name)
{
	int result, idx;
	struct wimaxll_mc_handle *mch;

	d_fnstart(3, wmx, "(wmx %p mc_name %s)\n", wmx, mc_name);
	idx = wimaxll_mc_idx_by_name(wmx, mc_name);
	if (idx < 0) {
		result = idx;
		wimaxll_msg(wmx, "E: mc group \"%s\" "
			  "not supported: %d\n", mc_name, result);
		goto error_mc_idx_by_name;
	}
	d_printf(2, wmx, "D: idx is %d\n", idx);
	result = -EBUSY;
	if (wmx->gnl_mc[idx].mch) {
		wimaxll_msg(wmx, "E: BUG! trying to open handle to multicast "
			  "group \"%s\", which is already open\n", mc_name);
		goto error_reopen;
	}

	/* Alloc a new multicast group handle */
	result = -ENOMEM;
	mch = malloc(sizeof(*mch));
	if (mch == NULL) {
		wimaxll_msg(wmx, "E: mc group %s: cannot allocate handle\n",
			  mc_name);
		goto error_alloc;
	}
	memset(mch, 0, sizeof(*mch));
	mch->wmx = wmx;
	mch->nlh_rx = nl_handle_alloc();
	if (mch->nlh_rx == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: mc group %s: cannot allocate RX netlink "
			  "handle: %d\n", mc_name, result);
		goto error_nl_handle_alloc_rx;
	}
	result = nl_connect(mch->nlh_rx, NETLINK_GENERIC);
	if (result < 0) {
		wimaxll_msg(wmx, "E: mc group %s: cannot connect RX netlink: "
			  "%d\n", mc_name, result);
		goto error_nl_connect_rx;
	}

	result = nl_socket_add_membership(mch->nlh_rx, wmx->gnl_mc[idx].id);
	if (result < 0) {
		wimaxll_msg(wmx, "E: mc group %s: cannot join multicast group "
			  "%u: %d\n", mc_name, wmx->gnl_mc[idx].id, result);
		goto error_nl_add_membership;
	}

	mch->nl_cb = nl_cb_alloc(NL_CB_VERBOSE);
	if (mch->nl_cb == NULL) {
		result = -ENOMEM;
		wimaxll_msg(wmx, "E: mc group %s: cannot allocate callback\n",
			  mc_name);
		goto error_cb_alloc;
	}

	nl_cb_set(mch->nl_cb, NL_CB_ACK, NL_CB_CUSTOM, wimaxll_gnl_ack_cb, mch);
	nl_cb_set(mch->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  wimaxll_seq_check_cb, NULL);
	nl_cb_set(mch->nl_cb, NL_CB_VALID, NL_CB_CUSTOM, wimaxll_gnl_cb, mch);
	nl_cb_err(mch->nl_cb, NL_CB_CUSTOM, wimaxll_gnl_error_cb, mch);
	wmx->gnl_mc[idx].mch = mch;
	d_fnend(3, wmx, "(wmx %p mc_name %s) = %d\n", wmx, mc_name, idx);
	return idx;

error_cb_alloc:
	/* No need to drop membership, it is removed when we close the
	 * handle */
error_nl_add_membership:
	nl_close(mch->nlh_rx);
error_nl_connect_rx:
	nl_handle_destroy(mch->nlh_rx);
error_nl_handle_alloc_rx:
	free(mch);
error_alloc:
error_reopen:
error_mc_idx_by_name:
	d_fnend(3, wmx, "(wmx %p mc_name %s) = %d\n", wmx, mc_name, result);
	return result;
}


/**
 * Close a multicast group handle
 *
 * \param wmx WiMAX handle
 * \param idx Multicast group handle (as returned by wimaxll_mc_rx_open()).
 *
 * Releases resources associated to open multicast group handle.
 *
 * \ingroup mc_rx
 */
void wimaxll_mc_rx_close(struct wimaxll_handle *wmx, unsigned idx)
{
	struct wimaxll_mc_handle *mch;
	d_fnstart(3, wmx, "(wmx %p idx %u)\n", wmx, idx);
	if (idx >= WIMAXLL_MC_MAX) {
		wimaxll_msg(wmx, "E: BUG! multicast group index %u "
			  "higher than allowed maximum %u\n",
			  idx, WIMAXLL_MC_MAX);
		goto out;
	}
	mch = wmx->gnl_mc[idx].mch;
	wmx->gnl_mc[idx].mch = NULL;
	nl_cb_put(mch->nl_cb);
	/* No need to drop handle membership to the msg group, closing
	 * it does it */
	nl_close(mch->nlh_rx);
	nl_handle_destroy(mch->nlh_rx);
	free(mch);
out:
	d_fnend(3, wmx, "(wmx %p idx %u) = void\n", wmx, idx);
}


/**
 * Return the multicast group handle associated to a Pipe ID
 *
 * \internal
 *
 * \param wmx WiMAX device handle
 * \param pipe_id Multicast group ID, as returned by
 *     wimaxll_mc_rx_open().
 * \return file descriptor associated to the multicast group, that can
 *     be fed to functions like select().
 *
 * \ingroup mc_rx
 */
struct wimaxll_mc_handle *__wimaxll_get_mc_handle(struct wimaxll_handle *wmx,
					      int pipe_id)
{
	struct wimaxll_mc_handle *mch = NULL;

	if (pipe_id >= WIMAXLL_MC_MAX) {
		wimaxll_msg(wmx, "E: BUG! mc group #%u does not exist!\n",
			  pipe_id);
		goto error;
	}
	mch = wmx->gnl_mc[pipe_id].mch;
	if (mch == NULL) {
		wimaxll_msg(wmx, "E: BUG! trying to read from non-opened "
			  "mc group #%u\n", pipe_id);
		goto error;
	}
error:
	return mch;
}


/**
 * Return the file descriptor associated to a multicast group
 *
 * \param wmx WiMAX handle
 * \param pipe_id Multicast group handle, as returned by
 *     wimaxll_mc_rx_open().
 * \return file descriptor associated to the multicast group, that can
 *     be fed to functions like select().
 *
 * This allows to select() on the file descriptor, which will block
 * until a message is available, that then can be read with
 * wimaxll_mc_rx_read().
 *
 * \ingroup mc_rx
 */
int wimaxll_mc_rx_fd(struct wimaxll_handle *wmx, unsigned pipe_id)
{
	int result = -EBADFD;
	struct wimaxll_mc_handle *mch;

	d_fnstart(3, wmx, "(wmx %p pipe_id %u)\n", wmx, pipe_id);
	mch = __wimaxll_get_mc_handle(wmx, pipe_id);
	if (mch != NULL)
		result = nl_socket_get_fd(mch->nlh_rx);
	d_fnend(3, wmx, "(wmx %p pipe_id %u) = %zd\n", wmx, pipe_id, result);
	return result;
}


/**
 * Read from a multicast group
 *
 * \param wmx WiMAX device handle
 * \param index Multicast group handle, as returned by
 *     wimaxll_mc_rx_open().
 * \return Value returned by the callback functions (depending on the
 *     implementation of the callback). On error, a negative errno
 *     code:
 *
 *     -%EINPROGRESS: the message was not received.
 *
 *     -%ENODATA: messages were received, but none of the known types.
 *
 * Read one or more messages from a multicast group and for each valid
 * one, execute the callbacks set in the multi cast handle.
 *
 * The callbacks are expected to handle the messages and set
 * information in the context specific to the mc handle
 * (mch->cb_ctx). In case of any type of errors (cb_ctx.result < 0),
 * it is expected that no resources will be tied to the context.
 *
 * \remarks This is a blocking call.
 *
 * \ingroup mc_rx
 *
 * \internal
 *
 * This calls nl_recvmsgs() on the handle specific to a multi-cast
 * group; wimaxll_gnl_cb() will be called for succesfully received
 * generic netlink messages from the kernel and execute the callbacks
 * for each.
 */
ssize_t wimaxll_mc_rx_read(struct wimaxll_handle *wmx, unsigned index)
{
	ssize_t result;
	struct wimaxll_mc_handle *mch;

	d_fnstart(3, wmx, "(wmx %p index %u)\n", wmx, index);
	if (index >= WIMAXLL_MC_MAX) {
		wimaxll_msg(wmx, "E: BUG! mc group #%u does not exist!\n",
			    index);
		result = -EINVAL;
		goto error_bad_index;
	}
	mch = wmx->gnl_mc[index].mch;
	if (mch == NULL) {
		wimaxll_msg(wmx, "E: BUG! trying to read from non-opened "
			  "mc group #%u\n", index);
		result = -EBADF;
		goto error_not_open;
	}

	/*
	 * The reading and processing happens here
	 *
	 * libnl's nl_recvmsgs() will read and call the different
	 * callbacks we specified at wimaxll_mc_rx_open() time. That's
	 * where the processing of the message content is done.
	 *
	 * Now, messages from the kernel don't carry ACKs or NLERRs,
	 * so we are just receiving a message packet all the
	 * time--except if things go wrong. 
	 */
	mch->result = -EINPROGRESS;
	mch->msg_done = 0;
	d_printf(2, wmx, "I: Calling nl_recvmsgs()\n");
	result = nl_recvmsgs(mch->nlh_rx, mch->nl_cb);
	if (result < 0)
		wimaxll_msg(wmx, "E: %s: nl_recvmgsgs failed: %d\n",
			    __func__, result);
	else
		result = mch->result;
	/* No complains on error; the kernel might just be sending an
	 * error out; pass it through. */
error_not_open:
error_bad_index:
	d_fnend(3, wmx, "(wmx %p index %u) = %zd\n", wmx, index, result);
	return result;
}
