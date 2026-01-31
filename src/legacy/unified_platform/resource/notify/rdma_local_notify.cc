/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rdma_local_notify.h"
#include "not_support_exception.h"

namespace Hccl {

RdmaLocalNotify::RdmaLocalNotify(RdmaHandle rdmaHandle, bool devUsed)
    : BaseLocalNotify(RmaType::RDMA, devUsed), rdmaHandle(rdmaHandle)
{
    notify   = std::make_unique<RtsNotify>(true);
    u64 va   = 0;
    u64 size = 0;
    HrtRaGetNotifyBaseAddr(rdmaHandle, &va, &size);
    if (va > (UINT64_MAX - notify->GetOffset())) {
        THROW<InternalException>("addr occur integer overflow ");
    }
    addr = va + notify->GetOffset();
    // 待修改: 利用 rdmaHandle 从 HCCP 新接口获取 key
}

void RdmaLocalNotify::Wait(const Stream &stream, u32 timeout) const
{
    notify->Wait(stream, timeout);
}

void RdmaLocalNotify::Post(const Stream &stream) const
{
    HCCL_ERROR("RdmaLocalNotify does not support submit record task");
    throw NotSupportException("RdmaLocalNotify does not support submit record task");
}

string RdmaLocalNotify::Describe() const
{
    return StringFormat("RdmaLocalNotify[notify=%s, addr=0x%llx]", notify->Describe().c_str(), addr);
}

} // namesapce Hccl

