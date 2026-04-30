/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reg_mem_manager.h"

#include <cstdint>
#include <string>

#include "securec.h"
#include "log.h"
#include "comm_mems.h"
#include "my_rank.h"

namespace hccl {

HcclResult RegMemMgr::RegisterMemory(HcommMem mem, const char *memTag, HcommMemHandle *memHandle)
{
    CHK_PTR_NULL(memHandle);
    CHK_PTR_NULL(myRank_);
    CommMems *commMems = myRank_->GetCommMems();
    CHK_PTR_NULL(commMems);

    auto newStart = reinterpret_cast<uintptr_t>(mem.addr);
    auto newEnd = newStart + mem.size;

    for (const auto &entry : memInfoVec_) {
        const CommMem &cached = entry.second.mem;
        auto cachedStart = reinterpret_cast<uintptr_t>(cached.addr);
        auto cachedEnd = cachedStart + cached.size;
        // 完全包含（含相等），不允许部分交叉
        if (newStart >= cachedStart && newEnd <= cachedEnd) {
            *memHandle = entry.first;
            HCCL_INFO("[RegMemMgr][RegisterMemory] reuse handle[%p] for addr[%p] size[%llu] "
                "covered by cached addr[%p] size[%llu]",
                *memHandle, mem.addr, mem.size, cached.addr, cached.size);
            return HCCL_SUCCESS;
        }
    }

    void *rawHandle = nullptr;
    std::string tagStr = (memTag != nullptr) ? std::string(memTag) : std::string{};
    CHK_RET(commMems->CommRegMem(tagStr, mem, &rawHandle));
    *memHandle = static_cast<HcommMemHandle>(rawHandle);

    CommMemInfo info{};
    info.mem = mem;
    if (memTag != nullptr) {
        size_t tagLen = strnlen(memTag, HCOMM_RES_TAG_MAX_LEN - 1);
        if (tagLen > 0) {
            CHK_SAFETY_FUNC_RET(memcpy_s(info.memTag, sizeof(info.memTag), memTag, tagLen));
        }
    }
    memInfoVec_.emplace_back(*memHandle, info);
    HCCL_INFO("[RegMemMgr][RegisterMemory] registered handle[%p] for addr[%p] size[%llu] tag[%s]",
        *memHandle, mem.addr, mem.size, info.memTag);
    return HCCL_SUCCESS;
}

}
