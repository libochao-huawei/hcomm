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
#include "hccl/hccl_res_exp.h"

using namespace hccl;

HcclResult HcclResult HcclCommAddExchangeInfo(HcclComm comm, void* data, uint32_t length)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(data);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    return hcclComm->AddExchangeInfo(data, length);
}

HcclResult HcclCommGetExchangeInfo(HcclComm comm, uint32_t remoteRank, void* data, uint32_t length)
{
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(data);
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    return hcclComm->GetExchangeInfo(remoteRank, data, length);
}