/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm_c_adpt.h"
#include "op_base.h"
#include "../coll_comm_mgr.h"
#include <memory>

using namespace hccl;
static std::unique_ptr<CollCommMgr> g_collCommMgr = std::make_unique<CollCommMgr>();

/**
 * @note 职责：集合通信的通信域管理的C接口的C到C++适配
 */

HcclResult HcclCommGetStatus(const char * commId, HcclCommStatus *status)
{
    CHK_PTR_NULL(commId);
    CHK_PTR_NULL(status);
    std::shared_ptr<hccl::hcclComm> hcclComm;
    CHK_RET(HcclGetCommHandle(commId, hcclComm));
    CHK_PRT_RET(hcclComm == nullptr, HCCL_ERROR("%s hcclComm is null, commId[%s]", __func__, commId), HCCL_E_PTR);
    return hcclComm->GetCommStatus(*status);
}

/**
 * @note C接口适配参考示例
 * @code {.c}
 * HcclResult HcclCommDestroy(HcclComm comm) {
 *     return g_collCommMgr->DestroyComm(comm);
 * }
 * @endcode
 */