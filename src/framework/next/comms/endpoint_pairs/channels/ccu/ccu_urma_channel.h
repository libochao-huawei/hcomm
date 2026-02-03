/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_URMA_CHANNEL_H
#define CCU_URMA_CHANNEL_H

#include <memory>
#include <vector>

#include "../channel.h"

#include "urma_endpoint.h"
#include "ccu_transport_.h"

namespace hcomm {

class CcuUrmaChannel : public Channel {
public:
    // 当前仅支持交换hccl buffer
    CcuUrmaChannel(const EndpointHandle locEndpointHandle,
        const HcommChannelDesc &channelDesc);
    ~CcuUrmaChannel() = default;

    HcclResult Init() override;
    ChannelStatus GetStatus() override;

    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;

public:
    uint32_t GetDieId() const;
    uint32_t GetChannelId() const;

    HcclResult GetLocCkeByIndex(const uint32_t index, uint32_t &locCkeId) const;
    HcclResult GetLocXnByIndex(const uint32_t index, uint32_t &locXnId) const;

    HcclResult GetRmtCkeByIndex(const uint32_t index, uint32_t &rmtCkeId) const;
    HcclResult GetRmtXnByIndex(const uint32_t index, uint32_t &rmtXnId) const;

    HcclResult GetRmtBuffer(uint64_t &addr, uint32_t &size,
        uint32_t &tokenId, uint32_t &tokenValue) const;

private:
    std::unique_ptr<CcuTransport> impl_{nullptr};
    EndpointHandle locEndpointHandle_{nullptr};
    HcommChannelDesc channelDesc_{};
    // 当前CCU不支持自定义内存交换，仅包含 hccl buffer
    std::unique_ptr<HcclMem> hcclBufferInfoPtr_{};
    std::string memTag_{"HcclBuffer"};
};

}  // namespace hcomm
#endif  // CCU_URMA_CHANNEL_H
