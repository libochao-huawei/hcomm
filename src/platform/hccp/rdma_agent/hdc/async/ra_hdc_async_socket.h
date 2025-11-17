/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ASYNC_SOCKET_H
#define RA_ASYNC_SOCKET_H

#include "hccp_common.h"
#include "ra_async.h"
#include "ra_hdc.h"

struct ra_response_socket_recv {
    void *data;
    unsigned long long size;
    unsigned long long *received_size;
};

struct ra_response_socket_listen {
    struct socket_listen_info_t *conn;
    unsigned int num;
};

struct ra_response_socket_batch_close {
    struct socket_close_info_t *conn;
    unsigned int num;
};

int ra_hdc_socket_send_async(const struct socket_hdc_info *fd_handle, const void *data, unsigned long long size,
    unsigned long long *sent_size, void **req_handle);
void ra_hdc_async_handle_socket_send(struct ra_request_handle *req_handle);
int ra_hdc_socket_recv_async(const struct socket_hdc_info *fd_handle, void *data, unsigned long long size,
    unsigned long long *received_size, void **req_handle);
void ra_hdc_async_handle_socket_recv(struct ra_request_handle *req_handle);
int ra_hdc_socket_listen_start_async(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle);
void ra_hdc_async_handle_socket_listen_start(struct ra_request_handle *req_handle);
int ra_hdc_socket_listen_stop_async(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    void **req_handle);
int ra_hdc_socket_batch_connect_async(unsigned int phy_id, struct socket_connect_info_t conn[], unsigned int num,
    void **req_handle);
int ra_hdc_socket_batch_close_async(unsigned int phy_id, struct socket_close_info_t conn[], unsigned int num,
    void **req_handle);
void ra_hdc_async_handle_socket_batch_close(struct ra_request_handle *req_handle);
#endif // RA_ASYNC_SOCKET_H
