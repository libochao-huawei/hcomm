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
    uint32_t                        userMemCount = 0;
    bool                            &cacheValid;
    std::vector<T>                  &rmtBufferVec;
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &remoteUserMemTag;
    std::vector<CommMem>            &remoteUserMems;
    std::vector<std::string>        &tagCopies;
    std::vector<char*>              &tagPointers;
    std::function<void(RemoteMemCtx<T> &remoteMemCtx, uint32_t index)> cacheBuilder;
    CommMem                         **remoteMem;
    char                            ***memTags;
    uint32_t                        *memNum;

    RemoteMemCtx(uint32_t userMemCount, bool &cacheValid, std::vector<T> &rmtBufferVec,
        std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &remoteUserMemTag,
        std::vector<CommMem> &remoteUserMems, std::vector<std::string> &tagCopies,
        std::vector<char*> &tagPointers,
        std::function<void(RemoteMemCtx<T> &remoteMemCtx, uint32_t index)> cacheBuilder,
        CommMem **remoteMem, char ***memTags, uint32_t *memNum) :
        userMemCount(userMemCount), cacheValid(cacheValid), rmtBufferVec(rmtBufferVec),
        remoteUserMemTag(remoteUserMemTag), remoteUserMems(remoteUserMems),
        tagCopies(tagCopies), tagPointers(tagPointers), cacheBuilder(cacheBuilder),
        remoteMem(remoteMem), memTags(memTags), memNum(memNum)
    {};
};

template<typename T>
HcclResult GetRemoteUserMem(RemoteMemCtx<T> &remoteMemCtx)
{
    CHK_PRT_RET(!remoteMemCtx.remoteMem, HCCL_ERROR("[GetUserRemoteMem] remoteMem is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!remoteMemCtx.memTags, HCCL_ERROR("[GetUserRemoteMem] memTags is nullptr"), HCCL_E_PARA);
    CHK_PRT_RET(!remoteMemCtx.memNum, HCCL_ERROR("[GetUserRemoteMem] memNum is nullptr"), HCCL_E_PARA);
    *(remoteMemCtx.remoteMem) = nullptr;
    *(remoteMemCtx.memTags) = nullptr;
    *(remoteMemCtx.memNum) = 0;
    if (remoteMemCtx.userMemCount == 0) {
        HCCL_INFO("[GetUserRemoteMem] No user remote memory found");
        return HCCL_SUCCESS;
    }
    // 检查是否有缓存
    if (!remoteMemCtx.cacheValid) {
        remoteMemCtx.remoteUserMems.resize(remoteMemCtx.userMemCount);
        remoteMemCtx.tagCopies.clear();
        remoteMemCtx.tagCopies.reserve(remoteMemCtx.userMemCount);
        remoteMemCtx.tagPointers.clear();
        remoteMemCtx.tagPointers.reserve(remoteMemCtx.userMemCount);
        for (uint32_t i = 0; i < remoteMemCtx.userMemCount; ++i) {
            remoteMemCtx.cacheBuilder(remoteMemCtx, i);
            const char* src = remoteMemCtx.remoteUserMemTag[i + 1].data();
            std::string tagCopy(src, strnlen(src, HCCL_RES_TAG_MAX_LEN));
            remoteMemCtx.tagCopies.push_back(std::move(tagCopy));
            remoteMemCtx.tagPointers.push_back(const_cast<char*>(remoteMemCtx.tagCopies.back().c_str()));
        }
        remoteMemCtx.cacheValid = true;
    }
    *(remoteMemCtx.remoteMem) = remoteMemCtx.remoteUserMems.data();
    *(remoteMemCtx.memTags) = remoteMemCtx.tagPointers.data();
    *(remoteMemCtx.memNum) = remoteMemCtx.userMemCount;
    return HCCL_SUCCESS;
}
} // namespace Hccl

#endif // USER_REMOTE_MEM_GETTER_H