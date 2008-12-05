/*
 * Linux WiMax
 * Pipe implementation
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
/**
 * \defgroup the_pipe_interface_group The pipe interface
 *
 * This is a collection of tools for managing access to the different
 * pipes that a WiMAX kernel driver can export.
 *
 * There is always a default pipe, the \e message pipe, on which the
 * kernel sends notifications (such as the <em> \ref state_change_group
 * "state change" </em> notification) and \ref the_messaging_interface
 * "driver-specific messages".
 *
 * The driver can create other pipes for sending messages out of band
 * without clogging the default \e message pipe. This can be used, for
 * example, for high bandwidth driver-specific diagnostics.
 *
 * This is a low level interface and other than wimaxll_pipe_read()
 * for reading notifications from the kernel stack in a mainloop, a
 * normal library user should not need to use much of it.
 *
 * It is implemented a very thin layer on top of the \ref mc_rx
 * "multicast RX interface".
 *
 * \section usage Usage
 *
 * If a WiMAX driver exports a set of pipes (each one with a different
 * name), a handle to it can be opened with:
 *
 * \code
 * pipe_id = wimaxll_pipe_open(wmx, "pipename");
 * \endcode
 *
 * likewise, to close said pipe:
 *
 * \code
 * wimaxll_pipe_close(wmx, pipe_id);
 * \endcode
 *
 * To obtain the file descriptor associated to an opened pipe so that
 * it can be fed to select():
 *
 * \code
 * fd = wimaxll_pipe_fd(wmx, pipe_id);
 * \endcode
 *
 * and finally to read from said pipe and execute the callbacks
 * associated to each different notification from the kernel
 *
 * \code
 * wimaxll_pipe_read(wmx, pipe_id);
 * \endcode
 *
 * The default \e message pipe is always open, and it's \e pipe_id can
 * be obtained with wimaxll_msg_pipe_id().
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


/**
 * Open a handle to receive messages from a WiMAX pipe
 *
 * \param wmx WiMAX device handle
 * \param pipe_name Name of the pipe to open
 *
 * \return If successful, a non-negative pipe handle number (\e {the
 *     pipe id}). In case of error, a negative errno code.
 *
 * Opens a handle to receive data from the given WiMAX named
 * pipe. wimaxll_pipe_msg_read() can be used to listen for data sent by
 * the kernel on the named pipe.
 *
 * Only one handle may be opened at the same time to each pipe.
 *
 * \ingroup the_pipe_interface_group
 */
int wimaxll_pipe_open(struct wimaxll_handle *wmx, const char *pipe_name)
{
	return wimaxll_mc_rx_open(wmx, pipe_name);
}


/**
 * Return the file descriptor associated to a multicast group
 *
 * \param wmx WiMAX device handle
 * \param pipe_id Pipe ID, as returned by wimaxll_pipe_open().
 * \return file descriptor associated to the multicast group, that can
 *     be fed to functions like select().
 *
 * This allows to select() on the file descriptor, which will block
 * until a message is available, that then can be read with
 * wimaxll_pipe_read() or wimaxll_pipe_msg_read().
 *
 * \ingroup the_pipe_interface_group
 */
int wimaxll_pipe_fd(struct wimaxll_handle *wmx, unsigned pipe_id)
{
	return wimaxll_mc_rx_fd(wmx, pipe_id);
}


/**
 * Read any kind of kernel messages from a pipe and execute callbacks
 *
 * \param wmx WiMAX device handle
 * \param pipe_id Pipe to read from [as returned by wimaxll_pipe_open()].
 * \return If successful, 0 and the callbacks to known notifications will
 *     be called. On error, a negative errno code:
 *
 *     -%EINPROGRESS: no messages received
 *
 * When reading notifications from the kernel, any unknown type will
 * be ignored.
 *
 * \note This is a blocking call.
 *
 * \ingroup the_pipe_interface_group
 */
ssize_t wimaxll_pipe_read(struct wimaxll_handle *wmx, unsigned pipe_id)
{
	ssize_t result = -EINPROGRESS;
	d_fnstart(3, wmx, "(wmx %p pipe_id %u)\n", wmx, pipe_id);
	while (result == -EINPROGRESS || result == -ENODATA)
		result = wimaxll_mc_rx_read(wmx, pipe_id);
	d_fnend(3, wmx, "(wmx %p pipe_id %u) = %zd\n", wmx, pipe_id, result);
	return result;
}


/**
 * Close an a connection to a WiMAX pipe
 *
 * @param wmx WiMAX device handle
 * \param pipe_id Pipe to close [as returned by wimaxll_pipe_open()].
 *
 * Closes the connection to the given WiMAX pipe.
 *
 * \ingroup the_pipe_interface_group
 */
void wimaxll_pipe_close(struct wimaxll_handle *wmx, unsigned pipe_id)
{
	wimaxll_mc_rx_close(wmx, pipe_id);
}
