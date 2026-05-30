/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_ubg_channel.h"
#include "endpoint.h"
#include "orion_adpt_utils.h"

namespace hcomm {

AicpuTsUbgChannel::AicpuTsUbgChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc)
    : AicpuTsUboeChannel(endpointHandle, channelDesc)
{
}

HcclResult AicpuTsUbgChannel::BuildConnection()
{
    Hccl::OpMode        opMode = Hccl::OpMode::OPBASE;
    bool                devUsed  = true;
    Hccl::LinkProtocol  protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));

    Hccl::IpAddress locIpv4Addr;
    Hccl::IpAddress rmtIpv4Addr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locIpv4Addr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtIpv4Addr));
    HCCL_INFO("[AicpuTsUbgChannel][%s] LinkProtocol[%s], locIpv4Addr[%s], rmtIpv4Addr[%s]",
        __func__, protocol.Describe().c_str(), locIpv4Addr.Describe().c_str(), rmtIpv4Addr.Describe().c_str());

    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    Hccl::TpManager::GetInstance(deviceLogicId).Init();

    std::unique_ptr<Hccl::DevUbConnection> ubConn = std::make_unique<Hccl::DevUbUboeConnection>(rdmaHandle_,
        locAddr_, rmtAddr_, opMode, devUsed, Hccl::HrtUbJfcMode::STARS_POLL, locIpv4Addr, rmtIpv4Addr, true);
    CHK_SMART_PTR_NULL(ubConn);

    commonRes_.connVec.clear();
    commonRes_.connVec.emplace_back(ubConn.get());
    connections_.clear();
    connections_.push_back(std::move(ubConn));

    return HCCL_SUCCESS;
}

} // namespace hcomm