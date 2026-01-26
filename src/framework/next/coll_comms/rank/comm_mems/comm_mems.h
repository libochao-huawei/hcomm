/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef COMM_MEMS_H
#define COMM_MEMS_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "hccl_types.h"
#include "log.h"
#include "hccl_mem_defs.h"

namespace hccl {
/**
 * @note 职责：集合通信域内MyRank的通信内存管理，包括HCCL Buffer和其他待注册到EndPoint内存
 */
class CommMems {
public:
    explicit CommMems(uint64_t bufferSize);
    // CommMems(void* addr,u32 size );
    ~CommMems() = default;

    HcclResult Add(void *addr, uint64_t len);

    HcclResult GetHcclBuffer(void *&addr, uint64_t &len);

    HcclResult Init(HcclMem cclBuffer);

    HcclResult GetMemoryHandles(std::vector<HcclMem> &mem);

private:
    uint64_t bufferSize_{};
    void*   addr_{nullptr};
    std::size_t size_{0};
    HcclMemType memType_{HcclMemType::HCCL_MEM_TYPE_DEVICE};
    // RmaBufferMgr<BufferKey<uintptr_t, uint64_t>, void*>;
};
}

#endif // COMM_MEMS_H
