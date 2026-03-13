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
#include "hccl_independent_common.h"

namespace hccl {

typedef struct {
    NotifyType type;
    uint64_t identifier;  // AicpuTs -> Cpu 场景下, type == DEVICE_MEM，为一块 device 能直接访问的地址
} NotifyEntity;

typedef struct {
    uint64_t addr;         // on HBM
    uint64_t headIdxAddr;  // on HBM, 指向 head 索引的地址
    uint64_t tailIdxAddr;  // on HBM, 指向 tail 索引的地址
    uint32_t msgSize;
    uint32_t capacity;
} QueueInfo;

/**
 * @note 申请内存大小: sizeof(ThreadEntity) + notifyNum * sizeof(NotifyEntity)
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
        } cpuRes;                // 如果是 AICPU Engine + CPU Type, 选择这个成员
        uint64_t threadObjAddr;  // 如果是 AICPU Engine + TS Type, 为 Device 侧的 AicpuTsThread 对象地址
                                 // 如果是 CPU Engine + TS Type, 为 Host 侧的 CpuTsThread 对象地址
        uint32_t raws[128];
    };
    uint32_t notifyNum;
    NotifyEntity notifies[0];  // 变长数据区

    std::string DescribeAttr() {
        return "engine=" + std::to_string(engine) + ", type=" + std::to_string(type);
    }
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

typedef struct {
    uint32_t msgId;
    uint64_t serviceHandle;
    void *args;  // -> RecordServiceArgs, WaitServiceArgs, or other custom args struct
    uint64_t argsSizeByte;
} ThreadMsgEntity;

} // namespace hccl

#endif // RESOURCE_ENTITIES_H
