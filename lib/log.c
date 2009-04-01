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


/**
 * Log a message to (varargs version) if log level allows it
 *
 * \param level level of the messagve
 * \param current_level current logging level
 * \param tag a str to print along with \e line (if not NULL, its
 *     not printed).
 * \param line an integer to print along with \e tag (if \e tag is not
 *     NULL)
 * \param fmt printf-like format string
 * \param vargs arguments to \e fmt
 *
 * \internal
 *
 * Use w_*() functions only
 *
 * @ingroup: helper_log
 */
void __w_vmsg(unsigned level, unsigned current_level,
	      const char *tag, unsigned line,
	      const char *fmt, va_list vargs)
{
	FILE *f;
	f = level != W_PRINT? stderr : stdout;
	if (level <= current_level || level == W_PRINT) {
		if (tag)
			fprintf(f, "%s:%u: ", tag, line);
		vfprintf(f, fmt, vargs);
	}
}


/**
 * Log a message to if log level allows it
 *
 * \param level level of the messagve
 * \param current_level current logging level
 * \param tag a str to print along with \e line (if not NULL, its
 *     not printed).
 * \param line an integer to print along with \e tag (if \e tag is not
 *     NULL)
 * \param fmt printf-like format string, plus their arguments
 *
 * \internal
 *
 * Use w_*() functions only
 *
 * @ingroup: helper_log
 */
void __w_msg(unsigned level, unsigned current_level,
	     const char *tag, unsigned line,
	     const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	__w_vmsg(level, current_level, tag, line, fmt, vargs);
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
	__w_vmsg(W_ERROR, W_ERROR, __FILE__, __LINE__, fmt, vargs);
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
