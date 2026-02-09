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
#include "tc_ut_rs.h"
#include "tc_ut_rs_ping.h"
#include "tc_ut_rs_ping_urma.h"
#include "tc_ut_rs_tlv.h"
}

#include <stdio.h>
#include <mockcpp/mockcpp.hpp>
#include "gtest/gtest.h"
#include "tc_ut_rs_ub.h"
#include "tc_ut_rs_ctx.h"

using namespace std;

class RS : public testing::Test
{
protected:
   static void SetUpTestCase()
    {
        std::cout << "\033[36m--RoCE RS SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--RoCE RS TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
	 GlobalMockObject::verify();
    }
};

TEST_M(RS, tc_rs_socket_listen);
TEST_M(RS, tc_rs_socket_listen_ipv6);

TEST_M(RS, tc_rs_qp_create);
TEST_M(RS, tc_rs_qp_create_with_attrs);
TEST_M(RS, tc_rs_mem_pool);
TEST_M(RS, tc_rs_get_qp_status);
TEST_M(RS, tc_rs_get_notify_ba);
TEST_M(RS, tc_rs_setup_sharemem);

TEST_M(RS, tc_rs_mr_abnormal);

TEST_M(RS, tc_rs_send_wr);
TEST_M(RS, tc_rs_send_wrlist_normal);
TEST_M(RS, tc_rs_send_wrlist_exp);

TEST_M(RS, tc_rs_socket_ops);

TEST_M(RS, tc_rs_white_list);
TEST_M(RS, tc_rs_peer_get_ifaddrs);
TEST_M(RS, tc_rs_get_ifnum);
TEST_M(RS, tc_rs_get_interface_version);

TEST_M(RS, tc_rs_get_cur_time);
TEST_M(RS, tc_rs_get_host_rdev_index);
TEST_M(RS, tc_rs_rdev_cb_init);
TEST_M(RS, tc_rs_ssl_free);
TEST_M(RS, tc_rs_ssl_deinit);
TEST_M(RS, tc_rs_ssl_ca_ky_init);
TEST_M(RS, tc_rs_check_pridata);
TEST_M(RS, tc_tls_load_cert);
TEST_M(RS, tc_rs_ssl_err_string);
TEST_M(RS, tc_rs_socket_fill_wlist_by_phyID);
 TEST_M(RS, tc_rs_notify_cfg_set);
TEST_M(RS, tc_rs_server_send_wlist_check_result);

TEST_M(RS, tc_rs_socket_deinit2);
TEST_M(RS, tc_RsApiInit);

TEST_M(RS, tc_rs_drv_query_notify_and_alloc_pd);
TEST_M(RS, tc_rs_send_normal_wrlist);
TEST_M(RS, tc_rs_drv_send_exp);
TEST_M(RS, tc_rs_drv_normal_qp_create_init);
TEST_M(RS, tc_rs_register_mr);
TEST_M(RS, tc_rs_epoll_ctl_mod);
TEST_M(RS, tc_rs_epoll_ctl_mod_01);
TEST_M(RS, tc_rs_epoll_ctl_mod_02);
TEST_M(RS, tc_rs_epoll_ctl_mod_03);
TEST_M(RS, tc_rs_epoll_ctl_del);
TEST_M(RS, tc_rs_epoll_ctl_del_01);
TEST_M(RS, tc_rs_set_tcp_recv_callback);
TEST_M(RS, tc_rs_epoll_event_in_handle);
TEST_M(RS, tc_rs_epoll_event_in_handle_01);
TEST_M(RS, tc_rs_epoll_tcp_recv);
TEST_M(RS, tc_rs_epoll_event_ssl_accept_in_handle);
TEST_M(RS, tc_rs_send_exp_wrlist);
TEST_M(RS, tc_rs_drv_poll_cq_handle);
TEST_M(RS, tc_rs_normal_qp_create);
TEST_M(RS, tc_rs_query_event);
TEST_M(RS, tc_rs_create_cq);
TEST_M(RS, tc_rs_create_normal_qp);
TEST_M(RS, tc_rs_create_comp_channel);
TEST_M(RS, tc_rs_get_cqe_err_info);
TEST_M(RS, tc_rs_get_cqe_err_info_num);
TEST_M(RS, tc_rs_get_cqe_err_info_list);
TEST_M(RS, tc_rs_save_cqe_err_info);
TEST_M(RS, tc_rs_cqe_callback_process);
TEST_M(RS, tc_rs_create_srq);
TEST_M(RS, tc_rs_get_ipv6_scope_id);
TEST_M(RS, tc_rs_create_event_handle);
TEST_M(RS, tc_rs_ctl_event_handle);
TEST_M(RS, tc_rs_wait_event_handle);
TEST_M(RS, tc_rs_destroy_event_handle);
TEST_M(RS, tc_rs_epoll_create_epollfd);
TEST_M(RS, tc_rs_epoll_destroy_fd);
TEST_M(RS, tc_rs_epoll_wait_handle);
TEST_M(RS, tc_rs_get_vnic_ip_info);
TEST_M(RS, tc_rs_typical_register_mr);
TEST_M(RS, tc_rs_typical_qp_modify);
TEST_M(RS, tc_rs_remap_mr);
TEST_M(RS, tc_RsRoceGetApiVersion);
TEST_M(RS, tc_rs_get_tls_enable);
TEST_M(RS, tc_rs_get_sec_random);
TEST_M(RS, tc_rs_get_hccn_cfg);
TEST_M(RS, tc_rs_jfc_callback_process);

/**
 ******************** Beginning of UB TEST ***************************
 */

TEST_M(RS, tc_rs_ub_get_rdev_cb);
TEST_M(RS, tc_rs_urma_api_init_abnormal);

TEST_M(RS, tc_rs_get_dev_eid_info_num);
TEST_M(RS, tc_rs_get_dev_eid_info_list);
TEST_M(RS, tc_rs_ctx_init);
TEST_M(RS, tc_rs_ctx_deinit);
TEST_M(RS, tc_rs_ctx_token_id_alloc);
TEST_M(RS, tc_rs_ctx_token_id_free);
TEST_M(RS, tc_rs_ctx_lmem_reg);
TEST_M(RS, tc_rs_ctx_lmem_unreg);
TEST_M(RS, tc_rs_ctx_rmem_import);
TEST_M(RS, tc_rs_ctx_rmem_unimport);
TEST_M(RS, tc_rs_ctx_chan_create);
TEST_M(RS, tc_rs_ctx_chan_destroy);
TEST_M(RS, tc_rs_ctx_cq_create);
TEST_M(RS, tc_rs_ctx_cq_destroy);
TEST_M(RS, tc_rs_ctx_qp_create);
TEST_M(RS, tc_rs_ctx_qp_destroy);
TEST_M(RS, tc_rs_ctx_qp_import);
TEST_M(RS, tc_rs_ctx_qp_unimport);
TEST_M(RS, tc_rs_ctx_qp_bind);
TEST_M(RS, tc_rs_ctx_qp_unbind);
TEST_M(RS, tc_rs_ctx_batch_send_wr);
TEST_M(RS, tc_rs_ctx_update_ci);
TEST_M(RS, tc_rs_ctx_custom_channel);
TEST_M(RS, tc_rs_ctx_esched);
TEST_M(RS, tc_rs_get_eid_by_ip);

/* pingMesh ut cases */
TEST_M(RS, tc_rs_payload_header_resv_custom_check);
TEST_M(RS, tc_rs_ping_handle_init);
TEST_M(RS, tc_rs_ping_handle_deinit);
TEST_M(RS, tc_rs_ping_init);
TEST_M(RS, tc_rs_get_ping_cb);
TEST_M(RS, tc_rs_ping_client_post_send);
TEST_M(RS, tc_rs_ping_get_results);
TEST_M(RS, tc_rs_ping_task_stop);
TEST_M(RS, tc_rs_ping_target_del);
TEST_M(RS, tc_rs_ping_deinit);
TEST_M(RS, tc_rs_ping_compare_rdma_info);
TEST_M(RS, tc_rs_ping_roce_find_target_node);
TEST_M(RS, tc_rs_pong_find_target_node);
TEST_M(RS, tc_rs_pong_find_alloc_target_node);
TEST_M(RS, tc_rs_ping_poll_send_cq);
TEST_M(RS, tc_rs_ping_server_post_send);
TEST_M(RS, tc_rs_ping_post_recv);
TEST_M(RS, tc_rs_ping_client_poll_cq);
TEST_M(RS, tc_rs_epoll_event_ping_handle);
TEST_M(RS, tc_rs_ping_get_trip_time);
TEST_M(RS, tc_rs_ping_cb_init_mutex);
TEST_M(RS, tc_rs_ping_resolve_response_packet);
TEST_M(RS, tc_rs_ping_server_poll_cq);
TEST_M(RS, tc_rs_ping_common_deinit_local_buffer);
TEST_M(RS, tc_rs_ping_common_deinit_local_qp);
TEST_M(RS, tc_rs_ping_roce_poll_scq);
TEST_M(RS, tc_rs_ping_pong_init_local_info);
TEST_M(RS, tc_rs_ping_handle);
TEST_M(RS, tc_rs_ping_roce_ping_cb_deinit);

TEST_M(RS, tc_rs_epoll_event_ping_handle_urma);
TEST_M(RS, tc_rs_ping_init_deinit_urma);
TEST_M(RS, tc_rs_ping_target_add_del_urma);
TEST_M(RS, tc_rs_ping_urma_post_send);
TEST_M(RS, tc_rs_ping_urma_poll_scq);
TEST_M(RS, tc_rs_ping_client_poll_cq_urma);
TEST_M(RS, tc_rs_ping_server_poll_cq_urma);
TEST_M(RS, tc_rs_ping_get_results_urma);
TEST_M(RS, tc_rs_ping_server_post_send_urma);
TEST_M(RS, tc_rs_pong_jetty_find_alloc_target_node);
TEST_M(RS, tc_rs_ping_common_poll_send_jfc);
TEST_M(RS, tc_rs_pong_jetty_find_target_node);
TEST_M(RS, tc_rs_pong_jetty_resolve_response_packet);
TEST_M(RS, tc_rs_ping_common_import_jetty);
TEST_M(RS, tc_rs_ping_urma_reset_recv_buffer);
TEST_M(RS, tc_rs_ping_common_jfr_post_recv);

TEST_M(RS, tc_rs_peer_socket_recv);
TEST_M(RS, tc_rs_socket_get_server_socket_err_info);
TEST_M(RS, tc_rs_socket_accept_credit_add);
TEST_M(RS, tc_rs_epoll_event_ssl_recv_tag_in_handle);

TEST_M(RS, tc_rs_get_tp_info_list);
TEST_M(RS, tc_rs_ub_ctx_drv_jetty_import);
TEST_M(RS, tc_rs_ub_ctx_init);
TEST_M(RS, tc_rs_ub_ctx_jfc_destroy);
TEST_M(RS, tc_rs_ub_ctx_ext_jetty_delete);
TEST_M(RS, tc_rs_ub_ctx_chan_create);
TEST_M(RS, tc_rs_ub_init_seg_cb);
TEST_M(RS, tc_rs_ub_ctx_lmem_reg);
TEST_M(RS, tc_rs_ub_ctx_jfc_create_fail);
TEST_M(RS, tc_rs_ub_ctx_init_jetty_cb);
TEST_M(RS, tc_rs_ub_ctx_jetty_create_fail);
TEST_M(RS, tc_rs_ub_ctx_jetty_import_fail);
TEST_M(RS, tc_rs_ub_ctx_batch_send_wr_fail);
TEST_M(RS, tc_rs_ub_get_eid_by_ip);
TEST_M(RS, tc_rs_ctx_get_aux_info);
TEST_M(RS, tc_rs_ub_ctx_get_aux_info);

TEST_M(RS, tc_rs_nslb_init);
TEST_M(RS, tc_rs_nslb_deinit);
TEST_M(RS, tc_rs_nslb_request);
TEST_M(RS, tc_rs_tlv_assemble_send_data);
TEST_M(RS, tc_rs_get_tlv_cb);
TEST_M(RS, tc_rs_epoll_nslb_event_handle);
TEST_M(RS, tc_rs_nslb_api_init);
TEST_M(RS, tc_rs_free_dev_list);
TEST_M(RS, tc_rs_free_rdev_list);
TEST_M(RS, tc_rs_free_udev_list);
TEST_M(RS, tc_rs_ccu_request);
TEST_M(RS, tc_dl_ccu_api_init);
TEST_M(RS, tc_rs_ctx_qp_destroy_batch);
TEST_M(RS, tc_rs_ctx_qp_query_batch);
TEST_M(RS, tc_rs_ub_ctx_jetty_destroy_batch);
TEST_M(RS, tc_rs_ub_ctx_query_jetty_batch);
TEST_M(RS, tc_rs_net_api_init_deinit);
TEST_M(RS, tc_rs_net_alloc_jfc_id);
TEST_M(RS, tc_rs_net_free_jfc_id);
TEST_M(RS, tc_rs_net_alloc_jetty_id);
TEST_M(RS, tc_rs_net_free_jetty_id);
TEST_M(RS, tc_rs_net_get_cqe_base_addr);
TEST_M(RS, tc_rs_ccu_get_cqe_base_addr);
TEST_M(RS, tc_RsNslbNetcoInitDeinit);
TEST_M(RS, tc_rs_get_tp_attr);
TEST_M(RS, tc_rs_set_tp_attr);
TEST_M(RS, tc_rs_ub_get_tp_attr);
TEST_M(RS, tc_rs_ub_set_tp_attr);
TEST_M(RS, tc_rs_ctx_get_cr_err_info_list);
TEST_M(RS, tc_rs_retry_timeout_exception_check);
