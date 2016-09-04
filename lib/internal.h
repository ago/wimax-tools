/*
 * Linux WiMax
 * Internal API and declarations
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
 */
#ifndef __lib_internal_h__
#define __lib_internal_h__

#include <wimaxll.h>

struct nl_msg;
struct nl_sock;
struct nlmsgerr;
struct sockaddr_nl;

enum {
#define __WIMAXLL_IFNAME_LEN 32
	/**
	 * WIMAXLL_IFNAME_LEN - Maximum size of a wimax interface
	 *     name.
	 */
	WIMAXLL_IFNAME_LEN = __WIMAXLL_IFNAME_LEN,
};


/**
 * A WiMax control pipe handle
 *
 * This type is opaque to the user
 *
 * \internal
 *
 * In order to simplify multithread support, we use to different \a
 * libnl handles, one for sending to the kernel, one for receiving
 * from the kernel (multicast group). This allows us to parallelize \c
 * wimaxll_msg_write() and \c wimaxll_msg_read() at the same time in a
 * multithreaded environment, for example.
 *
 * \param ifidx Interface Index (of the network interface); if 0, the
 *     interface name will be \c "any" and this means that this handle
 *     works for \e any WiMAX interface.
 * \param gnl_family_id Generic Netlink Family ID assigned to the
 *     device; we maintain it here (for each interface) because we
 *     want to discover it every time we open. This solves the case of
 *     the WiMAX modules being reloaded (and the ID changing) while
 *     this library is running; this way it takes only a new open when
 *     the new device is discovered.
 * \param mcg_id Id of the 'msg' multicast group
 * \param name name of the wimax interface
 * \param priv Private pointer set with wimaxll_priv_set() or other
 *     accessors. Use wimaxll_priv_get() to access it.
 * \param nlh_tx handle for writing to the kernel.
 *     Internal note: You \b have \b to set the handlers for
 *     %NL_CB_VALID and nl_cb_err() callbacks, as each callsite will
 *     do it to suit their needs. See wimaxll_rfkill() for an
 *     example. Any other callback you are supposed to restore to what
 *     it was before.
 * \param nlh_rx handle for reading from the kernel.
 * \param nl_rx_cb Callbacks for the nlh_rx handle
 *
 * FIXME: add doc on callbacks
 */
struct wimaxll_handle {
	unsigned ifidx;
	int gnl_family_id, mcg_id;
	char name[__WIMAXLL_IFNAME_LEN];
	void *priv;

	struct nl_sock *nlh_tx;
	struct nl_sock *nlh_rx;

	wimaxll_msg_to_user_cb_f msg_to_user_cb;
	void *msg_to_user_priv;

	wimaxll_state_change_cb_f state_change_cb;
	void *state_change_priv;
};


/* Utilities */
int wimaxll_wait_for_ack(struct wimaxll_handle *);
int wimaxll_gnl_handle_msg_to_user(struct wimaxll_handle *, struct nl_msg *);
int wimaxll_gnl_handle_state_change(struct wimaxll_handle *, struct nl_msg *);
int wimaxll_gnl_error_cb(struct sockaddr_nl *, struct nlmsgerr *, void *);
int wimaxll_gnl_ack_cb(struct nl_msg *msg, void *_mch);

/*
 * wimaxll_family_id - Return the associated Generic Netlink family ID
 *
 * @wmx: WiMax interface for which to provide the ID.
 */
static inline
int wimaxll_family_id(struct wimaxll_handle *wmx)
{
	return wmx->gnl_family_id;
}


void wimaxll_msg(struct wimaxll_handle *, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/* Generic Netlink utilities */

int nl_get_multicast_groups(struct nl_sock *, const char *,
			    void (*cb)(void *, const char *, int),
			    void *);
int genl_ctrl_get_version(struct nl_sock *, const char *);

#endif /* #ifndef __lib_internal_h__ */
