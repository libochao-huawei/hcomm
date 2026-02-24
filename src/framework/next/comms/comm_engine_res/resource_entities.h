/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef RESOURCE_ENTITIES_H
#define RESOURCE_ENTITIES_H

#include "hccl_res.h"

typedef enum {
    THREAD_TYPE_INVALID = -1,
    THREAD_TYPE_CPU = 0,
    THREAD_TYPE_TS = 1,
} ThreadType;

typedef enum {
    NOTIFY_TYPE_INVALID = -1,
    NOTIFY_TYPE_HOST_MEM = 0,
} NotifyType;

typedef struct {
    uint64_t addr;  // on HBM
    uint64_t head;
    uint64_t tail;
    uint32_t msgSize;
    uint32_t capacity;
} QueueInfo;

typedef struct {
    NotifyType type;
    uint64_t notifyIdentifier;  // Host 侧为映射到 Device 侧的 notify_dVA
} NotifyEntity;

/**
 * @brief ThreadHandle 线程实体结构体（Device 侧）
 * @note 申请内存大小: sizeof(ThreadENtity) + notifyNum * sizeof(NotifyEntity)
 */
typedef struct {
    ThreadType type;
    CommEngine engine;
    union {
        struct {
            QueueInfo sendQueue;
            // QueueInfo completionQueue;  // TODO: for DFX, not planned for now.
            ThreadServiceHandle waitService;
            ThreadServiceHandle notifyService;
        } cpuRes;
        ThreadHandle threadObj;
        uint32_t raws[128];
    };
    uint32_t notifyNum;
    NotifyEntity notifies[0];  // 变长数据区
} ThreadEntity;

#endif // RESOURCE_ENTITIES_H
