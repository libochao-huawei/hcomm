/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_dispatcher_ctx.h"
#include "dispatcher_ctx.h"
static DispatcherCtxPtr gDispatcherCtx = nullptr;
HcclResult CreateDispatcherCtx(DispatcherCtxPtr *ctx, u32 devPhyId)
{
    HCCL_INFO("CreateCtx");
    CHK_PRT_RET(devPhyId == INVALID_UINT, HCCL_ERROR("[CreateCtx] devPhyId invalid"), HCCL_E_PARA);
    CHK_PTR_NULL(ctx);
    hccl::DispatcherCtx *Ctx_tmp = new (std::nothrow) hccl::DispatcherCtx(devPhyId);
    CHK_PTR_NULL(Ctx_tmp);
    // 创建ctx，内部创建dispatcher和notify pool实例  目前没有pool
    HcclResult ret = Ctx_tmp->Init();
    if (ret != HCCL_SUCCESS) {
        delete Ctx_tmp;
        HCCL_ERROR("[CreateCtx] CTX init fail");
        return ret;
    }
    
    *ctx = Ctx_tmp;
    gDispatcherCtx = Ctx_tmp;
    return HCCL_SUCCESS;
}

HcclResult DestoryDispatcherCtx(DispatcherCtxPtr ctx)
{
    HCCL_INFO("DestoryCtx");
    CHK_PTR_NULL(ctx);
    if (gDispatcherCtx == ctx) {
        gDispatcherCtx = nullptr;
    } else {
        HCCL_WARNING("[DestoryCtx] gDispatcherCtx and ctx do not match.");
    }
    hccl::DispatcherCtx *Ctx_tmp = reinterpret_cast<hccl::DispatcherCtx*>(ctx);
    HcclResult ret = Ctx_tmp->Destroy();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[CreateCtx] CTX Destroy fail");
    }
    delete Ctx_tmp;
    ctx = nullptr;
    return ret;
}

HcclResult SetDispatcherCtx(const DispatcherCtxPtr ctx)
{
    HCCL_INFO("SetCtx");
    CHK_PTR_NULL(ctx);
    gDispatcherCtx = ctx;
    return HCCL_SUCCESS;
}

DispatcherCtxPtr GetDispatcherCtx()
{
    HCCL_INFO("GetCtx");
    return gDispatcherCtx;
}
