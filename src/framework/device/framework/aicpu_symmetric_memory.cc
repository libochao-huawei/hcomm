/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_symmetric_memory.h"

namespace hccl {

class SymmetricMemory::SimpleVaAllocator {
    public:
        // 不需要任何成员，只要让编译器觉得它是个完整的类就行
        SimpleVaAllocator() {} 
        ~SimpleVaAllocator() {}
};

SymmetricMemory::~SymmetricMemory() {}

void *HcclGetSymPtr(HcclWindow winHandle, int32_t peerRank, size_t offset)
{
    SymmetricWindow *symWin = reinterpret_cast<SymmetricWindow *>(winHandle);
    size_t peerOffset = peerRank * symWin->stride + offset;
    void *peerVa = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(symWin->baseVa) + peerOffset);
    HCCL_INFO("[HcclGetSymPtr] Get Ptr[%p] from winHandle[%p], rank[%d], peerOffset[%llu]", peerVa, winHandle, peerRank, peerOffset);
    return peerVa;
}

}