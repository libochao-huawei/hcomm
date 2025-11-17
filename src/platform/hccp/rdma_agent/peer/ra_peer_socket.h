/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_PEER_SOCKET_H
#define RA_PEER_SOCKET_H

#include "hccp_common.h"

int ra_peer_get_client_socket_err_info(unsigned int phy_id, struct socket_connect_info_t conn[],
    struct socket_err_info err[], unsigned int num);
int ra_peer_get_server_socket_err_info(unsigned int phy_id, struct socket_listen_info_t conn[],
    struct server_socket_err_info err[], unsigned int num);
int ra_peer_socket_accept_credit_add(unsigned int phy_id, struct socket_listen_info_t conn[], unsigned int num,
    unsigned int credit_limit);
#endif // RA_PEER_SOCKET_H
