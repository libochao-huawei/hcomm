/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef MY_RANK_H
#define MY_RANK_H

#include "hcomm_res_defs.h"
#include "mem_host_pub.h"
#include "rank_pair_mgr.h"
#include "endpoint_mgr.h"
#include "comm_config_pub.h"
#include "manager_common.h"
#include "common.h"
#include "comm_mems/comm_mems.h"
#include "engine_ctxs/engine_ctxs.h"
#include "endpoint_mgr.h"
#include "communicator/ns_recovery/ns_recovery.h"
#include "hdc_pub.h"
#include "rank_graph.h"
#include "orion_adapter_hccp.h"

#include "../../comms/comm_engine_res/ccu/ccu_res_container.h"


namespace hccl {

/**
 * @note 职责：管理当前通信域下本Rank的信息和通信资源
 */
class MyRank {
public:
    MyRank(aclrtBinHandle binHandle, uint32_t rankId, const CommConfig& config, const ManagerCallbacks& callbacks, RankGraph* rankGraph);
    ~MyRank();

    HcclResult Init(HcclMem cclBuffer, const uint32_t opExpansionMode, uint32_t rankNum);

    CommMems* GetCommMems() const { return commMems_.get(); }

    EngineCtxs* GetEngineCtxs() const { return engineCtxs_.get(); }

    hcomm::CcuResContainer *GetCcuResContainer() { return ccuResContainer_.get(); }

    uint32_t GetOpExpansionMode() {
        return opExpansionMode_;
    }

    HcclResult CreateChannels(CommEngine engine, const std::string &commTag, 
        const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle *channels);
    
    HcclResult ChannelGetHcclBuffer(ChannelHandle channel, void **buffer, uint64_t *size);
    HcclResult ChannelGetRemoteMem(ChannelHandle channel, CommMem **remoteMem, char ***memTag, uint32_t *memNum);

    // Ns recovery
    void SetKfcControlTransfer(std::shared_ptr<HDCommunicate> kfcControlTransferH2D, 
        std::shared_ptr<HDCommunicate> kfcStatusTransferD2H);
    std::vector<ChannelHandle> GetAllChannelList();
    HcclResult StopLaunch();
    HcclResult Clean();
    HcclResult Resume();

private:
    HcclResult BatchCreateSockets(const HcclChannelDesc* channelDescs, uint32_t channelNum,
        const std::string &commTag, std::vector<HcommChannelDesc> &hcommDescs);
    HcclResult BatchCreateChannels(CommEngine engine, const HcclChannelDesc* channelDescs, uint32_t channelNum,
        std::vector<HcommChannelDesc> &hcommDescs, ChannelHandle *channelHandles);
    HcclResult BatchConnectChannels(const HcclChannelDesc* channelDescs, ChannelHandle *channelHandles, uint32_t channelNum);
    HcclResult CheckChannelParam(CommEngine engine, const HcclChannelDesc &channelDesc, uint32_t index);
    HcclResult QueryListenPort(uint32_t localRank, uint32_t remoteRank, const EndpointDesc &localEndpointDesc, 
        const EndpointDesc &remoteEndpointDesc, uint32_t &listenPort, HcommChannelDesc &hcommDesc);
    HcclResult GetLocalTlsStatus(Hccl::TlsStatus &tlsStatus) const;

    HcclResult TryInitCcuInstance();
    HcclResult DestroyNewChannels(CommEngine engine, const HcclChannelDesc* channelDescs);

    aclrtBinHandle binHandle_{nullptr};
    uint32_t rankId_{};
    CommConfig config_{};

    // 当前通信域初始化没有处理CommConfig，暂时只使用展开模式
    uint32_t opExpansionMode_{0};

    std::unique_ptr<RankPairMgr> rankPairMgr_{nullptr};
    std::unique_ptr<hcomm::EndpointMgr> endpointMgr_{nullptr};
    std::unique_ptr<CommMems> commMems_{nullptr};
    std::unique_ptr<EngineCtxs> engineCtxs_{nullptr};

    // 当前CommEngineResMgr复用a3代码，为不影响a3流程，先将ccu资源管理放在MyRank
    std::unique_ptr<hcomm::CcuResContainer> ccuResContainer_{nullptr};

    ManagerCallbacks callbacks_;

    // RankGraph (临时放在myRank里面，后面会随着createchannel整体迁移到RankPairMgr上)
    RankGraph* rankGraph_{nullptr};

    // 记录每次调用BatchCreateChannels时新增的channelIndex, reuseIdx
    std::vector<std::pair<u32, u32>> newChannels_{};

    // Ns recovery
    std::unique_ptr<NsRecoveryProcessor> nsRecoveryProcessor_{nullptr};
};

} // namespace hccl

#endif // MY_RANK_H
