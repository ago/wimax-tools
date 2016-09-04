/*
 * Linux WiMax
 * Export the kernel's WiMAX stack wimaxll_rfkill() function
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
 * Control the software RF Kill switch and obtain switch status
 *
 * \param wmx WiMAX device handle
 *
 * \param state State to which you want to toggle the sofware RF Kill
 *     switch (%WIMAX_RF_ON, %WIMAX_RF_OFF or %WIMAX_RF_QUERY for just
 *     querying the current state of the hardware and software
 *     switches).
 *
 * \return Negative errno code on error. Otherwise, radio kill switch
 *     status (bit 0 \e hw switch, bit 1 \e sw switch, \e 0 OFF, \e 1
 *     ON):
 *     - 3 @c 0b11: Both HW and SW switches are \e on, radio is \e on
 *     - 2 @c 0b10: HW switch is \e off, radio is \e off
 *     - 1 @c 0b01: SW switch is \e on, radio is \e off
 *     - 0 @c 0b00: Both HW and SW switches are \e off, radio is \e off
 *
 * Allows the caller to control the state of the software RF Kill
 * switch (if present) and in return, obtain the current status of
 * both the hardware and software RF Kill switches.
 *
 * If there is no hardware or software switch, that switch is assumed
 * to be always on (radio on).
 *
 * Changing the radio state might cause the device to change state,
 * and cause the kernel to send reports indicating so.
 *
 * \note The state of the radio (\e ON or \e OFF) is the inverse of
 *       the state of the RF-Kill switch (\e enabled/on kills the
 *       radio, radio \e off; \e disabled/off allows the radio to
 *       work, radio \e on).
 *
 * \ingroup device_management
 * \internal
 *
 * This implementation simply marshalls the call to the kernel's
 * wimax_rfkill() and returns it's return code.
 */
int wimaxll_rfkill(struct wimaxll_handle *wmx, enum wimax_rf_state state)
{
	ssize_t result;
	struct nl_msg *msg;

	d_fnstart(3, wmx, "(wmx %p state %u)\n", wmx, state);
	result = -EBADF;
	if (wmx->ifidx == 0)
		goto error_not_any;
	msg = nlmsg_alloc();
	if (!msg) {
		result = errno;
		wimaxll_msg(wmx, "E: RFKILL: cannot allocate generic netlink "
			  "message: %m\n");
		goto error_msg_alloc;
	}
	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ,
			wimaxll_family_id(wmx), 0, 0,
			WIMAX_GNL_OP_RFKILL, WIMAX_GNL_VERSION) == NULL) {
		result = -ENOMEM;
		wimaxll_msg(wmx, "E: RFKILL: error preparing message: "
			  "%zd 0x%08x\n", result, (unsigned int) result);
		goto error_msg_prep;
	}
	nla_put_u32(msg, WIMAX_GNL_RFKILL_IFIDX, (__u32) wmx->ifidx);
	nla_put_u32(msg, WIMAX_GNL_RFKILL_STATE, (__u32) state);
	result = nl_send_auto_complete(wmx->nlh_tx, msg);
	if (result < 0) {
		wimaxll_msg(wmx, "E: RFKILL: error sending message: %zd\n",
			  result);
		goto error_msg_send;
	}
	/* Read the message ACK from netlink */
	result = wimaxll_wait_for_ack(wmx);
	if (result < 0 && result != -ENODEV)
		wimaxll_msg(wmx, "E: RFKILL: operation failed: %zd\n", result);
error_msg_prep:
error_msg_send:
	nlmsg_free(msg);
error_msg_alloc:
error_not_any:
	d_fnend(3, wmx, "(wmx %p state %u) = %zd\n", wmx, state, result);
	return result;
}
