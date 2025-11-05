/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ASYNC_H
#define RA_ASYNC_H

#include "ra.h"
#include "ra_rs_comm.h"

struct ra_request_handle {
    unsigned int req_id;
    struct ra_async_op_handle *op_handle;
    unsigned int phy_id;
    unsigned int data_size;

    void *recv_buf;
    unsigned int recv_len;
    int op_ret;
    bool is_done;

    unsigned int dev_index;
    void *priv_data;
    void *priv_handle;

    struct ra_list_head list;
};

struct ra_async_op_handle {
    enum op_type opcode;
    enum module_type op_module;
    void (*priv_data_handle)(struct ra_request_handle *);
    unsigned int data_size;
};
#endif // RA_ASYNC_H
