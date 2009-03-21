/*
 * Linux WiMax
 * Shared/common routines
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
 * These are a set of facilities used by the implementation of the
 * different ops in this library.
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <linux/types.h>
#include <netlink/msg.h>
#include <netlink/genl/genl.h>
#include <wimaxll.h>
#include "internal.h"
#define D_LOCAL 0
#include "debug.h"
#include "names-vals.h"


enum wimax_st wimaxll_state_by_name(const char *name)
{
	size_t itr, max = wimaxll_array_size(wimax_st_names_vals) - 1;
	
	for (itr = 0; itr < max; itr++)
		if (strcmp(wimax_st_names_vals[itr].name, name) == 0)
			return wimax_st_names_vals[itr].value;
	return __WIMAX_ST_INVALID;
}


const char * wimaxll_state_to_name(enum wimax_st st)
{
	size_t itr, max = wimaxll_array_size(wimax_st_names_vals) - 1;
	
	for (itr = 0; itr < max; itr++)
		if (wimax_st_names_vals[itr].value == st)
			return wimax_st_names_vals[itr].name;
	return NULL;
}


size_t wimaxll_states_snprintf(char *str, size_t size)
{
	size_t itr, max = wimaxll_array_size(wimax_st_names_vals) - 1;
	size_t bytes = 0;
	
	for (itr = 0; itr < max && bytes < size; itr++)
		bytes += snprintf(str + bytes, size - bytes,
				  "%s ", wimax_st_names_vals[itr].name);
	return bytes;
}
