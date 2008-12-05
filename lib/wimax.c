/*
 * Linux WiMax
 * Shared/common routines
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
 * These are a set of facilities used by the implementation of the
 * different ops in this library.
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
 * Netlink callback to process netlink callback errors
 *
 * \internal
 *
 * \param nla Source netlink address
 * \param nlerr Netlink error descritor
 * \param _mch Pointer to (\a struct wimaxll_mc_handle)
 *
 * \return 'enum nl_cb_action', NL_OK if there is no error, NL_STOP on
 *     error and _mch->result possibly updated.
 *
 * While reading from netlink and processing with callbacks (using
 * nl_recvmsgs()), we use this for the callback 'state machine' to
 * store the result of an error message from the kernel.
 */
int wimaxll_gnl_error_cb(struct sockaddr_nl *nla, struct nlmsgerr *nlerr,
		       void *_mch)
{
	struct wimaxll_mc_handle *mch = _mch;
	struct wimaxll_handle *wmx = mch->wmx;

	d_fnstart(7, wmx, "(nla %p nlnerr %p [%d] mch %p)\n",
		  nla, nlerr, nlerr->error, _mch);
	wimaxll_mch_maybe_set_result(mch, nlerr->error);
	mch->msg_done = 1;
	d_fnend(7, wmx, "(nla %p nlnerr %p [%d] mch %p) = NL_STOP\n",
		nla, nlerr, nlerr->error, _mch);
	return NL_STOP;
}


/**
 * Netlink callback to process an ack message and pass the 'error' code
 *
 * \internal
 *
 * Process a netlink ack message and extract the error code, which is
 * placed in the context passed as argument for the calling function
 * to use.
 *
 * We use this so that ACKers in the kernel can pass a simple error
 * code (integer) in the ACK that netlink sends, without having to
 * send an extra message.
 *
 * Complementary to wimaxll_gnl_error_cb().
 *
 * Frontend to this is wimaxll_wait_for_ack()
 */
int wimaxll_gnl_ack_cb(struct nl_msg *msg, void *_mch)
{
	int result;
	struct nlmsghdr *nl_hdr;
	struct nlmsgerr *nl_err;
	size_t size = nlmsg_len(nlmsg_hdr(msg));
	struct wimaxll_mc_handle *mch = _mch;

	d_fnstart(7, NULL, "(msg %p mch %p)\n", msg, _mch);
	nl_hdr = nlmsg_hdr(msg);
	size = nlmsg_len(nl_hdr);
	nl_err = nlmsg_data(nl_hdr);

	if (size < sizeof(*nl_err)) {
		wimaxll_msg(NULL, "E: netlink ack: buffer too small "
			  "(%zu vs %zu expected)\n",
			  size, sizeof(*nl_hdr) + sizeof(*nl_err));
		result = -EIO;
		goto error_ack_short;
	}
	d_printf(4, NULL, "netlink ack: nlmsghdr len %u type %u flags 0x%04x "
		 "seq 0x%x pid %u\n", nl_hdr->nlmsg_len, nl_hdr->nlmsg_type,
		 nl_hdr->nlmsg_flags, nl_hdr->nlmsg_seq, nl_hdr->nlmsg_pid);
	if (nl_hdr->nlmsg_type != NLMSG_ERROR) {
		wimaxll_msg(NULL, "E: netlink ack: message is not an ack but "
			  "type %u\n", nl_hdr->nlmsg_type);
		result = -EBADE;
		goto error_bad_type;
	}
	d_printf(4, NULL, "netlink ack: nlmsgerr error %d for "
		 "nlmsghdr len %u type %u flags 0x%04x seq 0x%x pid %u\n",
		 nl_err->error,
		 nl_err->msg.nlmsg_len, nl_err->msg.nlmsg_type,
		 nl_err->msg.nlmsg_flags, nl_err->msg.nlmsg_seq,
		 nl_err->msg.nlmsg_pid);
	wimaxll_mch_maybe_set_result(mch, nl_err->error);
	if (nl_err->error < 0)
		d_printf(2, NULL, "D: netlink ack: received netlink error %d\n",
			  nl_err->error);
	mch->msg_done = 1;
error_ack_short:
error_bad_type:
	d_fnend(7, NULL, "(msg %p mch %p) = NL_STOP\n", msg, _mch);
	return NL_STOP;
}


/**
 * Wait for a netlink ACK and pass on the result code it passed
 *
 * \internal
 *
 * \param wmx WiMAX device handle
 * \return error code passed by the kernel in the nlmsgerr structure
 *     that contained the ACK.
 *
 * Similar to nl_wait_for_ack(), but returns the value in
 * nlmsgerr->error, so it can be used by the kernel to return simple
 * error codes.
 */
int wimaxll_wait_for_ack(struct wimaxll_handle *wmx)
{
	int result;
	struct nl_cb *cb;
	struct wimaxll_mc_handle fake_mch;

	fake_mch.wmx = wmx;
	fake_mch.result = -EINPROGRESS;
	fake_mch.msg_done = 0;

	cb = nl_socket_get_cb(wmx->nlh_tx);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wimaxll_gnl_ack_cb, &fake_mch);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, wimaxll_gnl_error_cb, &fake_mch);
	do
		result = nl_recvmsgs(wmx->nlh_tx, cb);
	while (fake_mch.msg_done == 0 && result >= 0);
	result = fake_mch.result;
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_err(cb, NL_CB_CUSTOM, NL_CB_DEFAULT, NULL);
	nl_cb_put(cb);
	if (result < 0)
		return result;
	else
		return fake_mch.result;
}


/**
 * \defgroup diagnostics_group Output of diagnostics messages
 *
 * The \e libwimaxll library prints diagnostics by default to \a
 * stderr. Said destination can be changed by the user by setting the
 * wimaxll_vmsg() function pointer before calling any other \a
 * libwimaxll function.
 *
 * To restore the default diagnostics destination, set wimaxll_vmsg()
 * back to wimaxll_vmsg_stderr().
 */


/**
 * Deliver \e libwimaxll diagnostics messages to \e stderr
 *
 * \param fmt printf-like format
 * \param vargs variable-argument list as created by
 *     stdargs.h:va_list() that will be formatted according to \e
 *     fmt.
 *
 * Default diagnostics printing function.
 *
 * \ingroup diagnostics_group
 */
void wimaxll_vmsg_stderr(const char *fmt, va_list vargs)
{
	vfprintf(stderr, fmt, vargs);
}



/**
 * Print library diagnostics messages [backend]
 *
 * @param fmt printf-like format
 * @param vargs variable-argument list as created by
 *     stdargs.h:va_list() that will be formatted according to \e
 *     fmt.
 *
 * Prints/writes the \e libwimaxll's diagnostics messages to a
 * destination as selected by the user of the library.
 *
 * \note This function pointer must be set \b before calling any other
 *     \e libwimaxll function.
 *
 * By default, diagnostics are printed with wimaxll_vmsg_stderr() to
 * \a stderr.
 *
 * For example, to deliver diagnostics to syslog:
 *
 * @code
 * #include <syslog.h>
 * ...
 * static
 * void wimaxll_vmsg_syslog(const char *fmt, va_list vargs)
 * {
 *         vsyslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), fmt, vargs);
 * }
 * ...
 * wimaxll_vmsg = wimaxll_vmsg_syslog();
 * ...
 * wimaxll_open(BLAH);
 * @endcode
 *
 * \ingroup diagnostics_group
 * \internal
 *
 * The internal function wimaxll_msg() is used as as a frontend to
 * this function.
 */
void (*wimaxll_vmsg)(const char *fmt, va_list vargs) = wimaxll_vmsg_stderr;


static
void __wimaxll_msg(const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	wimaxll_vmsg(fmt, vargs);
	va_end(vargs);
}


/**
 * \internal
 *
 * Prints library diagnostic messages with a predefined format [frontend]
 *
 * @param wmx WiMAX handle; if NULL, no device header will be presented.
 * @param fmt printf-like format followed by any arguments
 *
 * Called by the library functions to print status/error messages. By
 * default these are sent over to stderr.
 *
 * However, library users can change this default behaviour by setting
 * wimaxll_vmsg() as documented in that function pointer's
 * documentation.
 *
 * \ingroup diagnostics_group
 */
void wimaxll_msg(struct wimaxll_handle *wmx, const char *fmt, ...)
{
	va_list vargs;
	if (wmx == NULL)
		__wimaxll_msg("libwimax: ");
	else if ((unsigned long) wmx < 4096) {
		__wimaxll_msg("libwimax: E: Corrupt device handle %p\n", wmx);
		__wimaxll_msg("libwimax[n/a]: ");
	} else
		__wimaxll_msg("libwimax[%s]: ", wmx->name);
	va_start(vargs, fmt);
	wimaxll_vmsg(fmt, vargs);
	va_end(vargs);
}


/**
 * Return the name of a the system's WiMAX interface associated to an
 * open handle
 *
 * \param wmx WiMAX device handle
 * \return Interface name (only valid while the handle is open)
 *
 * \ingroup device_management
 */
const char *wimaxll_ifname(const struct wimaxll_handle *wmx)
{
	return wmx->name;
}

void wimax_ifname() __attribute__ ((weak, alias("wimaxll_ifname")));
