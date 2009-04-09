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
#ifndef __wimaxll__i2400m_h__
#define __wimaxll__i2400m_h__

#include <sys/types.h>
#include <linux/wimax/i2400m.h>

struct i2400m;
struct wimaxll_handle;

/*
 * Callback called by i2400m_msg_to_dev() when a reply to the executed
 * command arrives.
 *
 * In struct i2400m, the fields mt_cb_priv, mt_orig, and mt_orig_size are
 * set for reference.
 *
 * The callback is passed the reply and to only return some error
 * value that i2400m_msg_to_dev() will return to the caller.
 *
 * You CANNOT execute other commands with i2400m_msg_to_dev() inside
 * this function neither wait for reports to arrive. You'd deadlock.
 */
typedef int (*i2400m_reply_cb)(
	struct i2400m *, void *priv,
	const struct i2400m_l3l4_hdr *reply, size_t reply_size);

/**
 * Callback for handling i2400m reports.
 *
 * This function is called when the i2400m sends a report/indication.
 *
 * You cannot execute commands or wait for other reports from this
 * callback or it woul deadlock. You need to spawn off a thread or do
 * some other arrangement for it.
 *
 * @param i2400m i2400m device descriptor; use i2400m_priv() to obtain
 *     the private pointer for it
 * @param l3l4 Pointer to the report data in L3L4 message format; note
 *     this buffer is only valid in this execution context. Once the
 *     callback returns, it will be destroyed.
 * @param l3l4_size Size of the buffer pointed to by l3l4.
 */
typedef void (*i2400m_report_cb)(
	struct i2400m *i2400m,
	const struct i2400m_l3l4_hdr *l3l4, size_t l3l4_size);

int i2400m_create(struct i2400m **, const char *, void *, i2400m_report_cb);
int i2400m_create_from_handle(struct i2400m **, struct wimaxll_handle *,
			      void *, i2400m_report_cb);
void i2400m_destroy(struct i2400m *);
int i2400m_msg_to_dev(struct i2400m *, const struct i2400m_l3l4_hdr *, size_t,
		      i2400m_reply_cb, void *);
void *i2400m_priv(struct i2400m *);
struct wimaxll_handle *i2400m_wmx(struct i2400m *);

ssize_t i2400m_tlv_match(
	const struct i2400m_tlv_hdr *, enum i2400m_tlv, ssize_t);

const struct i2400m_tlv_hdr *i2400m_tlv_buffer_walk(
	const void *, size_t, const struct i2400m_tlv_hdr *);

const struct i2400m_tlv_hdr *i2400m_tlv_find(
	const struct i2400m_tlv_hdr *, size_t, enum i2400m_tlv, ssize_t);

#endif /* #define __wimaxll__i2400m_h__ */
