/*
 * Linux WiMax
 * i2400m specific helpers
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
 * @defgroup i2400m_group Helpers to control an Intel 2400m based device
 *
 * This set of helpers simplify the task of sending commands / waiting
 * for the acks and receiving reports/indications from the i2400m.
 *
 * It boils down to a framework to support that only one thread can
 * send a command at the same time; this is because the commands don't
 * have a cookie to identify the issuer -- so a place is needed where
 * to store the "I am waiting for a response for command X".
 *
 * When the callback from libwimaxll comes back with the response, if
 * it was a reply to said message, then the waiter for that is woken
 * up (using pthread mutexes and conditional variables).
 *
 * When a report is received, the report callback is called; care has
 * to be taken not to deadlock. See i2400m_report_cb().
 *
 * For usage, create a handle:
 *
 * @code
 * {
 * 	int r;
 * 	struct i2400m *i2400m;
 * 	...
 * 	r = i2400m_create(&i2400m, "wmx0", my_priv_pointer, my_report_cb);
 * 	if (r < 0)
 * 		error;
 * 	...
 * 	// create a message
 * 	...
 * 	r = i2400m_msg_to_dev(i2400m, &message, message_size,
 * 			      message_cb, message_cb_priv);
 * 	if (r < 0)
 * 		error;
 * 	// message_cb has been called
 * 	i2400m_destroy(i2400m);
 * }
 * @endcode
 *
 * Remember there are limited things that can be done in the callback;
 * calling i2400m_msg_to_dev() will deadlock, as well as waiting for a
 * report.
 *
 * A report callback with some TLV processing example would be:
 *
 * @code
 * static
 * void my_report_cb(struct i2400m *i2400m,
 * 		  const struct i2400m_l3l4_hdr *l3l4, size_t l3l4_size)
 * {
 * 	struct my_priv *my_priv = i2400m_priv(i2400m);
 * 	struct wimaxll_handle *wmx = i2400m_wmx(i2400m);
 *
 * 	// do something with the report...;
 *
 * 	struct i2400m_tlv *tlv = NULL;
 *
 * 	while ((tlv = i2400m_tlv_buffer_walk(l3l4->pl, l3l4_size, tlv))) {
 * 		tlv_type = wimaxll_le16_to_cpu(tlv->type);
 * 		tlv_length = wimaxll_le16_to_cpu(tlv->length);
 * 		// do whatever with the tlv
 * 	}
 *
 * 	// or find a tlv in a buffer
 * 	tlv = i2400m_tlv_find(l3l4->pl, l3l4_size - sizeof(*l3l4),
 * 			      I2400M_TLV_SOMETHING, -1);
 *
 * }
 * @endcode
 */
#include <wimaxll/i2400m.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <wimaxll.h>
#include <internal.h>


/**
 * Descriptor for a Intel 2400m
 *
 * @param wmx libwimaxll handle
 * @param priv Private storage as set by the owner
 *
 * @param mutex Mutex for command execution (protects mt_*)
 * @param cond Conditional variable for command execution (protected
 *     by \e mutex). Threads wait on this conditional variable waiting
 *     for commands to finish executing (at which point the mt_cb is
 *     called back).
 *
 * @param mt_pending Message type of the reply that a thread is
 *     waiting for.
 * @param mt_cb Callback to execute when the \e mt_pending reply
 *     arrives.
 * @param mt_cb_priv Private data to pass to the \e mt_cb
 * @param mt_result Updated with the result of executing the \e mt_cb
 *     callback. If cancelled, it will be -%EINTR.
 *
 * @param report_cb Callback to execute when a report/indication is
 *     received.
 * @param report_cb_priv Private data passed to the report callback.
 *
 * @internal
 * @ingroup i2400m_group
 */
struct i2400m {
	struct wimaxll_handle *wmx;
	void *priv;

	pthread_mutex_t mutex;
	pthread_cond_t cond;

	enum i2400m_mt mt_pending;
	i2400m_reply_cb mt_cb;
	void *mt_cb_priv;
	int mt_result;

	i2400m_report_cb report_cb;
	void *report_cb_priv;
};


/*
 * When a message comes with an ack or report, chew it
 *
 * Only takes messages on the default pipe, as that's where the device
 * passes them. Executes the callback for a command ack if it's
 * message type is the one that was waited for, otherwise they are
 * ignored.
 *
 * The driver takes care of coordinating so that only one command is
 * executed at the same time.
 *
 * If it is a report, just run the callback.
 */
static
int i2400m_msg_to_user_cb(struct wimaxll_handle *wmx, void *_i2400m,
			  const char *pipe_name,
			  const void *data, size_t size)
{
	struct i2400m *i2400m = _i2400m;
	const struct i2400m_l3l4_hdr *hdr = data;
	enum i2400m_mt mt;

	if (pipe_name != NULL)
		goto out;

	mt = wimaxll_le16_to_cpu(hdr->type);
	pthread_mutex_lock(&i2400m->mutex);
	if (mt == i2400m->mt_pending) {
		i2400m->mt_pending = I2400M_MT_INVALID;
		if (i2400m->mt_cb != NULL)
			i2400m->mt_result = i2400m->mt_cb(
				i2400m, i2400m->mt_cb_priv, data, size);
		else
			i2400m->mt_result = 0;
		pthread_cond_signal(&i2400m->cond);
	}
	pthread_mutex_unlock(&i2400m->mutex);
	/* this is ran outside of the lock because it doesn't need
	 * much tracking info. */
	if (mt & I2400M_MT_REPORT_MASK && i2400m->report_cb)
		i2400m->report_cb(i2400m, data, size);
out:
	return 0;
}


/**
 * Create a i2400m handle
 *
 * Creates a handle usable to execute commands and use the i2400m
 * helpers.
 *
 * @param _i2400m where to store the handler value (pointer to
 *     the descriptor).
 *
 * @param ifname name of the network interface where the i2400m is
 *
 * @param priv Pointer that the callbacks can recover from the
 *     handle with i2400m_priv()
 *
 * @param report_cb Callback function called when a report arrives
 *
 * @ingroup i2400m_group
 */
int i2400m_create(struct i2400m **_i2400m, const char *ifname,
		  void *priv, i2400m_report_cb report_cb)
{
	int result;
	struct i2400m *i2400m;

	i2400m = calloc(sizeof(*i2400m), 1);
	if (i2400m == NULL)
		goto error_calloc;
	result = -ENODEV;
	i2400m->wmx = wimaxll_open(ifname);
	if (i2400m->wmx == NULL) {
		result = -errno;
		goto error_open;
	}
	pthread_mutex_init(&i2400m->mutex, NULL);
	pthread_cond_init(&i2400m->cond, NULL);
	i2400m->priv = priv;
	i2400m->report_cb = report_cb;
	i2400m->mt_pending = I2400M_MT_INVALID;

	wimaxll_set_cb_msg_to_user(
		i2400m->wmx, i2400m_msg_to_user_cb, i2400m);

	*_i2400m = i2400m;
	return 0;

error_open:
	free(i2400m);
error_calloc:
	return result;
}


/**
 * Destroy a descriptor created with i2400m_create()
 *
 * @param i2400m Handle for an i2400m as returned by
 *     i2400m_create().
 *
 * @ingroup i2400m_group
 */
void i2400m_destroy(struct i2400m *i2400m)
{
	pthread_mutex_lock(&i2400m->mutex);
	i2400m->mt_result = -EINTR;
	pthread_cond_broadcast(&i2400m->cond);
	pthread_mutex_unlock(&i2400m->mutex);
	wimaxll_close(i2400m->wmx);
	free(i2400m);
}


/**
 * Return the private data associated to a \e i2400m
 *
 * @param i2400m i2400m handle
 * @returns pointer to priv data as set at i2400m_create() time
 *
 * @ingroup i2400m_group
 */
void * i2400m_priv(struct i2400m *i2400m)
{
	return i2400m->priv;
}


/**
 * Return the libwimaxll handle associated to a \e i2400m
 *
 * @param i2400m i2400m handle
 * @returns wimaxll handle
 *
 * @ingroup i2400m_group
 */
struct wimaxll_handle * i2400m_wmx(struct i2400m *i2400m)
{
	return i2400m->wmx;
}


/**
 * Execute an i2400m command and wait for a response
 *
 * @param i2400m i2400m handle
 *
 * @param l3l4 Pointer to buffer containing a L3L4 message to send to
 *     the device.
 *
 * @param l3l4_size size of the buffer pointed to by \e l3l4 (this
 *     includes the message header and the TLV payloads, if any)
 *
 * @param cb Callback function to execute when the reply is received.
 *
 * @param cb_priv Private pointer to pass to the callback function.
 *
 * If the message execution fails in the device, the return value from
 * wimaxll_msg_write() will tell it. It can also be taken (with more
 * detail) by setting a callback function and parsing the reply.
 *
 * This call can be executed from multiple threads on the same \e
 * i2400m handle at the same time, as it will be mutexed properly and
 * only one will execute at the same time (likewise, the driver will
 * make sure only one command from different threads is ran at the
 * same time).
 *
 * @note
 *
 * This call blocks waiting for the reply to the message; from the
 * callback context no calls to i2400m_msg_dev() or waits for reports
 * on the same handle as the callback can be done, as it would
 * deadlock.
 *
 * @ingroup i2400m_group
 */
int i2400m_msg_to_dev(struct i2400m *i2400m,
		      const struct i2400m_l3l4_hdr *l3l4, size_t l3l4_size,
		      i2400m_reply_cb cb, void *cb_priv)
{
	int result;
	enum i2400m_mt msg_type;

	msg_type = wimaxll_le16_to_cpu(l3l4->type);
	/* No need to check msg & payload consistency, the kernel will do for us */
	/* Setup the completion, ack_skb ("we are waiting") and send
	 * the message to the device */
	pthread_mutex_lock(&i2400m->mutex);
	i2400m->mt_pending = msg_type;
	i2400m->mt_cb = cb;
	i2400m->mt_cb_priv = cb_priv;
	result = wimaxll_msg_write(i2400m->wmx, NULL, l3l4, l3l4_size);
	if (result < 0)
		goto error_msg_write;
	/* The driver guarantees that either we get the response to
	 * the command or only a notification, so we just need to wait
	 * for the reply to come */
#warning FIXME: _timeout?
	pthread_cond_wait(&i2400m->cond, &i2400m->mutex);
	result = i2400m->mt_result;
error_msg_write:
	i2400m->mt_pending = I2400M_MT_INVALID;
	pthread_mutex_unlock(&i2400m->mutex);
	return result;
}


/**
 * Return if a TLV is of a give type and size
 *
 * @param tlv pointer to the TLV
 * @param tlv_type type of the TLV we are looking for
 * @param tlv_size expected size of the TLV we are looking for (if -1,
 *     don't check the size). Size includes the TLV header.
 * @returns 0 if the TLV matches, < 0 if it doesn't match at all, > 0
 *     total TLV + payload size, if the type matches, but not the size
 *
 * @ingroup i2400m_group
 */
ssize_t i2400m_tlv_match(const struct i2400m_tlv_hdr *tlv,
			 enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	if (wimaxll_le16_to_cpu(tlv->type) != tlv_type)	/* Not our type? skip */
		return -1;
	if (tlv_size != -1
	    && wimaxll_le16_to_cpu(tlv->length) + sizeof(*tlv) != tlv_size)
		return wimaxll_le16_to_cpu(tlv->length)  + sizeof(*tlv);
	return 0;
}


/**
 * Iterate over a buffer of TLVs
 *
 * Allows to safely iterate over a buffer of TLVs, making sure bounds
 * are properly checked. Usage:
 *
 * @code
 * tlv_itr = NULL;
 * while (tlv_itr = i2400m_tlv_buffer_walk(i2400m, buf, size, tlv_itr))  {
 *         ...
 *         // Do stuff with tlv_itr, DON'T MODIFY IT
 *         ...
 * }
 * @endcode
 *
 * @param tlv_buf pointer to the beginning of the TLV buffer
 *
 * @param buf_size buffer size in bytes
 *
 * @param tlv_pos seek position; this is assumed to be a pointer returned
 *     by i2400m_tlv_buffer_walk() [and thus, validated]. The TLV
 *     returned will be the one following this one.
 *
 * @returns pointer to the next TLV from the seek position or NULL if
 *     the end of the buffer was reached.
 *
 * @ingroup i2400m_group
 */
const struct i2400m_tlv_hdr *i2400m_tlv_buffer_walk(
	const void *tlv_buf, size_t buf_size,
	const struct i2400m_tlv_hdr *tlv_pos)
{
	const struct i2400m_tlv_hdr *tlv_top = tlv_buf + buf_size;
	size_t offset, length, avail_size;
	unsigned type;

	if (tlv_pos == NULL)	/* Take the first one? */
		tlv_pos = tlv_buf;
	else			/* Nope, the next one */
		tlv_pos = (void *) tlv_pos
			+ wimaxll_le16_to_cpu(tlv_pos->length) + sizeof(*tlv_pos);
	if (tlv_pos == tlv_top) {
		tlv_pos = NULL;		/* buffer done */
		goto error_beyond_end;
	}
	if (tlv_pos > tlv_top) {
		tlv_pos = NULL;
		goto error_beyond_end;
	}
	offset = (void *) tlv_pos - (void *) tlv_buf;
	avail_size = buf_size - offset;
	if (avail_size < sizeof(*tlv_pos)) {
		wimaxll_msg(NULL,
			    "HW BUG? tlv_buf %p [%zu bytes], tlv @%zu: "
			    "short header\n", tlv_buf, buf_size, offset);
		goto error_short_header;
	}
	type = wimaxll_le16_to_cpu(tlv_pos->type);
	length = wimaxll_le16_to_cpu(tlv_pos->length);
	if (avail_size < sizeof(*tlv_pos) + length) {
		wimaxll_msg(NULL,
			    "HW BUG? tlv_buf %p [%zu bytes], "
			    "tlv type 0x%04x @%zu: "
			    "short data (%zu bytes vs %zu needed)\n",
			    tlv_buf, buf_size, type, offset, avail_size,
			    sizeof(*tlv_pos) + length);
		goto error_short_header;
	}
error_short_header:
error_beyond_end:
	return tlv_pos;
}


/**
 * Find a TLV by type (and maybe length) in a buffer of TLVs
 *
 * @param tlv_hdr pointer to the first TLV in the sequence
 *
 * @param size size of the buffer in bytes; all TLVs are assumed to fit
 *     fully in the buffer (otherwise we'll complain).
 *
 * @param tlv_type type of the TLV we are looking for
 *
 * @param tlv_size expected size of the TLV we are looking for (if -1,
 *     don't check the size). This includes the header
 *
 * @returns NULL if the TLV is not found, otherwise a pointer to
 *     it. If the sizes don't match, an error is printed and NULL
 *     returned.
 *
 * @ingroup i2400m_group
 */
const struct i2400m_tlv_hdr *i2400m_tlv_find(
	const struct i2400m_tlv_hdr *tlv_hdr, size_t size,
	enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	ssize_t match;
	const struct i2400m_tlv_hdr *tlv = NULL;
	while ((tlv = i2400m_tlv_buffer_walk(tlv_hdr, size, tlv))) {
		match = i2400m_tlv_match(tlv, tlv_type, tlv_size);
		if (match == 0)		/* found it :) */
			break;
		if (match > 0)
			wimaxll_msg(NULL,
				    "TLV type 0x%04x found with size "
				    "mismatch (%zu vs %zu needed)\n",
				    tlv_type, match, tlv_size);
	}
	return tlv;
}
