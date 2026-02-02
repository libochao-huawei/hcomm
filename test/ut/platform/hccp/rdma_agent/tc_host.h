/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra peer unit test.
 * Author:
 * Create: 2020-07-21
 */
#ifndef TC_RA_HOST_H
#define TC_RA_HOST_H
extern "C" void tc_host();
extern "C" void tc_ifaddr();
extern "C" void tc_ra_recv_wrlist(void);
extern "C" void tc_host_ra_send_wrlist_ext();
extern "C" void tc_host_ra_send_normal_wrlist();
extern "C" void tc_ra_set_qp_attr_qos(void);
extern "C" void tc_ra_set_qp_attr_timeout(void);
extern "C" void tc_ra_set_qp_attr_retry_cnt(void);
extern "C" void tc_ra_create_event_handle(void);
extern "C" void tc_ra_ctl_event_handle(void);
extern "C" void tc_ra_wait_event_handle(void);
extern "C" void tc_ra_destroy_event_handle(void);
extern "C" void tc_ra_poll_cq(void);
extern "C" void tc_get_vnic_ip_infos(void);
extern "C" void tc_ra_socket_batch_abort(void);
extern "C" void tc_ra_get_client_socket_err_info(void);
extern "C" void tc_ra_get_server_socket_err_info(void);
extern "C" void tc_ra_socket_accept_credit_add(void);
extern "C" void tc_ra_remap_mr(void);
extern "C" void tc_ra_register_mr(void);
#endif