/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_API_DATA_COMMON_H
#define HCCL_API_DATA_COMMON_H

#include <string>
#include <vector>

#include "dispatcher.h"
#include "launch_context.h"
#include "new/hccl_primitive_local.h"
#include "new/hccl_primitive_remote.h"

namespace hccl {
namespace dataapi {
inline void AddThreadCommon(LaunchContext &launchCtx, ThreadHandle thread)
{
    launchCtx.AddThread(thread);
}

inline bool IsSupportReduceCommon(HcommDataType dataType, HcommReduceOp op)
{
    bool checkDataType =
        (dataType == HCOMM_DATA_TYPE_FP32 || dataType == HCOMM_DATA_TYPE_FP16 || dataType == HCOMM_DATA_TYPE_INT8 ||
        dataType == HCOMM_DATA_TYPE_INT16 || dataType == HCOMM_DATA_TYPE_INT32 || dataType == HCOMM_DATA_TYPE_BFP16);
    bool checkReduceType = (op == HCOMM_REDUCE_SUM || op == HCOMM_REDUCE_MAX || op == HCOMM_REDUCE_MIN);
    return checkDataType && checkReduceType;
}

inline HcclResult CommTaskPrepareCommon(char *key, uint32_t keyLen)
{
    std::string keyStr = "temp_key";
    if (key != nullptr && keyLen != 0) {
        keyStr = std::string(key, keyLen);
        HCCL_DEBUG("[CommTaskPrepare]key[%s], keyLen[%u]", key, keyLen);
    } else {
        HCCL_DEBUG("[CommTaskPrepare]disable cache, key[0x%llx], keyLen[%u]", key, keyLen);
    }

    return HcclTaskPrepare(const_cast<char_t *>(keyStr.c_str()), keyStr.length());
}

inline HcclResult CommTaskLaunchByStreamsCommon(ThreadHandle *threads, uint32_t threadNum)
{
    CHK_PTR_NULL(threads);
    CHK_PRT_RET(threadNum < 1, HCCL_ERROR("[CommTaskLaunch]threadNum is less than 1"), HCCL_E_PARA);

    std::vector<hccl::Stream> streams;
    streams.reserve(threadNum);
    for (uint32_t i = 0; i < threadNum; i++) {
        hccl::Stream *stream = GetStream(threads[i]);
        CHK_PTR_NULL(stream);
        streams.push_back(*stream);
    }

    return HcclTaskLaunch(streams.data(), threadNum);
}

inline HcclResult CommFenceCommon(ThreadHandle thread, ChannelHandle channel)
{
    HCCL_DEBUG("[CommFence] thread[0x%llx], channel[0x%llx].", thread, channel);
    Stream *stream = GetStream(thread);
    CHK_PTR_NULL(stream);

    return HcclRemoteFence(stream, reinterpret_cast<void *>(channel), false);
}

inline int32_t HcommSetLaunchModeCommon(LaunchContext &launchCtx, const char *launchTag, HcommLaunchMode mode)
{
    HCCL_DEBUG("HcommSetLaunchMode launchTag[%s]", launchTag);
    return launchCtx.SetLaunchMode(launchTag, mode);
}

inline int32_t HcommBatchModeStartCommon(LaunchContext &launchCtx, const char *batchTag)
{
    return HcommSetLaunchModeCommon(launchCtx, batchTag, HCOMM_LAUNCH_MODE_BATCH);
}

inline int32_t HcommBatchModeEndCommon(LaunchContext &launchCtx, const char *batchTag)
{
    return HcommSetLaunchModeCommon(launchCtx, batchTag, HCOMM_LAUNCH_MODE_EAGER);
}
} // namespace dataapi
} // namespace hccl

#endif // HCCL_API_DATA_COMMON_H
