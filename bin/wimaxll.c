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
 *
 * 
 * Plugin based general tool -- it does the common infrastructure, but
 * as of now, there is not much for it. Exported functions (to
 * plugins) are wimaxll_*().
 *
 * A plugin must declare a non-static 'struct plugin plugin' data
 * structure, with DECLARE_PLUGIN() [wimaxll-tool.h] in order to count.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <net/if.h>
#include <errno.h>
#include <argp.h>
#include <dlfcn.h>
#include <wimaxll.h>
#include <wimaxll/version.h>
#include "config.h"
#include <wimaxll/cmd.h>
#include <glib.h>
#include <stdarg.h>


/* Global state ... */

struct main_args
{
	char ifname[IFNAMSIZ];
	unsigned ifindex;
	int verbosity;
	char **cmd_argv;
	size_t cmd_argc;
} main_args;


/*
 * Messages
 *
 * If the log level is zero, we assume is a normal message that the
 * user wants to see and send it to stdout. If it is negative, we
 * consider it debugging info; if it is over zero, a warning/error
 * message.  
 */
void w_msg(unsigned level, const char *file, unsigned line,
	   const char *fmt, ...)
{
	FILE *f;
	va_list vargs;
	f = level != W_PRINT? stderr : stdout;
	if (level <= main_args.verbosity || level == W_PRINT) {
		va_start(vargs, fmt);
		vfprintf(f, fmt, vargs);
		va_end(vargs);
	}
}

void w_vmsg(unsigned level, const char *file, unsigned line,
	   const char *fmt, va_list vargs)
{
	FILE *f;
	f = level != W_PRINT? stderr : stdout;
	if (level <= main_args.verbosity || level == W_PRINT)
		vfprintf(f, fmt, vargs);
}

void w_abort(int result, const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	w_vmsg(W_ERROR, __FILE__, __LINE__, fmt, vargs);
	va_end(vargs);
	exit(result);
}


/* Command handling */

static GList *__cmd_list = NULL;

int w_cmd_register(struct cmd *cmd)
{
	__cmd_list = g_list_append(__cmd_list, cmd);
	return 0;
}

void w_cmd_unregister(struct cmd *cmd)
{
	__cmd_list = g_list_remove(__cmd_list, cmd);
}

void w_cmd_need_if(struct wimaxll_handle *wmx)
{
	if (wmx == NULL)
		w_abort(1, "E: no interface specified; use -i or environment "
			"WIMAXLL_IF\n");
}

static
void __cmd_list_itr(gpointer _cmd, gpointer _f)
{
	struct cmd *cmd = _cmd;
	
	w_print("%s: ", cmd->name);
	argp_help(&cmd->argp, stdout,
		  0
//			  | ARGP_HELP_SHORT_USAGE
//			  | ARGP_HELP_SEE
		  | ARGP_HELP_LONG
		  | ARGP_HELP_PRE_DOC
		  | ARGP_HELP_POST_DOC
		  | ARGP_HELP_DOC
//			  | ARGP_HELP_BUG_ADDR
//			  | ARGP_HELP_LONG_ONLY
		  ,
		  cmd->name);
}


static
void cmd_list(void)
{
	g_list_foreach(__cmd_list, __cmd_list_itr, NULL);
	w_print("\nFor each command, --help is available\n");
}


static
int __cmd_get_itr(gconstpointer _cmd, gconstpointer _name)
{
	const struct cmd *cmd = _cmd;
	const char *name = _name;
	return strcmp(name, cmd->name);
}

static
struct cmd * cmd_get(const char *name)
{
	GList *l;
	l = g_list_find_custom(__cmd_list, name, __cmd_get_itr);
	return l == NULL? NULL : l->data;
}


/* Plugin handling */

static GList *plugin_list = NULL;

static
int plugin_init(void)
{
	GList *itr;
	GDir *dir;
	const gchar *file;
	gchar *filename;
	GPatternSpec *pattern;
	
	dir = g_dir_open(PLUGINDIR, 0, NULL);
	if (dir == NULL)
		goto error_no_plugindir;
	
	pattern = g_pattern_spec_new("wimaxll-pl-*.so");
	if (pattern == NULL)
		goto error_pattern;
	
	while ((file = g_dir_read_name(dir)) != NULL) {
		void *handle;
		struct plugin *plugin;

		if (g_pattern_match(pattern, strlen(file), file, NULL) == FALSE) {
			w_d2("skipping %s\n", file);
			continue;
		}
		filename = g_build_filename(PLUGINDIR, file, NULL);
		
		handle = dlopen(filename, RTLD_NOW);
		if (handle == NULL) {
			w_error("Can't load %s: %s\n", filename, dlerror());
			goto error_pl_dlopen;
		}

		plugin = dlsym(handle, "plugin");
		if (plugin == NULL) {
			w_error("Can't load symbol 'plugin': %s\n", plugin->name);
			goto error_pl_dlsym;
		}
		plugin->dl_handle = handle;
		plugin->active = 0;
		
		if (plugin->init == NULL) {
			w_error("Plugin %s lacks init method\n", dlerror());
			goto error_pl_noinit;
		}
		
		if (strcmp(plugin->version, WIMAXLL_VERSION)) {
			w_error("Plugin '%s': version mismatch (%s vs %s needed)\n",
				plugin->name, plugin->version, WIMAXLL_VERSION);
			goto error_pl_version;
		}
		
		plugin_list = g_list_append(plugin_list, plugin);
		g_free(filename);
		continue;

	error_pl_version:
	error_pl_noinit:
	error_pl_dlsym:
		dlclose(handle);
	error_pl_dlopen:
		g_free(filename);
		continue;
	}
	
	for (itr = plugin_list; itr; itr = itr->next) {
		int result;
		struct plugin *plugin = itr->data;
		
		result = plugin->init();
		if (result < 0) {
			w_error("Plugin '%s' failed to initialize: %d\n",
				plugin->name, result);			
			continue;
		}
		plugin->active = 1;
	}
	
	g_pattern_spec_free(pattern);
error_pattern:
	g_dir_close(dir);
error_no_plugindir:
	return 0;
}
	
static
void plugin_exit(void)
{
	GList *itr;

	for (itr = plugin_list; itr; itr = itr->next) {
		struct plugin *plugin = itr->data;
		if (plugin->active && plugin->exit)
			plugin->exit();
		dlclose(plugin->dl_handle);
	}
	g_list_free(plugin_list);
}


/* Main program & cmd line handling */

const
char *argp_program_version = "wimaxll v" VERSION;

const
char *argp_program_bug_address = PACKAGE_BUGREPORT;

static
struct argp_option main_options[] = {
	{ "verbose",  'v', 0,       0,
	  "Increase verbosity" },
	{ "quiet",    'q', 0,       0,
	  "Don't produce any output" },
	{ "silent",   's', 0,       OPTION_ALIAS },

	{ 0, 0, 0, 0, " " },
	{ "interface",'i', "INTERFACE", 0, 
	  "Network interface to work on (specify name or index); this value is "
	  "obtained by default from the environment variable WIMAXLL_IF." },

	{ 0, 0, 0, 0, " " },
	{ "commands", 'c', 0, 0, 
	  "List available commands" },

	{ 0 }
};


#define __STRINGFY(a) #a
#define STRINGFY(a) __STRINGFY(a)

static
int parse_if(struct main_args *args, const char *arg) 
{
	if (arg == NULL) {	/* to chain getenv */
		args->ifindex = 0;
		strcpy(args->ifname, "");
		return 0;
	}
	if (sscanf(arg, "%u", &args->ifindex) == 1) {
		if (if_indextoname(args->ifindex, args->ifname) == NULL) {
			w_error("Cannot find interface index '%u'\n",
				args->ifindex);
			return -ENODEV;
		}
	}
	else if (sscanf(arg, "%" STRINGFY(IFNAMSIZ) "s", args->ifname) == 1) {
		args->ifindex = if_nametoindex(args->ifname);
		if (args->ifindex == 0) {
			w_error("Cannot find interface named '%s'\n",
				args->ifname);
			return -ENODEV;
		}
	}
	else {
		w_error("Cannot parse '%s' as network "
			"interface name or index\n", arg);
		return -EINVAL;
	}
	return 0;
}

static
int main_parser(int key, char *arg, struct argp_state *state)
{
	int result = 0;
	struct main_args *args = state->input;

	w_d3("key %08x arg_num %d arg %s\n", key, state->arg_num, arg);
	switch (key)
	{
	case 'q': case 's':
		args->verbosity = 0;
		break;
	case 'v':
		args->verbosity++;
		break;
	case 'i':
		result = parse_if(args, arg);
		break;
	case 'c':
		cmd_list();
		exit(0);
		break;

	case ARGP_KEY_NO_ARGS:
		argp_usage(state);

	case ARGP_KEY_ARG:
		w_d3("key arg, argc %d next %d\n", state->argc, state->next);
		args->cmd_argv = &state->argv[state->next - 1];
		args->cmd_argc = state->argc - (state->next - 1);
		/* Stop consuming args right here */ 
		state->next = state->argc;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return result;
}

static
struct argp main_argp = {
	.options = main_options,
	.parser = main_parser,
	.args_doc = "COMMAND [COMMAND OPTIONS...]",
	.doc =
	"Control WiMAX devices at a very low level\n"
	"\v"
	"For a list of available commands, run with --commands"
};

int main(int argc, char **argv)
{
	int cnt;
	int result;
	struct cmd *cmd;
	struct wimaxll_handle *wmx;
	char *str;
	
	str = getenv("WIMAXLL_VERBOSITY");
	main_args.verbosity = str? atoi(str) : 0;
	parse_if(&main_args, getenv("WIMAXLL_IF"));

	plugin_init();
	
	result = argp_parse(&main_argp, argc, argv, ARGP_IN_ORDER, 0, &main_args);
	if (result < 0)
		goto error_argp_parse;
	
	w_d3("default args\n"
	     "   ifname    %s\n"
	     "   ifindex   %d\n"
	     "   verbosity %d\n"
	     "   command   %s\n"
	     "   cmd opts  [%zu]:\n",
	     main_args.ifname, main_args.ifindex, main_args.verbosity,
	     main_args.cmd_argv[0], main_args.cmd_argc);
	for(cnt = 0; cnt < main_args.cmd_argc; cnt++)
		w_d3("     %s\n", main_args.cmd_argv[cnt]);

	cmd = cmd_get(main_args.cmd_argv[0]);
	if (cmd == NULL) {
		w_error("command '%s' unrecognized; "
			"check --commands\n", main_args.cmd_argv[0]);
		result = -EINVAL;
		goto error_cmd_get;
	}
	
	if (main_args.ifindex != 0) {
		wmx = wimaxll_open(main_args.ifname);
		if (wmx == NULL) {
			w_error("%s: cannot open: %m\n", main_args.ifname);
			result = -errno;
			goto error_wimaxll_open;
		}
	}
	else
		wmx = NULL;
	result = cmd->fn(cmd, wmx, main_args.cmd_argc, main_args.cmd_argv);
	if (result < 0)
		w_error("%s: failed: %s\n", cmd->name, strerror(-result));
	wimaxll_close(wmx);
	result = 0;
error_wimaxll_open:
error_cmd_get:
error_argp_parse:
	plugin_exit();
	return result;
}

