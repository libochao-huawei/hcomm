/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "engine_ctxs.h"
#include "hccl_comm_pub.h"
#include "log.h"
#include "adapter_rts_common.h"
#include "hcomm_c_adpt.h"

namespace hccl {
EngineCtxs::EngineCtxs()
{
}

EngineCtxs::~EngineCtxs()
{
}

HcclResult EngineCtxs::CreateCommEngineCtx(const std::string &tag, CommEngine engine, uint64_t size, void **ctx)
{
    std::lock_guard<std::mutex> lock(mutex_); 
    // 阻止重复创建
    if (contextMap_.find(tag) != contextMap_.end()) {
        auto engineCtxMap = contextMap_[tag];
        CHK_PRT_RET(engineCtxMap.find(engine) != engineCtxMap.end(),
            HCCL_ERROR("[%s] already exist a context with same key, tag[%s], engine[%d]",
            __func__, tag.c_str(), engine), HCCL_E_PARA);
    }

    CHK_RET(HcommEngineCtxCreate(engine, size, ctx));
    contextMap_[tag][engine] = {HCCL_MEM_TYPE_NUM, *ctx, size}; // type不需要使用
    HCCL_INFO("[%s]create context success, tag[%s], engine[%d]", __func__, tag.c_str(), engine);
    return HCCL_SUCCESS;
}

HcclResult EngineCtxs::GetCommEngineCtx(const std::string &tag, CommEngine engine, void **ctx, uint64_t *size)
{
    std::lock_guard<std::mutex> lock(mutex_); 
    // Ctx未创建返回
    if (contextMap_.find(tag) == contextMap_.end()) {
        HCCL_INFO("[%s] not exist a context with tag[%s]", __func__, tag.c_str());
        return HCCL_E_PARA;
    } else {
        auto engineCtxMap = contextMap_[tag];
        if (engineCtxMap.find(engine) == engineCtxMap.end()) {
            HCCL_INFO("[%s] not exist a context with tag[%s], engine[%d]", __func__, tag.c_str(), engine);
            return HCCL_E_PARA;
        }
    }

    *ctx = contextMap_[tag][engine].addr;
    *size = contextMap_[tag][engine].size;
    HCCL_INFO("[%s]get context success, tag[%s], engine[%d]", __func__, tag.c_str(), engine);    
    return HCCL_SUCCESS;
}

HcclResult EngineCtxs::CopyCommEngineCtx(const std::string &tag, CommEngine engine, const void *srcCtx,
    uint64_t size, uint64_t dstCtxOffset)
{
    void *dstCtx;
    uint64_t dstSize = 0;
    CHK_RET(GetCommEngineCtx(tag, engine, &dstCtx, &dstSize));
    CHK_RET(HcommEngineCtxCopy(engine, reinterpret_cast<uint8_t*>(dstCtx) + dstCtxOffset, srcCtx, size)); // 增加大小判断，增加强转
    HCCL_INFO("[%s]copy engine ctx success, tag[%s], engine[%d]", __func__, tag.c_str(), engine);
    return HCCL_SUCCESS;
}

HcclResult EngineCtxs::DestroyEngineCtx(const std::string &tag, CommEngine engine)
{
    std::lock_guard<std::mutex> lock(mutex_); 
    // Ctx不存在返回错误
    if (contextMap_.find(tag) == contextMap_.end()) {
        HCCL_ERROR("[%s] not exist a context with tag[%s]", __func__, tag.c_str());
        return HCCL_E_PARA;
    }
    auto& engineCtxMap = contextMap_[tag];
    if (engineCtxMap.find(engine) == engineCtxMap.end()) {
        HCCL_ERROR("[%s] not exist a context with tag[%s], engine[%d]", __func__, tag.c_str(), engine);
        return HCCL_E_PARA;
    }
    // 获取内存信息
    HcclMem& memInfo = engineCtxMap[engine];
    CHK_RET(HcommEngineCtxDestroy(engine, memInfo.addr));
    // 从映射中移除
    engineCtxMap.erase(engine);
    if (engineCtxMap.empty()) {
        contextMap_.erase(tag);
    }

    HCCL_INFO("[%s]destroy context success, tag[%s], engine[%d]", __func__, tag.c_str(), engine);   
    return HCCL_SUCCESS;
}
}