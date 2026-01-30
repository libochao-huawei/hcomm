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
#include "common.h"
#include "comm_mems/comm_mems.h"
#include "endpoint_mgr.h"
namespace hccl {
/**
 * @note 职责：管理当前通信域下本Rank的信息和通信资源
 */
class MyRank {
public:
    MyRank(aclrtBinHandle binHandle, uint32_t rankId, const RankInfo& rankInfo, const CommConfig& config);
    MyRank(aclrtBinHandle binHandle, uint32_t rankId, const CommConfig& config);
    ~MyRank();
    // 获取通信内存管理器
    CommMems* GetCommMems() const { return commMems_.get(); }
    // 初始化MyRank资源
    HcclResult Init(HcclMem cclBuffer);

    // 创建通信Channel
    HcclResult CreateChannels(CommEngine engine, const std::string &commTag, 
        const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle *channels);
    
    HcclResult ChannelGetHcclBuffer(ChannelHandle channel, void **buffer, uint64_t *size);

private:
    HcclResult BatchCreateSockets(const HcclChannelDesc* channelDescs, uint32_t channelNum,
        const std::string &commTag, std::vector<HcommChannelDesc> &hcommDescs);
    HcclResult BatchCreateChannels(CommEngine engine, const HcclChannelDesc* channelDescs, uint32_t channelNum,
        std::vector<HcommChannelDesc> &hcommDescs, ChannelHandle *channelHandles);
    HcclResult BatchConnectChannels(ChannelHandle *channelHandles, uint32_t channelNum);

    aclrtBinHandle binHandle_{nullptr};
    uint32_t rankId_{};
    CommConfig config_{};
    std::unique_ptr<RankPairMgr> rankPairMgr_{nullptr};
    std::unique_ptr<hcomm::EndpointMgr> endpointMgr_{nullptr};
    std::unique_ptr<CommMems> commMems_{nullptr};
};

} // namespace hccl

#endif // MY_RANK_H
