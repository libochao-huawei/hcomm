/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_comm_pub.h"

namespace hccl {
HcclResult hcclComm::AddExchangeInfo(void* data, uint32_t length)
{
    CHK_PTR_NULL(data);

    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    exchangeInfoBuf_.resize(length);
    s32 sRet = memcpy_s(exchangeInfoBuf_.data(), length, data, length);
    CHK_PRT_RET(sRet != EOK, 
        HCCL_ERROR("[AddExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);
    exchangeInfoLen_ = length;
    HCCL_INFO("[AddExchangeInfo] success, length[%u].", length);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::GetExchangeInfo(uint32_t remoteRank, void* data, uint32_t &length)
{
    CHK_PTR_NULL(data);

    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    auto iter = remoteExchangeInfoMap_.find(remoteRank);
    if (iter == remoteExchangeInfoMap_.end()) {
        length = 0;
        return HCCL_SUCCESS;
    }
    length = iter->second.length;
    
    s32 sRet = memcpy_s(data, iter->second.length, iter->second.data.data(), iter->second.length);
    CHK_PRT_RET(sRet != EOK, 
        HCCL_ERROR("[GetExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);

    // 读后清除
    remoteExchangeInfoMap_.erase(iter);
    HCCL_INFO("[GetExchangeInfo] success, remoteRank[%u], length[%u].", remoteRank, length);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::StoreRemoteExchangeInfo(uint32_t remoteRank, const std::vector<u8>& data)
{
    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    ExchangeInfoEntry infoEntry;
    infoEntry.data = data;
    infoEntry.length = static_cast<uint32_t>(data.size());
    remoteExchangeInfoMap_[remoteRank] = std::move(infoEntry);
    HCCL_INFO("[StoreRemoteExchangeInfo] success, remoteRank[%u], length[%u].", remoteRank, infoEntry.length);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::ResetExchangeInfo()
{
    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    exchangeInfoBuf_.clear();
    exchangeInfoLen_ = 0;
    HCCL_INFO("[ResetExchangeInfo] exchange info state cleared.");
    return HCCL_SUCCESS;
}

const std::vector<u8>& hcclComm::GetExchangeInfoBuf() const
{
    return exchangeInfoBuf_;
}

uint32_t hcclComm::GetExchangeInfoLen() const
{
    return exchangeInfoLen_;
}

bool hcclComm::IsNewRemoteRank(uint32_t remoteRank) const
{
    return checkedRemoteRanks_.find(remoteRank) == checkedRemoteRanks_.end();
}

void hcclComm::MarkRemoteRankChecked(uint32_t remoteRank)
{
    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    checkedRemoteRanks_.insert(remoteRank);
    HCCL_DEBUG("[MarkRemoteRankChecked] remoteRank[%u] marked as checked.", remoteRank);
}

}

