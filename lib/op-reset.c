/*
 * Linux WiMax
 * Export the kernel's WiMAX stack wimaxll_reset() function
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
#include <wimaxll.h>
#include "internal.h"
#define D_LOCAL 0
#include "debug.h"


/**
 * Reset a WiMAX device
 *
 * \param wmx WiMAX device handle
 *
 * \return result of the operation.
 *
 *     - 0: warm reset suceeded
 *     - -ENODEV: warm reset failed and had to resort to a cold/bus
 *       reset; the device was disconnected from the system and the
 *       current handle is invalid and should be closed.
 *     - Any other negative error code: unrecoverable error, shutdown
 *       and go home
 *
 * When there is a need to reset the device wimaxll_reset() can be used
 * to issue a warm reset. That won't invalidate the existing handles,
 * while still moving the device to power on state.
 *
 * If the device cannot be properly reset, the WiMAX kernel stack may
 * fall back to a cold reset, which will most likely disconnect the
 * device from the driver (and bus) and reconnect it; that means
 * device handles will be invalid from there on. In those cases,
 * -ENODEV is returned.
 *
 * \note This call is synchronous; when success is returned, the
 * device has completed its internal reset.
 *
 * \ingroup device_management
 * \internal
 *
 * This implementation simply marshalls the call to the kernel's
 * wimax_reset() and returns it's return code.
 */
int wimaxll_reset(struct wimaxll_handle *wmx)
{
	ssize_t result;
	struct nl_msg *msg;

	d_fnstart(3, wmx, "(wmx %p)\n", wmx);
	result = -EBADF;
	if (wmx->ifidx == 0)
		goto error_not_any;

	msg = nlmsg_alloc();
	if (!msg) {
		result = errno;
		wimaxll_msg(wmx, "E: RESET: cannot allocate generic netlink "
			  "message: %m\n");
		goto error_msg_alloc;
	}
	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ,
			wimaxll_family_id(wmx), 0, 0,
			WIMAX_GNL_OP_RESET, WIMAX_GNL_VERSION) == NULL) {
		result = -ENOMEM;
		wimaxll_msg(wmx, "E: RESET: error preparing message: "
			  "%zd 0x%08x\n", result, (unsigned int) result);
		goto error_msg_prep;
	}
	nla_put_u32(msg, WIMAX_GNL_RESET_IFIDX, (__u32) wmx->ifidx);
	result = nl_send_auto_complete(wmx->nlh_tx, msg);
	if (result < 0) {
		wimaxll_msg(wmx, "E: RESET: error sending message: %zd\n",
			  result);
		goto error_msg_send;
	}
	/* Read the message ACK from netlink */
	result = wimaxll_wait_for_ack(wmx);
	if (result < 0)
		wimaxll_msg(wmx, "E: RESET: operation failed: %zd\n", result);
error_msg_prep:
error_msg_send:
	nlmsg_free(msg);
error_msg_alloc:
error_not_any:
	d_fnend(3, wmx, "(wmx %p) = %zd\n", wmx, result);
	return result;
}
