/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra hdc unit test header file.
 * Author:
 * Create: 2020-07-21
 */
#ifndef TC_RA_HDC_H
#define TC_RA_HDC_H
#ifdef __cplusplus
extern "C" {
#endif
void tc_hdc();
void tc_hdc_init();
void tc_hdc_init_fail();
void tc_hdc_deinit_fail();
void tc_hdc_socket_batch_connect();
void tc_hdc_socket_batch_close();
void tc_hdc_socket_listen_start();
void tc_hdc_socket_batch_abort();
void tc_hdc_socket_listen_stop();
void tc_hdc_get_sockets();
void tc_hdc_socket_send();
void tc_hdc_socket_recv();
void tc_hdc_qp_create_destroy();
void tc_hdc_get_qp_status();
void tc_hdc_qp_connect_async();
void tc_hdc_mr_reg();
void tc_hdc_mr_dereg();
void tc_hdc_send_wr();
void tc_hdc_send_wrlist();
void tc_hdc_get_notify_base_addr();
void tc_hdc_socket_init();
void tc_hdc_socket_deinit();
void tc_hdc_rdev_init();
void tc_hdc_rdev_deinit();
void tc_hdc_socket_white_list_add();
void tc_hdc_socket_white_list_del();
void tc_hdc_get_ifaddrs();
void tc_hdc_get_ifaddrs_v2();
void tc_hdc_get_ifnum();
void tc_hdc_message_process_fail();
void tc_hdc_socket_recv_fail();
void tc_ra_hdc_send_wrlist_ext_init();
void tc_ra_hdc_send_wrlist_ext();
void tc_ra_hdc_send_normal_wrlist();
void tc_ra_hdc_set_qp_attr_qos();
void tc_ra_hdc_set_qp_attr_timeout();
void tc_ra_hdc_set_qp_attr_retry_cnt();
void tc_ra_hdc_get_cqe_err_info();
void tc_ra_hdc_get_cqe_err_info_list();
void tc_ra_hdc_qp_create_op();
void tc_ra_hdc_get_qp_status_op();
void tc_hdc_send_wr_op();
void tc_hdc_lite_send_wr_op();
void tc_hdc_recv_wrlist();
void tc_hdc_poll_cq();
void tc_hdc_get_lite_support();
void tc_ra_rdev_get_support_lite();
void tc_ra_rdev_get_handle();
void tc_ra_is_first_or_last_used();
void tc_ra_hdc_lite_ctx_init();
void rc_ra_hdc_lite_qp_create();
void tc_ra_hdc_tlv_request();
void tc_ra_hdc_get_tlv_recv_msg();
#ifdef __cplusplus
}
#endif
#endif