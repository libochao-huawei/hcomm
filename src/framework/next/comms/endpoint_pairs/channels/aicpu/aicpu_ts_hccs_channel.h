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
#include "hccl_dispatcher_ctx.h"
#include "rma_buffer.h"
#include "hcomm_c_adpt.h"
#include "../../../endpoints/aicputs_hccs_endpoint.h"

namespace hcomm {
/**
 * @note 职责：Channel的AicpuTs通信引擎、HCCS协议的类派生
 */
class AicpuTsHccsChannel : public Channel {
public:
    struct HccsExchangeInfo {
        s32 memNum = 0;
    };
    AicpuTsHccsChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc);
    virtual ~AicpuTsHccsChannel();

    HcclResult Init() override;
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override;
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override;
    ChannelStatus GetStatus() override;

    std::shared_ptr<hccl::Transport> GetTransport() {return transport_;}
    HcclResult Clean() override;
    HcclResult Resume() override;

    u64 GetMyId() {return myId_;};

    // for launch channel kernel data
    HcclResult Serialize(std::shared_ptr<hccl::DeviceMem> &out) override;
    HcommChannelKind GetChannelKind() const override;

private:
    HcclResult ParseInputParam();
    HcclResult BuildConnection();
    HcclResult GetFirstIpByPhyId(u32 devicePhyId, u32 superDevId, hccl::HcclIpAddress &ip);
    HcclResult SetMachinePara(hccl::MachinePara &machinePara);
    void SetTransportParam(hccl::TransportPara &para);
    HcclResult TransportInit();
    HcclResult BuildHcclChannelHccsRes(HcclChannelHccsRes &channelHccsRes);

private:
    u64 myId_;
    // --------------------- 入参 ---------------------
    EndpointHandle                                              endpointHandle_{nullptr};
    HcommChannelDesc                                            channelDesc_;

    // --------------------- 转换参数 ---------------------
    HcclNetDevCtx                                               netDevCtx_{nullptr};
    EndpointDesc                                                localEp_{};
    EndpointDesc                                                remoteEp_{};
    hccl::HcclIpAddress                                         localIp_;
    hccl::HcclIpAddress                                         remoteIp_;
    uint32_t                                                    notifyNum_{0};
    AicpuTsHccsEndPoint                                         *localEpPtr_{nullptr};
    uint32_t                                                    serverPort_{AICPU_CHANNEL_DEFUALT_PORT};
    uint32_t                                                    socketTagIdx_;
    // --------------------- 具体成员 ---------------------
    std::shared_ptr<hccl::HcclSocket>                           socket_{};
    std::string                                                 socketTag_{};
    bool                                                        isSocketServer_{false};
    // for create TransportMem
    HcclDispatcher                                              dispatcher_; // dispatcher放到最后析构
    DispatcherCtxPtr                                            dispatcherCtx_{nullptr};
    std::unique_ptr<hccl::NotifyPool>                           notifyPool_;
    std::shared_ptr<hccl::Transport>                            transport_{nullptr};

    // for get mem temp
    std::vector<HcclMem>                                        remoteIpcRmaBufferVec_;
    std::vector<HcclMemEx>                                      localIpcRmaBufferVecEx_;
    std::vector<HcclMemEx>                                      remoteIpcRmaBufferVecEx_;
};
}

#endif // AICPU_TS_HCCS_CHANNEL_H
