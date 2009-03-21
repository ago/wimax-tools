/*
 * Linux WiMax
 * Swiss-army WiMAX knife: wait for an state change
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


struct wfsc_args
{
	struct cmd *cmd;
	enum wimax_st state;
	char **argv;
	size_t argc;
	unsigned timeout;
};


static
struct argp_option wfsc_options[] = {
#if 0
	/* not really done ... will have to rewrite some code and just
	 * use the callback with a select stuff */ 
	{ "timeout",  't', "TIMEOUT",       0,
	  "Time (in seconds) to wait for the state change; "
	  "0 to wait for ever." },
#endif
	{ "help-states",  's', 0,       0,
	  "List known WiMAX states." },
	{ 0 }
};


static
int wfsc_parser(int key, char *arg, struct argp_state *state)
{
	int result = 0;
	struct wfsc_args *args = state->input;
	char str[256];
	
	switch (key)
	{
	case 't':
		if (sscanf(arg, "%u", &args->timeout) != 1)
			argp_error(state, "E: %s: cannot parse as a timeout (in seconds)\n",
				   arg);
		break;
		
	case 's':
		wimaxll_states_snprintf(str, sizeof(str));
		w_print("%s: known WiMAX device states: %s\n",
			args->cmd->name, str);
		exit(0);
		break;
		
	case ARGP_KEY_ARG:
		args->state = wimaxll_state_by_name(arg);
		if (args->state == __WIMAX_ST_INVALID)
			argp_error(state, "E: %s: unknown wimax state\n", arg);
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
int wfsc_fn(struct cmd *cmd, struct wimaxll_handle *wmx,
	      int argc, char **argv)
{
	int result;
	struct wfsc_args args;
	enum wimax_st old_state, new_state;
	
	args.cmd = cmd;
	args.state = __WIMAX_ST_INVALID;	/* meaning any */
	result = argp_parse(&cmd->argp, argc, argv,
			    0, 0, &args);
	if (result < 0)
		goto error_argp_parse;
	w_cmd_need_if(wmx);
	while(1) {
		result = wimaxll_wait_for_state_change(wmx, &old_state, &new_state);
		if (result < 0)
			w_abort(1, "%s: error waiting: %d (%s)\n", cmd->name,
				result, strerror(result));
		w_info("%d: %s\n", new_state, wimaxll_state_to_name(new_state));
		w_print("%d: %s\n", new_state, wimaxll_state_to_name(new_state));
		if (new_state == args.state)
			break;
		if (args.state == __WIMAX_ST_INVALID)
			break;
	}
error_argp_parse:
	return result;
}

static
struct cmd wfsc_cmd = {
	.name = "wait-for-state-change",
	.argp = {
		.options = wfsc_options,
		.parser = wfsc_parser,
		.args_doc = "[STATE]",
		.doc = "Wait for a device state change; if no state is "
		"specified, waits until any state transition happens\n",
	},
	.fn = wfsc_fn,
};


static
int wfsc_init(void) 
{
	return w_cmd_register(&wfsc_cmd);
}

static
void wfsc_exit(void) 
{
	w_cmd_unregister(&wfsc_cmd);
}

PLUGIN("wfsc", WIMAXLL_VERSION, wfsc_init, wfsc_exit);
