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
 *   __wimaxll_cmd_open()
 *   nl_recvmsgs()
 *     wimaxll_gnl_rp_ifinfo_cb()
 *       __wimaxll_ifinfo_parse_groups()
 *     wimaxll_gnl_error_cb()
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
#include <netlink/msg.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <wimaxll.h>
#include "internal.h"
#define D_LOCAL 0
#include "debug.h"


/**
 * Generic Netlink: IFINFO message policy
 *
 * \internal
 * \ingroup device_management
 *
 * Authoritative reference for this is in drivers/net/wimax/op-open.c;
 * this is just a copy.
 */
static
struct nla_policy wimaxll_gnl_ifinfo_policy[WIMAX_GNL_ATTR_MAX + 1] = {
	[WIMAX_GNL_IFINFO_MC_GROUPS] = { .type = NLA_NESTED },
	[WIMAX_GNL_IFINFO_MC_GROUP] = { .type = NLA_NESTED },
	[WIMAX_GNL_IFINFO_MC_NAME] = {
		.type = NLA_STRING,
		.maxlen = GENL_NAMSIZ
	},
	[WIMAX_GNL_IFINFO_MC_ID] = { .type = NLA_U16 },
};


/**
 * Callback context for processing IFINFO messages
 *
 * \ingroup device_management
 * \internal
 *
 * All the callbacks store the information they parse from the
 * messages here; we do it so that only once everything passes with
 * flying colors, then we commit the information the kernel sent.
 *
 * We include a \a struct wimaxll_gnl_cb_context so we can share this
 * same data structure with the error handler and not require to check
 * different data in different places.
 *
 * gnl_mc stands for Generic Netlink MultiCast group
 */
struct wimaxll_ifinfo_context {
	struct wimaxll_gnl_cb_context gnl;
	struct wimaxll_mc_group gnl_mc[WIMAXLL_MC_MAX];
};


/**
 * Parse the list of multicast groups sent as part of a IFINFO reply
 *
 * \ingroup device_management
 * \internal
 *
 * \param ctx IFINFO context, containing the WiMAX handle
 *
 * \param nla_groups Netlink attribute (type
 *     WIMAXLL_GNL_IFINFO_MC_GROUPS) that contains the list of (nested)
 *     attributes (type WIMAXLL_GNL_IFINFO_MC_GROUP).
 *     Just pass tb[WIMAXLL_GNL_IFINFO_MC_GROUPS]; this function will
 *     check for it to be ok (non-NULL).
 *
 * \return 0 if ok, < 0 errno code on error.
 *
 *     On success, the \a wmx->mc_group array will be cleared and
 *     overwritten with the list of reported groups (and their
 *     IDs).
 *
 *     On error, nothing is modified.
 *
 * The kernel has sent a list of attributes: a nested attribute
 * (_MC_GROUPS) containing a list of nested attributes (_MC_GROUP)
 * each with a _MC_NAME and _MC_ID attribute].
 *
 * We need to parse it and extract the multicast group data. At least
 * a multicast group ('reports') has to exist; the rest are optional
 * to a maximum of WIMAXLL_MC_MAX (without counting 'reports').
 *
 * \note The private data for this callback is not an \a struct
 *     wimaxll_mc_handle as most of the other ones!
 */
static
int __wimaxll_ifinfo_parse_groups(struct wimaxll_ifinfo_context *ctx,
				struct nlattr *nla_groups)
{
	int result, remaining, cnt = 0;
	struct wimaxll_handle *wmx = ctx->gnl.wmx;
	struct nlattr *nla_group;

	d_fnstart(7, wmx, "(ctx %p [wmx %p] nla_groups %p)\n",
		  ctx, wmx, nla_groups);
	if (nla_groups == NULL) {
		wimaxll_msg(wmx, "E: %s: the kernel didn't send a "
			  "WIMAXLL_GNL_IFINFO_MC_GROUPS attribute\n",
			  __func__);
		result = -EBADR;
		goto error_no_groups;
	}

	memset(ctx->gnl_mc, 0, sizeof(ctx->gnl_mc));
	nla_for_each_nested(nla_group, nla_groups, remaining) {
		char *name;
		int id;
		struct nlattr *tb[WIMAX_GNL_ATTR_MAX+1];

		d_printf(8, wmx, "D: group %d, remaining %d\n",
			 cnt, remaining);
		result = nla_parse_nested(tb, WIMAX_GNL_ATTR_MAX,
					  nla_group,
					  wimaxll_gnl_ifinfo_policy);
		if (result < 0) {
			wimaxll_msg(wmx, "E: %s: can't parse "
				  "WIMAX_GNL_MC_GROUP attribute: %d\n",
				  __func__, result);
			continue;
		}

		if (tb[WIMAX_GNL_IFINFO_MC_NAME] == NULL) {
			wimaxll_msg(wmx, "E: %s: multicast group missing "
				  "WIMAX_GNL_IFINFO_MC_NAME attribute\n",
				  __func__);
			continue;
		}
		name = nla_get_string(tb[WIMAX_GNL_IFINFO_MC_NAME]);

		if (tb[WIMAX_GNL_IFINFO_MC_ID] == NULL) {
			wimaxll_msg(wmx, "E: %s: multicast group missing "
				  "WIMAX_GNL_IFINFO_MC_ID attribute\n",
				  __func__);
			continue;
		}
		id = nla_get_u16(tb[WIMAX_GNL_IFINFO_MC_ID]);

		d_printf(6, wmx, "D: MC group %s:%d\n", name, id);
		strncpy(ctx->gnl_mc[cnt].name, name,
			sizeof(ctx->gnl_mc[cnt].name));
		ctx->gnl_mc[cnt].id = id;
		cnt++;
	}
	result = 0;
error_no_groups:
	d_fnend(7, wmx, "(ctx %p [wmx %p] nla_groups %p) = %d\n",
		ctx, wmx, nla_groups, result);
	return result;
}


/*
 * Same as wimaxll_gnl_error_cb(), but takes a different type of
 * context, so, need another one (fitting an mch in there made little
 * sense).
 */
int wimaxll_gnl_rp_ifinfo_error_cb(struct sockaddr_nl *nla,
				   struct nlmsgerr *nlerr,
				   void *_ctx)
{
	struct wimaxll_ifinfo_context *ctx = _ctx;
	struct wimaxll_handle *wmx = ctx->gnl.wmx;

	d_fnstart(7, wmx, "(nla %p nlnerr %p [%d] ctx %p)\n",
		  nla, nlerr, nlerr->error, _ctx);
	if (ctx->gnl.result == -EINPROGRESS)
		ctx->gnl.result = nlerr->error;
	d_fnend(7, wmx, "(nla %p nlnerr %p [%d] ctx %p) = %d\n",
		nla, nlerr, nlerr->error, _ctx, NL_STOP);
	return NL_STOP;
}


/**
 * Seek for and process a WIMAX_GNL_RP_IFINFO message from the kernel
 *
 * \ingroup device_management
 * \internal
 *
 * \param msg Netlink message containing the reply
 * \param _ctx Pointer to a \a struct wimaxll_gnl_cb_context where the
 *     context will be returned.
 *
 * \return 'enum nl_cb_action', NL_OK if there is no error, NL_STOP on
 *     error and _ctx possibly updated.
 *
 * This will take a received netlink message, check it is a \a
 * WIMAX_GNL_RP_IFINFO, parse the contents and store them in an
 * struct wimaxll_ifinfo_context (for the caller to use later on).
 */
static
int wimaxll_gnl_rp_ifinfo_cb(struct nl_msg *msg, void *_ctx)
{
	int result;
	struct wimaxll_ifinfo_context *ctx = _ctx;
	struct wimaxll_handle *wmx = ctx->gnl.wmx;
	struct nlmsghdr *nl_hdr;
	struct genlmsghdr *genl_hdr;
	struct nlattr *tb[WIMAX_GNL_ATTR_MAX + 1];
	unsigned major, minor;

	d_fnstart(7, wmx, "(msg %p ctx %p)\n", msg, ctx);
	nl_hdr = nlmsg_hdr(msg);
	genl_hdr = nlmsg_data(nl_hdr);

	if (genl_hdr->cmd != WIMAX_GNL_RP_IFINFO) {
		ctx->gnl.result = -ENXIO;
		result = NL_SKIP;
		d_printf(1, wmx, "D: ignoring unknown reply %d\n",
			 genl_hdr->cmd);
		goto error_parse;
	}

	/* Check version compatibility -- check include/linux/wimax.h
	 * for a complete description. The idea is to allow for good
	 * expandability of the interface without causing breakage. */
	major = genl_hdr->version / 10;
	minor = genl_hdr->version % 10;
	if (major != WIMAX_GNL_VERSION / 10) {
		ctx->gnl.result = -EBADR;
		result = NL_SKIP;
		wimaxll_msg(wmx, "E: kernel's major WiMAX GNL interface "
			    "version (%d) is different that supported %d; "
			    "aborting\n", major, WIMAX_GNL_VERSION / 10);
		goto error_bad_major;
	}
	if (minor < WIMAX_GNL_VERSION % 10)
		wimaxll_msg(wmx, "W: kernel's minor WiMAX GNL interface "
			    "version (%d) is lower that supported %d; things "
			    "might not work\n", minor, WIMAX_GNL_VERSION % 10);

	/* Parse the attributes */
	result = genlmsg_parse(nl_hdr, 0, tb, WIMAX_GNL_IFINFO_MAX,
			       wimaxll_gnl_ifinfo_policy);
	if (result < 0) {
		wimaxll_msg(wmx, "E: %s: genlmsg_parse() failed: %d\n",
			  __func__, result);
		ctx->gnl.result = result;
		result = NL_SKIP;
		goto error_parse;
	}
	result = __wimaxll_ifinfo_parse_groups(ctx,
					     tb[WIMAX_GNL_IFINFO_MC_GROUPS]);
	if (result < 0) {
		ctx->gnl.result = result;
		result = NL_SKIP;
	} else {
		ctx->gnl.result = 0;
		result = NL_OK;
	}
error_parse:
error_bad_major:
	d_fnend(7, wmx, "(msg %p ctx %p) = %d\n", msg, ctx, result);
	return result;
}


static
int __wimaxll_cmd_open(struct wimaxll_handle *wmx)
{
	int result;
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct wimaxll_ifinfo_context ctx;

	d_fnstart(5, wmx, "(wmx %p)\n", wmx);
	msg = nlmsg_new();
	if (msg == NULL) {
		result = -errno;
		wimaxll_msg(wmx, "E: %s: cannot allocate generic netlink "
			  "message: %m\n", __func__);
		goto error_msg_alloc;
	}
	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ,
			wimaxll_family_id(wmx), 0, 0,
			WIMAX_GNL_OP_OPEN, WIMAX_GNL_VERSION) == NULL) {
		result = -errno;
		wimaxll_msg(wmx, "E: %s: error preparing message: %m\n",
			  __func__);
		goto error_msg_prep;
	}
	result = nl_send_auto_complete(wmx->nlh_tx, msg);
	if (result < 0) {
		wimaxll_msg(wmx, "E: %s: error sending message: %d\n",
			  __func__, result);
		goto error_msg_send;
	}

	/* Read the reply, that includes a WIMAX_GNL_RP_IFINFO message
	 *
	 * We need to set the call back error handler, so we get the
	 * default cb handler and modify it.
	 */
	ctx.gnl.wmx = wmx;
	ctx.gnl.result = -EINPROGRESS;
	cb = nl_socket_get_cb(wmx->nlh_tx);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
		  wimaxll_gnl_rp_ifinfo_cb, &ctx);
	nl_cb_err(cb, NL_CB_CUSTOM, wimaxll_gnl_rp_ifinfo_error_cb, &ctx.gnl);
	nl_recvmsgs_default(wmx->nlh_tx);
	nl_cb_put(cb);
	result = ctx.gnl.result;
	if (result >= 0)
		nl_wait_for_ack(wmx->nlh_tx);
	if (result == -EINPROGRESS)
		wimaxll_msg(wmx, "E: %s: the kernel didn't reply with a "
			  "WIMAX_GNL_RP_IFINFO message\n", __func__);
	d_printf(1, wmx, "D: processing result is %d\n", result);

	/* All fine and dandy, commit the data */
	memcpy(wmx->gnl_mc, ctx.gnl_mc, sizeof(ctx.gnl_mc));
error_msg_prep:
error_msg_send:
	nlmsg_free(msg);
error_msg_alloc:
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
 * \param device device name of the WiMAX network interface
 * \return WiMAX device handle on success; on error, %NULL is returned
 *     and the \a errno variable is updated with a corresponding
 *     negative value.
 *
 * When opening the handle to the device, a basic check of API
 * versioning will be done. If the kernel interface has a different
 * major version, the \c wimaxll_open() call will fail (existing
 * interfaces modified or removed). A higher kernel minor version is
 * allowed (new interfaces added); a lower kernel minor version is not
 * (the library needs interfaces that are not in the kernel).
 *
 * \ingroup device_management
 * \internal
 *
 * Allocates the netlink handles needed to talk to the kernel. With
 * that, looks up the Generic Netlink Family ID associated (if none,
 * it's not a WiMAX device). This is done by querying generic netlink
 * for a family called "WiMAX <DEVNAME>".
 *
 * An open command is issued to get and process interface information
 * (like multicast group mappings, etc). This also does an interface
 * versioning verification.
 *
 * The bulk of this code is in the parsing of the generic netlink
 * reply that the \a WIMAX_GNL_OP_OPEN command returns (\a
 * WIMAX_GNL_RP_IFINFO) with information about the WiMAX control
 * interface.
 *
 * We process said reply using the \e libnl callback mechanism,
 * invoked by __wimaxll_cmd_open(). All the information is stored in a
 * struct wimaxll_ifinfo_context by the callbacks. When the callback
 * (and thus message) processing finishes, __wimaxll_cmd_open(), if all
 * successful, will commit the information from the context to the
 * handle. On error, nothing is modified.
 *
 * \note Because events are going to ben processed, sequence checks
 * have to be disabled (as indicated by the generic netlink
 * documentation).
 */
struct wimaxll_handle *wimaxll_open(const char *device)
{
	int result;
	struct wimaxll_handle *wmx;
	char buf[64];

	d_fnstart(3, NULL, "(device %s)\n", device);
	result = ENOMEM;
	wmx = malloc(sizeof(*wmx));
	if (wmx == NULL) {
		wimaxll_msg(NULL, "E: cannot allocate WiMax handle: %m\n");
		goto error_gnl_handle_alloc;
	}
	memset(wmx, 0, sizeof(*wmx));
	strncpy(wmx->name, device, sizeof(wmx->name));

	/* Setup the TX side */
	wmx->nlh_tx = nl_handle_alloc();
	if (wmx->nlh_tx == NULL) {
		result = nl_get_errno();
		wimaxll_msg(wmx, "E: cannot open TX netlink handle: %d\n",
			  result);
		goto error_nl_handle_alloc_tx;
	}
	result = nl_connect(wmx->nlh_tx, NETLINK_GENERIC);
	if (result < 0) {
		wimaxll_msg(wmx, "E: cannot connect TX netlink: %d\n", result);
		goto error_nl_connect_tx;
	}

	/* Lookup the generic netlink family */
	snprintf(buf, sizeof(buf), "WiMAX %s", wmx->name);
	result = genl_ctrl_resolve(wmx->nlh_tx, buf);
	if (result < 0) {
		wimaxll_msg(wmx, "E: device %s presents no WiMAX interface; "
			  "it might not exist, not be be a WiMAX device or "
			  "support an interface unknown to libwimax: %d\n",
			  wmx->name, result);
		goto error_ctrl_resolve;
	}
	wmx->gnl_family_id = result;
	d_printf(1, wmx, "D: WiMAX device %s, genl family ID %d\n",
		 wmx->name, wmx->gnl_family_id);

	result = __wimaxll_cmd_open(wmx);	/* Get interface information */
	if (result < 0)
		goto error_cmd_open;

	result = wimaxll_mc_rx_open(wmx, "msg");
	if (result == -EPROTONOSUPPORT)		/* not open? */
		wmx->mc_msg = WIMAXLL_MC_MAX;	/* for wimaxll_mc_rx_read() */
	else if (result < 0) {
		wimaxll_msg(wmx, "E: cannot open 'msg' multicast group: "
			  "%d\n", result);
		goto error_msg_open;
	} else
		wmx->mc_msg = result;
	d_fnend(3, wmx, "(device %s) = %p\n", device, wmx);
	return wmx;

	wimaxll_mc_rx_close(wmx, wmx->mc_msg);
error_msg_open:
error_cmd_open:
error_ctrl_resolve:
	nl_close(wmx->nlh_tx);
error_nl_connect_tx:
	nl_handle_destroy(wmx->nlh_tx);
error_nl_handle_alloc_tx:
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
 * Performs the natural oposite actions done in wimaxll_open(). All
 * generic netlink multicast groups are destroyed, the netlink handle
 * is closed and destroyed and finally, the actual handle is released.
 */
void wimaxll_close(struct wimaxll_handle *wmx)
{
	unsigned cnt;

	d_fnstart(3, NULL, "(wmx %p)\n", wmx);
	for (cnt = 0; cnt < WIMAXLL_MC_MAX; cnt++)
		if (wmx->gnl_mc[cnt].mch)
			wimaxll_mc_rx_close(wmx, cnt);
	nl_close(wmx->nlh_tx);
	nl_handle_destroy(wmx->nlh_tx);
	wimaxll_free(wmx);
	d_fnend(3, NULL, "(wmx %p) = void\n", wmx);
}

void wimax_open() __attribute__ ((weak, alias("wimaxll_open")));
void wimax_close() __attribute__ ((weak, alias("wimaxll_close")));
