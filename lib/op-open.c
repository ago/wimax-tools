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


static
void wimaxll_mc_group_cb(void *_wmx, const char *name, int id)
{
	struct wimaxll_handle *wmx = _wmx;

	if (wmx->mc_n < sizeof(wmx->gnl_mc) / sizeof(wmx->gnl_mc[0])) {
		struct wimaxll_mc_group *gmc = &wmx->gnl_mc[wmx->mc_n];
		strncpy(gmc->name, name, sizeof(gmc->name));
		gmc->id = id;
		wmx->mc_n++;
	}
}


static
int wimaxll_gnl_resolve(struct wimaxll_handle *wmx)
{
	int result, version;
	char buf[64];
	unsigned major, minor;
	
	d_fnstart(5, wmx, "(wmx %p)\n", wmx);
	/* Lookup the generic netlink family */
	wmx->ifidx = if_nametoindex(wmx->name);
	if (wmx->ifidx == 0) {
		wimaxll_msg(wmx, "E: device %s does not exist\n", wmx->name);
		goto error_no_dev;
	}
	snprintf(buf, sizeof(buf), "WiMAX %u", wmx->ifidx);
	result = genl_ctrl_resolve(wmx->nlh_tx, buf);
	if (result < 0) {
		wimaxll_msg(wmx, "E: device %s presents no WiMAX interface; "
			  "it might not exist, not be be a WiMAX device or "
			  "support an interface unknown to libwimaxll: %d\n",
			  wmx->name, result);
		goto error_ctrl_resolve;
	}
	wmx->gnl_family_id = result;
	d_printf(1, wmx, "D: WiMAX device %s, genl family ID %d\n",
		 wmx->name, wmx->gnl_family_id);
	nl_get_multicast_groups(wmx->nlh_tx, buf, wimaxll_mc_group_cb, wmx);

	version = genl_ctrl_get_version(wmx->nlh_tx, buf);
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
error_ctrl_resolve:
error_no_dev:
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

	result = wimaxll_gnl_resolve(wmx);	/* Get genl information */
	if (result < 0)
		goto error_gnl_resolve;

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
error_gnl_resolve:
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
