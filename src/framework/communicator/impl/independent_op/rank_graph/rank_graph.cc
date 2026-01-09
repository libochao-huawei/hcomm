/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rank_graph.h"
#include <string>

namespace hccl {  

// 根据 rankId 获取 rank 信息
const RankInfo_t* RankGraph::FindRank(uint32_t rankId) const {
    auto it = rankIndex_.find(rankId);
    if (it == rankIndex_.end()) {
        return nullptr;
    }
    return &(it->second.rankInfo);
}

HcclResult RankGraph::DevTypeToCommProtocol(DevType type, CommProtocol &protocol)
{
    switch (type) {
        case DevType::DEV_TYPE_910B:
        case DevType::DEV_TYPE_910_93:
        case DevType::DEV_TYPE_910:
            protocol = CommProtocol::COMM_PROTOCOL_ROCE;
            break;
        case DevType::DEV_TYPE_310P1:
        case DevType::DEV_TYPE_310P3:
            protocol = CommProtocol::COMM_PROTOCOL_PCIE;
            break;
        case DevType::DEV_TYPE_NOSOC:
            protocol = CommProtocol::COMM_PROTOCOL_PCIE;
            break;
        case DevType::DEV_TYPE_910_95:
            // 待扩展UB的协议，当前先不支持
            protocol = CommProtocol::COMM_PROTOCOL_RESERVED;
            break;
        default:
            HCCL_ERROR("[RankGraph] Unknown comm devType: %d", type);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult RankGraph::Init(const RankTable_t &rankTable,  const HcclTopoAttr &topoAttr)
{
    rankTable_ = rankTable;
    topoAttr_ = topoAttr;
    rankIndex_.clear();
    rankPairInfo_.clear();
    HCCL_INFO("[RankGraph][%s] rankNum[%zu]", __func__, rankTable_.rankList.size());
    // 解析 rankTable，建立 rankId -> RankGraphInfo 映射
    for (const auto& r : rankTable_.rankList) {
        RankGraphInfo info;
        info.rankInfo = r;

        // 构造 EndpointDescs
        // 1. 存在device ip理论上就有COMM_PROTOCOL_ROCE能力
        // 2. 利用hccp接口可以查询虚拟网卡地址，但虚拟网卡不等于HCCS
        // 3. 此处只收集device的EndpointDesc（ROCE），todo：收集虚拟网卡的EndpointDesc（HCCS等）
        std::vector<HcclIpAddress> addrs = r.deviceInfo.deviceIp;
        CommProtocol protocol = CommProtocol::COMM_PROTOCOL_RESERVED;
        CHK_RET(DevTypeToCommProtocol(r.deviceInfo.deviceType, protocol));
        for (const auto &addr :addrs) {
            EndpointDesc point;
            EndpointDescInit(&point, 1);
            if (addr.IsIPv6()) {
                point.commAddr.type = COMM_ADDR_TYPE_IP_V6;
                point.commAddr.addr6 = addr.GetBinaryAddress().addr6;
            } else {
                point.commAddr.type = COMM_ADDR_TYPE_IP_V4;
                point.commAddr.addr = addr.GetBinaryAddress().addr;
            }
            point.protocol = protocol;
            point.loc.locType = ENDPOINT_LOC_TYPE_DEVICE; // 当前默认device
            point.loc.device.devPhyId = r.deviceInfo.devicePhyId;
            point.loc.device.superDevId = r.superDeviceId;
            point.loc.device.serverIdx = r.serverIdx;
            point.loc.device.superPodIdx = r.superPodIdx;
            info.endPoints.push_back(std::move(point));
        }
        rankIndex_[r.rankId] = std::move(info);
    }
    rankGraph_ = rankTable_.rankList;
    CHK_RET(InitRankInfo());
    CHK_RET(InitNetLayer());
    CHK_RET(InitHeterogMode());
    return HCCL_SUCCESS;
}

HcclResult RankGraph::Init(const HcclTopoAttr &topoAttr)
{
    topoAttr_ = topoAttr;
    rankIndex_.clear();
    rankPairInfo_.clear();
    HCCL_INFO("[RankGraph][%s] rankNum[%zu]", __func__, rankTable_.rankList.size());
    CHK_RET(InitRankInfo());
    CHK_RET(InitNetLayer());
    CHK_RET(InitHeterogMode());
    return HCCL_SUCCESS;
}

CommProtocol RankGraph::GetCommProtocolFromRankInfo(const RankInfo_t srcInfo, const RankInfo_t dstInfo)
{
    if (srcInfo.deviceInfo.deviceType != dstInfo.deviceInfo.deviceType) {
        HCCL_ERROR("[RankGraph][%s] srcType[%d] != dstType[%d]", __func__,
            srcInfo.deviceInfo.deviceType, dstInfo.deviceInfo.deviceType);
        return CommProtocol::COMM_PROTOCOL_RESERVED;
    }
    // 首先判断是否在同一机内
    if (srcInfo.serverIdx == dstInfo.serverIdx) {
        // 310P间链路为PCIE
        if (srcInfo.deviceInfo.deviceType == DevType::DEV_TYPE_310P3 ||
            srcInfo.deviceInfo.deviceType == DevType::DEV_TYPE_310P1) {
            return CommProtocol::COMM_PROTOCOL_PCIE;
        }
        LinkTypeInServer linkType = LinkTypeInServer::RESERVED_LINK_TYPE;
        hrtGetPairDeviceLinkType(srcInfo.deviceInfo.devicePhyId, dstInfo.deviceInfo.devicePhyId, linkType);
        HCCL_INFO("[RankGraph][%s] ranks[%u,%u] intra-server linkType[%d]", __func__,
            srcInfo.rankId, dstInfo.rankId, linkType);
        if (linkType == LinkTypeInServer::HCCS_TYPE || linkType == LinkTypeInServer::HCCS_SW_TYPE) {
            return CommProtocol::COMM_PROTOCOL_HCCS;
        } else if (linkType == LinkTypeInServer::SIO_TYPE) {
            return CommProtocol::COMM_PROTOCOL_SIO;
        } else {
            return CommProtocol::COMM_PROTOCOL_RESERVED;
        }
    } else {
        // 超节点间链路为HCCS
        if (!srcInfo.superPodId.empty() && srcInfo.superPodId == dstInfo.superPodId) {
            HCCL_INFO("[RankGraph][%s] ranks[%u,%u] inter-server but same superPod[%s]", __func__,
                srcInfo.rankId, dstInfo.rankId, srcInfo.superPodId.c_str());
            return CommProtocol::COMM_PROTOCOL_HCCS;
        } else {
            HCCL_INFO("[RankGraph][%s] ranks[%u,%u] inter-server use ROCE", __func__,
                srcInfo.rankId, dstInfo.rankId);
            return CommProtocol::COMM_PROTOCOL_ROCE;
        }
    }
    return CommProtocol::COMM_PROTOCOL_RESERVED;
}

HcclResult RankGraph::GetLinks(uint32_t netLayer, uint32_t srcRank, uint32_t dstRank,
    CommLink **linkList, uint32_t *listSize)
{
    if (rankIndex_.find(srcRank) == rankIndex_.end() || rankIndex_.find(dstRank) == rankIndex_.end() ||
        FindRank(srcRank) == nullptr || FindRank(dstRank) == nullptr) {
        HCCL_ERROR("[RankGraph][%s] srcRank[%u] and dstRank[%u] are not existed in rankTable",
            __func__, srcRank, dstRank);
        return HCCL_E_PARA;
    }

    auto &srcEndpointDescs = rankIndex_[srcRank].endPoints;
    auto &dstEndpointDescs = rankIndex_[dstRank].endPoints;

    const RankInfo_t srcInfo = rankIndex_[srcRank].rankInfo;
    const RankInfo_t dstInfo = rankIndex_[dstRank].rankInfo;

    // 1. 查询是否有缓存CommLink信息
    auto key = std::make_pair(srcRank, dstRank);
    auto it = rankPairInfo_.find(key);
    if (it == rankPairInfo_.end()) {
        // 没有则创建
        HCCL_INFO("[RankGraph][%s] no cached links, build new srcRank[%u] dstRank[%u]", __func__, srcRank, dstRank);
        std::vector<CommLink> links;
        for (size_t i = 0; i < srcEndpointDescs.size(); i++) {
            for (size_t j = 0; j < dstEndpointDescs.size(); j++) {
                CommLink link;
                CommLinkInit(&link, 1);

                link.srcEndpointDesc = srcEndpointDescs[i];
                link.dstEndpointDesc = dstEndpointDescs[j];
                link.linkAttr.linkProtocol = GetCommProtocolFromRankInfo(srcInfo, dstInfo);
    
                links.push_back(std::move(link));
            }
        }
        it = rankPairInfo_.emplace(std::make_pair(srcRank, dstRank), std::move(links)).first;
    }

    auto &links = it->second;
    *listSize = static_cast<uint32_t>(links.size());
    if (links.empty()) {
        *linkList = nullptr;
        HCCL_ERROR("[RankGraph][%s] links empty for srcRank[%u] dstRank[%u]", __func__, srcRank, dstRank);
    } else {
        *linkList = links.data(); // 连续数组首地址
        HCCL_INFO("[RankGraph][%s] srcRank[%u] dstRank[%u] linkNum[%u]", __func__, srcRank, dstRank, *linkList);
    }

    return HCCL_SUCCESS;
}

HcclResult RankGraph::InitHeterogMode() {
    if (topoAttr_.rankInfoList.empty()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. rankInfoList is empty", __func__);
        return HCCL_E_INTERNAL;
    }

    std::set<DevType> devTypes;
    for (u32 index = 0; index < topoAttr_.rankInfoList.size(); index++) {
        devTypes.insert(topoAttr_.rankInfoList[index].deviceType);
    }

    // 只包含一种芯片的同构组网
    if (devTypes.size() == 1) {
        heterogMode_ = HcclHeterogMode::HCCL_HETEROG_MODE_HOMOGENEOUS;
        return HCCL_SUCCESS;
    }

    // 包含两种芯片的异构混合组网
    if (devTypes.size() == 2 && devTypes.find(DevType::DEV_TYPE_910B) != devTypes.end() && devTypes.find(DevType::DEV_TYPE_910_93) != devTypes.end()) {
        heterogMode_ = HcclHeterogMode::HCCL_HETEROG_MODE_MIX_A2_A3;
        return HCCL_SUCCESS;
    }

    std::string devStr;
    for (auto itSet = devTypes.begin(); itSet !=devTypes.end(); itSet++) {
        if (itSet != devTypes.begin()) {
            devStr +=", ";
        }
        devStr += std::to_string(static_cast<int>(*itSet));
    }
    HCCL_ERROR("[rankGraph][%s] Unknown mode[%d], devtypes[%s]", __func__, HcclHeterogMode::HCCL_HETEROG_MODE_INVALID, devStr.c_str());
    return HCCL_E_INTERNAL;
}

HcclResult RankGraph::GetHeterogMode(HcclHeterogMode *mode)
{
    *mode = heterogMode_;
    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetNetLayers(uint32_t **netLayers, uint32_t *netLayerNum)
{
    if (netLayer_.empty()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. netLayer is empty", __func__);
        return HCCL_E_INTERNAL;
    }
    *netLayers = netLayer_.data();
    *netLayerNum = netLayer_.size();
    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetInstTopoTypeByNetLayer(uint32_t netLayer, CommTopo *topoType)
{
    if (netLayer >= netLayer_.size()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. netlayer[%u]", __func__, netLayer);
        return HCCL_E_PARA;
    }
    DevType deviceType = topoAttr_.deviceType;
    if (deviceType == DevType::DEV_TYPE_910_93) {
        if (netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0)) {
            *topoType = CommTopo::COMM_TOPO_910_93;
        } else if ((netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1) ||
                    (netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L2)))) {
            *topoType = CommTopo::COMM_TOPO_CLOS;
        }
    } else if (deviceType == DevType::DEV_TYPE_910B || deviceType == DevType::DEV_TYPE_910) {
        if (netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0)) {
            *topoType = CommTopo::COMM_TOPO_1DMESH;
        } else if (netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1)) {
            *topoType = CommTopo::COMM_TOPO_CLOS;
        }
    } else if (deviceType == DevType::DEV_TYPE_310P3) {
        if (netLayer == static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0)) {
            *topoType = CommTopo::COMM_TOPO_310P;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetInstSizeByNetLayer(uint32_t netLayer, uint32_t *rankNum)
{
    if (netLayer >= netLayer_.size()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. netlayer[%u]", __func__, netLayer);
        return HCCL_E_PARA;
    }
 
    if (rankList_.find(netLayer) == rankList_.end()) {
        HCCL_ERROR("[rankGraph][%s]failed to find rankList map. netlayer[%u]", __func__, netLayer);
        return HCCL_E_INTERNAL;
    }
    *rankNum = rankList_[netLayer].size();
 
    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetInstRanksByNetLayer(uint32_t netLayer, uint32_t **rankList, uint32_t *rankNum)
{
    if (netLayer >= netLayer_.size()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. netlayer[%u]", __func__, netLayer);
        return HCCL_E_PARA;
    }
 
    if (rankList_.find(netLayer) == rankList_.end()) {
        HCCL_ERROR("[rankGraph][%s]failed to find rankList map. netlayer[%u]", __func__, netLayer);
        return HCCL_E_INTERNAL;
    }
    *rankNum = rankList_[netLayer].size();
    *rankList = rankList_[netLayer].data();
 
    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetInstSizeListByNetLayer(uint32_t netLayer, uint32_t **instSizeList, uint32_t *listSize)
{
    if (netLayer >= netLayer_.size()) {
        HCCL_ERROR("[rankGraph][%s] invalid para. netlayer[%u]", __func__, netLayer);
        return HCCL_E_PARA;
    }
 
    if (rankSizeList_.find(netLayer) == rankSizeList_.end()) {
        HCCL_ERROR("[rankGraph][%s]failed to find rankSizeList map. netlayer[%u]", __func__, netLayer);
        return HCCL_E_INTERNAL;
    }
    *instSizeList = rankSizeList_[netLayer].data();
    *listSize = rankSizeList_[netLayer].size();
 
    return HCCL_SUCCESS;
}
 
bool RankGraphSort(const RankInfo &first, const RankInfo &second)
{
    if (first.serverIdx != second.serverIdx) {
        return first.serverIdx < second.serverIdx;
    } else {
        return first.userRank < second.userRank;
    }
}

HcclResult RankGraph::InitGraphRankInfo()
{
    for (u32 index = 0; index < rankGraph_.size(); index++) {
        struct GraphRankInfo graphRankInfo = {};
        graphRankInfo.rankId = rankGraph_[index].rankId;
        graphRankInfo.localRank = rankGraph_[index].localRank;
        graphRankInfo.serverId = rankGraph_[index].serverId;
        graphRankInfo.serverIdx = rankGraph_[index].serverIdx;
        graphRankInfo.superDeviceId = rankGraph_[index].superDeviceId;
        graphRankInfo.superPodId = rankGraph_[index].superPodId;
        graphRankInfo.superPodIdx = rankGraph_[index].superPodIdx;
        graphRankInfo.hostPort = rankGraph_[index].hostPort;
        graphRankInfo.nodeId = rankGraph_[index].nodeId;
        graphRankInfo.itemId = rankGraph_[index].itemId;
        graphRankInfo.deviceInfo.devicePhyId = rankGraph_[index].deviceInfo.devicePhyId;
        graphRankInfo.deviceInfo.deviceType = rankGraph_[index].deviceInfo.deviceType;
        graphRankInfo.deviceInfo.port = rankGraph_[index].deviceInfo.port;
        graphRankInfo.deviceInfo.vnicPort = rankGraph_[index].deviceInfo.vnicPort;
        graphRankInfo.deviceInfo.backupPort = rankGraph_[index].deviceInfo.backupPort;
        graphRankInfo.bindDeviceId = rankGraph_[index].bindDeviceId;
        graphRankInfo.originalSuperPodId = rankGraph_[index].originalSuperPodId;
        
        graphRankInfo_.push_back(graphRankInfo);
    }

    return HCCL_SUCCESS;
}

HcclResult RankGraph::GetRankGraphInfo(GraphType type, void **graph, uint32_t *len)
{
    switch (type) {
        case RANK_GRAPH_910_93: {
            *graph = graphRankInfo_.data();
            *len = graphRankInfo_.size() * sizeof(GraphRankInfo);
            break;
        }
        default: {
            HCCL_ERROR("[RankGraph][%s]Graph type[%d] is invalid", __func__, type);
            return HCCL_E_NOT_SUPPORT;
        }
    }
    return HCCL_SUCCESS;
}
 
HcclResult RankGraph::InitRankInfo()
{
    auto& rankInfoList = topoAttr_.rankInfoList;
    for (u32 index = 0; index < rankInfoList.size(); index++) {
        if (topoAttr_.userRank == rankInfoList[index].userRank) {
            rankData_ = rankInfoList[index];
            break;
        }
    }
    CHK_RET(InitServerRankInfo());
    CHK_RET(InitSuperPodRankInfo());
    CHK_RET(InitGraphRankInfo());
    return HCCL_SUCCESS;
}

HcclResult RankGraph::InitServerRankInfo()
{
    u32 serverIdx = 0;
    auto& rankInfoList = topoAttr_.rankInfoList;
    for (u32 index = 0; index < rankInfoList.size(); index++) {
        serverIdx = rankInfoList[index].serverIdx;
        auto itServer = serverToRank_.find(serverIdx);
        if (itServer != serverToRank_.end()) {  
            itServer->second.push_back(rankInfoList[index]);
        } else {
            std::vector<RankInfo> rankVecTmp;
            rankVecTmp.push_back(rankInfoList[index]);
            serverToRank_.insert(std::make_pair(serverIdx, rankVecTmp));
        }
    }
    // 调整每个server内的user_rank排序(server内userRank从小到大,一定连续)
    for (auto iterMap = serverToRank_.begin(); iterMap != serverToRank_.end(); iterMap++) {
        if (!(iterMap->second).empty()) {
            std::sort(iterMap->second.begin(), iterMap->second.end(), RankGraphSort);
        }
    }
    serverIdx = rankData_.serverIdx;
    auto rankVec = serverToRank_.find(serverIdx);
    if (rankVec != serverToRank_.end()) {
        std::string rankIdListServer;
        for (auto iter : serverToRank_[serverIdx]) {
            rankIdListServer += std::to_string(iter.userRank) + " ";
        }
        HCCL_INFO("[RankGraph][%s] devtype[%d], curRank[%u], serverToRanklist[%s]", __func__,
            topoAttr_.deviceType, rankData_.userRank, rankIdListServer.c_str());
    }
    return HCCL_SUCCESS;
}

HcclResult RankGraph::InitSuperPodRankInfo()
{
    auto& rankInfoList = topoAttr_.rankInfoList;
    for (u32 index = 0; index < rankInfoList.size(); index++) {
        // 填充superPodRankMap_, 记录superPodId -> rankInfo
        HCCL_DEBUG("[RankGraph][%s] superPodIdx[%d],superPodId[%s]", __func__, 
            rankInfoList[index].superPodIdx, rankInfoList[index].superPodId.c_str());
        auto itSuperPod = superPodToRank_.find(rankInfoList[index].superPodIdx);
        if (itSuperPod != superPodToRank_.end()) {
            itSuperPod->second.push_back(rankInfoList[index]);
        } else {
            std::vector<RankInfo> rankVecTmp;
            rankVecTmp.push_back(rankInfoList[index]);
            superPodToRank_.insert(std::make_pair(rankInfoList[index].superPodIdx, rankVecTmp));
        }
    }
 
    // 调整每个superPod内的user_rank排序, 按照serverIdx从小到大、userRank从小到大排序
    for (auto iterMap = superPodToRank_.begin(); iterMap != superPodToRank_.end(); iterMap++) {
        if (!(iterMap->second).empty()) {
            std::sort(iterMap->second.begin(), iterMap->second.end(), RankGraphSort);
        }
    }
    
    if (superPodToRank_.find(rankData_.superPodIdx) != superPodToRank_.end()) {
        std::string rankIdListPod;
        for (auto iter : superPodToRank_[rankData_.superPodIdx]) {
            rankIdListPod += std::to_string(iter.userRank) + " ";
        }
        HCCL_INFO("[RankGraph][%s] curRank[%d], curSuperPod[%s] superPodToRanklist[%s]",
            __func__, rankData_.userRank, rankData_.superPodId.c_str(), rankIdListPod.c_str());
    }
    return HCCL_SUCCESS;
}

HcclResult RankGraph::InitNetLayer()
{
    netLayer_.clear();
    netLayer_.push_back(static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0));
 
    u32 serverIdx = 0;
    serverIdx = rankData_.serverIdx;
    auto rankVec = serverToRank_.find(serverIdx);
    if (rankVec == serverToRank_.end()) {
        HCCL_ERROR("[RankGraph][%s]find serverToRank failed, serverIdx[%u]", __func__, serverIdx);
        return HCCL_E_INTERNAL;
    }
    std::vector<u32> rankListTmp;
    for (auto iter : serverToRank_[serverIdx]) {
        rankListTmp.push_back(iter.userRank);
    }
    rankList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0), rankListTmp});
 
    std::vector<u32> rankSizeListTmp;
    for (auto iter : serverToRank_) {
        rankSizeListTmp.push_back(iter.second.size());
    }
    rankSizeList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L0), rankSizeListTmp});
 
    DevType deviceType = topoAttr_.deviceType;
    if (serverToRank_.size() > 1) {
        netLayer_.push_back(static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1));
        if (deviceType == DevType::DEV_TYPE_910B || deviceType == DevType::DEV_TYPE_910) {
            std::vector<u32> rankListTmp1;
            for (auto& pair : serverToRank_) {
                for (auto iter : pair.second) {
                    rankListTmp1.push_back(iter.userRank);
                }
            }
            rankList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1), rankListTmp1});
            rankSizeList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1), {topoAttr_.userRankSize}});
        } else if (deviceType == DevType::DEV_TYPE_910_93) {
            auto it = superPodToRank_.find(rankData_.superPodIdx);
            if (it == superPodToRank_.end()) {
                HCCL_ERROR("[rankGraph][%s]find superPodToRank_ failed, superPodIdx[%u]", __func__, rankData_.superPodIdx);
                return HCCL_E_INTERNAL;
            }
            std::vector<u32> rankListTmp1;
            for (auto iter : superPodToRank_[rankData_.superPodIdx]) {
                rankListTmp1.push_back(iter.userRank);
            }
            std::vector<u32> rankSizeListTmp1;
            for (auto iter : superPodToRank_) {
                rankSizeListTmp1.push_back(iter.second.size());
            }
            rankList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1), rankListTmp1});
            rankSizeList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L1), rankSizeListTmp1});
        }
    }
 
    if (deviceType == DevType::DEV_TYPE_910_93 && superPodToRank_.size() > 1) {
        netLayer_.push_back(static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L2));
        std::vector<u32> rankListTmp2;
        for (const auto& pair : superPodToRank_) {
           for (auto iter : pair.second) {
               rankListTmp2.push_back(iter.userRank);
           }
        }
        rankList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L2), rankListTmp2});
        rankSizeList_.insert({static_cast<uint32_t>(HcclNetLayerlevel::HCCL_NetLayer_L2), {topoAttr_.userRankSize}});
    }
    return HCCL_SUCCESS;
}
};
