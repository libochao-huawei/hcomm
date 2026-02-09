/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TC_UT_RS_PING_H
#define TC_UT_RS_PING_H

void tc_rs_payload_header_resv_custom_check();
void tc_rs_ping_handle_init();
void tc_rs_ping_handle_deinit();
void tc_rs_ping_init();
void tc_rs_ping_target_add();
void tc_rs_get_ping_cb();
void tc_rs_ping_client_post_send();
void tc_rs_ping_get_results();
void tc_rs_ping_task_stop();
void tc_rs_ping_target_del();
void tc_rs_ping_deinit();
void tc_rs_ping_compare_rdma_info();
void tc_rs_ping_roce_find_target_node();
void tc_rs_pong_find_target_node();
void tc_rs_pong_find_alloc_target_node();
void tc_rs_ping_poll_send_cq();
void tc_rs_ping_server_post_send();
void tc_rs_ping_post_recv();
void tc_rs_ping_client_poll_cq();
void tc_rs_epoll_event_ping_handle();
void tc_rs_ping_get_trip_time();
void tc_rs_ping_cb_init_mutex();
void tc_rs_ping_resolve_response_packet();
void tc_rs_ping_server_poll_cq();
void tc_rs_ping_cb_get_dev_rdev_index();
void tc_rs_ping_init_mr_cb();
void tc_rs_ping_common_deinit_local_buffer();
void tc_rs_ping_common_deinit_local_qp();
void tc_rs_ping_roce_poll_scq();
void tc_rs_ping_pong_init_local_info();
void tc_rs_ping_handle();
void tc_rs_ping_roce_ping_cb_deinit();

#endif
