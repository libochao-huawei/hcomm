/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

extern "C" {
#include "ut_dispatch.h"
#include "tc_rdma_agent.h"
#include "tc_ra_ping.h"
#include "tc_ra_peer.h"
#include "tc_ra_adp_tlv.h"
#include "tc_ra_tlv.h"
}

#include <stdio.h>
#include "gtest/gtest.h"
#include "tc_adp.h"
#include "tc_hdc.h"
#include "tc_host.h"
#include "tc_ra_ctx.h"
#include "tc_ra_async.h"

using namespace std;

class RdmaAgent : public testing::Test
{
protected:
   static void SetUpTestCase()
    {
        std::cout << "\033[36m--RoCE RdmaAgent SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RoCE RdmaAgent TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }
};

TEST_M(RdmaAgent, tc_hdc_socket_batch_connect);
TEST_M(RdmaAgent, tc_hdc_socket_listen_start);
TEST_M(RdmaAgent, tc_ifaddr);
TEST_M(RdmaAgent, tc_host);
TEST_M(RdmaAgent, tc_peer);
TEST_M(RdmaAgent, tc_hdc_init);
TEST_M(RdmaAgent, tc_hdc_init_fail);
TEST_M(RdmaAgent, tc_hdc_deinit_fail);
TEST_M(RdmaAgent, tc_hdc_socket_batch_close);
TEST_M(RdmaAgent, tc_hdc_socket_batch_abort);
TEST_M(RdmaAgent, tc_hdc_socket_listen_stop);
TEST_M(RdmaAgent, tc_hdc_get_sockets);
TEST_M(RdmaAgent, tc_hdc_socket_send);
TEST_M(RdmaAgent, tc_hdc_socket_recv);
TEST_M(RdmaAgent, tc_hdc_qp_create_destroy);
TEST_M(RdmaAgent, tc_hdc_get_qp_status);
TEST_M(RdmaAgent, tc_hdc_qp_connect_async);

/* pingMesh ut cases */
TEST_M(RdmaAgent, tc_ra_rs_ping_init);
TEST_M(RdmaAgent, tc_ra_rs_ping_target_add);
TEST_M(RdmaAgent, tc_ra_rs_ping_task_start);
TEST_M(RdmaAgent, tc_ra_rs_ping_get_results);
TEST_M(RdmaAgent, tc_ra_rs_ping_task_stop);
TEST_M(RdmaAgent, tc_ra_rs_ping_target_del);
TEST_M(RdmaAgent, tc_ra_rs_ping_deinit);

TEST_M(RdmaAgent, tc_hdc_socket_init);
TEST_M(RdmaAgent, tc_hdc_socket_deinit);
TEST_M(RdmaAgent, tc_hdc_rdev_init);
TEST_M(RdmaAgent, tc_hdc_rdev_deinit);
TEST_M(RdmaAgent, tc_hdc_socket_white_list_add);
TEST_M(RdmaAgent, tc_hdc_socket_white_list_del);
TEST_M(RdmaAgent, tc_hdc_get_ifaddrs);
TEST_M(RdmaAgent, tc_hdc_get_ifaddrs_v2);
TEST_M(RdmaAgent, tc_hdc_get_ifnum);
TEST_M(RdmaAgent, tc_hdc_message_process_fail);
TEST_M(RdmaAgent, tc_hdc_socket_recv_fail);
TEST_M(RdmaAgent, tc_ra_hdc_send_wrlist_ext_init);
TEST_M(RdmaAgent, tc_ra_hdc_send_wrlist_ext);
TEST_M(RdmaAgent, tc_ra_hdc_send_normal_wrlist);

TEST_M(RdmaAgent, tc_hccp_init);
TEST_M(RdmaAgent, tc_hccp_init_fail);
TEST_M(RdmaAgent, tc_hccp_deinit_fail);
TEST_M(RdmaAgent, tc_socket_connect);
TEST_M(RdmaAgent, tc_socket_close);
TEST_M(RdmaAgent, tc_socket_abort);
TEST_M(RdmaAgent, tc_socket_listen_start);
TEST_M(RdmaAgent, tc_socket_listen_stop);
TEST_M(RdmaAgent, tc_socket_info);
TEST_M(RdmaAgent, tc_socket_send);
TEST_M(RdmaAgent, tc_socket_recv);
TEST_M(RdmaAgent, tc_socket_init);
TEST_M(RdmaAgent, tc_socket_deinit);
TEST_M(RdmaAgent, tc_set_tsqp_depth);
TEST_M(RdmaAgent, tc_get_tsqp_depth);
TEST_M(RdmaAgent, tc_qp_create);
TEST_M(RdmaAgent, tc_qp_destroy);
TEST_M(RdmaAgent, tc_qp_status);
TEST_M(RdmaAgent, tc_qp_info);
TEST_M(RdmaAgent, tc_qp_connect);
TEST_M(RdmaAgent, tc_ra_rs_remap_mr);
TEST_M(RdmaAgent, tc_ra_rs_get_tls_enable0);
TEST_M(RdmaAgent, tc_mr_reg);
TEST_M(RdmaAgent, tc_mr_dreg);
TEST_M(RdmaAgent, tc_send_wr);
TEST_M(RdmaAgent, tc_send_wrlist);
TEST_M(RdmaAgent, tc_rdev_init);
TEST_M(RdmaAgent, tc_rdev_deinit);
TEST_M(RdmaAgent, tc_get_notify_ba);
TEST_M(RdmaAgent, tc_set_pid);
TEST_M(RdmaAgent, tc_get_vnic_ip);
TEST_M(RdmaAgent, tc_socket_white_list_add);
TEST_M(RdmaAgent, tc_socket_white_list_del);
TEST_M(RdmaAgent, tc_get_ifaddrs);
TEST_M(RdmaAgent, tc_get_ifaddrs_v2);
TEST_M(RdmaAgent, tc_get_ifnum);
TEST_M(RdmaAgent, tc_get_interface_version);
TEST_M(RdmaAgent, tc_tlv_init);
TEST_M(RdmaAgent, tc_tlv_deinit);
TEST_M(RdmaAgent, tc_tlv_request);
TEST_M(RdmaAgent, tc_ra_rs_test_ctx_ops);

TEST_M(RdmaAgent, tc_host_abnormal_qp_mode_test);
TEST_M(RdmaAgent, tc_ra_peer_socket_white_list_add_01);
TEST_M(RdmaAgent, tc_ra_peer_socket_white_list_add_02);
TEST_M(RdmaAgent, tc_ra_peer_rdev_init_01);
TEST_M(RdmaAgent, tc_ra_peer_rdev_init_02);
TEST_M(RdmaAgent, tc_ra_peer_rdev_init_03);
TEST_M(RdmaAgent, tc_ra_peer_rdev_init_04);
TEST_M(RdmaAgent, tc_ra_peer_rdev_deinit_01);
TEST_M(RdmaAgent, tc_ra_peer_rdev_deinit_02);
TEST_M(RdmaAgent, tc_ra_peer_rdev_deinit_03);
TEST_M(RdmaAgent, tc_ra_peer_socket_white_list_del);
TEST_M(RdmaAgent, tc_ra_peer_socket_batch_connect);
TEST_M(RdmaAgent, tc_ra_peer_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_peer_socket_listen_start_01);
TEST_M(RdmaAgent, tc_ra_peer_socket_listen_start_02);
TEST_M(RdmaAgent, tc_ra_peer_socket_listen_stop);
TEST_M(RdmaAgent, tc_ra_peer_set_rs_conn_param);
TEST_M(RdmaAgent, tc_ra_inet_pton_01);
TEST_M(RdmaAgent, tc_ra_inet_pton_02);
TEST_M(RdmaAgent, tc_ra_socket_init);
TEST_M(RdmaAgent, tc_ra_socket_init_v1);
TEST_M(RdmaAgent, tc_ra_send_wrlist);
TEST_M(RdmaAgent, tc_ra_rdev_init);
TEST_M(RdmaAgent, tc_ra_rdev_get_port_status);
TEST_M(RdmaAgent, tc_ra_hdc_rdev_deinit);
TEST_M(RdmaAgent, tc_ra_hdc_socket_white_list_add);
TEST_M(RdmaAgent, tc_ra_hdc_socket_white_list_del);
TEST_M(RdmaAgent, tc_ra_hdc_socket_accept_credit_add);
TEST_M(RdmaAgent, tc_ra_hdc_rdev_init);
TEST_M(RdmaAgent, tc_ra_hdc_init_apart);
TEST_M(RdmaAgent, tc_ra_hdc_qp_destroy);
TEST_M(RdmaAgent, tc_ra_hdc_qp_destroy_01);
TEST_M(RdmaAgent, tc_ra_get_socket_connect_info);
TEST_M(RdmaAgent, tc_ra_get_socket_listen_info);
TEST_M(RdmaAgent, tc_ra_get_socket_listen_result);
TEST_M(RdmaAgent, tc_peer_fail);
TEST_M(RdmaAgent, tc_ra_peer_init_fail_001);
TEST_M(RdmaAgent, tc_ra_peer_socket_deinit_001);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init_001);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init_002);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init_003);

TEST_M(RdmaAgent, tc_host_notify_base_addr_init_005);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init_006);

TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit);
TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit_001);
TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit_002);
TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit_003);
TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit_004);
TEST_M(RdmaAgent, tc_host_notify_base_addr_uninit_005);

TEST_M(RdmaAgent, tc_ra_peer_send_wrlist);
TEST_M(RdmaAgent, tc_ra_peer_send_wrlist_001);

TEST_M(RdmaAgent, tc_ra_recv_wrlist);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_qos);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_timeout);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_retry_cnt);
TEST_M(RdmaAgent, tc_ra_get_cqe_err_info);
TEST_M(RdmaAgent, tc_ra_rdev_get_cqe_err_info_list);
TEST_M(RdmaAgent, tc_ra_rs_get_ifnum);

TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_add);
TEST_M(RdmaAgent, tc_ra_peer_set_tcp_recv_callback);
TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_mod);
TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_del);
TEST_M(RdmaAgent, tc_ra_get_qp_context);

TEST_M(RdmaAgent, tc_host_ra_send_wrlist_ext);
TEST_M(RdmaAgent, tc_host_ra_send_normal_wrlist);
TEST_M(RdmaAgent, tc_ra_create_cq);
TEST_M(RdmaAgent, tc_ra_create_notmal_qp);
TEST_M(RdmaAgent, tc_ra_peer_cq_create);
TEST_M(RdmaAgent, tc_ra_peer_normal_qp_create);
TEST_M(RdmaAgent, tc_ra_create_comp_channel);
TEST_M(RdmaAgent, tc_ra_create_srq);

TEST_M(RdmaAgent, tc_ra_create_event_handle);
TEST_M(RdmaAgent, tc_ra_ctl_event_handle);
TEST_M(RdmaAgent, tc_ra_wait_event_handle);
TEST_M(RdmaAgent, tc_ra_destroy_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_create_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_ctl_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_wait_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_destroy_event_handle);
TEST_M(RdmaAgent, tc_ra_loopback_qp_create);
TEST_M(RdmaAgent, tc_ra_peer_loopback_qp_create);
TEST_M(RdmaAgent, tc_ra_peer_loopback_single_qp_create);

TEST_M(RdmaAgent, tc_ra_poll_cq);
TEST_M(RdmaAgent, tc_hdc_recv_wrlist);
TEST_M(RdmaAgent, tc_hdc_poll_cq);
TEST_M(RdmaAgent, tc_hdc_get_lite_support);
TEST_M(RdmaAgent, tc_ra_rdev_get_support_lite);
TEST_M(RdmaAgent, tc_ra_rdev_get_handle);
TEST_M(RdmaAgent, tc_ra_is_first_or_last_used);

TEST_M(RdmaAgent, tc_ra_rs_socket_port_is_use);
TEST_M(RdmaAgent, tc_get_vnic_ip_infos);
TEST_M(RdmaAgent, tc_ra_rs_get_vnic_ip_infos_v1);
TEST_M(RdmaAgent, tc_ra_rs_get_vnic_ip_infos);
TEST_M(RdmaAgent, tc_ra_rs_typical_mr_reg);
TEST_M(RdmaAgent, tc_ra_rs_typical_qp_create);
TEST_M(RdmaAgent, tc_ra_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_hdc_lite_ctx_init);
TEST_M(RdmaAgent, tc_ra_remap_mr);
TEST_M(RdmaAgent, tc_ra_register_mr);
TEST_M(RdmaAgent, tc_ra_hdc_recv_handle_send_pkt_unsuccess);
TEST_M(RdmaAgent, tc_ra_hdc_get_eid_by_ip);
TEST_M(RdmaAgent, tc_ra_rs_get_eid_by_ip);
TEST_M(RdmaAgent, tc_ra_peer_get_eid_by_ip);
TEST_M(RdmaAgent, tc_ra_ctx_get_aux_info);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_get_aux_info);
TEST_M(RdmaAgent, tc_ra_rs_ctx_get_aux_info);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_get_cr_err_info_list);
TEST_M(RdmaAgent, tc_ra_rs_ctx_get_cr_err_info_list);

/* pingMesh ut cases */
TEST_M(RdmaAgent, tc_ra_ping_init_get_handle_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_init_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_target_add_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_task_start_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_get_results_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_target_del_abnoraml);
TEST_M(RdmaAgent, tc_ra_ping_task_stop_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_deinit_para_check_abnormal);
TEST_M(RdmaAgent, tc_ra_ping_deinit_abnoaml);
TEST_M(RdmaAgent, tc_ra_ping);

TEST_M(RdmaAgent, tc_ra_get_dev_eid_info_num);
TEST_M(RdmaAgent, tc_ra_get_dev_eid_info_list);
TEST_M(RdmaAgent, tc_ra_ctx_init);
TEST_M(RdmaAgent, tc_ra_get_dev_base_attr);
TEST_M(RdmaAgent, tc_ra_ctx_deinit);
TEST_M(RdmaAgent, tc_ra_ctx_lmem_register);
TEST_M(RdmaAgent, tc_ra_ctx_rmem_import);
TEST_M(RdmaAgent, tc_ra_ctx_chan_create);
TEST_M(RdmaAgent, tc_ra_ctx_token_id_alloc);
TEST_M(RdmaAgent, tc_ra_ctx_cq_create);
TEST_M(RdmaAgent, tc_ra_ctx_qp_create);
TEST_M(RdmaAgent, tc_ra_ctx_qp_import);
TEST_M(RdmaAgent, tc_ra_ctx_qp_bind);
TEST_M(RdmaAgent, tc_ra_batch_send_wr);
TEST_M(RdmaAgent, tc_ra_ctx_update_ci);
TEST_M(RdmaAgent, tc_ra_custom_channel);
TEST_M(RdmaAgent, tc_ra_get_eid_by_ip);
TEST_M(RdmaAgent, tc_ra_ctx_get_cr_err_info_list);

TEST_M(RdmaAgent, tc_ra_rs_async_hdc_session_connect);
TEST_M(RdmaAgent, tc_ra_hdc_async_send_pkt);
TEST_M(RdmaAgent, tc_ra_hdc_pool_add_task);
TEST_M(RdmaAgent, tc_ra_hdc_async_recv_pkt);
TEST_M(RdmaAgent, tc_hdc_async_recv_pkt);
TEST_M(RdmaAgent, tc_ra_hdc_pool_create);
TEST_M(RdmaAgent, tc_ra_async_handle_pkt);
TEST_M(RdmaAgent, tc_ra_hdc_async_handle_socket_listen_start);
TEST_M(RdmaAgent, tc_ra_hdc_async_handle_qp_import);
TEST_M(RdmaAgent, tc_ra_peer_ctx_init);
TEST_M(RdmaAgent, tc_ra_peer_ctx_deinit);
TEST_M(RdmaAgent, tc_ra_peer_get_dev_eid_info_num);
TEST_M(RdmaAgent, tc_ra_peer_get_dev_eid_info_list);
TEST_M(RdmaAgent, tc_ra_peer_ctx_token_id_alloc);
TEST_M(RdmaAgent, tc_ra_peer_ctx_token_id_free);
TEST_M(RdmaAgent, tc_ra_peer_ctx_lmem_register);
TEST_M(RdmaAgent, tc_ra_peer_ctx_lmem_unregister);
TEST_M(RdmaAgent, tc_ra_peer_ctx_rmem_import);
TEST_M(RdmaAgent, tc_ra_peer_ctx_rmem_unimport);
TEST_M(RdmaAgent, tc_ra_peer_ctx_chan_create);
TEST_M(RdmaAgent, tc_ra_peer_ctx_chan_destroy);
TEST_M(RdmaAgent, tc_ra_peer_ctx_cq_create);
TEST_M(RdmaAgent, tc_ra_peer_ctx_cq_destroy);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_create);
TEST_M(RdmaAgent, tc_ra_ctx_prepare_qp_create);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_destroy);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_import);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_unimport);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_bind);
TEST_M(RdmaAgent, tc_ra_peer_ctx_qp_unbind);

TEST_M(RdmaAgent, tc_ra_ctx_lmem_register_async);
TEST_M(RdmaAgent, tc_ra_ctx_lmem_unregister_async);
TEST_M(RdmaAgent, tc_ra_ctx_qp_create_async);
TEST_M(RdmaAgent, tc_ra_ctx_qp_destroy_async);
TEST_M(RdmaAgent, tc_ra_ctx_qp_import_async);
TEST_M(RdmaAgent, tc_ra_ctx_qp_unimport_async);
TEST_M(RdmaAgent, tc_ra_socket_send_async);
TEST_M(RdmaAgent, tc_ra_socket_recv_async);
TEST_M(RdmaAgent, tc_ra_get_async_req_result);
TEST_M(RdmaAgent, tc_ra_socket_batch_connect_async);
TEST_M(RdmaAgent, tc_ra_socket_listen_start_async);
TEST_M(RdmaAgent, tc_ra_socket_listen_stop_async);
TEST_M(RdmaAgent, tc_ra_socket_batch_close_async);
TEST_M(RdmaAgent, tc_ra_hdc_async_init_session);
TEST_M(RdmaAgent, tc_ra_get_eid_by_ip_async);
TEST_M(RdmaAgent, tc_ra_hdc_get_eid_by_ip_async);

TEST_M(RdmaAgent, tc_ra_tlv_init);
TEST_M(RdmaAgent, tc_ra_tlv_deinit);
TEST_M(RdmaAgent, tc_ra_tlv_request);
TEST_M(RdmaAgent, tc_ra_hdc_tlv_request);
TEST_M(RdmaAgent, tc_ra_rs_tlv_init);
TEST_M(RdmaAgent, tc_ra_rs_tlv_deinit);
TEST_M(RdmaAgent, tc_ra_rs_tlv_request);
TEST_M(RdmaAgent, tc_ra_get_tp_info_list_async);
TEST_M(RdmaAgent, tc_ra_hdc_get_tp_info_list_async);
TEST_M(RdmaAgent, tc_ra_rs_get_tp_info_list);
TEST_M(RdmaAgent, tc_ra_get_sec_random);
TEST_M(RdmaAgent, tc_ra_rs_get_sec_random);
TEST_M(RdmaAgent, tc_ra_get_tls_enable);
TEST_M(RdmaAgent, tc_ra_rs_get_tls_enable);
TEST_M(RdmaAgent, tc_ra_get_hccn_cfg);
TEST_M(RdmaAgent, tc_ra_rs_get_hccn_cfg);
TEST_M(RdmaAgent, tc_hdc_async_del_req_handle);
TEST_M(RdmaAgent, tc_ra_hdc_uninit_async);

TEST_M(RdmaAgent, tc_ra_ctx_qp_destroy_batch_async);
TEST_M(RdmaAgent, tc_qp_destroy_batch_param_check);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_qp_destroy_batch_async);
TEST_M(RdmaAgent, tc_ra_rs_ctx_qp_destroy_batch);
TEST_M(RdmaAgent, tc_ra_ctx_qp_query_batch);
TEST_M(RdmaAgent, tc_qp_query_batch_param_check);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_qp_query_batch);
TEST_M(RdmaAgent, tc_ra_rs_ctx_qp_query_batch);
