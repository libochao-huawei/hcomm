/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "ffts_ctx_provider.h"
#include "ffts_common.h"
#include "legacy_common.h"
#include "legacy_log.h"
 
using namespace std;
 
namespace GraphMgr {
FftsCtxProvider::FftsCtxProvider()
{
}
 
FftsCtxProvider::~FftsCtxProvider()
{
    fftsCtxMap_.clear();
    fftsCtxMapV2_.clear();
}

HcclFftsContextsInfo *FftsCtxProvider::GetFftsCtx(const char *key, uint32_t keyLen, bool useGraphConstructorV2)
{
    CHK_PRT_RET(key == nullptr, HCCL_ERROR("GetGraphCtx key is nullpty"), nullptr);
    std::string sKey(key, keyLen);
    HCCL_INFO("GetFftsCtx key[%s] sKey[%s] useGraphConstructorV2_[%u]",
        key, sKey.c_str(), useGraphConstructorV2);
    HcclResult ret = LegacyParseDebugConfig();
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_ERROR("GetFftsCtx LegacyParseDebugConfig failed"), nullptr);
    if (sKey == "") {
        alwaysNewFftsCtx_.reset(new (std::nothrow) HcclFftsContextsInfo());
        return alwaysNewFftsCtx_.get();
    }
    if (useGraphConstructorV2) {
        return &fftsCtxMapV2_[sKey];
    }
    return &fftsCtxMap_[sKey];
}
} // namespace GraphMgr