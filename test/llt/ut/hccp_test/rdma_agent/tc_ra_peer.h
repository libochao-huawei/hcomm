/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra peer unit test.
 * Author:
 * Create: 2020-07-21
 */
#ifndef TC_RA_PEER_H
#define TC_RA_PEER_H
void tc_peer();
void tc_peer_fail();
void tc_ra_peer_epoll_ctl_add();
void tc_ra_peer_set_tcp_recv_callback();
void tc_ra_peer_epoll_ctl_mod();
void tc_ra_peer_epoll_ctl_del();
void tc_ra_peer_cq_create();
void tc_ra_peer_normal_qp_create();
void tc_ra_peer_create_event_handle();
void tc_ra_peer_ctl_event_handle();
void tc_ra_peer_wait_event_handle();
void tc_ra_peer_destroy_event_handle();
void tc_ra_peer_socket_batch_abort();
void tc_ra_loopback_qp_create();
void tc_ra_peer_loopback_qp_create();
void tc_ra_peer_loopback_single_qp_create();
#endif