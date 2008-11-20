/*
 * Linux WiMax
 * Test of pipe API: dump messages received from named pipe
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
void dump(const void *_ptr, size_t size)
{
	const unsigned char *ptr = _ptr;
	char str[64];
	size_t cnt, itr;
	for (itr = cnt = 0; cnt < size; cnt++) {
		itr += snprintf(str + itr, sizeof(str) - itr,
				"%02x ", ptr[cnt]);
		if ((cnt > 0 && (cnt + 1) % 8 == 0) || (cnt == size - 1)) {
			printf("%s\n", str);
			itr = 0;
		}
	}
}


int main(int argc, char **argv)
{
	ssize_t result;
	unsigned pipe_id;
	struct wimaxll_handle *wmx;
	char *dev_name, *pipe_name;
	void *buf;

	if (argc < 3) {
		fprintf(stderr, "E: need two arguments: IFNAME PIPENAME\n");
		return 1;
	}

	dev_name = argv[1];
	pipe_name = argv[2];
	wmx = wimaxll_open(dev_name);
	if (wmx == NULL) {
		fprintf(stderr, "E: libwimax: open of interface %s "
			"failed: %m\n", dev_name);
		result = -errno;
		goto error_wimaxll_open;
	}

	fprintf(stderr, "I: Reading from pipe %s\n", pipe_name);
	result = wimaxll_pipe_open(wmx, pipe_name);
	if (result < 0) {
		fprintf(stderr, "E: cannot open pipe %s: %d\n",
			pipe_name, result);
		goto error_pipe_open;
	}
	pipe_id = result;

	while (1) {
		result = wimaxll_pipe_msg_read(wmx, pipe_id, &buf);
		if (result < 0) {
			fprintf(stderr, "E: reading from pipe %s failed: %d\n",
				pipe_name, result);
			break;
		}
		printf("I: message received from pipe %s, %zu bytes\n",
		       pipe_name, result);
		dump(buf, result);
		wimaxll_pipe_msg_free(buf);
	}
	wimaxll_pipe_close(wmx, pipe_id);
	result = 0;
error_pipe_open:
	wimaxll_close(wmx);
error_wimaxll_open:
	return result;
}

