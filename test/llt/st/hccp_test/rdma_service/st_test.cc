extern "C" {
#include "ut_dispatch.h"
#include "tc_rs.h"
#include "tc_ut_rs_ping.h"
#include "tc_ut_rs_ping_urma.h"
#include "tc_rs_tlv.h"
}

#include <stdio.h>
#include "gtest/gtest.h"
#include "tc_ut_rs_ctx.h"
#include "tc_ut_rs_ub.h"

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
    }
};


//NORMAL TC
TEST_M(RS, tc_rs_init);
// TEST_M(RS, tc_rs_socket_init);
TEST_M(RS, tc_rs_socket_listen);
TEST_M(RS, tc_rs_socket_listen_ipv6);
// TEST_M(RS, tc_rs_socket_connect02);

// TEST_M(RS, tc_rs_get_tsqp_depth);
// TEST_M(RS, tc_rs_set_tsqp_depth);
TEST_M(RS, tc_rs_qp_create);
TEST_M(RS, tc_rs_qp_create_with_attrs);
TEST_M(RS, tc_rs_typical_register_mr);
TEST_M(RS, tc_rs_typical_qp_modify);
TEST_M(RS, tc_rs_mem_pool);
TEST_M(RS, tc_rs_get_qp_status);
TEST_M(RS, tc_rs_get_notify_ba);
TEST_M(RS, tc_rs_setup_sharemem);

TEST_M(RS, tc_rs_mr_create);
TEST_M(RS, tc_rs_mr_abnormal);


// TEST_M(RS, tc_rs_cq_handle);
// TEST_M(RS, tc_rs_buf_print);

TEST_M(RS, tc_rs_send_wr);
TEST_M(RS, tc_rs_send_wrlist_normal);
TEST_M(RS, tc_rs_send_wrlist_exp);
// TEST_M(RS, tc_rs_post_recv);

//ABNORMAL TC
// TEST_M(RS, tc_rs_abnormal);
TEST_M(RS, tc_rs_abnormal2);

// TEST_M(RS, tc_rs_get_sockets);
TEST_M(RS, tc_rs_socket_ops);

TEST_M(RS, tc_rs_deinit);
// TEST_M(RS, tc_rs_socket_deinit);

// TEST_M(RS, tc_rs_dfx);
// TEST_M(RS, tc_rs_mr_sync);
// TEST_M(RS, tc_rs_ssl_test1);
// TEST_M(RS, tc_rs_ssl_test2); 
TEST_M(RS, tc_rs_get_ifaddrs);
TEST_M(RS, tc_rs_get_ifaddrs1);
TEST_M(RS, tc_rs_get_ifaddrs_v2);
TEST_M(RS, tc_rs_get_ifnum);
// TEST_M(RS, tc_rs_get_interface_version);
TEST_M(RS, tc_tls_abnormal);

// TEST_M(RS, tc_rs_socket_deinit1);
TEST_M(RS, tc_rs_socket_deinit2);
TEST_M(RS, tc_rs_tls_inner_enable);
TEST_M(RS, tc_rs_ssl_ca_ky_init);
TEST_M(RS, tc_rs_rs_ssl_crl_init);
TEST_M(RS, tc_rs_tls_peer_cert_verify);
TEST_M(RS, tc_rs_ssl_err_string);
TEST_M(RS, tc_tls_get_cert_chain);
TEST_M(RS, tc_rs_ssl_skid_get_from_chain);
TEST_M(RS, tc_rs_ssl_get_cert);
TEST_M(RS, tc_rs_ssl_X509_store_init);
TEST_M(RS, tc_rs_ssl_skids_subjects_get);
TEST_M(RS, tc_rs_ssl_X509_store_add_cert);
TEST_M(RS, tc_rs_ssl_put_cert_ca_pem);
TEST_M(RS, tc_rs_ssl_put_cert_end_pem);
TEST_M(RS, tc_rs_check_pridata);
TEST_M(RS, tc_rs_peer_get_ifaddrs);
TEST_M(RS, tc_rs_get_device_type);
TEST_M(RS, tc_rs_peer_get_ifnum);
TEST_M(RS, tc_rs_white_list);
TEST_M(RS, tc_rs_socket_fill_wlist_by_phyID);
TEST_M(RS, tc_dl_hal_set_clear_user_config);
// TEST_M(RS, tc_rs_socket_nodeid2vnic);
// TEST_M(RS, tc_rs_server_valid_async_init);

// TEST_M(RS, tc_rs_set_fd_nonblock);
// TEST_M(RS, tc_rs_epoll_event_ssl_listen_in_handle);
// TEST_M(RS, tc_rs_ssl_recv_tag_in_handle);
TEST_M(RS, tc_rs_socket_listen_bind_listen);
TEST_M(RS, tc_rs_server_send_wlist_check_result);

// TEST_M(RS, tc_rs_recv_wrlist);
// TEST_M(RS, tc_rs_drv_post_recv);
// TEST_M(RS, tc_rs_drv_reg_notify_mr);
// TEST_M(RS, tc_rs_drv_query_notify_and_alloc_pd);
TEST_M(RS, tc_rs_send_normal_wrlist);
TEST_M(RS, tc_rs_connect_handle);
TEST_M(RS, tc_rs_roce_user_api_init);
TEST_M(RS, tc_rs_notify_cfg_set);
TEST_M(RS, tc_rs_drv_send_exp);
TEST_M(RS, tc_rs_drv_normal_qp_create_init);
TEST_M(RS, tc_rs_drv_exp_qp_create);
TEST_M(RS, tc_rs_drv_exp_qp_create_init);
TEST_M(RS, tc_rs_qpcb_init);
//TEST_M(RS, tc_rs_qp_query_info);
TEST_M(RS, tc_rs_register_mr);
TEST_M(RS, tc_rs_epoll_ctl_add);
TEST_M(RS, tc_rs_epoll_ctl_add_01);
TEST_M(RS, tc_rs_epoll_ctl_add_03);
TEST_M(RS, tc_rs_epoll_ctl_mod);
TEST_M(RS, tc_rs_epoll_ctl_mod_01);
TEST_M(RS, tc_rs_epoll_ctl_mod_02);
TEST_M(RS, tc_rs_epoll_ctl_mod_03);
TEST_M(RS, tc_rs_epoll_ctl_del);
TEST_M(RS, tc_rs_epoll_ctl_del_01);
TEST_M(RS, tc_rs_set_tcp_recv_callback);
TEST_M(RS, tc_rs_epoll_event_in_handle);
TEST_M(RS, tc_rs_epoll_tcp_recv);
TEST_M(RS, tc_rs_drv_poll_cq_handle);
TEST_M(RS, tc_rs_send_exp_wrlist);

TEST_M(RS, tc_rs_get_qp_context);
TEST_M(RS, tc_rs_normal_qp_create);
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
TEST_M(RS, tc_ssl_verify_callback);
TEST_M(RS, tc_rs_get_vnic_ip_info);
TEST_M(RS, tc_rs_ssl_verify_cert);
TEST_M(RS, tc_rs_socket_get_bind_by_chip);
TEST_M(RS, tc_rs_get_host_rdev_index);
TEST_M(RS, tc_rs_socket_batch_abort);
TEST_M(RS, tc_rs_socket_send_and_recv_log_test);
TEST_M(RS, tc_rs_tcp_recv_tag_in_handle);
TEST_M(RS, tc_rs_server_valid_async_abnormal);
TEST_M(RS, tc_rs_server_valid_async_abnormal_01);

TEST_M(RS, tc_rs_ub_get_rdev_cb);
TEST_M(RS, tc_rs_urma_api_init_abnormal);
TEST_M(RS, tc_rs_ub_v2);
TEST_M(RS, tc_rs_ub_get_dev_eid_info_num);
TEST_M(RS, tc_rs_ub_get_dev_eid_info_list);
TEST_M(RS, tc_rs_ub_ctx_token_id_alloc);
TEST_M(RS, tc_rs_ub_ctx_jfce_create);
TEST_M(RS, tc_rs_ub_ctx_jfc_create);
TEST_M(RS, tc_rs_ub_ctx_jetty_create);
TEST_M(RS, tc_rs_ub_ctx_jetty_import);
TEST_M(RS, tc_rs_ub_ctx_jetty_bind);
TEST_M(RS, tc_rs_ub_ctx_batch_send_wr);
TEST_M(RS, tc_rs_ub_free_cb_list);
TEST_M(RS, tc_rs_ub_ctx_ext_jetty_create);
TEST_M(RS, tc_rs_ub_ctx_rmem_import);

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

/* pingMesh ut cases */
TEST_M(RS, tc_rs_payload_header_resv_custom_check);
TEST_M(RS, tc_rs_ping_handle_init);
TEST_M(RS, tc_rs_ping_handle_deinit);
TEST_M(RS, tc_rs_ping_init);
TEST_M(RS, tc_rs_ping_target_add);
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
TEST_M(RS, tc_rs_ping_cb_get_dev_rdev_index);
TEST_M(RS, tc_rs_ping_init_mr_cb);
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

TEST_M(RS, tc_rs_get_cur_time);

TEST_M(RS, tc_rs_abnormal_ping_handle_init);
TEST_M(RS, tc_rs_abnormal_ping_handle_deinit);
TEST_M(RS, tc_rs_abnormal_ping_init);
TEST_M(RS, tc_rs_abnormal_ping_target_add);
TEST_M(RS, tc_rs_abnormal_ping_task_start);
TEST_M(RS, tc_rs_abnormal_ping_get_results);
TEST_M(RS, tc_rs_abnormal_ping_task_stop);
TEST_M(RS, tc_rs_abnormal_ping_target_del);
TEST_M(RS, tc_rs_abnormal_ping_deinit);
TEST_M(RS, tc_rs_ping_urma_check_fd);
TEST_M(RS, tc_rs_abnormal_ping_cb_get_ib_ctx_and_index);

// others
TEST_M(RS, tc_rs_peer_socket_recv);
TEST_M(RS, tc_rs_socket_get_server_socket_err_info);
TEST_M(RS, tc_rs_socket_accept_credit_add);
TEST_M(RS, tc_rs_epoll_event_ssl_recv_tag_in_handle);
TEST_M(RS, tc_rs_remap_mr);
TEST_M(RS, tc_rs_roce_get_api_version);
TEST_M(RS, tc_rs_get_tls_enable);
TEST_M(RS, tc_rs_get_sec_random);

TEST_M(RS, tc_rs_tlv_init);
TEST_M(RS, tc_rs_tlv_deinit);
TEST_M(RS, tc_rs_tlv_request);
TEST_M(RS, tc_rs_nslb_init);
TEST_M(RS, tc_rs_nslb_deinit);
TEST_M(RS, tc_rs_nslb_request);
TEST_M(RS, tc_rs_tlv_assemble_send_data);
TEST_M(RS, tc_rs_get_nslb_cb);
TEST_M(RS, tc_rs_epoll_nslb_event_handle);
TEST_M(RS, tc_rs_nslb_api_init);

TEST_M(RS, tc_rs_get_tp_info_list);
TEST_M(RS, tc_rs_ub_ctx_drv_jetty_import);
TEST_M(RS, tc_rs_free_dev_list);
TEST_M(RS, tc_rs_free_rdev_list);
TEST_M(RS, tc_rs_free_udev_list);
