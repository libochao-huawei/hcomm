/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_channel_transport_process.h"
#include "aicpu_res_package_helper.h"
#include "aicpu_channel_hccs_process.h"

using namespace hccl;

namespace hccl {

HcclResult AicpuChannelTransportProcess::InitTransportChannel(HcclChannelTransportResSet *transportResSet, uint32_t channelIndex,
    std::unique_ptr<hccl::Transport> &transport)
{
    if (transportResSet->protocol == COMM_PROTOCOL_HCCS) {
        CHK_PRT(AicpuChannelHccsProcess::InitP2pChannel(transportResSet, channelIndex, transport));
    }
    return HCCL_SUCCESS;
}
}