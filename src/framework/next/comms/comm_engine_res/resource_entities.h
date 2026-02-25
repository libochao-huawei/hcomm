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

namespace hccl {
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
    NotifyType type;
    union {
        uint64_t deviceVA; // type == HOST_MEM 时，为映射到 Device 侧的 notify_dVA
        uint64_t notifyId;
    } u;
} NotifyEntity;

typedef struct {
    uint64_t addr;  // on HBM
    uint64_t head;
    uint64_t tail;
    uint32_t msgSize;
    uint32_t capacity;
} QueueInfo;

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
            ThreadServiceHandle recordService;
        } cpuRes;
        ThreadHandle threadObjAddr;
        uint32_t raws[128];
    };
    uint32_t notifyNum;
    NotifyEntity notifies[0];  // 变长数据区
} ThreadEntity;

typedef struct {
    ThreadHandle threadHandle;
    ThreadHandle dstThreadHandle;
    uint32_t dstNotifyIdx;
} RecordServiceArgs;

typedef struct {
    ThreadHandle threadHandle;
    uint32_t notifyIdx;
} WaitServiceArgs;

} // namespace hccl

#endif // RESOURCE_ENTITIES_H
