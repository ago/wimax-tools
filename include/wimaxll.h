/*
 * Linux WiMax
 * User Space API
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
 *
 */
/**
 * @mainpage
 *
 * This is a simple library to control WiMAX devices through the
 * control API exported by the Linux kernel WiMAX Stack. It provides
 * means to execute functions exported by the stack and to receive its
 * notifications.
 *
 * Because of this, this is a callback oriented library. It is
 * designed to be operated asynchronously and/or in an event loop. For
 * the very simple cases, helpers that implement synchronous
 * functionality are available.
 *
 * This library is provided as a convenience and using it is not
 * required to talk to the WiMAX kernel stack. It is possible to do so
 * by interacting with it over generic netlink directly.
 *
 * \note this is a very low level library. It does not provide the
 * caller with means to scan, connect, disconnect, etc from a WiMAX
 * network. Said capability is provided by higher level services which
 * might be users of this library.
 *
 * @section conventions Conventions
 *
 * Most function calls return an integer with a negative \a errno
 * error code when there is an error.
 *
 * @section general_usage General usage
 *
 * The first operation to start controlling a WiMAX device is to open
 * a handle for it:
 *
 * @code
 *  struct wimaxll_handle *wmx = wimaxll_open("wmx0");
 * @endcode
 *
 * With an open handle you can execute all the WiMax API
 * operations. When done with the handle, it has to be closed:
 *
 * @code
 *  wimaxll_close(wmx);
 * @endcode
 *
 * If the device is unloaded/disconnected, the handle will be marked
 * as invalid and any operation will fail with -ENODEV.
 *
 * To reset a WiMAX device, use:
 *
 * @code
 *  wimaxll_reset(wmx);
 * @endcode
 *
 * To turn a device \e on or \e off, or to query it's status, use:
 *
 * @code
 *  wimaxll_rfkill(wmx, WIMAX_RF_ON);
 * @endcode
 *
 * WIMAX_RF_ON and WIMAX_RF_OFF turn the radio on and off
 * respectively, using the software switch and return the current
 * status of both switches. WIMAX_RF_QUERY just returns the status of
 * both the \e HW and \e SW switches.
 *
 * See \ref device_management "device management" for more information.
 *
 * \section receiving Receiving notifications from the WiMAX kernel stack
 *
 * The WiMAX kernel stack will broadcast notifications and
 * driver-specific messages to all the user space clients connected to
 * it over a generic netlink multicast group.
 *
 * To listen to said notifications, a library client needs to block
 * waiting for them or set \ref callbacks "callbacks" and integrate
 * into some kind of main loop using \e select() call to detect
 * incoming notifications.
 *
 * Simple example of mainloop integration:
 *
 * @code
 * int fd = wimaxll_recv_fd(wmx);
 * fd_set pipe_fds, read_fds;
 * ...
 * // Main loop
 * FD_ZERO(&pipe_fds);
 * FD_SET(fd, &pipe_fds);
 * while(1) {
 *         read_fds = pipe_fds;
 *         select(FD_SETSIZE, &read_fds, NULL, NULL, NULL);
 *         if (FD_ISSET(fd, &read_fds))
 *                 wimaxll_recv(wmx);
 * }
 * @endcode
 *
 * This code will call wimaxll_recv() when notifications are available
 * for delivery. Calling said function will execute, for each
 * notification, the callback associated to it.
 *
 * To wait for a \e state \e change notification, for example:
 *
 * @code
 * result = wimaxll_wait_for_state_change(wmx, &old_state, &new_state);
 * @endcode
 *
 * The same thing can be accomplished setting a callback for a state
 * change notification with wimaxll_set_cb_state_change() and then
 * waiting on a main loop. See \ref state_change_group "state changes"
 * for more information.
 *
 * To wait for a (device-specific) message from the driver, an
 * application would use:
 *
 * @code
 * void *msg;
 * ...
 *
 * size = wimaxll_msg_read(wmx, &msg);
 *
 * ... <act on the message>
 *
 * wimaxll_msg_free(msg);            // done with the message
 * @endcode
 *
 * \e msg points to a buffer which contains the message payload as
 * sent by the driver. When done with \a msg it has to be freed with
 * wimaxll_msg_free().
 *
 * As with \e state \e change notifications, a callback can be set
 * that will be executed from a mainloop every time a message is
 * received from a message pipe. See
 * wimaxll_pipe_set_cb_msg_to_user().
 *
 * A message can be sent to the driver with wimaxll_msg_write().
 * not the default \e message pipe.
 *
 * For more details, see \ref the_messaging_interface.
 *
 * @section miscellaneous Miscellaneous
 *
 * @subsection diagnostics Controlling the ouput of diagnostics
 *
 * \e libwimaxll will output messages by default to \a stderr. See
 * \ref helper_log for changing the default destination.
 *
 * @subsection bytesex Endianess conversion
 *
 * The following convenience helpers are provided for endianess
 * conversion:
 *
 * - wimaxll_le32_to_cpu() and wimaxll_cpu_to_le32()
 * - wimaxll_le16_to_cpu() and wimaxll_cpu_to_le16()
 * - wimaxll_swap_16() and wimaxll_swap_32()
 *
 * @section multithreading Multithreading
 *
 * This library is not threaded or locked. Internally it uses two
 * different netlink handles, one for receiving and one for sending;
 * thus the maximum level of paralellism you can do with one handle
 * is:
 *
 * - Functions that can't be executed in parallel when using the same
 *   wimaxll handle (need to be serialized):
 *   <ul>
 *     <li> wimaxll_msg_write(), wimaxll_rfkill(), wimax_reset()
 *     <li> wimaxll_recv(), wimaxll_msg_read(),
 *          wimaxll_wait_for_state_change()
 *     <li> wimax_get_cb_*() and wimax_set_cb_*().
 *     <li> wimaxll_recv_fd(), as long as the handle is valid.
 *   </ul>
 *
 * - Function calls that have to be always serialized with respect to
 *   any other:
 *   <ul>
 *     <li> wimaxll_open() and wimaxll_close()
 *     <li> wimax_get_cb_*() and wimax_set_cb_*().
 *   </ul>
 *
 * - callbacks are all executed serially; don't call wimax_recv() from
 *   inside a callback.
 *
 * Any function not covered by the in this list can be parallelizable.
 */


#ifndef __lib_wimaxll_h__
#define __lib_wimaxll_h__
#include <sys/errno.h>
#include <sys/types.h>
#include <endian.h>
#include <byteswap.h>
#include <stdarg.h>
#include <linux/wimax.h>

struct wimaxll_handle;

struct nlattr;


/**
 * \defgroup callbacks Callbacks
 *
 * When notification callbacks are being executed, the processing of
 * notifications from the kernel is effectively blocked by it. Care
 * must be taken not to call blocking functions, especially
 * wimaxll_recv().
 *
 * Callbacks are always passed a pointer to a private context as set
 * by the application.
 *
 * Callbacks can return -%EBUSY to have wimaxll_recv() stop processing
 * messages and pass control to the caller (which will see it
 * returning -%EBUSY). Callbacks *SHOULD NOT* return -%EINPROGRESS, as
 * it is used internally by wimaxll_recv().
 */


/**
 * Callback for a \e message \e to \e user generic netlink message
 *
 * A \e driver \e specific message has been received from the kernel;
 * the pointer to the data and the size are passed in \a data and \a
 * size. The callback can access that data, but it's lifetime is valid
 * only while the callback is executing. If it will be accessed later,
 * it has to be copied to a safe location.
 *
 * \note See \ref callbacks callbacks for a set of warnings and
 * guidelines for using callbacks.
 *
 * \param wmx WiMAX device handle
 * \param priv Context passed by the user with
 *     wimaxll_pipe_set_cb_msg_to_user().
 * \param pipe_name Name of the pipe the message is sent for
 * \param data Pointer to a buffer with the message data.
 * \param size Size of the buffer
 * \return >= 0 if it is ok to keep processing messages, -EBUSY if
 *     message processing should stop and control be returned to the
 *     caller. Any other negative error code to continue processing
 *     messages skipping the current one.
 *
 * \ingroup the_messaging_interface
 */
typedef int (*wimaxll_msg_to_user_cb_f)(struct wimaxll_handle *wmx,
					void *priv,
					const char *pipe_name,
					const void *data, size_t size);

/**
 * Callback for a \e state \e change notification from the WiMAX
 * kernel stack.
 *
 * The WiMAX device has changed state from \a old_state to \a
 * new_state.
 *
 * \note See \ref callbacks callbacks for a set of warnings and
 * guidelines for using callbacks.
 *
 * \param wmx WiMAX device handle
 * \param priv ctx Context passed by the user with
 *     wimaxll_set_cb_state_change().
 * \param old_state State the WiMAX device left
 * \param new_state State the WiMAX device entered
 * \return >= 0 if it is ok to keep processing messages, -EBUSY if
 *     message processing should stop and control be returned to the
 *     caller. Any other negative error code to continue processing
 *     messages skipping the current one.
 *
 * \ingroup state_change_group
 */
typedef int (*wimaxll_state_change_cb_f)(
	struct wimaxll_handle *, void *priv,
	enum wimax_st old_state, enum wimax_st new_state);


/* Basic handle management */
struct wimaxll_handle *wimaxll_open(const char *device_name);
void *wimaxll_priv_get(struct wimaxll_handle *);
void wimaxll_priv_set(struct wimaxll_handle *, void *);
void wimaxll_close(struct wimaxll_handle *);
const char *wimaxll_ifname(const struct wimaxll_handle *);
unsigned wimaxll_ifidx(const struct wimaxll_handle *);

/* Wait for data from the kernel, execute callbacks */
int wimaxll_recv_fd(struct wimaxll_handle *);
ssize_t wimaxll_recv(struct wimaxll_handle *);

/* Default (bidirectional) message pipe from the kernel */
ssize_t wimaxll_msg_write(struct wimaxll_handle *, const char *,
			  const void *, size_t);

void wimaxll_get_cb_msg_to_user(struct wimaxll_handle *,
				wimaxll_msg_to_user_cb_f *, void **);
void wimaxll_set_cb_msg_to_user(struct wimaxll_handle *,
				wimaxll_msg_to_user_cb_f, void *);

#define WIMAX_PIPE_ANY (NULL-1)
ssize_t wimaxll_msg_read(struct wimaxll_handle *, const char *pine_name,
			 void **);
void wimaxll_msg_free(void *);

/* generic API */
int wimaxll_rfkill(struct wimaxll_handle *, enum wimax_rf_state);
int wimaxll_reset(struct wimaxll_handle *);
int wimaxll_state_get(struct wimaxll_handle *);

void wimaxll_get_cb_state_change(
	struct wimaxll_handle *, wimaxll_state_change_cb_f *,
	void **);
void wimaxll_set_cb_state_change(
	struct wimaxll_handle *, wimaxll_state_change_cb_f,
	void *);
ssize_t wimaxll_wait_for_state_change(struct wimaxll_handle *wmx,
				      enum wimax_st *old_state,
				      enum wimax_st *new_state);

/*
 * Basic diagnostics
 *
 * Deprecated, see wimaxll/log.h 
 */
extern void (*wimaxll_vmsg)(const char *, va_list)
	__attribute__((deprecated));
void wimaxll_vmsg_stderr(const char *, va_list)
	__attribute__((deprecated));


/**
 * \defgroup miscellaneous_group Miscellaneous utilities
 */
enum wimax_st wimaxll_state_by_name(const char *);
size_t wimaxll_states_snprintf(char *, size_t);
const char * wimaxll_state_to_name(enum wimax_st);

#define wimaxll_array_size(a) (sizeof(a)/sizeof(a[0]))

#define wimaxll_container_of(pointer, type, member)			\
({									\
	type *object = NULL;						\
	size_t offset = (void *) &object->member - (void *) object;	\
	(type *) ((void *) pointer - offset);				\
})

static inline	// ugly hack for doxygen
/**
 *
 * Swap the nibbles in a 16 bit number.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned short wimaxll_swap_16(unsigned short x)
 */
unsigned short wimaxll_swap_16(unsigned short x)
{
	return bswap_16(x);
}


static inline	// ugly hack for doxygen
/**
 * Swap the nibbles in a 32 bit number.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned long wimaxll_swap_32(unsigned long x)
 */
unsigned long wimaxll_swap_32(unsigned long x)
{
	return bswap_32(x);
}


static inline	// ugly hack for doxygen
/**
 * Convert a cpu-order 16 bits to little endian.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned short wimaxll_cpu_to_le16(unsigned short x)
 */
unsigned short wimaxll_cpu_to_le16(unsigned short x)
{
	unsigned short le16;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	le16 = x;
#elif __BYTE_ORDER == __BIG_ENDIAN
	le16 = wimaxll_swap_16(x);
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return le16;
}


static inline	// ugly hack for doxygen
/**
 * Convert a little-endian 16 bits to cpu order.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned short wimaxll_le16_to_cpu(unsigned short le16)
 */
unsigned short wimaxll_le16_to_cpu(unsigned short le16)
{
	unsigned short cpu;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	cpu = le16;
#elif __BYTE_ORDER == __BIG_ENDIAN
	cpu = wimaxll_swap_16(le16);
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return cpu;
}


static inline	// ugly hack for doxygen
/**
 * Convert a cpu-order 32 bits to little endian.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned long wimaxll_cpu_to_le32(unsigned long x)
 */
unsigned long wimaxll_cpu_to_le32(unsigned long x)
{
	unsigned long le32;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	le32 = x;
#elif __BYTE_ORDER == __BIG_ENDIAN
	le32 = wimaxll_swap_32(x);
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return le32;
}


static inline	// ugly hack for doxygen
/**
 * Convert a little-endian 32 bits to cpu order.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned long wimaxll_le32_to_cpu(unsigned long le32)
 */
unsigned long wimaxll_le32_to_cpu(unsigned long le32)
{
	unsigned long cpu;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	cpu = le32;
#elif __BYTE_ORDER == __BIG_ENDIAN
	cpu = wimaxll_swap_32(le32);
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return cpu;
}


static inline	// ugly hack for doxygen
/**
 * Convert a cpu-order 16 bits to big endian.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned short wimaxll_cpu_to_be16(unsigned short x)
 */
unsigned short wimaxll_cpu_to_be16(unsigned short x)
{
	unsigned short be16;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	be16 = wimaxll_swap_16(x);
#elif __BYTE_ORDER == __BIG_ENDIAN
	be16 = x;
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return be16;
}


static inline	// ugly hack for doxygen
/**
 * Convert a big-endian 16 bits to cpu order.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned short wimaxll_be16_to_cpu(unsigned short be16)
 */
unsigned short wimaxll_be16_to_cpu(unsigned short be16)
{
	unsigned short cpu;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	cpu = wimaxll_swap_16(be16);
#elif __BYTE_ORDER == __BIG_ENDIAN
	cpu = be16;
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return cpu;
}


static inline	// ugly hack for doxygen
/**
 * Convert a cpu-order 32 bits to big endian.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned long wimaxll_cpu_to_be32(unsigned long x)
 */
unsigned long wimaxll_cpu_to_be32(unsigned long x)
{
	unsigned long be32;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	be32 = wimaxll_swap_32(x);
#elif __BYTE_ORDER == __BIG_ENDIAN
	be32 = x;
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return be32;
}


static inline	// ugly hack for doxygen
/**
 * Convert a big-endian 32 bits to cpu order.
 *
 * \ingroup miscellaneous_group
 * \fn unsigned long wimaxll_be32_to_cpu(unsigned long be32)
 */
unsigned long wimaxll_be32_to_cpu(unsigned long be32)
{
	unsigned long cpu;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	cpu = be32;
#elif __BYTE_ORDER == __BIG_ENDIAN
	cpu = wimaxll_swap_32(be32);
#else
#error ERROR: unknown byte sex - FIXME
#endif
	return cpu;
}


#define __WIMAXLL_ALIGN2_MASK(n, m) (((n) + (m)) & ~(m))
/**
 * Return the value \e n aligned to an order-of-two value \a o2.
 */
#define WIMAXLL_ALIGN2(n, o2) __WIMAXLL_ALIGN2_MASK(n, (typeof(n)) (o2) - 1)

#endif /* #ifndef __lib_wimaxll_h__ */
