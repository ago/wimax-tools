/*
 * Linux WiMax
 * User Space API Debug Support
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
 * Simple debug printing macros
 *
 * FIXME: doc
 * Invoke like:
 *
 * #define D_LOCAL 4
 * #include "debug.h"
 *
 * At the end of your include files.
 */
#include <wimaxll.h>

/* Master debug switch; !0 enables, 0 disables */
#define D_MASTER (!0)

/* Local (per-file) debug switch; #define before #including */
#ifndef D_LOCAL
#define D_LOCAL 0
#endif

#undef __d_printf
#undef d_fnstart
#undef d_fnend
#undef d_printf
#undef d_dump

static inline
void __d_dev_head(char *head, size_t size, const struct wimaxll_handle *_dev)
{
	if (_dev == NULL)
		snprintf(head, size, "libwimax: ");
	else if ((unsigned long)_dev < 4096) {
		fprintf(stderr, "libwimax: E: Corrupt "
			"device handle %p\n", _dev);
		snprintf(head, size, "libwimax[dev_n/a]: ");
	} else
		snprintf(head, size,
			 "libwimax[%s]: ", _dev->name);
}


#define __d_printf(l, _tag, _dev, f, a...)				\
do {									\
	const struct wimaxll_handle *__dev = (_dev);			\
	if (D_MASTER && D_LOCAL >= (l)) {				\
		char __head[64] = "";					\
		__d_dev_head(__head, sizeof(__head), __dev);		\
		fprintf(stderr, "%s%s" _tag ": " f, __head,		\
			__func__, ## a);				\
	}								\
} while (0 && _dev)

#define d_fnstart(l, _dev, f, a...) __d_printf(l, " FNSTART", _dev, f, ## a)
#define d_fnend(l, _dev, f, a...) __d_printf(l, " FNEND", _dev, f, ## a)
#define d_printf(l, _dev, f, a...) __d_printf(l, "", _dev, f, ## a)
#define d_test(l) (D_MASTER && D_LOCAL >= (l))

static inline
void __d_dump(const struct wimaxll_handle *dev,
	      const void *_ptr, size_t size)
{
	const unsigned char *ptr = _ptr;
	char str[64];
	size_t cnt, itr;
	for (itr = cnt = 0; cnt < size; cnt++) {
		itr += snprintf(str + itr, sizeof(str) - itr,
				"%02x ", ptr[cnt]);
		if ((cnt > 0 && (cnt + 1) % 8 == 0) || (cnt == size - 1)) {
			__d_printf(D_LOCAL, "", dev, "%s\n", str);
			itr = 0;
		}
	}
}

#define d_dump(l, dev, ptr, size)		\
do {						\
	if (d_test(l))				\
		__d_dump(dev, ptr, size);	\
} while (0)
