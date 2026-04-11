/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_TS_ROCE_CHANNEL_H
#define AICPU_TS_ROCE_CHANNEL_H

#include "aicpu_ts_roce_mem_view.h"
#include "../channel.h"
#include "hccl_dispatcher_ctx.h"
#include "hccl_socket.h"
#include "transport_pub.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hcomm {

class AicpuTsRoceChannel : public Channel {
public:
    explicit AicpuTsRoceChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);
    ~AicpuTsRoceChannel() override;

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;
    HcclResult GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum) override;
    HcclResult PrepareAicpuKernelDeviceBlob(std::shared_ptr<hccl::DeviceMem> &out) override;
    HcommChannelKind GetChannelKind() const override;

private:
    HcclResult ParseInputParam();
    HcclResult BuildDataSocket();
    HcclResult BuildDispatcherAndTransport();
    HcclResult BuildSocketTagName(std::string &outTag) const;

    const char *SocketRoleTag() const noexcept
    {
        return isLocalIpClient_ ? "client" : "server";
    }

    EndpointHandle endpointHandle_{};
    HcommChannelDesc channelDesc_{};

    EndpointDesc localEp_{};
    EndpointDesc remoteEp_{};
    bool isLocalIpClient_{false};
    uint32_t notifyNum_{0};
    RdmaHandle rdmaHandle_{nullptr};

    std::vector<AicpuTsRoceMemView> regMems_{};

    std::shared_ptr<hccl::HcclSocket> dataSocket_{};
    std::string dispatcherCommId_{};
    DispatcherCtxPtr dispatcherCtx_{nullptr};
    bool ownsDispatcherCtx_{false};

    std::unique_ptr<hccl::NotifyPool> notifyPoolHolder_{};
    hccl::MachinePara machinePara_{};
    hccl::TransportPara transportPara_{};
    std::unique_ptr<hccl::Transport> transport_{};

    bool inited_{false};
};

} // namespace hcomm

#endif // AICPU_TS_ROCE_CHANNEL_H
