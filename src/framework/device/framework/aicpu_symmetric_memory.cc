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
#include "symmetric_memory/symmetric_memory.h"

using namespace hccl;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

HcclResult HcommSymWinGetPeerPointer(CommSymWindow winHandle, size_t offset, int peerRank, void* ptr)
{
    SymmetricWindow *symWin = reinterpret_cast<SymmetricWindow *>(winHandle);
    size_t peerOffset = peerRank * symWin->stride + offset;
    ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(symWin->baseVa) + peerOffset);
    HCCL_INFO("[HcommSymWinGetPeerPointer] Get Ptr[%p] from winHandle[%p], rank[%d], peerOffset[%llu]", ptr, winHandle, peerRank, peerOffset);
    return HCCL_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus