/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_EPOLL_H
#define RS_EPOLL_H


#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "securec.h"
#include "rs.h"
#include "rs_inner.h"

int rs_epoll_connect_handle_init(struct rs_cb *rscb);
int rs_epoll_ctl(int epollfd, int op, int fd, unsigned int state);
int rs_epoll_ctl_fd_handle(int epollfd, int op, int fd, unsigned int state, void *fd_handle);
void rs_destroy_epoll(struct rs_cb *rs_cb);
int rs_epoll_create_epollfd(int *epollfd);
int rs_epoll_destroy_fd(int *fd);
int rs_epoll_wait_handle(int event_handle, struct epoll_event *events, int timeout, unsigned int maxevents,
    unsigned int *events_num);
int rs_epoll_event_listen_in_handle(struct rs_cb *rs_cb, int fd);
int rs_epoll_event_qp_mr_in_handle(struct rs_cb *rs_cb, int fd);
int rs_socket_copy_conn_info(struct rs_conn_info *conn_tmp, struct rs_conn_info *conn);
int rs_white_list_check_valid(unsigned int chip_id, struct rs_conn_cb *conn_cb, struct rs_conn_info *conn);
#endif // RS_EPOLL_H
