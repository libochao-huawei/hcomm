/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DL_NETCO_FUNCTION_H
#define DL_NETCO_FUNCTION_H

#include "netco_api.h"

struct rs_netco_ops {
    void *(*rs_netco_init)(int epollfd, NetCoIpPortArg ipPortArg);
    void (*rs_netco_deinit)(void *co);
    unsigned int (*rs_netco_event_dispatch)(void *co, int fd, unsigned int curEvents);
    int (*rs_netco_tbl_add_upd)(void *netco_handle, unsigned int type, char *data, unsigned int data_len);
};

void rs_nslb_api_deinit(void);
int rs_nslb_api_init(void);
void *rs_netco_init(int epollfd, NetCoIpPortArg ipPortArg);
void rs_netco_deinit(void *co);
unsigned int rs_netco_event_dispatch(void *co, int fd, unsigned int curEvents);
int rs_netco_tbl_add_upd(void *netco_handle, unsigned int type, char *data, unsigned int data_len);
#endif // DL_NETCO_FUNCTION_H
