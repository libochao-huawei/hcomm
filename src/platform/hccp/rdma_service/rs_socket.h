/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_SOCKET_H
#define RS_SOCKET_H
#include "rs_drv_socket.h"

#define RS_MAX_VNIC_NUM 16

int rs_inet_ntop(int family, union hccp_ip_addr *ip, char read_addr[], unsigned int len);
void rs_socket_save_err_info(int action, int err_no, struct socket_err_info *err_info);
#endif // RS_SOCKET_H
