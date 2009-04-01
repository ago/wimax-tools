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

void __w_vmsg(unsigned level, unsigned current_level,
	      const char *tag, unsigned line,
	      const char *fmt, va_list vargs);

void __w_msg(unsigned level, unsigned current_level,
	     const char *tag, unsigned line,
	     const char *fmt, ...);

void w_abort(int result, const char *fmt, ...);

#define w_error(fmt...) __w_msg(W_ERROR, W_VERBOSITY, __func__, __LINE__, "E: " fmt)
#define w_warn(fmt...) __w_msg(W_WARN, W_VERBOSITY, __func__, __LINE__, "W: " fmt)
#define w_info(fmt...) __w_msg(W_INFO, W_VERBOSITY, __func__, __LINE__, "I: " fmt)
#define w_print(fmt...) __w_msg(W_PRINT, W_VERBOSITY, __func__, __LINE__, fmt)
#define w_d0(fmt...) __w_msg(W_D0, W_VERBOSITY, __func__, __LINE__, "D0: " fmt)
#define w_d1(fmt...) __w_msg(W_D1, W_VERBOSITY, __func__, __LINE__, "D1: " fmt)
#define w_d2(fmt...) __w_msg(W_D2, W_VERBOSITY, __func__, __LINE__, "D2: " fmt)
#define w_d3(fmt...) __w_msg(W_D3, W_VERBOSITY, __func__, __LINE__, "D3: " fmt)
#define w_d4(fmt...) __w_msg(W_D4, W_VERBOSITY, __func__, __LINE__, "D4: " fmt)
#define w_d5(fmt...) __w_msg(W_D5, W_VERBOSITY, __func__, __LINE__, "D5: " fmt)
#define w_d6(fmt...) __w_msg(W_D6, W_VERBOSITY, __func__, __LINE__, "D6: " fmt)
#define w_d7(fmt...) __w_msg(W_D7, W_VERBOSITY, __func__, __LINE__, "D7: " fmt)

#endif /* #define __wimaxll__log_h__ */
