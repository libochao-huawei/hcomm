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
#define COMM_ID_LEN_MAX 128
class DevAicpuTsHccsChannel : public DevAicpuTsChannel  {
public:
    DevAicpuTsHccsChannel() = default;
    ~DevAicpuTsHccsChannel() override;

    HcclResult Create(const void *blob, u64 blobBytes,
                      const HcommDeviceInfo &deviceInfo,
                      ChannelHandle &outHandle) override;
    bool Destroy(ChannelHandle handle) override;

private:
    HcclResult SetTransportMachinePara(hccl::MachinePara &machinePara, HcclChannelHccsRes &channelHccsRes);

    struct HccsSlot {
        DispatcherCtxPtr dispatcherCtx_{nullptr};
        std::shared_ptr<hccl::Transport> transport;
        std::string tag{""};
    };

    std::unordered_map<ChannelHandle, HccsSlot> slots_;
    std::mutex mutex_;
};
}

#endif // __AICPU_CHANNEL_HCCS_PROCESS_H__