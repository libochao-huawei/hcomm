/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_comm_pub.h"
#include "rank_consistentcy_checker.h"

namespace hccl {
HcclResult hcclComm::AddExchangeInfo(void* data, uint32_t length)
{
    CHK_PTR_NULL(data);
    CHK_PRT_RET(length == 0, HCCL_ERROR("[AddExchangeInfo] length is 0."), HCCL_E_PARA);

    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    CHK_PRT_RET(exchangeInfoReady_, 
        HCCL_ERROR("[AddExchangeInfo] exchangeInfo already exists, consume it first via HcclChannelAcquire."), 
        HCCL_E_PARA);

    exchangeInfoBuf_.resize(length);
    s32 sRet = memcpy_s(exchangeInfoBuf_.data(), length, data, length);
    CHK_PRT_RET(sRet != EOK, 
        HCCL_ERROR("[AddExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);
    exchangeInfoLen_ = length;
    exchangeInfoReady_ = true;
    
    return HCCL_SUCCESS;
}

HcclResult hcclComm::GetExchangeInfo(uint32_t remoteRank, void* data, uint32_t length)
{
    CHK_PTR_NULL(data);

    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    auto iter = remoteExchangeInfoMap_.find(remoteRank);
    CHK_PRT_RET(iter == remoteExchangeInfoMap_.end(), 
        HCCL_ERROR("[GetExchangeInfo] no exchange info for remoteRank[%u]", remoteRank), 
        HCCL_E_PARA);
    CHK_PRT_RET(length < iter->sencond.length, 
        HCCL_ERROR("[GetExchangeInfo] buffer length[%u] < actual[%u] for remoteRank[%u]", length, iter->sencond.length, remoteRank), 
        HCCL_E_PARA);
    
    s32 sRet = memcpy_s(data. length, iter->second.data.data(), iter->sencond.length);
    CHK_PRT_RET(sRet != EOK, 
        HCCL_ERROR("[GetExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);

    // 读请
    remoteExchangeInfoMap_.erase(iter);

    return HCCL_SUCCESS;
}

HcclResult hcclComm::StoreRemoteExchangeInfo(uint32_t remoteRank, const std::vector<u8>& data)
{
    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    ExchangeInfoEntry infoEntry;
    infoEntry.data = data;
    infoEntry.length = static_cast<uint32_t>(data.size());
    remoteExchangeInfoMap_[remoteRank] = std::move(infoEntry);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::ClearExchangeInfoState()
{
    std::lock_guard<std::mutex> lock(exchangeInfoMutex_);
    exchangeInfoReady_ = false;
    exchangeInfoBuf_.clear();
    exchangeInfoLen_ = 0;
}

HcclResult hcclComm::IsExchangeInfoReady() const
{
    return exchangeInfoReady_;
}

const std::vector<u8>& hcclComm::GetExchangeInfoBuf() const
{
    return exchangeInfoBuf_;
}

uint32_t hcclComm::GetExchangeInfoLen() const
{
    return exchangeInfoLen_;
}

}