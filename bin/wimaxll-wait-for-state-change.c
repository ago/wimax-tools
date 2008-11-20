/*
 * Linux WiMax
 * Execute rfkill commands
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
 * FIXME: docs
 */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wimaxll.h>


int main(int argc, char **argv)
{
	int result;
	struct wimaxll_handle *wmx;
	char *dev_name;
	enum wimax_st old, new;

	if (argc < 2) {
		fprintf(stderr, "E: need an argument "
			"(device interface name)\n");
		return 1;
	}

	dev_name = argv[1];

	wmx = wimaxll_open(dev_name);
	if (wmx == NULL) {
		fprintf(stderr, "E: libwimax: open of interface %s "
			"failed: %m\n", dev_name);
		result = -errno;
		goto error_wimaxll_open;
	}

	result = wimaxll_wait_for_state_change(wmx, &old, &new);
	if (result < 0) {
		fprintf(stderr, "E: wimaxll_wait_for_state_change: %d\n",
			result);
		goto error_op;
	}
	fprintf(stderr, "I: old state %u, new state %u\n", old, new);
	result = 0;
error_op:
	wimaxll_close(wmx);
error_wimaxll_open:
	return result;
}

