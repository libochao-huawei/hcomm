/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_COMM_H
#define RA_COMM_H

#include "ra.h"
#include "hccp_common.h"
#include "ra_rs_comm.h"

#define RA_HDC_RECV_SEND_TIMEOUT    120000
#define RA_HDC_RETRY_SEND_TIMEOUT    10000
#define SOCKET_USE_PORT_BIT 31U
#define SOCKET_DISUSE_LINGER_BIT 31U

int RaGetSocketListenInfo(const struct socket_listen_info_t conn[], unsigned int num,
    struct socket_listen_info rsConn[], unsigned int rsNum);

int RaGetSocketListenResult(const struct socket_listen_info rsConn[], unsigned int rsNum,
    struct socket_listen_info_t conn[], unsigned int num);

int RaGetSocketConnectInfo(const struct socket_connect_info_t conn[], unsigned int num,
    struct socket_connect_info rsConn[], unsigned int rsNum);
#endif // RA_COMM_H
