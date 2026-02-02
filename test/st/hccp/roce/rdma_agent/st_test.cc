extern "C" {
#include "ut_dispatch.h"
#include "tc_rdma_agent.h"
#include "tc_ra_ping.h"
#include "tc_ra_tlv.h"
#include "tc_ra_adp_tlv.h"
}

#include <stdio.h>
#include "gtest/gtest.h"
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

// TEST_M(RdmaAgent, tc_abnormal);
TEST_M(RdmaAgent, tc_normal_offline);
//TEST_M(RdmaAgent, tc_normal_online);
// TEST_M(RdmaAgent, tc_peer);
// TEST_M(RdmaAgent, tc_hdc_fail);
TEST_M(RdmaAgent, tc_peer_fail);
// TEST_M(RdmaAgent, tc_comm_fail);
// TEST_M(RdmaAgent, tc_hdc_fail_01);
TEST_M(RdmaAgent, tc_host_fail);
TEST_M(RdmaAgent, tc_ra_rdev_get_port_status);
// TEST_M(RdmaAgent, tc_ra_recv_wrlist);
TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_add);
TEST_M(RdmaAgent, tc_ra_peer_set_tcp_recv_callback);
TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_mod);
TEST_M(RdmaAgent, tc_ra_peer_epoll_ctl_del);
TEST_M(RdmaAgent, tc_ra_get_qp_context);
TEST_M(RdmaAgent, tc_peer_log_test);
TEST_M(RdmaAgent, tc_ra_rs_send_wr_list);
TEST_M(RdmaAgent, tc_ra_rs_send_wr_list_ext);
TEST_M(RdmaAgent, tc_ra_hdc_send_wrlist_ext_init);
TEST_M(RdmaAgent, tc_ra_hdc_send_wrlist_ext);
TEST_M(RdmaAgent, tc_ra_rs_send_normal_wrlist);
TEST_M(RdmaAgent, tc_host_ra_send_wrlist_ext);
TEST_M(RdmaAgent, tc_host_ra_send_normal_wrlist);
TEST_M(RdmaAgent, tc_ra_hdc_send_normal_wrlist);
TEST_M(RdmaAgent, tc_ra_create_cq);
TEST_M(RdmaAgent, tc_ra_create_notmal_qp);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_qos);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_timeout);
TEST_M(RdmaAgent, tc_ra_set_qp_attr_retry_cnt);
TEST_M(RdmaAgent, tc_ra_create_comp_channel);
TEST_M(RdmaAgent, tc_ra_get_cqe_err_info);
TEST_M(RdmaAgent, tc_ra_rdev_get_cqe_err_info_list);
TEST_M(RdmaAgent, tc_ra_rs_get_ifnum);
TEST_M(RdmaAgent, tc_ra_rs_ping);
TEST_M(RdmaAgent, tc_ra_create_srq);
TEST_M(RdmaAgent, tc_ra_rs_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_rs_socket_credit_add);

TEST_M(RdmaAgent, tc_ra_get_qp_attr);
TEST_M(RdmaAgent, tc_hdc_send_wr_op);
TEST_M(RdmaAgent, tc_hdc_lite_send_wr_op);
TEST_M(RdmaAgent, tc_ra_create_event_handle);
TEST_M(RdmaAgent, tc_ra_ctl_event_handle);
TEST_M(RdmaAgent, tc_ra_wait_event_handle);
TEST_M(RdmaAgent, tc_ra_destroy_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_create_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_ctl_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_wait_event_handle);
TEST_M(RdmaAgent, tc_ra_peer_destroy_event_handle);
TEST_M(RdmaAgent,tc_ra_peer_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_poll_cq);
TEST_M(RdmaAgent, tc_hdc_recv_wrlist);
TEST_M(RdmaAgent, tc_hdc_poll_cq);
TEST_M(RdmaAgent, tc_hdc_get_lite_support);
TEST_M(RdmaAgent, tc_ra_rdev_get_support_lite);
TEST_M(RdmaAgent, tc_ra_rdev_get_handle);
TEST_M(RdmaAgent, tc_ra_is_first_or_last_used);
TEST_M(RdmaAgent, tc_ra_rs_socket_port_is_use);
TEST_M(RdmaAgent, tc_get_vnic_ip_infos);
TEST_M(RdmaAgent, tc_ra_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_rs_get_vnic_ip_infos_v1);
TEST_M(RdmaAgent, tc_ra_rs_get_vnic_ip_infos);
TEST_M(RdmaAgent, tc_ra_rs_typical_mr_reg);
TEST_M(RdmaAgent, tc_ra_rs_get_cqe_err_info);
TEST_M(RdmaAgent, tc_ra_rs_rdev_init);
TEST_M(RdmaAgent, tc_ra_rs_rdev_init_with_backup);
TEST_M(RdmaAgent, tc_ra_rs_get_qp_status);
TEST_M(RdmaAgent, tc_ra_rs_get_qp_info);
TEST_M(RdmaAgent, tc_ra_rs_remap_mr);
TEST_M(RdmaAgent, tc_ra_rs_get_sockets);
TEST_M(RdmaAgent, tc_ra_rs_ai_qp_create);
TEST_M(RdmaAgent, tc_ra_rs_typical_qp_modify);
TEST_M(RdmaAgent, tc_ra_rs_get_cqe_err_info_01);
TEST_M(RdmaAgent, tc_ra_hdc_get_cqe_err_info_list);
TEST_M(RdmaAgent, tc_ra_hdc_lite_ctx_init);
TEST_M(RdmaAgent, rc_ra_hdc_lite_qp_create);
TEST_M(RdmaAgent, tc_ra_get_client_socket_err_info);
TEST_M(RdmaAgent, tc_ra_get_server_socket_err_info);
TEST_M(RdmaAgent, tc_ra_socket_accept_credit_add);
TEST_M(RdmaAgent, tc_ra_remap_mr);
TEST_M(RdmaAgent, tc_ra_register_mr);
TEST_M(RdmaAgent, tc_ra_hdc_recv_handle_send_pkt_unsuccess);
TEST_M(RdmaAgent, tc_ra_rs_async_hdc_session_connect);
TEST_M(RdmaAgent, tc_hdc_async_recv_pkt);
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
TEST_M(RdmaAgent, tc_ra_rs_ctx_token_id_alloc);
TEST_M(RdmaAgent, tc_ra_ctx_cq_create);
TEST_M(RdmaAgent, tc_ra_ctx_qp_create);
TEST_M(RdmaAgent, tc_ra_ctx_qp_import);
TEST_M(RdmaAgent, tc_ra_ctx_qp_bind);
TEST_M(RdmaAgent, tc_ra_rs_ctx_ops);
TEST_M(RdmaAgent, tc_ra_batch_send_wr);
TEST_M(RdmaAgent, tc_ra_ctx_update_ci);
TEST_M(RdmaAgent, tc_ra_custom_channel);
TEST_M(RdmaAgent, tc_ra_get_eid_by_ip);
TEST_M(RdmaAgent, tc_ra_ctx_get_cr_err_info_list);

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
TEST_M(RdmaAgent, tc_ra_hdc_pool_create);
TEST_M(RdmaAgent, tc_ra_hdc_async_init_session);
TEST_M(RdmaAgent, tc_ra_get_eid_by_ip_async);
TEST_M(RdmaAgent, tc_ra_hdc_get_eid_by_ip_async);

TEST_M(RdmaAgent, tc_ra_hdc_session_accept);

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
TEST_M(RdmaAgent, tc_hdc_socket_batch_abort);
TEST_M(RdmaAgent, tc_ra_hdc_socket_accept_credit_add);
TEST_M(RdmaAgent, tc_ra_save_snapshot_input);
TEST_M(RdmaAgent, tc_ra_save_snapshot_pre);
TEST_M(RdmaAgent, tc_ra_save_snapshot_post);
TEST_M(RdmaAgent, tc_hdc_async_del_req_handle);
TEST_M(RdmaAgent, tc_ra_loopback_qp_create);
TEST_M(RdmaAgent, tc_ra_peer_loopback_qp_create);
TEST_M(RdmaAgent, tc_ra_peer_loopback_single_qp_create);
TEST_M(RdmaAgent, tc_ra_hdc_uninit_async);
TEST_M(RdmaAgent, tc_ra_hdc_qp_create_with_attrs);
TEST_M(RdmaAgent, tc_host_notify_base_addr_init);

TEST_M(RdmaAgent, tc_ra_ctx_qp_destroy_batch_async);
TEST_M(RdmaAgent, tc_qp_destroy_batch_param_check);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_qp_destroy_batch_async);
TEST_M(RdmaAgent, tc_ra_rs_ctx_qp_destroy_batch);
TEST_M(RdmaAgent, tc_ra_ctx_qp_query_batch);
TEST_M(RdmaAgent, tc_qp_query_batch_param_check);
TEST_M(RdmaAgent, tc_ra_hdc_ctx_qp_query_batch);
TEST_M(RdmaAgent, tc_ra_rs_ctx_qp_query_batch);
TEST_M(RdmaAgent, tc_ra_hdc_async_session_close);
TEST_M(RdmaAgent, tc_ra_get_tp_attr_async);
TEST_M(RdmaAgent, tc_ra_hdc_get_tp_attr_async);
TEST_M(RdmaAgent, tc_ra_rs_get_tp_attr);
TEST_M(RdmaAgent, tc_ra_set_tp_attr_async);
TEST_M(RdmaAgent, tc_ra_hdc_set_tp_attr_async);
TEST_M(RdmaAgent, tc_ra_rs_set_tp_attr);
