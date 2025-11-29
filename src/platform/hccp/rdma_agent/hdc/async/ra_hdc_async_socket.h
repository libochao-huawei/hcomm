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

int RaHdcSocketSendAsync(const struct socket_hdc_info *fdHandle, const void *data, unsigned long long size,
    unsigned long long *sentSize, void **reqHandle);
void RaHdcAsyncHandleSocketSend(struct ra_request_handle *reqHandle);
int RaHdcSocketRecvAsync(const struct socket_hdc_info *fdHandle, void *data, unsigned long long size,
    unsigned long long *receivedSize, void **reqHandle);
void RaHdcAsyncHandleSocketRecv(struct ra_request_handle *reqHandle);
int RaHdcSocketListenStartAsync(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num,
    void **reqHandle);
void RaHdcAsyncHandleSocketListenStart(struct ra_request_handle *reqHandle);
int RaHdcSocketListenStopAsync(unsigned int phyId, struct socket_listen_info_t conn[], unsigned int num,
    void **reqHandle);
int RaHdcSocketBatchConnectAsync(unsigned int phyId, struct socket_connect_info_t conn[], unsigned int num,
    void **reqHandle);
int RaHdcSocketBatchCloseAsync(unsigned int phyId, struct socket_close_info_t conn[], unsigned int num,
    void **reqHandle);
void RaHdcAsyncHandleSocketBatchClose(struct ra_request_handle *reqHandle);
#endif // RA_ASYNC_SOCKET_H
