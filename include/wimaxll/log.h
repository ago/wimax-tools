/*
 * Linux WiMax
 * Simple log helpers
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
 *
 * \defgroup helper_log Set of simple log helpers
 *
 * Log messages to stdout/stderr, with simple log level management.
 *
 * If the log level is W_PRINT, we assume is a normal message that the
 * user wants to see and send it to stdout. If it is any other,
 * evaluate if it should be printed based on the current level and
 * then print it to stderr.
 *
 * Before including, W_VERBOSITY must be defined to something that
 * yields a numeric value out of of \e { enum w_levels }. When any of
 * the w_*() functions/macros is called, if the level it is called
 * with is less or equal than the current level defied by W_VERBOSITY,
 * it'll be ran, if not, it'll be ignored:
 *
 * @code
 *
 * #define W_VERBOSITY W_INFO (or myglobalstruct.verbosity)
 * #include <wimaxll/log.h>
 *
 * somefunc()
 * {
 *        ...
 *        w_d1("debug message\n");
 *        ...
 * }
 *
 * @endcode
 *
 * To control where the log/progress messages go and how they are
 * formatted, the client can set a couple of function pointers
 * wimaxll_msg_hdr_cb() (which controls how a header/prefix for the
 * message is created) and wimaxll_vlmsg_cb(), which takes the message
 * and delivers it to whichever destination.
 *
 * The default implementations are wimaxll_msg_hdr_default() and
 * wimaxll_vlmsg_default(), which add a
 * "libwimall[DEVICENAME]:" header (with an optional "(@
 * FUNCTION:LINE)") and deliver the message to \e stdout if it is a
 * normal message (\e W_PRINT) or else if it is an error, warning,
 * info or debug message, it is sent to \e stderr.
 */

#ifndef __wimaxll__log_h__
#define __wimaxll__log_h__

#include <stdio.h>
#include <stdarg.h>

#ifndef W_VERBOSITY
#error Please #define W_VERBOSITY before including this file
#endif

/* Logging / printing */
enum w_levels {
	W_ERROR,
	W_WARN,
	W_INFO,
	W_PRINT,
	W_D0,
	W_D1,
	W_D2,
	W_D3,
	W_D4,
	W_D5,
	W_D6,
	W_D7,
};

struct wimaxll_handle;

void wimaxll_msg(struct wimaxll_handle *, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

void wimaxll_lmsg(unsigned level, unsigned current_level,
		  const char *origin_str, unsigned origin_line,
		  struct wimaxll_handle *wmx, const char *fmt, ...)
	__attribute__ ((format(printf, 6, 7)));

extern void (*wimaxll_vlmsg_cb)(struct wimaxll_handle *, unsigned,
				const char *, const char *, va_list);
void wimaxll_vlmsg_stderr(struct wimaxll_handle *, unsigned,
			  const char *, const char *, va_list);

extern void (*wimaxll_msg_hdr_cb)(char *, size_t, struct wimaxll_handle *,
				  enum w_levels, const char *, unsigned);
void wimaxll_msg_hdr_default(char *, size_t, struct wimaxll_handle *,
			     enum w_levels, const char *, unsigned);

void w_abort(int result, const char *fmt, ...);

#define w_error(fmt...) wimaxll_lmsg(W_ERROR, W_VERBOSITY, __func__, __LINE__, NULL, "E: " fmt)
#define w_warn(fmt...) wimaxll_lmsg(W_WARN, W_VERBOSITY, __func__, __LINE__, NULL, "W: " fmt)
#define w_info(fmt...) wimaxll_lmsg(W_INFO, W_VERBOSITY, __func__, __LINE__, NULL, "I: " fmt)
#define w_print(fmt...) wimaxll_lmsg(W_PRINT, W_VERBOSITY, __func__, __LINE__, NULL, fmt)
#define w_d0(fmt...) wimaxll_lmsg(W_D0, W_VERBOSITY, __func__, __LINE__, NULL, "D0: " fmt)
#define w_d1(fmt...) wimaxll_lmsg(W_D1, W_VERBOSITY, __func__, __LINE__, NULL, "D1: " fmt)
#define w_d2(fmt...) wimaxll_lmsg(W_D2, W_VERBOSITY, __func__, __LINE__, NULL, "D2: " fmt)
#define w_d3(fmt...) wimaxll_lmsg(W_D3, W_VERBOSITY, __func__, __LINE__, NULL, "D3: " fmt)
#define w_d4(fmt...) wimaxll_lmsg(W_D4, W_VERBOSITY, __func__, __LINE__, NULL, "D4: " fmt)
#define w_d5(fmt...) wimaxll_lmsg(W_D5, W_VERBOSITY, __func__, __LINE__, NULL, "D5: " fmt)
#define w_d6(fmt...) wimaxll_lmsg(W_D6, W_VERBOSITY, __func__, __LINE__, NULL, "D6: " fmt)
#define w_d7(fmt...) wimaxll_lmsg(W_D7, W_VERBOSITY, __func__, __LINE__, NULL, "D7: " fmt)

#endif /* #define __wimaxll__log_h__ */
