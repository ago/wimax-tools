/*
 * Linux WiMax
 * log helpers
 *
 *
 * Copyright (C) 2009 Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#define W_VERBOSITY W_ERROR
#include <wimaxll/log.h>
#include "internal.h"


/**
 * Deliver \e libwimaxll diagnostics messages to \e stderr or \e stdout.
 *
 * \param wmx WiMAX handle this message is related to 
 * \param level Message level
 * \param header Header for the message; the implementation must
 *     decide if it has to be printed or not.
 * \param fmt printf-like format
 * \param vargs variable-argument list as created by
 *     stdargs.h:va_list() that will be formatted according to \e
 *     fmt.
 *
 * Default diagnostics printing function. If the log level is
 * "W_PRINT", we put it on stdout without a header, otherwise in
 * stderr with header.
 *
 * \ingroup helper_log
 */
void wimaxll_vlmsg_default(struct wimaxll_handle *wmx, unsigned level,
			   const char *header, 
			   const char *fmt, va_list vargs)
{
	FILE *f = level != W_PRINT? stderr : stdout;

	/* Backwards compat */
	if (wimaxll_vmsg) {
		if (header)
			wimaxll_vmsg(header, NULL);
		wimaxll_vmsg(fmt, vargs);
		return;
	}
	if (level == W_PRINT)
		f = stdout;
	else {
		f = stderr;
		fprintf(f, header);
	}
	vfprintf(f, fmt, vargs);
}


/**
 * Print library diagnostics messages [backend]
 *
 * @param wmx WiMAX handle this message is related to 
 * @param level Message level 
 * @param header Header to print for the message (the implementation
 *     must decide if to print it or not).
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
 * By default, diagnostics are printed with wimaxll_vlmsg_default() to
 * \a stderr or stdout based on the level.
 *
 * For example, to deliver diagnostics to syslog:
 *
 * @code
 * #include <syslog.h>
 * ...
 * static
 * void wimaxll_vlmsg_syslog(....const char *fmt, va_list vargs)
 * {
 *         syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), header);
 *         vsyslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), fmt, vargs);
 * }
 * ...
 * wimaxll_vlmsg = wimaxll_vlmsg_syslog();
 * ...
 * wimaxll_open(BLAH);
 * @endcode
 *
 * The internal function wimaxll_msg() and wimaxll_lmsg() are used as
 * as a frontend to this function.
 * 
 * \ingroup helper_log
 */
void (*wimaxll_vlmsg_cb)(struct wimaxll_handle *wmx, unsigned level,
			 const char *header,
			 const char *fmt, va_list vargs) =
	wimaxll_vlmsg_default;


/**
 * Default header for diagnostic messages
 *
 * If there is no handle, prints just "libwimaxll", otherwise
 * "libwimaxll[device]"; if the message is debug, also adds a
 * "(@ FUNCTION:LINE)" origin tag.
 * 
 * \ingroup helper_log
 */
void wimaxll_msg_hdr_default(char *buf, size_t buf_len,
			     struct wimaxll_handle *wmx, enum w_levels level,
			     const char *origin_str, unsigned origin_line)
{
	size_t bytes;
	if (wmx == NULL)
		bytes = snprintf(buf, buf_len, "libwimaxll: ");
	else if ((unsigned long) wmx < 4096)
		bytes = snprintf(buf, buf_len, "libwimaxll[bad handle %p]: ",
				 wmx);
	else
		bytes = snprintf(buf, buf_len, "libwimaxll[%s]: ", wmx->name);
	if (level >= W_D0 && origin_str != NULL)
		snprintf(buf + bytes, buf_len - bytes,
			 "(@ %s:%u) ", origin_str, origin_line);
}


/**
 * Create a header for library diagnostic messages [backend]
 *
 * @param buf Buffer where to place the header
 * @param buf_len Size of the buffer
 * @param wmx WiMAX handle the message is being generated for (can be
 *     NULL or invalid).
 * @param level Level of the log message this header is being
 *     generated for.
 * @param origin_str Origin of the message (used for a source file
 *     name or function name). If %NULL, the origin information should
 *     not be considered. 
 * @param origin_line Origin of the message (used for a line in a
 *     source file or function).
 *
 * Creates a header to prefix to ever message printed with
 * wimaxll_msg().
 *
 * By default, diagnostics are printed with wimaxll_msg_hdr_default()
 * is used, which creates a "libwimaxll[DEVICENAME]" prefix.
 *
 * To change it:
 *
 * @code
 * ...
 * static
 * void my_wimaxll_msg_hdr(char *buf, size_t len, struct
 *                         wimaxll_handle *wmx)
 * {
 *         snprintf(buf, len, "my prefix: ");
 * }
 * ...
 * wimaxll_msg_hdr = my_wimaxll_msg_hdr;
 * ...
 * wimaxll_open(BLAH);
 * @endcode
 *
 * \ingroup helper_log
 */
void (*wimaxll_msg_hdr_cb)(char *buf, size_t buf_len,
			   struct wimaxll_handle *wmx, enum w_levels level,
			   const char *origin_str, unsigned origin_line) =
	wimaxll_msg_hdr_default;


static
void wimaxll_vlmsg(unsigned level, unsigned current_level,
		  const char *origin_str, unsigned origin_int,
		  struct wimaxll_handle *wmx, const char *fmt, va_list vargs)
{
	char header[64] = "";
	if (level > current_level && level != W_PRINT)
		return;
	if (wimaxll_msg_hdr_cb)
		wimaxll_msg_hdr_cb(header, sizeof(header), wmx,
				   level, origin_str, origin_int);
	wimaxll_vlmsg_cb(wmx, level, header, fmt, vargs);
}


/**
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
 * \ingroup helper_log
 */
void wimaxll_msg(struct wimaxll_handle *wmx, const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	wimaxll_vlmsg(W_PRINT, W_PRINT, NULL, 0, wmx, fmt, vargs);
	va_end(vargs);
}


/**
 * Prints library diagnostic messages with a predefined format
 * [frontend] and log level control
 *
 * @param level level of the messagve
 * @param current_level current logging level
 * @param origin_str Origin of the message (used for a source file
 *     name or function name).
 * @param origin_line Origin of the message (used for a line in a
 *     source file or function).
 * @param wmx WiMAX handle; if NULL, no device header will be presented.
 * @param fmt printf-like format followed by any arguments
 *
 * Called by the library functions to print status/error messages if
 * the current log level allows it. By default these are sent over to
 * stderr.
 *
 * However, library users can change this default behaviour by setting
 * wimaxll_vmsg() as documented in that function pointer's
 * documentation.
 *
 * \ingroup helper_log
 */
void wimaxll_lmsg(unsigned level, unsigned current_level,
		  const char *origin_str, unsigned origin_line,
		  struct wimaxll_handle *wmx, const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	wimaxll_vlmsg(level, current_level, origin_str, origin_line,
		      wmx, fmt, vargs);
	va_end(vargs);
}


/**
 * Log an error message to stderr and abort
 *
 * \param result exit code to abort with
 * \param fmt: printf-like format string and its arguments
 *
 * @ingroup: helper_log
 */
void w_abort(int result, const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	wimaxll_vlmsg(W_ERROR, W_ERROR, __FILE__, __LINE__, NULL, fmt, vargs);
	va_end(vargs);
	exit(result);
}

/* These are defined in the header file */

/**
 * Log a printf-like error message to stderr
 *
 * @fn w_error(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like warning message to stderr
 *
 * @fn w_warn(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like error message to stderr
 *
 * @fn w_info(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like error message to stdout
 *
 * @fn w_print(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 0)
 *
 * @fn w_d0(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 1)
 *
 * @fn w_d1(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 2)
 *
 * @fn w_d2(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 3)
 *
 * @fn w_d3(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 4)
 *
 * @fn w_d4(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 5)
 *
 * @fn w_d5(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 6)
 *
 * @fn w_d6(fmt...)
 * @ingroup helper_log
 */

/**
 * Log a printf-like debug message (level 7)
 *
 * @fn w_d7(fmt...)
 * @ingroup helper_log
 */
