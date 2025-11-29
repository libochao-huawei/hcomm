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
HcclResult hcclComm::RegistTaskAbortHandler() const
{
    return HCCL_SUCCESS;
}
 
HcclResult hcclComm::UnRegistTaskAbortHandler() const
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::GetOneSidedService(IHcclOneSidedService** service)
{
    return HCCL_SUCCESS;
}
HcclResult hcclComm::InitOneSidedServiceNetDevCtx(u32 remoteRankId)
{
    return HCCL_SUCCESS;
}
HcclResult hcclComm::OneSidedServiceStartListen(NicType nicType,HcclNetDevCtx netDevCtx)
{
    return HCCL_SUCCESS;
}
HcclResult hcclComm::GetOneSidedServiceDevIpAndPort(NicType nicType, HcclIpAddress& ipAddress, u32& port)
{
    return HCCL_SUCCESS;
}
HcclResult hcclComm::DeinitOneSidedService()
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::RegisterCommUserMem(void* addr, u64 size, void **handle)
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::DeregisterCommUserMem(void* handle)
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::ExchangeCommUserMem(void* handle, std::vector<u32>& peerRanks)
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::SetIndependentOpConfig(const CommConfig &commConfig, const RankTable_t &rankTable)
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::InitIndependentOp()
{
    return HCCL_SUCCESS;
}

HcclResult hcclComm::PrepareChannelMem(const std::string &tag, TransportIOMem &transMem)
{
    return HCCL_SUCCESS;
}
HcclResult hcclComm::IndOpTransportAlloc(const std::string &tag, OpCommTransport &opCommTransport, bool isAicpuModeEn)
{
    return HCCL_SUCCESS;
}
}  // namespace hccl
 