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
#include <wimaxll.h>
#include <wimaxll/version.h>
#include <wimaxll/cmd.h>


struct rfkill_args
{
	struct cmd *cmd;
	enum wimax_rf_state op;
	char **argv;
	size_t argc;
};


static
struct argp_option rfkill_options[] = {
	{ 0 }
};


static
int rfkill_parser(int key, char *arg, struct argp_state *state)
{
	int result = 0;
	struct rfkill_args *args = state->input;
	
	switch (key)
	{
	case ARGP_KEY_ARG:
		if (!strcasecmp("on", arg))
			args->op = WIMAX_RF_ON;
		else if (!strcasecmp("off", arg))
			args->op = WIMAX_RF_OFF;
		else if (!strcasecmp("query", arg))
			args->op = WIMAX_RF_QUERY;
		else
			argp_error(state, "E: unknown rfkill operation '%s'\n",
				   arg);
		args->argv = &state->argv[state->next];
		args->argc = state->argc - state->next;
		/* Stop consuming args right here */
		state->next = state->argc;
		break;

	default:
		result = ARGP_ERR_UNKNOWN;
	}
	return result;
}


static
char *rfkill_status_to_str(int status)
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


static
int rfkill_fn(struct cmd *cmd, struct wimaxll_handle *wmx,
	      int argc, char **argv)
{
	int result;
	struct rfkill_args args;

	args.cmd = cmd;
	args.op = WIMAX_RF_QUERY;
	result = argp_parse(&cmd->argp, argc, argv,
			    0, 0, &args);
	if (result < 0)
		goto error_argp_parse;
	w_cmd_need_if(wmx);
	result = wimaxll_rfkill(wmx, args.op);
	if (result < 0) {
		w_error("rfkill failed: %d (%s)\n", result, strerror(-result));
		goto error_rfkill;
	}
	w_print("rfkill status is 0x%x (%s)\n", result,
		rfkill_status_to_str(result));
	if (args.op != WIMAX_RF_QUERY && (result & 0x2) >> 1 != args.op) {
		w_error("rfkill failed to change device\n");
		result = -EIO;
	}
error_rfkill:
error_argp_parse:
	return result;
}

static
struct cmd rfkill_cmd = {
	.name = "rfkill",
	.argp = {
		.options = rfkill_options,
		.parser = rfkill_parser,
		.args_doc = "[query]|on|off",
		.doc = "Control the WiMAX radio state\n",
	},
	.fn = rfkill_fn,
};


static
int rfkill_init(void) 
{
	return w_cmd_register(&rfkill_cmd);
}

static
void rfkill_exit(void) 
{
	w_cmd_unregister(&rfkill_cmd);
}

PLUGIN("rfkill", WIMAXLL_VERSION, rfkill_init, rfkill_exit);
