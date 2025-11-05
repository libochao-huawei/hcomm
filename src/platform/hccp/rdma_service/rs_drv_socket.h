/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_DRV_SOCKET_H
#define RS_DRV_SOCKET_H

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

#include "securec.h"
#include "rs.h"
#include "rs_inner.h"

int rs_drv_connect(int fd, struct rs_ip_addr_info *server_ip, struct rs_ip_addr_info *client_ip, uint16_t port);
int rs_drv_socket_send(int fd, const void *data, uint64_t size, int flags);
int rs_drv_socket_recv(int fd, void *data, uint64_t size, int flags);
int rs_drv_ssl_bind_fd(struct rs_conn_info *conn, int fd);
#endif // RS_DRV_SOCKET_H
