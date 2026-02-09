/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ORION_ADPT_UTILS_H
#define ORION_ADPT_UTILS_H

#include "hccl/hccl_types.h"
#include "hcomm_res_defs.h"

// Orion
#include "ip_address.h"
#include "virtual_topo.h"

namespace hcomm {

HcclResult CommAddrToIpAddress(const CommAddr &commAddr, Hccl::IpAddress &ipAddr);
HcclResult IpAddressToCommAddr(const Hccl::IpAddress &ipAddr, CommAddr &commAddr);
HcclResult CommProtocolToLinkProtocol(CommProtocol commProtocol, Hccl::LinkProtocol &linkProtocol);
Hccl::LinkData BuildDefaultLinkData();
HcclResult EndpointDescPairToLinkData(const EndpointDesc &locEp, const EndpointDesc &rmtEp, Hccl::LinkData &linkData);

} // namespace hcomm

#endif // ORION_ADPT_UTILS_H