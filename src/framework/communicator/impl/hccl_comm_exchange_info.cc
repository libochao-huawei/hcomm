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
    exchangeInfoBuf_.resize(length);
    s32 sRet = memcpy_s(exchangeInfoBuf_.data(), length, data, length);
    CHK_PRT_RET(sRet != EOK, 
        HCCL_ERROR("[AddExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);
    HCCL_INFO("[AddExchangeInfo] success, length[%u].", length);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::GetExchangeInfo(uint32_t remoteRank, uint32_t length, void* data, uint32_t* actualLength)
{
    auto iter = remoteExchangeInfoMap_.find(remoteRank);
    if (iter == remoteExchangeInfoMap_.end()) {
        *actualLength = 0;
        return HCCL_SUCCESS;
    }
    *actualLength = static_cast<uint32_t>(iter->second.size());
    
    if (length == *actualLength || *actualLength == 0) {
        s32 sRet = memcpy_s(data, length, iter->second.data(), iter->second.size());
        CHK_PRT_RET(sRet != EOK, 
            HCCL_ERROR("[GetExchangeInfo] memcpy_s failed, ret[%d]", sRet), HCCL_E_MEMORY);
    } else {
        HCCL_ERROR("[GetExchangeInfo] failed, length[%u] actualLength[%u].", length, *actualLength);
    }
    
    // 读后清除
    remoteExchangeInfoMap_.erase(iter);
    HCCL_INFO("[GetExchangeInfo] success, remoteRank[%u], actualLength[%u].", remoteRank, *actualLength);
    return HCCL_SUCCESS;
}

HcclResult hcclComm::StoreRemoteExchangeInfo(uint32_t remoteRank, const std::vector<u8>& data)
{
    remoteExchangeInfoMap_[remoteRank] = std::move(data);
    HCCL_INFO("[StoreRemoteExchangeInfo] success, remoteRank[%u], length[%u].", remoteRank, data.size());
    return HCCL_SUCCESS;
}

HcclResult hcclComm::ResetExchangeInfo()
{
    exchangeInfoBuf_.clear();
    HCCL_INFO("[ResetExchangeInfo] exchange info state cleared.");
    return HCCL_SUCCESS;
}

HcclResult hcclComm::GetExchangeInfoBuf(std::vector<u8> &exchangeInfoBuf)
{
    exchangeInfoBuf = exchangeInfoBuf_;
    return HCCL_SUCCESS;
}

uint32_t hcclComm::GetExchangeInfoLen() const
{
    return exchangeInfoBuf_.size();
}

}

