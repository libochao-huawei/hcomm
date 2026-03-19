/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef __AICPU_CHANNEL_HCCS_PROCESS_H__
#define __AICPU_CHANNEL_HCCS_PROCESS_H__

#include "common.h"
#include "channel_param.h"
#include "hccl_dispatcher_ctx.h"

namespace hccl {
class AicpuChannelHccsProcess {
public:
    ~AicpuChannelHccsProcess();
    static HcclResult SetTransportMachinePara(hccl::MachinePara &machinePara, HcclChannelHccsRes &channelHccsRes);
    static HcclResult InitP2pChannel(HcclChannelTransportResSet *transportResSet, uint32_t channelIndex,
        std::unique_ptr<hccl::Transport> &transport);

private:
    // static HcclDispatcher   dispatcher_; // dispatcher放到最后析构
    static DispatcherCtxPtr dispatcherCtx_;
};
}

#endif // __AICPU_CHANNEL_HCCS_PROCESS_H__