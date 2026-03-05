/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ns_recovery.h"
#include "comms/api_c_adpt/hcomm_c_adpt.cc"


namespace hccl 
{

HcclResult HcommChannelClean(const ChannelHandle *channelList, uint32_t listNum)
{
    CHK_PTR_NULL(channelList);

    for (uint32_t i = 0; i < listNum; ++i) {
        const ChannelHandle inHandle = channelList[i];
        // 单锁：D2H 映射 + 查 map + 锁内调用 Clean()
        HcclResult ret = hcomm::WithChannelByHandleLocked(inHandle, [&](Channel &channel) -> HcclResult {
            return channel.Clean();  
        });

        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[%s] ChannelHandle Clean failed.", __func__);
            return ret;
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult HcommChannelResumeConcurrency(const ChannelHandle *channelList, uint32_t channelNum)
{
    u32 readyCount = 0;
    for (uint32_t i = 0; i < channelNum; ++i) {
        const ChannelHandle inHandle = channelList[i];
        HcclResult ret = hcomm::WithChannelByHandleLocked(inHandle, [&](Channel &channel) -> HcclResult {
            return channel.Resume();
        });

        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Get ChannelHandle failed.", __func__);
            return ret;
        }
        readyCount += (ret == HcclResult::HCCL_SUCCESS) ? 1 : 0;
    }

    HcclResult finalRet = (readyCount == channelNum) ? HCCL_SUCCESS : HCCL_E_AGAIN;
    return finalRet;
}
HcclResult HcommChannelResume(const ChannelHandle *channelList, uint32_t channelNum)
{
    CHK_PTR_NULL(channelList);

    constexpr uint32_t timeoutSec = 120;
    constexpr auto timeout = std::chrono::seconds(timeoutSec);
    auto startTime = std::chrono::steady_clock::now();
    HCCL_INFO("[%s] start resuming channels, timeout[%u]sec", __func__, timeoutSec);

    uint32_t retryCount{0};
    while (true) {
        HcclResult ret = HcommChannelResumeConcurrency(channelList, channelNum);
        // 1. 检查超时
        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            HCCL_ERROR("[%s] channel resume timeout after %u sec, channelNum[%u], elapsed[%lld]ms, retryCount[%u]",
                __func__, timeoutSec, channelNum, elapsed, retryCount);
            return HCCL_E_TIMEOUT;
        }

        // 2. 处理重试（去除频繁的重试日志，一秒可能重试上千次）
        if (ret == HCCL_E_AGAIN) {
            ++retryCount;
            continue;
        }

        // 3. 处理失败
        if (ret != HCCL_SUCCESS) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            HCCL_ERROR("[%s] channel connect failed, channelNum[%u], ret[%d], elapsed[%lld]ms, retryCount[%u]",
                __func__, channelNum, ret, elapsed, retryCount);
            return ret;
        }

        // 4. 正常情况：所有通道连接成功
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        HCCL_INFO("[%s] all channels connected successfully, channelNum[%u], elapsed[%lld]ms, retryCount[%u]",
            __func__, channelNum, elapsed, retryCount);
        break;
    }

    return HcclResult::HCCL_SUCCESS;
}

}