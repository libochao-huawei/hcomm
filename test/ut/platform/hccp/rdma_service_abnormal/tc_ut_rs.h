/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _TC_UT_RS_H
#define _TC_UT_RS_H

#include <stdio.h>

#define RS_TEST_MEM_SIZE  32
#define RS_TEST_MEM_PAGE_SIZE  4096

void tc_rs_init2();
void tc_rs_deinit2();

void tc_rs_socket_init();
void tc_rs_socket_deinit();

void tc_rs_rdev_init();
void tc_rs_rdev_deinit();

void tc_rs_get_tsqp_depth_abnormal();
void tc_rs_set_tsqp_depth_abnormal();

void tc_rs_socket_listen_start2();
void tc_rs_socket_batch_connect2();
void tc_rs_qp_create2();
void tc_rs_mr_ops2();

void tc_rs_abnormal2();
void tc_rs_epoll_ops2();
void tc_rs_socket_ops2();
void tc_rs_socket_close2();
void tc_rs_mr_abnormal2();
void tc_rs_get_gid_index2();
void tc_rs_qp_connect_async2();
void tc_rs_send_wr2();
void tc_tls_abnormal1();
void tc_rs_socket_nodeid2vnic();
void tc_rs_server_valid_async_init();
void tc_rs_connect_handle();
void tc_rs_get_qp_context();
void tc_rs_socket_get_bind_by_chip();
void tc_rs_socket_batch_abort();
void tc_rs_socket_send_and_recv_log_test();
void tc_rs_tcp_recv_tag_in_handle();
void tc_rs_server_valid_async_abnormal();
void tc_rs_server_valid_async_abnormal_01();
void tc_rs_net_api_init_fail();
#endif
