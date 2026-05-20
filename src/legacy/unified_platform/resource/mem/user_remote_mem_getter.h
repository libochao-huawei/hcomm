/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef USER_REMOTE_MEM_GETTER_H
#define USER_REMOTE_MEM_GETTER_H

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <functional>
#include "hccl/hccl_res.h"
#include "hccl/hccl_types.h"
#include "log.h"

constexpr uint32_t MAX_BUFFER_NUM = 30000;

namespace Hccl {

template <typename T>
struct RemoteMemCtx{
    uint32_t                        memCount = 0;
    bool                            &cacheValid;
    std::vector<T>                  &rmtBufferVec;
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &remoteMemTag;
    std::vector<HcclMem>            &remoteMems;
    std::vector<std::string>        &tagCopies;
    std::vector<char*>              &tagPointers;
    std::function<void(RemoteMemCtx<T> &remoteMemCtx, uint32_t index)> cacheBuilder;
    HcclMem                         **remoteMem;
    uint32_t                        *memNum;
    char                            ***memTags;

    RemoteMemCtx(uint32_t memCount, bool &cacheValid, std::vector<T> &rmtBufferVec,
        std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &remoteMemTag,
        std::vector<HcclMem> &remoteMems, std::vector<std::string> &tagCopies,
        std::vector<char*> &tagPointers,
        std::function<void(RemoteMemCtx<T> &remoteMemCtx, uint32_t index)> cacheBuilder,
        HcclMem **remoteMem, uint32_t *memNum, char ***memTags) :
        memCount(memCount), cacheValid(cacheValid), rmtBufferVec(rmtBufferVec),
        remoteMemTag(remoteMemTag), remoteMems(remoteMems),
        tagCopies(tagCopies), tagPointers(tagPointers), cacheBuilder(cacheBuilder),
        remoteMem(remoteMem), memNum(memNum), memTags(memTags)
    {};
};

template<typename T>
HcclResult GetRemoteUserMems(RemoteMemCtx<T> &remoteMemCtx)
{
    CHK_PRT_RET(!remoteMemCtx.remoteMem, HCCL_ERROR("[GetRemoteUserMems] remoteMem is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!remoteMemCtx.memTags, HCCL_ERROR("[GetRemoteUserMems] memTags is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!remoteMemCtx.memNum, HCCL_ERROR("[GetRemoteUserMems] memNum is nullptr"), HCCL_E_PARA);
    *(remoteMemCtx.remoteMem) = nullptr;
    *(remoteMemCtx.memTags) = nullptr;
    *(remoteMemCtx.memNum) = 0;
    if (remoteMemCtx.memCount == 0) {
        HCCL_INFO("[GetRemoteUserMems] No memory found");
        return HCCL_SUCCESS;
    }
    // 检查是否有缓存
    if (!remoteMemCtx.cacheValid) {
        remoteMemCtx.remoteMems.resize(remoteMemCtx.memCount);
        remoteMemCtx.tagCopies.clear();
        remoteMemCtx.tagCopies.reserve(remoteMemCtx.memCount);
        remoteMemCtx.tagPointers.clear();
        remoteMemCtx.tagPointers.reserve(remoteMemCtx.memCount);
        for (uint32_t i = 0; i < remoteMemCtx.memCount; ++i) {
            remoteMemCtx.cacheBuilder(remoteMemCtx, i);
            const char* src = remoteMemCtx.remoteMemTag[i].data();
            std::string tagCopy(src, strnlen(src, HCCL_RES_TAG_MAX_LEN));
            remoteMemCtx.tagCopies.push_back(std::move(tagCopy));
            remoteMemCtx.tagPointers.push_back(const_cast<char*>(remoteMemCtx.tagCopies.back().c_str()));
        }
        remoteMemCtx.cacheValid = true;
    }
    *(remoteMemCtx.remoteMem) = remoteMemCtx.remoteMems.data();
    *(remoteMemCtx.memTags) = remoteMemCtx.tagPointers.data();
    *(remoteMemCtx.memNum) = remoteMemCtx.memCount;
    return HCCL_SUCCESS;
}
} // namespace Hccl

#endif // USER_REMOTE_MEM_GETTER_H