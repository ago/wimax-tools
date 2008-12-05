/*
 * Linux WiMax
 * Internal API and declarations
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
 */
#ifndef __lib_internal_h__
#define __lib_internal_h__

enum {
#define __WIMAXLL_IFNAME_LEN 32
	/**
	 * WIMAXLL_IFNAME_LEN - Maximum size of a wimax interface
	 *     name.
	 */
	WIMAXLL_IFNAME_LEN = __WIMAXLL_IFNAME_LEN,
	/**
	 * WMAX_MC_MAX - Maximum number of multicast groups that a
	 *     WiMAX interface can offer (this doesn't count the
	 *     reports group, which is separate).
	 */
	WIMAXLL_MC_MAX = 5,
};


struct wimaxll_mc_handle;


/**
 * A description of a generic netlink multicast group
 *
 * \param name Name of the group
 * \param id ID of the group
 */
struct wimaxll_mc_group {
	char name[GENL_NAMSIZ];
	int id;
	struct wimaxll_mc_handle *mch;
};


/**
 * A WiMax control pipe handle
 *
 * This type is opaque to the user
 *
 * \internal
 *
 * In order to simplify multithread support, we use to different \a
 * libnl handles, one for sending to the kernel, one (for each pipe
 * open to a multicast group) for reading from the kernel. This allows
 * us to parallelize \c wimaxll_msg_write() and \c wimaxll_msg_read() at
 * the same time in a multithreaded environment.
 *
 * FIXME: this needs some rewriting
 *
 * \param ifidx Interface Index 
 * \param nlh_tx handle for writing to the kernel.
 *     Internal note: You \b have \b to set the handlers for
 *     %NL_CB_VALID and nl_cb_err() callbacks, as each callsite will
 *     do it to suit their needs. See wimaxll_rfkill() for an
 *     example. Any other callback you are supposed to restore to what
 *     it was before.
 * \param gnl_family_id Generic Netlink Family ID assigned to the device
 * \param mc_msg Index in the \a gnl_mc array of the "msg"
 *     multicast group.
 * \param name name of the wimax interface
 * \param gnl_mc Array of information about the different multicast
 *     groups supported by the device. At least the "msg" group is
 *     always supported. The rest are optional and depend on what the
 *     driver implements.
 */
struct wimaxll_handle {
	unsigned ifidx;
	struct nl_handle *nlh_tx;
	int gnl_family_id;
	unsigned mc_msg;
	char name[__WIMAXLL_IFNAME_LEN];
	struct wimaxll_mc_group gnl_mc[WIMAXLL_MC_MAX];
};


/**
 * Multicast group handle
 *
 * \internal
 *
 * This structure encapsulates all that we need to read from a single
 * multicast group. We could have a single handle for doing all, but
 * by definition of the interface, different multicast groups carry
 * different traffic (with different needs). Rather than multiplex it
 * here, we multiplex at the kernel by sending it via an specific pipe
 * that knows how to handle it already.
 *
 * This way the driver can define it's own private pipes (if needed)
 * for high bandwidth traffic (for example, tracing information)
 * without affecting the rest of the groups (channels).
 *
 * msg_done is used by the ack and error generic netlink callbacks to
 * indicate to the message receving loop that all the parts of the
 * message have been received.
 */
struct wimaxll_mc_handle {
	int idx;
	struct wimaxll_handle *wmx;
	struct nl_handle *nlh_rx;
	struct nl_cb *nl_cb;
	ssize_t result;
	unsigned msg_done:1;		/* internal */
	
	wimaxll_msg_to_user_cb_f msg_to_user_cb;
	struct wimaxll_gnl_cb_context *msg_to_user_context;

	wimaxll_state_change_cb_f state_change_cb;
	struct wimaxll_gnl_cb_context *state_change_context;
};


static inline
void wimaxll_mch_maybe_set_result(struct wimaxll_mc_handle *mch, int val)
{
	if (mch->result == -EINPROGRESS)
		mch->result = val;
}


/* Utilities */
int wimaxll_wait_for_ack(struct wimaxll_handle *);
int wimaxll_gnl_handle_msg_to_user(struct wimaxll_handle *,
				 struct wimaxll_mc_handle *,
				 struct nl_msg *);
int wimaxll_gnl_handle_state_change(struct wimaxll_handle *,
				  struct wimaxll_mc_handle *,
				  struct nl_msg *);
int wimaxll_gnl_error_cb(struct sockaddr_nl *, struct nlmsgerr *, void *);
int wimaxll_gnl_ack_cb(struct nl_msg *msg, void *_mch);
struct wimaxll_mc_handle *__wimaxll_get_mc_handle(struct wimaxll_handle *,
					      int pipe_id);


#define wimaxll_container_of(pointer, type, member)			\
({									\
	type *object = NULL;						\
	size_t offset = (void *) &object->member - (void *) object;	\
	(type *) ((void *) pointer - offset);				\
})


/*
 * wimaxll_family_id - Return the associated Generic Netlink family ID
 *
 * @wmx: WiMax interface for which to provide the ID.
 */
static inline
int wimaxll_family_id(struct wimaxll_handle *wmx)
{
	return wmx->gnl_family_id;
}


void wimaxll_msg(struct wimaxll_handle *, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

#endif /* #ifndef __lib_internal_h__ */
