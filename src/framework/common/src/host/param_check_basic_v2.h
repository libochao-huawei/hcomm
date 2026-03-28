/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PARAM_CHECK_PUB_BASIC_V2_H
#define PARAM_CHECK_PUB_BASIC_V2_H

#include <cstring>

enum class HcclV2SupportStatus {
    UNKNOWN,  // 获取socName失败
    SUPPORTED,  // 支持V2
    NOT_SUPPORTED  // 不支持V2
};

inline static bool IsChipSupportHCCLV2(const char *socNamePtr)
{
    HCCL_DEBUG("[%s]SocVersion = %s.", __func__, socNamePtr);
    return std::strstr(socNamePtr, "Ascend950") != nullptr;
}

inline HcclV2SupportStatus IsSupportHCCLV2Cached()
{
    static thread_local HcclV2SupportStatus cachedStatus = HcclV2SupportStatus::UNKNOWN;
    static thread_local bool isInited = false;
    
    if (!isInited) {
        const char *socNamePtr = aclrtGetSocName();
        if (socNamePtr == nullptr) {
            // 获取socName失败，返回UNKNOWN状态
            return HcclV2SupportStatus::UNKNOWN;
        }
        cachedStatus = IsChipSupportHCCLV2(socNamePtr) ?
                      HcclV2SupportStatus::SUPPORTED :
                      HcclV2SupportStatus::NOT_SUPPORTED;
        isInited = true;
    }
    return cachedStatus;
}

#define HCCLV2_FUNC_RUN(func, ...) \
    do { \
        const auto status = IsSupportHCCLV2Cached(); \
        if (status == HcclV2SupportStatus::SUPPORTED) { \
            return func; \
        } \
        if (status == HcclV2SupportStatus::UNKNOWN) { \
            HCCL_ERROR("[%s] Failed to get socName", __func__); \
            return HCCL_E_INTERNAL; \
        } \
    } while (0)
    
#endif //PARAM_CHECK_PUB_BASIC_V2_H