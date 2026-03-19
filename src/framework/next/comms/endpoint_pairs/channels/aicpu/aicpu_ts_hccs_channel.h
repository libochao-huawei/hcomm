/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef AICPU_TS_HCCS_CHANNEL_H
#define AICPU_TS_HCCS_CHANNEL_H
#include <memory>
#include <vector>

#include "hccl_mem_defs.h"
#include "transport_mem.h"
#include "hccl_res.h"
#include "hccl_socket.h"
#include "channel.h"
#include "channel_param.h"
#include "buffer.h"

namespace hcomm {
/**
 * @note 职责：Channel的AicpuTs通信引擎、HCCS协议的类派生
 */
class AicpuTsHccsChannel : public Channel {
public:
    AicpuTsHccsChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);
    virtual ~AicpuTsHccsChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;

    // for launch channel kernel data
    HcclResult H2DResPack(HcclChannelHccsRes &channelHccsRes);
    std::shared_ptr<hccl::Transport> GetTransport() {return transport_;}

private:
    HcclResult ParseInputParam();
    void ConstructTransTag(std::string& transTag);
    void GetUserRank(bool beLocal, uint32_t &userRank);
    HcclResult MakeLinkInfo(bool beLocal, EndpointDesc &endpointDesc, hccl::HcclRankLinkInfo &linkInfo);
    HcclResult BuildConnection();
    HcclResult SetMachinePara(hccl::HcclRankLinkInfo &localRankLinkInfo,
        hccl::HcclRankLinkInfo &remoteRankLinkInfo, hccl::MachinePara &machinePara);
    void SetTransportParam(hccl::TransportPara &para);
    HcclResult TransportInit();
    HcclResult CheckNotifyOrQPMaxNum(u64 &existNum, const u64 &MaxNum, const bool &isNotifyRes);
    HcclResult BuildHcclChannelHccsRes(HcclChannelHccsRes &channelHccsRes);

private:
    // --------------------- 入参 ---------------------
    EndpointHandle                                              endpointHandle_{nullptr};
    HcommChannelDesc                                            channelDesc_;

    // --------------------- 转换参数 ---------------------
    // init HcclNetDevCtx from AicpuTsHccsEndPoint.GetNetDevCtx()
    HcclNetDevCtx                                               netDevCtx_{nullptr};
    EndpointDesc                                                localEp_{};
    EndpointDesc                                                remoteEp_{};
    uint32_t                                                    notifyNum_{0};
    std::vector<std::shared_ptr<Hccl::Buffer>>                  bufs_{};

    // --------------------- 具体成员 ---------------------
    std::vector<std::shared_ptr<hccl::HcclSocket>>              socket_{nullptr};
    std::shared_ptr<hccl::Transport>                            transport_{nullptr};

    // for create TransportMem
    HcclDispatcher dispatcher_; // dispatcher放到最后析构
    std::unique_ptr<hccl::NotifyPool> notifyPool_;
    std::string connTag_{};
};
}

#endif // AICPU_TS_HCCS_CHANNEL_H
