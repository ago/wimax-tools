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

static
char *status_to_str(int status)
{
	int bytes = 0;
	static char str[64];

	if ((status & 0x1) == WIMAX_RF_OFF)
		bytes += snprintf(str + bytes, sizeof(str),
				  "HW off");
	else
		bytes += snprintf(str + bytes, sizeof(str),
				  "HW on");
	if ((status & 0x2) >> 1 == WIMAX_RF_OFF)
		bytes += snprintf(str + bytes, sizeof(str),
				  " SW off");
	else
		bytes += snprintf(str + bytes, sizeof(str),
				  " SW on");
	return str;
}

int main(int argc, char **argv)
{
	int result;
	struct wimaxll_handle *wmx;
	char *dev_name, *op_name;
	enum wimax_rf_state op;

	if (argc < 3) {
		fprintf(stderr, "E: need an argument "
			"(device interface name) and new status "
			"{on,off,query}\n");
		return 1;
	}

	dev_name = argv[1];
	op_name = argv[2];

	wmx = wimaxll_open(dev_name);
	if (wmx == NULL) {
		fprintf(stderr, "E: libwimax: open of interface %s "
			"failed: %m\n", dev_name);
		result = -errno;
		goto error_wimaxll_open;
	}

	result = -EINVAL;
	if (!strcasecmp("on", op_name))
		op = WIMAX_RF_ON;
	else if (!strcasecmp("off", op_name))
		op = WIMAX_RF_OFF;
	else if (!strcasecmp("query", op_name))
		op = WIMAX_RF_QUERY;
	else {
		fprintf(stderr, "E: unknown rfkill op %s\n", op_name);
		goto error_bad_op;
	}

	result = wimaxll_rfkill(wmx, op);
	if (result < 0) {
		fprintf(stderr, "E: wimaxll_rfkill(%s): %d\n", op_name, result);
		goto error_rfkill;
	}
	fprintf(stderr, "I: rfkill status is 0x%x (%s)\n", result,
		status_to_str(result));
	if (op != WIMAX_RF_QUERY && (result & 0x2) >> 1 != op) {
		fprintf(stderr, "E: rfkill failed to turn device %s\n",
			op_name);
		goto error_check;
	}
	result = 0;
error_check:
error_rfkill:
error_bad_op:
	wimaxll_close(wmx);
error_wimaxll_open:
	return result;
}

