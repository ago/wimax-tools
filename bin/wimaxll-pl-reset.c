/*
 * Linux WiMax
 * Swiss-army WiMAX knife
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
#define _GNU_SOURCE
#include <argp.h>
#include <error.h>
#include <wimaxll.h>
#include <wimaxll/version.h>
#include <wimaxll/cmd.h>


static
struct argp_option reset_options[] = {
	{ 0 }
};

static
int reset_parser(int key, char *arg, struct argp_state *state)
{
	return ARGP_ERR_UNKNOWN;
}

static
int reset_fn(struct cmd *cmd, struct wimaxll_handle *wmx,
	      int argc, char **argv)
{
	int result;
	result = argp_parse(&cmd->argp, argc, argv,
			    ARGP_IN_ORDER | ARGP_PARSE_ARGV0, 0, NULL);
	if (result < 0)
		goto error_argp_parse;
	w_cmd_need_if(wmx);
	result = wimaxll_reset(wmx);
	if (result < 0)
		w_error("reset failed: %d (%s)\n", result, strerror(-result));
error_argp_parse:
	return result;
}

static
struct cmd reset_cmd =  {
	.name = "reset",
	.argp = {
		.options = reset_options,
		.parser = reset_parser,
		.args_doc = "",
		.doc = "Resets a WiMAX device\n",
	},
	.fn = reset_fn,
};

static
int reset_init(void) 
{
	return w_cmd_register(&reset_cmd);
}

static
void reset_exit(void) 
{
	w_cmd_unregister(&reset_cmd);
}


PLUGIN("reset", WIMAXLL_VERSION, reset_init, reset_exit);
