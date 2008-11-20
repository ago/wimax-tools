#ifndef __lib_wimax_h__
#define __lib_wimax_h__
#warning DEPRECATED: this file is deprecated, use wimaxll.h
#include <wimaxll.h>

#define wimax_handle                                wimaxll_handle
#define wimax_gnl_cb_context                        wimaxll_gnl_cb_context
#define wimax_msg_to_user_cb_f                      wimaxll_msg_to_user_cb_f
#define wimax_state_change_cb_f                     wimaxll_state_change_cb_f
#define WIMAX_GNL_CB_CONTEXT_INIT(a)                WIMAXLL_GNL_CB_CONTEXT_INIT(a)
#define wimax_gnl_cb_context_init(a, b)             wimaxll_gnl_cb_context_init(a, b)
#define wimax_cb_context_set_result(a, b)           wimaxll_cb_context_set_result(a, b)

#define wimax_open(a)                               wimaxll_open(a)
#define wimax_close(a)                              wimaxll_close(a)
#define wimax_ifname(a)                             wimaxll_ifname(a)

#define wimax_mc_rx_open(a, b)                      wimaxll_mc_rx_open(a, b)
#define wimax_mc_rx_fd(a, b)                        wimaxll_mc_rx_fd(a, b)
#define wimax_mc_rx_close(a, b)                     wimaxll_mc_rx_close(a, b)
#define wimax_mc_rx_read(a, b)                      wimaxll_mc_rx_read(a, b)

#define wimax_pipe_open(a, b)                       wimaxll_pipe_open(a, b)
#define wimax_pipe_fd(a, b)                         wimaxll_pipe_fd(a, b)
#define wimax_pipe_read(a, b)                       wimaxll_pipe_read(a, b)
#define wimax_pipe_close(a, b)                      wimaxll_pipe_close(a, b)

#define wimax_pipe_msg_read(a, b, c)                wimaxll_pipe_msg_read(a, b, c)
#define wimax_pipe_msg_free(a)                      wimaxll_pipe_msg_free(a)
#define wimax_pipe_get_cb_msg_to_user(a, b, c, d)   wimaxll_pipe_get_cb_msg_to_user(a, b, c, d)
#define wimax_pipe_set_cb_msg_to_user(a, b, c, d)   wimaxll_pipe_set_cb_msg_to_user(a, b, c, d)
#define wimax_msg_fd(a)                             wimaxll_msg_fd(a)
#define wimax_msg_read(a, b)                        wimaxll_msg_read(a, b)
#define wimax_msg_write(a, b, c)                    wimaxll_msg_write(a, b, c)
#define wimax_msg_free(a)                           wimaxll_msg_free(a)
#define wimax_msg_pipe_id(a)                        wimaxll_msg_pipe_id(a)
#define wimax_rfkill(a, b)                          wimaxll_rfkill(a, b)
#define wimax_reset(a)                              wimaxll_reset(a)
#define wimax_get_cb_state_change(a, b, c)          wimaxll_get_cb_state_change(a, b, c)
#define wimax_set_cb_state_change(a, b, c)          wimaxll_set_cb_state_change(a, b, c)
#define wimax_wait_for_state_change(a, b, c)        wimaxll_wait_for_state_change(a, b, c)

#define wimax_swap_16(a)                            wimaxll_swap_16(a)
#define wimax_swap_32(a)                            wimaxll_swap_32(a)
#define wimax_cpu_to_le16(a)                        wimaxll_cpu_to_le16(a)
#define wimax_le16_to_cpu(a)                        wimaxll_le16_to_cpu(a)
#define wimax_cpu_to_le32(a)                        wimaxll_cpu_to_le32(a)
#define wimax_le32_to_cpu(a)                        wimaxll_le32_to_cpu(a)

#endif /* #ifndef __lib_wimax_h__ */
