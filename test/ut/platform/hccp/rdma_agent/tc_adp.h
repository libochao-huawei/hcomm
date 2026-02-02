/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TC_RA_ADP_H
#define TC_RA_ADP_H

#define MAX_TEST_MESSAGE 10
#ifdef __cplusplus
extern "C" {
#endif
void tc_adapter();
void tc_hccp_init();
void tc_hccp_init_fail();
void tc_hccp_deinit_fail();
void tc_socket_connect();
void tc_socket_close();
void tc_socket_abort();
void tc_socket_listen_start();
void tc_socket_listen_stop();
void tc_socket_info();
void tc_socket_send();
void tc_socket_recv();
void tc_socket_init();
void tc_socket_deinit();
void tc_set_tsqp_depth();
void tc_get_tsqp_depth();
void tc_qp_create();
void tc_qp_destroy();
void tc_qp_status();
void tc_qp_info();
void tc_qp_connect();
void tc_mr_reg();
void tc_mr_dreg();
void tc_send_wr();
void tc_send_wrlist();
void tc_rdev_init();
void tc_rdev_deinit();
void tc_get_notify_ba();
void tc_set_pid();
void tc_get_vnic_ip();
void tc_socket_white_list_add();
void tc_socket_white_list_del();
void tc_get_ifaddrs();
void tc_get_ifaddrs_v2();
void tc_get_ifnum();
void tc_get_interface_version();
void tc_message_process_fail();
void tc_set_notify_cfg();
void tc_get_notify_cfg();
void tc_tlv_init();
void tc_tlv_deinit();
void tc_tlv_request();
void tc_ra_rs_send_wr_list();
void tc_ra_rs_send_wr_list_ext();
void tc_ra_rs_send_normal_wrlist();
void tc_ra_rs_set_qp_attr_qos();
void tc_ra_rs_set_qp_attr_timeout();
void tc_ra_rs_set_qp_attr_retry_cnt();
void tc_ra_rs_get_cqe_err_info();
void tc_ra_rs_get_cqe_err_info_num();
void tc_ra_rs_get_cqe_err_info_list();
void tc_ra_rs_get_lite_support();
void tc_ra_RsGetLiteRdevCap();
void tc_ra_rs_get_lite_qp_cq_attr();
void tc_ra_rs_get_lite_connected_info();
void tc_ra_rs_socket_white_list_v2();
void tc_ra_rs_socket_credit_add();
void tc_ra_rs_get_lite_mem_attr();
void tc_ra_rs_ping_init();
void tc_ra_rs_ping_target_add();
void tc_ra_rs_ping_task_start();
void tc_ra_rs_ping_get_results();
void tc_ra_rs_ping_task_stop();
void tc_ra_rs_ping_target_del();
void tc_ra_rs_ping_deinit();
void tc_ra_rs_remap_mr();
void tc_ra_rs_test_ctx_ops();
void tc_ra_rs_get_tls_enable0();

#ifdef __cplusplus
}
#endif
#endif
