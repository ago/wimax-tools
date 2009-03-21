/*
 * Linux WiMax
 * Swiss-army WiMAX knife header file
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
 * This is the common interface for plugins to implement support for
 * the 'wimaxll' command line tool. 
 */
#ifndef __wimaxll__cmd_h__
#define __wimaxll__cmd_h__

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <net/if.h>
#include <errno.h>
#include <argp.h>
#include <wimaxll.h>

struct wimax_handle;

/* A plugin definition and declaration */
struct plugin {
	const char *name;
	const char *version;
	int (*init)(void);
	void (*exit)(void);
	void *dl_handle;
	int active;
};

#define PLUGIN(_name, _version, _init, _exit)		\
struct plugin plugin = {				\
	.name = _name,					\
	.version = _version,				\
	.init = _init,					\
	.exit = _exit,					\
};

/* Definining support for a command */
struct cmd {
	char *name;
	struct argp argp;
	int (*fn)(struct cmd *, struct wimaxll_handle *,
		  int argc, char **argv);
};

int w_cmd_register(struct cmd *);
void w_cmd_unregister(struct cmd *);

/* Misc utilities */
void w_cmd_need_if(struct wimaxll_handle *);
void w_abort(int result, const char *fmt, ...);
void w_msg(unsigned, const char *, unsigned, const char *fmt, ...);

/* Logging / printing */
enum {
	W_ERROR,
	W_WARN,
	W_INFO,
	W_PRINT,
	W_D1,
	W_D2,
	W_D3,
};

#define w_error(fmt...) w_msg(W_ERROR, __FILE__, __LINE__, "E: " fmt)
#define w_warn(fmt...) w_msg(W_WARN, __FILE__, __LINE__, "W: " fmt)
#define w_info(fmt...) w_msg(W_INFO, __FILE__, __LINE__, "I: " fmt)
#define w_print(fmt...) w_msg(W_PRINT, __FILE__, __LINE__, fmt)
#define w_d1(fmt...) w_msg(W_D1, __FILE__, __LINE__, "D1: " fmt)
#define w_d2(fmt...) w_msg(W_D2, __FILE__, __LINE__, "D2: " fmt)
#define w_d3(fmt...) w_msg(W_D3, __FILE__, __LINE__, "D3: " fmt)

#endif /* #define __wimaxll__cmd_h__ */
