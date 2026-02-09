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

#define rs_ut_msg(fmt, args...)	fprintf(stderr, "\t>>>>> " fmt, ##args)

void tc_rs_abnormal();
void tc_rs_abnormal2();
void tc_rs_init();
void tc_rs_socket_listen();
void tc_rs_socket_listen_ipv6();
void tc_rs_socket_connect();
void tc_rs_get_sockets();

void tc_rs_get_tsqp_depth();
void tc_rs_set_tsqp_depth();
void tc_rs_qp_create();
void tc_rs_qp_create_with_attrs();
void tc_rs_get_qp_status();
void tc_rs_get_notify_ba();
void tc_rs_setup_sharemem();
void tc_rs_send_wr();
void tc_rs_send_wrlist_normal();
void tc_rs_send_wrlist_exp();
void tc_rs_get_sq_index();
void tc_rs_post_recv();
void tc_rs_mem_pool();

void tc_rs_mr_create();
void tc_rs_mr_abnormal();

void tc_rs_cq_handle();
void tc_rs_socket_ops();

void tc_rs_dfx();

void tc_rs_socket_init();
void tc_rs_deinit();
void tc_rs_deinit2();
void tc_rs_socket_deinit1();
void tc_rs_socket_deinit2();
void tc_rs_conn_server_create();
void tc_rs_conn_server_create_1();
void tc_rs_conn_server_create_2();
void tc_rs_conn_server_create_3();
void tc_rs_white_list();
void tc_rs_ssl_test1();
void tc_rs_get_ifaddrs();
void tc_rs_get_ifaddrs_v2();
void tc_rs_peer_get_ifaddrs();
void tc_rs_get_ifnum();
void tc_rs_get_interface_version();
void tc_rs_get_cur_time();
void tc_RsRdev2rdevCb();
void tc_rs_compare_ip_gid();
void tc_rs_query_gid();
void tc_rs_get_host_rdev_index();
void tc_rs_get_ib_ctx_and_rdev_index();
void tc_rs_rdev_cb_init();
void tc_rs_rdev_init();
void tc_rs_ssl_free();
void tc_rs_drv_connect();
void tc_rs_qpn2qpcb();
void tc_rs_server_valid_async_init();
void tc_rs_drv_post_recv();
void tc_rs_ssl_deinit();
void tc_rs_tls_inner_enable();
void tc_rs_ssl_inner_init();
void tc_rs_ssl_ca_ky_init();
void tc_rs_ssl_crl_init();
void tc_rs_check_pridata();
void tc_rs_ssl_load_ca();
void tc_rs_ssl_get_ca_data();
void tc_rs_ssl_get_ca_data_1();
void tc_rs_ssl_get_crl_data_1();
void tc_rs_ssl_put_certs();
void tc_rs_ssl_check_mng_and_cert_chain();
void tc_rs_ssl_check_cert_chain();
void tc_rs_ssl_skid_get_from_chain();
void tc_rs_ssl_verify_cert_chain();
void tc_tls_get_cert_chain();
void tc_rs_ssl_get_leaf_cert();
void tc_tls_load_cert();
void tc_rs_tls_peer_cert_verify();
void tc_rs_ssl_err_string();
void tc_rs_server_send_wlist_check_result();
void tc_rs_server_valid_async_init();
void tc_rs_epoll_event_ssl_accept_in_handle();
void tc_rs_drv_ssl_bind_fd();
void tc_rs_socket_fill_wlist_by_phyID();
void tc_rs_get_vnic_ip();
void tc_rs_notify_cfg_set();
void tc_rs_notify_cfg_get();
void tc_crypto_decrypt_with_aes_gcm();
void tc_rs_listen_invalid_port();
void tc_rs_drv_qp_normal_fail();
void tc_RsApiInit();
void tc_rs_recv_wrlist();
void tc_rs_drv_post_recv();
void tc_rs_drv_reg_notify_mr();
void tc_rs_drv_query_notify_and_alloc_pd();
void tc_rs_send_normal_wrlist();
void tc_rs_drv_send_exp();
void tc_rs_drv_normal_qp_create_init();
void tc_rs_register_mr();
void tc_rs_epoll_ctl_add();
void tc_rs_epoll_ctl_add_01();
void tc_rs_epoll_ctl_add_02();
void tc_rs_epoll_ctl_add_03();
void tc_rs_epoll_ctl_mod();
void tc_rs_epoll_ctl_mod_01();
void tc_rs_epoll_ctl_mod_02();
void tc_rs_epoll_ctl_mod_03();
void tc_rs_epoll_ctl_del();
void tc_rs_epoll_ctl_del_01();
void tc_rs_set_tcp_recv_callback();
void tc_rs_epoll_event_in_handle();
void tc_rs_epoll_event_in_handle_01();
void tc_rs_socket_listen_bind_listen();
void tc_rs_epoll_tcp_recv();
void tc_rs_send_exp_wrlist();
void tc_rs_drv_poll_cq_handle();
void tc_rs_normal_qp_create();
void tc_rs_query_event();
void tc_rs_create_cq();
void tc_rs_create_normal_qp();
void tc_rs_create_comp_channel();
void tc_rs_get_cqe_err_info();
void tc_rs_get_cqe_err_info_num();
void tc_rs_get_cqe_err_info_list();
void tc_rs_save_cqe_err_info();
void tc_rs_cqe_callback_process();
void tc_rs_create_srq();
void tc_rs_get_ipv6_scope_id();
void tc_rs_create_event_handle();
void tc_rs_ctl_event_handle();
void tc_rs_wait_event_handle();
void tc_rs_destroy_event_handle();
void tc_rs_epoll_create_epollfd();
void tc_rs_epoll_destroy_fd();
void tc_rs_epoll_wait_handle();
void tc_ssl_verify_callback();
void tc_rs_ssl_verify_cert();
void tc_rs_get_vnic_ip_info();
void tc_rs_remap_mr();
void tc_RsRoceGetApiVersion();
void tc_rs_get_tls_enable();
void tc_rs_get_sec_random();
void tc_rs_get_hccn_cfg();
void tc_dl_hal_set_clear_user_config();

void tc_rs_typical_register_mr();
void tc_rs_typical_qp_modify();

void tc_rs_ssl_get_cert();
void tc_rs_ssl_X509_store_init();
void tc_rs_ssl_skids_subjects_get();
void tc_rs_ssl_put_cert_ca_pem();
void tc_rs_ssl_put_cert_end_pem();
void tc_rs_ssl_check_mng_and_cert_chain();
void tc_rs_remove_certs();
void tc_rs_ssl_X509_store_add_cert();
void tc_rs_peer_socket_recv();
void tc_rs_socket_get_server_socket_err_info();
void tc_rs_socket_accept_credit_add();
void tc_rs_epoll_event_ssl_recv_tag_in_handle();
void tc_rs_free_dev_list(void);
void tc_rs_free_rdev_list(void);
void tc_rs_free_udev_list(void);
void tc_rs_retry_timeout_exception_check();

#endif
