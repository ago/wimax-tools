/*
 * Linux WiMAX
 * Opening handles
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
 * @defgroup device_management WiMAX device management
 *
 * The main device management operations are wimaxll_open(),
 * wimaxll_close() and wimax_reset().
 *
 * It is allowed to have more than one handle opened at the same
 * time.
 *
 * Use wimaxll_ifname() to obtain the name of the WiMAX interface a
 * handle is open for.
 *
 * \internal
 * \section Roadmap Roadmap
 *
 * \code
 * wimaxll_open()
 *   wimaxll_gnl_resolve()
 *   wimaxll_mc_rx_open()
 *
 * wimaxll_close()
 *   wimaxll_mc_rx_close()
 *   wimaxll_free()
 *
 * wimaxll_ifname()
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
#include <net/if.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <wimaxll.h>
#include "internal.h"
#define D_LOCAL 0
#include "debug.h"


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
 * In wimaxll_recv(), -ENODATA is considered a retryable error --
 * effectively, the message is skipped.
 *
 * The wimaxll_gnl_handle_*() functions need to return:
 *
 * - >= 0 to indicate message processing should continue
 * - -EBUSY to indicate message processing should stop
 * - any other < 0 error code to indicate an error and that the
 *   message should be skipped.
 *
 * \fn int wimaxll_gnl_cb(struct nl_msg *msg, void *_ctx)
 */
int wimaxll_gnl_cb(struct nl_msg *msg, void *_ctx)
{
	ssize_t result;
	enum nl_cb_action result_nl;
	struct wimaxll_cb_ctx *ctx = _ctx;
	struct wimaxll_handle *wmx = ctx->wmx;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *gnl_hdr;

	d_fnstart(3, wmx, "(msg %p wmx %p)\n", msg, wmx);
	nl_hdr = nlmsg_hdr(msg);
	gnl_hdr = nlmsg_data(nl_hdr);

	d_printf(3, wmx, "E: %s: received gnl message %d\n",
		 __func__, gnl_hdr->cmd);
	switch (gnl_hdr->cmd) {
	case WIMAX_GNL_OP_MSG_TO_USER:
		if (wmx->msg_to_user_cb)
			result = wimaxll_gnl_handle_msg_to_user(wmx, msg);
		else
			result = 0;
		break;
	case WIMAX_GNL_RE_STATE_CHANGE:
		if (wmx->state_change_cb)
			result = wimaxll_gnl_handle_state_change(wmx, msg);
		else
			result = 0;
		break;
	default:
		d_printf(3, wmx, "E: %s: received unknown gnl message %d\n",
			 __func__, gnl_hdr->cmd);
		result = -ENODATA;
	}
	if (result == -EBUSY) {		/* stop signal from the user's callback */
		result_nl = NL_STOP;
		result = 0;
	}
	else if (result < 0)
		result_nl = NL_SKIP;
	else
		result_nl = NL_OK;
	wimaxll_cb_maybe_set_result(ctx, result);	
	d_fnend(3, wmx, "(msg %p ctx %p) = %zd\n", msg, ctx, result);
	return result_nl;
}


/**
 * Return the file descriptor associated to a WiMAX handle
 *
 * \param wmx WiMAX device handle
 *
 * \return file descriptor associated to the handle can be fed to
 *     functions like select() to wait for notifications to be ready..
 *
 * This allows to select() on the file descriptor, which will block
 * until a message is available, that then can be read with
 * wimaxll_recv().
 *
 * \ingroup the_messaging_interface
 */
int wimaxll_recv_fd(struct wimaxll_handle *wmx)
{
	return nl_socket_get_fd(wmx->nlh_rx);
}


/**
 * Read notifications from the WiMAX multicast group
 *
 * \param wmx WiMAX device handle
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
ssize_t wimaxll_recv(struct wimaxll_handle *wmx)
{
	ssize_t result;
	struct wimaxll_cb_ctx ctx = WIMAXLL_CB_CTX_INIT(wmx);
	struct nl_cb *cb;

	d_fnstart(3, wmx, "(wmx %p)\n", wmx);

	/*
	 * The reading and processing happens here
	 *
	 * libnl's nl_recvmsgs() will read and call the different
	 * callbacks we specified wimaxll_open() time. That's where
	 * the processing of the message content is done.
	 */
	cb = nl_socket_get_cb(wmx->nlh_rx);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wimaxll_gnl_ack_cb, &ctx);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  wimaxll_seq_check_cb, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wimaxll_gnl_cb, &ctx);
	nl_cb_err(cb, NL_CB_CUSTOM, wimaxll_gnl_error_cb, &ctx);
	d_printf(2, wmx, "I: Calling nl_recvmsgs()\n");
	do {
		ctx.result = -EINPROGRESS;
		result = nl_recvmsgs(wmx->nlh_rx, cb);
		d_printf(3, wmx, "I: ctx.result %zd result %zd\n",
			 ctx.result, result);
	} while ((ctx.result == -EINPROGRESS || ctx.result == -ENODATA)
		 && result > 0);
	if (result < 0)
		wimaxll_msg(wmx, "E: %s: nl_recvmgsgs failed: %zd\n",
			    __func__, result);
	else
		result = ctx.result;
	/* No complains on error; the kernel might just be sending an
	 * error out; pass it through. */
	d_fnend(3, wmx, "(wmx %p) = %zd\n", wmx, result);
	return result;
}


static
void wimaxll_mc_group_cb(void *_wmx, const char *name, int id)
{
	struct wimaxll_handle *wmx = _wmx;

	if (strcmp(name, "msg"))
		return;
	wmx->mcg_id = id;
}


static
int wimaxll_gnl_resolve(struct wimaxll_handle *wmx)
{
	int result, version;
	unsigned major, minor;

	d_fnstart(5, wmx, "(wmx %p)\n", wmx);
	/* Lookup the generic netlink family */
	result = genl_ctrl_resolve(wmx->nlh_tx, "WiMAX");
	if (result < 0) {
		wimaxll_msg(wmx, "E: can't find kernel's WiMAX API "
			    "over genetic netlink: %d\n", result);
		goto error_ctrl_resolve;
	}
	wmx->gnl_family_id = result;
	d_printf(1, wmx, "D: WiMAX device %s, genl family ID %d\n",
		 wmx->name, wmx->gnl_family_id);
	wmx->mcg_id = -1;
	nl_get_multicast_groups(wmx->nlh_tx, "WiMAX", wimaxll_mc_group_cb, wmx);
	if (wmx->mcg_id == -1) {
		wimaxll_msg(wmx, "E: %s: cannot resolve multicast group ID; "
			  "your kernel might be too old (< 2.6.23).\n",
			  wmx->name);
		result = -ENXIO;
		goto error_mcg_resolve;
	}

	version = genl_ctrl_get_version(wmx->nlh_tx, "WiMAX");
	/* Check version compatibility -- check include/linux/wimax.h
	 * for a complete description. The idea is to allow for good
	 * expandability of the interface without causing breakage. */
	major = version / 10;
	minor = version % 10;
	if (major != WIMAX_GNL_VERSION / 10) {
		result = -EBADR;
		wimaxll_msg(wmx, "E: kernel's major WiMAX GNL interface "
			    "version (%d) is different that supported %d; "
			    "aborting\n", major, WIMAX_GNL_VERSION / 10);
		goto error_bad_major;
	}
	if (minor < WIMAX_GNL_VERSION % 10)
		wimaxll_msg(wmx, "W: kernel's minor WiMAX GNL interface "
			    "version (%d) is lower that supported %d; things "
			    "might not work\n", minor, WIMAX_GNL_VERSION % 10);
error_bad_major:
error_mcg_resolve:
error_ctrl_resolve:
	d_fnend(5, wmx, "(wmx %p) = %d\n", wmx, result);
	return result;
}


static
void wimaxll_free(struct wimaxll_handle *wmx)
{
	free(wmx);
}


/**
 * Open a handle to the WiMAX control interface in the kernel
 *
 * \param device device name of the WiMAX network interface; to
 *     specify an interface by index, use the name "#IFNAME". To open
 *     a handle that will receive data for any device (but not allow
 *     sending commands to devices), set %NULL.
 * 
 * \return WiMAX device handle on success; on error, %NULL is returned
 *     and the \a errno variable is updated with a corresponding
 *     negative value.
 *
 * When opening the handle to the device, a basic check of API
 * versioning will be done. If the kernel interface has a different
 * major version, the \c wimaxll_open() call will fail (existing
 * interfaces modified or removed). A higher kernel minor version is
 * allowed (new interfaces added); a lower kernel minor version is
 * allowed with a warning (the library might need interfaces that are
 * not in the kernel).
 *
 * \ingroup device_management
 * \internal
 *
 * Allocates the netlink handles needed to talk to the kernel. With
 * that, looks up the Generic Netlink Family ID associated (if none,
 * it's not a WiMAX device). This is done by querying generic netlink
 * for a family called "WiMAX <DEVNAME>".
 *
 * Then the Generic Netlink Controller is used to lookup versioning
 * and the multicast group list that conform the different pipes
 * supported by an interface.
 */
struct wimaxll_handle *wimaxll_open(const char *device)
{
	int result;
	struct wimaxll_handle *wmx;

	d_fnstart(3, NULL, "(device %s)\n", device);
	result = ENOMEM;
	wmx = malloc(sizeof(*wmx));
	if (wmx == NULL) {
		wimaxll_msg(NULL, "E: cannot allocate WiMax handle: %m\n");
		goto error_gnl_handle_alloc;
	}
	memset(wmx, 0, sizeof(*wmx));
	if (device != NULL && sscanf(device, "#%u", &wmx->ifidx) == 1) {
		/* Open by interface index "#IFINDEX" */
		if (if_indextoname(wmx->ifidx, wmx->name) == NULL) {
			wimaxll_msg(wmx, "E: device index #%u does not exist\n",
				    wmx->ifidx);
			result = -ENODEV;
			goto error_no_dev;
		}
	} else if (device != NULL) {
		/* Open by interface name */
		strncpy(wmx->name, device, sizeof(wmx->name));
		wmx->ifidx = if_nametoindex(wmx->name);
		if (wmx->ifidx == 0) {
			wimaxll_msg(wmx, "E: device %s does not exist\n", wmx->name);
			result = -ENODEV;
			goto error_no_dev;
		}
	} else {
		/* "Any" device (just for receiving callbacks) */
		wmx->name[0] = 0;
		wmx->ifidx = 0;
	}

	/* Setup the TX side */
	wmx->nlh_tx = nl_handle_alloc();
	if (wmx->nlh_tx == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: TX: cannot allocate handle: %d (%s)\n",
			    result, nl_geterror());
		goto error_nl_handle_alloc_tx;
	}
	result = nl_connect(wmx->nlh_tx, NETLINK_GENERIC);
	if (result < 0) {
		wimaxll_msg(wmx, "E: TX: cannot connect netlink: %d (%s)\n",
			    result, nl_geterror());
		goto error_nl_connect_tx;
	}

	/* Set up the RX side */
	wmx->nlh_rx = nl_handle_alloc();
	if (wmx->nlh_rx == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: RX: cannot allocate handle: %d (%s)\n",
			    result, nl_geterror());
		goto error_nl_handle_alloc_rx;
	}
	result = nl_connect(wmx->nlh_rx, NETLINK_GENERIC);
	if (result < 0) {
		wimaxll_msg(wmx, "E: RX: cannot connect netlink: %d (%s)\n",
			    result, nl_geterror());
		goto error_nl_connect_rx;
	}

	result = wimaxll_gnl_resolve(wmx);	/* Get genl information */
	if (result < 0)				/* fills wmx->mcg_id */
		goto error_gnl_resolve;

	result = nl_socket_add_membership(wmx->nlh_rx, wmx->mcg_id);
	if (result < 0) {
		wimaxll_msg(wmx, "E: RX: cannot join multicast group %u: %d (%s)\n",
			    wmx->mcg_id, result, nl_geterror());
		goto error_nl_add_membership;
	}
	/* Now we check if the device is a WiMAX supported device, by
	 * just querying for the RFKILL status. If this is not a WiMAX
	 * device, it will fail with -ENODEV. */
	if (wmx->ifidx > 0) {		/* if this handle is for any, don't check */
		result = wimaxll_rfkill(wmx, WIMAX_RF_QUERY);
		if (result == -ENODEV) {
			wimaxll_msg(wmx, "E: device %s is not a WiMAX device; "
				    "or supports an interface unknown to "
				    "libwimaxll: %d\n", wmx->name, result);
			goto error_rfkill;
		}
	}
	d_fnend(3, wmx, "(device %s) = %p\n", device, wmx);
	return wmx;

error_rfkill:
error_nl_add_membership:
error_gnl_resolve:
	nl_close(wmx->nlh_rx);
error_nl_connect_rx:
	nl_handle_destroy(wmx->nlh_rx);
error_nl_handle_alloc_rx:
	nl_close(wmx->nlh_tx);
error_nl_connect_tx:
	nl_handle_destroy(wmx->nlh_tx);
error_nl_handle_alloc_tx:
error_no_dev:
	wimaxll_free(wmx);
error_gnl_handle_alloc:
	errno = -result;
	d_fnend(3, NULL, "(device %s) = NULL\n", device);
	return NULL;
}

/**
 * Close a device handle opened with wimaxll_open()
 *
 * \param wmx WiMAX device handle
 *
 * \ingroup device_management
 * \internal
 *
 * Performs the natural oposite actions done in wimaxll_open().
 */
void wimaxll_close(struct wimaxll_handle *wmx)
{
	d_fnstart(3, NULL, "(wmx %p)\n", wmx);
	nl_close(wmx->nlh_rx);
	nl_handle_destroy(wmx->nlh_rx);
	nl_close(wmx->nlh_tx);
	nl_handle_destroy(wmx->nlh_tx);
	wimaxll_free(wmx);
	d_fnend(3, NULL, "(wmx %p) = void\n", wmx);
}
