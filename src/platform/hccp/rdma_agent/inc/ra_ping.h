/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_PING_H
#define RA_PING_H

#include <pthread.h>
#include "hccp_ping.h"
#include "hccp_common.h"

struct ra_ping_handle {
    enum protocol_type protocol;
    union ping_dev dev;
    uint32_t buffer_size;

    struct ra_ping_ops *ping_ops;
    pthread_mutex_t mutex;
    uint32_t task_cnt;
    uint32_t target_cnt;

    uint32_t dev_index;
    unsigned int phy_id;
};

struct ra_ping_ops {
    int (*ra_ping_init)(struct ra_ping_handle *pingHandle, struct ping_init_attr *initAttr,
        struct ping_init_info *initInfo);
    int (*ra_ping_target_add)(struct ra_ping_handle *pingHandle, struct ping_target_info target[], uint32_t num);
    int (*ra_ping_task_start)(struct ra_ping_handle *pingHandle, struct ping_task_attr *attr);
    int (*ra_ping_get_results)(struct ra_ping_handle *pingHandle, struct ping_target_result target[], uint32_t *num);
    int (*ra_ping_target_del)(struct ra_ping_handle *pingHandle, struct ping_target_comm_info target[], uint32_t num);
    int (*ra_ping_task_stop)(struct ra_ping_handle *pingHandle);
    int (*ra_ping_deinit)(struct ra_ping_handle *pingHandle);
};
#endif // RA_PING_H
