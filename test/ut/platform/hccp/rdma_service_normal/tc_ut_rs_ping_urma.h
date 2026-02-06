/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _TC_UT_RS_UB_URMA_H
#define _TC_UT_RS_UB_URMA_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void tc_rs_epoll_event_ping_handle_urma();
void tc_rs_ping_init_deinit_urma();
void tc_rs_ping_target_add_del_urma();
void tc_rs_ping_urma_post_send();
void tc_rs_ping_urma_poll_scq();
void tc_rs_ping_client_poll_cq_urma();
void tc_rs_ping_server_poll_cq_urma();
void tc_rs_ping_get_results_urma();
void tc_rs_ping_server_post_send_urma();
void tc_rs_pong_jetty_find_alloc_target_node();
void tc_rs_ping_common_poll_send_jfc();
void tc_rs_pong_jetty_find_target_node();
void tc_rs_pong_jetty_resolve_response_packet();
void tc_rs_ping_common_import_jetty();
void tc_rs_ping_urma_reset_recv_buffer();
void tc_rs_ping_common_jfr_post_recv();

#ifdef __cplusplus
}
#endif
#endif
