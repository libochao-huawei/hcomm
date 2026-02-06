/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __TC_RDMA_AGENT_H
#define __TC_RDMA_AGENT_H

#include <stdio.h>

void tc_host_abnormal_qp_mode_test(); /* SOP mode test */

void tc_hdc_send_recv_pkt_recv_check();

void tc_ra_peer_socket_white_list_add_01();
void tc_ra_peer_socket_white_list_add_02();
void tc_ra_peer_socket_white_list_del();

void tc_ra_peer_rdev_init_01();
void tc_ra_peer_rdev_init_02();
void tc_ra_peer_rdev_init_03();
void tc_ra_peer_rdev_init_04();
void tc_ra_peer_rdev_deinit_01();
void tc_ra_peer_rdev_deinit_02();
void tc_ra_peer_rdev_deinit_03();
void tc_ra_peer_socket_batch_connect();
void tc_ra_peer_socket_listen_start_01();
void tc_ra_peer_socket_listen_start_02();
void tc_ra_peer_socket_listen_stop();
void tc_ra_peer_set_rs_conn_param();
void tc_ra_inet_pton_01();
void tc_ra_inet_pton_02();
void tc_ra_socket_init();
void tc_ra_socket_init_v1();
void tc_ra_send_wrlist();
void tc_ra_rdev_init();
void tc_ra_rdev_get_port_status();
void tc_ra_hdc_socket_batch_close();
void tc_ra_hdc_rdev_deinit();
void tc_ra_hdc_socket_white_list_add();
void tc_ra_hdc_socket_white_list_del();
void tc_ra_hdc_socket_accept_credit_add();
void tc_ra_hdc_rdev_init();
void tc_ra_hdc_init_apart();
void tc_ra_hdc_qp_destroy();
void tc_ra_hdc_qp_destroy_01();
void tc_msg_send_head_check();
void tc_msg_recv_head_check();
void tc_ra_get_socket_connect_info();
void tc_ra_get_socket_listen_info();
void tc_ra_get_socket_listen_result();
void tc_ra_hw_hdc_init();
void tc_hccp_init_deinit();
void tc_ra_peer_init_fail_001();
void tc_ra_peer_socket_deinit_001();
void tc_host_notify_base_addr_init();
void tc_host_notify_base_addr_init_001();
void tc_host_notify_base_addr_init_002();
void tc_host_notify_base_addr_init_003();
void tc_host_notify_base_addr_init_005();
void tc_host_notify_base_addr_init_006();
void tc_host_notify_base_addr_init_007();

void tc_host_notify_base_addr_uninit();
void tc_host_notify_base_addr_uninit_001();
void tc_host_notify_base_addr_uninit_002();
void tc_host_notify_base_addr_uninit_003();
void tc_host_notify_base_addr_uninit_004();
void tc_host_notify_base_addr_uninit_005();
void tc_ra_peer_send_wrlist();
void tc_ra_peer_send_wrlist_001();
void tc_ra_peer_get_all_sockets();
void tc_ra_peer_get_socket_num();
void tc_ra_get_qp_context();
void tc_ra_create_cq();
void tc_ra_create_notmal_qp();
void tc_ra_create_comp_channel();
void tc_ra_get_cqe_err_info();
void tc_ra_rdev_get_cqe_err_info_list();
void tc_ra_rs_get_ifnum();
void tc_ra_create_srq();
void tc_ra_rs_socket_port_is_use();
void tc_ra_rs_get_vnic_ip_infos_v1();
void tc_ra_rs_get_vnic_ip_infos();
void tc_ra_rs_typical_mr_reg();
void tc_ra_rs_typical_qp_create();
void tc_ra_hdc_recv_handle_send_pkt_unsuccess();
void tc_ra_get_tls_enable();
void tc_ra_get_sec_random();
void tc_ra_rs_get_sec_random();
void tc_ra_rs_get_tls_enable();
void tc_ra_get_hccn_cfg();
void tc_ra_rs_get_hccn_cfg();
void tc_ra_save_snapshot_input();
void tc_ra_save_snapshot_pre();
void tc_ra_save_snapshot_post();
void tc_hdc_async_del_req_handle();
void tc_ra_hdc_uninit_async();
#endif
