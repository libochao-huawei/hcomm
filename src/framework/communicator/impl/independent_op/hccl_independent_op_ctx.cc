/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api.h"
#include "independent_op_context_manager.h"
#include "log.h"
#include "hccl_comm_pub.h"
#include <string>

using namespace hccl;

HcclResult CommCreateEngineCtx(HcclComm comm, const char *engineTag, CommEngine engine, HcclMem *engineCtx)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(engineTag);
    CHK_PTR_NULL(engineCtx);
    CHK_PRT_RET(strlen(engineTag) > HCCL_OP_TAG_LEN_MAX,
        HCCL_ERROR("[%s] engineTag length exceeds maximum length, engineTag length[%zu], max length[%d]",
            __func__,
            strlen(engineTag),
            HCCL_OP_TAG_LEN_MAX),
        HCCL_E_PARA);
    CHK_PRT_RET(engineCtx->size == 0, HCCL_ERROR("[%s]Invalid CtxSize, CtxSize[%u]",
        __func__, engineCtx->size), HCCL_E_PARA);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->CreateCommEngineCtx(std::string(engineTag), engine, engineCtx));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}

HcclResult CommGetEngineCtx(HcclComm comm, const char *engineTag, CommEngine engine, HcclMem *engineCtx)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(engineTag);
    CHK_PRT_RET(strlen(engineTag) > HCCL_OP_TAG_LEN_MAX,
        HCCL_ERROR("[%s] engineTag length exceeds maximum length, engineTag length[%zu], max length[%d]",
            __func__,
            strlen(engineTag),
            HCCL_OP_TAG_LEN_MAX),
        HCCL_E_PARA);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    return hcclComm->GetCommEngineCtx(std::string(engineTag), engine, engineCtx);
}

HcclResult CommDestroyEngineCtx(HcclComm comm, const HcclMem *engineCtx)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(engineCtx);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    CHK_RET(hcclComm->DestroyCommEngineCtx(engineCtx));
    HCCL_RUN_INFO("[%s] success, group[%s]", __func__, hcclComm->GetIdentifier().c_str());
    return HCCL_SUCCESS;
}
