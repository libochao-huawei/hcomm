/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "topoinfo_ranktable_partition.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "topoinfo_ranktableParser_pub.h"

namespace hccl {



TopoinfoRanktablePartition::TopoinfoRanktablePartition(hccl::HcclCommParams &globalParams,
    hccl::RankTable_t &globalRankTable)
    : globalParams_(globalParams), globalRankTable_(globalRankTable)
{
}

TopoinfoRanktablePartition::~TopoinfoRanktablePartition()
{
}

HcclResult TopoinfoRanktablePartition::GenerateSubRankTable(const uint32_t rankNum, const uint32_t *rankIds,
    hccl::RankTable_t &subRankTable)
{
    subRankTable.version = globalRankTable_.version;
    subRankTable.taskId = globalRankTable_.taskId;
    subRankTable.collectiveId = globalRankTable_.collectiveId;
    subRankTable.nicDeploy = globalRankTable_.nicDeploy;

    std::unordered_map<uint32_t, size_t> rankInfoMap;
    for (size_t i = 0; i < globalRankTable_.rankList.size(); i++) {
        auto rankId = globalRankTable_.rankList[i].rankId;
        rankInfoMap[rankId] = i;
    }

    std::unordered_map<std::string, u32> serverIdMap;
    std::unordered_map<std::string, u32> superPodIdMap;
    std::unordered_set<uint32_t> rankIdSet;
    subRankTable.deviceNum = 0;
    for (size_t i = 0; i < rankNum; i++) {
        CHK_PTR_NULL(rankIds + i);
        uint32_t rankId = rankIds[i];
        CHK_PRT_RET(rankIdSet.find(rankId) != rankIdSet.end(),
            HCCL_ERROR("[TopoinfoRanktablePartition][GenerateSubRankTable]errNo[0x%016llx], duplicated rankId[%u] in rankIds.",
                HCCL_ERROR_CODE(HCCL_E_PARA), rankId),
            HCCL_E_PARA);

        auto iter = rankInfoMap.find(rankId);
        CHK_PRT_RET(iter == rankInfoMap.end(),
            HCCL_ERROR("[TopoinfoRanktablePartition][GenerateSubRankTable]errNo[0x%016llx], fail to find target rank[%u] in the global communicator.",
                HCCL_ERROR_CODE(HCCL_E_PARA), rankId),
            HCCL_E_PARA);

        hccl::RankInfo_t rankInfo = globalRankTable_.rankList[iter->second];
        serverIdMap.emplace(rankInfo.serverId, serverIdMap.size());
        superPodIdMap.emplace(rankInfo.superPodId, superPodIdMap.size());

        rankInfo.rankId = i;
        rankInfo.serverIdx = serverIdMap[rankInfo.serverId];
        rankInfo.superPodIdx = superPodIdMap[rankInfo.superPodId];
        subRankTable.rankList.emplace_back(rankInfo);
        rankIdSet.insert(rankId);

        if (rankInfo.deviceInfo.devicePhyId != HOST_DEVICE_ID) {
            subRankTable.deviceNum++;
        }
        HCCL_INFO("[TopoinfoRanktablePartition][GenerateSubRankTable]Pick rank[%u] from global comm as rank[%u] in sub comm, severId[%s], serverIdx[%u], superPodId[%s], superDeviceId[%u], devicePhyId[%d].",
            rankId, i, rankInfo.serverId.c_str(), rankInfo.serverIdx, rankInfo.superPodId.c_str(),
            rankInfo.superDeviceId, rankInfo.deviceInfo.devicePhyId);
    }
    CHK_RET(GenerateSubSuperPodId(subRankTable));
    subRankTable.serverNum = serverIdMap.size();
    subRankTable.rankNum = rankNum;

    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::GenerateSubSuperPodId(hccl::RankTable_t &subRankTable)
{
    std::map<std::string, std::vector<RankInfo_t *>> podGroupClusters;
    for (auto &rankInfo : subRankTable.rankList) {
        podGroupClusters[rankInfo.originalSuperPodId].emplace_back(&rankInfo);
    }
    std::set<std::string> superPodIdSet;
    std::map<std::string, std::pair<u32, u32>> superPodIdRanges;
    for (auto &subCluster : podGroupClusters) {
        auto &subClusterInfo = subCluster.second;
        superPodIdSet.insert(subCluster.first);
        if (!subClusterInfo.empty()) {
            superPodIdRanges[subCluster.first] = {subClusterInfo.front()->rankId, subClusterInfo.back()->rankId};
        }
        if (subClusterInfo.size() <= 1) {
            continue;
        }
        u32 groupId = 0;
        RankInfo_t preRank = *(subClusterInfo[0]);
        superPodIdRanges[preRank.superPodId] = {preRank.rankId, preRank.rankId};
        for (u32 i = 1; i < subClusterInfo.size(); ++i) {
            RankInfo_t &curRank = *(subClusterInfo[i]);
            if (curRank.rankId != preRank.rankId + 1) {
                std::string newSuperPodId = curRank.originalSuperPodId + "_HCCLSPLIT_" + std::to_string(groupId);
                curRank.superPodId = newSuperPodId;
                groupId++;
                superPodIdRanges[curRank.superPodId] = {curRank.rankId, curRank.rankId};
            } else {
                curRank.superPodId = preRank.superPodId;
                superPodIdRanges[curRank.superPodId].second = curRank.rankId;
            }
            superPodIdSet.insert(curRank.superPodId);
            preRank = curRank;
        }
    }

    for (const auto &entry : superPodIdRanges) {
        const auto &superPodId = entry.first;
        if (superPodId.find("_HCCLSPLIT_") != std::string::npos) {
            const auto &range = entry.second;
            HCCL_RUN_INFO("[TopoinfoRanktablePartition][%s]Split superPod, ID[%s], rank range[%u, %u]",
                __func__, superPodId.c_str(), range.first, range.second);
        }
    }
    subRankTable.superPodNum = superPodIdSet.size();
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::GenerateSubParams(const hccl::RankTable_t &subRankTable,
    const uint32_t subCommRankId, hccl::HcclCommParams &subParams)
{
    subParams.rank = subCommRankId;
    subParams.userRank = subRankTable.rankList[subCommRankId].rankId;
    subParams.totalRanks = subRankTable.rankList.size();
    subParams.logicDevId = globalParams_.logicDevId;
    subParams.serverId = subRankTable.rankList[subCommRankId].serverId;
    subParams.deviceType = globalParams_.deviceType;
    subParams.commPortConfig.devPortSwitchOn = globalParams_.commPortConfig.devPortSwitchOn;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::GetRankTableStr(const hccl::RankTable_t &subRankTable, std::string &rankTableStr)
{
    nlohmann::json basicJson;
    HcclResult ret = Struct2JsonRankTable(subRankTable, globalParams_.deviceType, basicJson);
    CHK_PRT_RET(ret != HCCL_SUCCESS, HCCL_RUN_WARNING("cluster info to json failed, ret[%d].", ret), HCCL_E_INTERNAL);
    rankTableStr = std::move(basicJson.dump());
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformLevelList(const RankInfo_t &rankInfo, nlohmann::json &levelListJson)
{
    for (const auto &levelInfo : rankInfo.levelList) {
        nlohmann::json levelJson = nlohmann::json::object();
        levelJson[hccl::PROP_NET_LAYER] = levelInfo.netLayer;
        levelJson[hccl::PROP_NET_INSTANCE_ID] = levelInfo.netInstanceId;
        levelJson[hccl::PROP_NET_TYPE] = levelInfo.netType;
        levelJson[hccl::PROP_NET_ATTR] = levelInfo.netAttr;

        nlohmann::json rankAddrListJson = nlohmann::json::array();
        for (const auto &addrInfo : levelInfo.rankAddrList) {
            nlohmann::json addrJson = nlohmann::json::object();
            addrJson[hccl::PROP_ADDR] = addrInfo.addr;
            addrJson[hccl::PROP_ADDR_TYPE] = addrInfo.addrType;
            addrJson["plane_id"] = addrInfo.planeId;
            addrJson[hccl::PROP_PORTS] = addrInfo.ports;
            rankAddrListJson.push_back(addrJson);
        }

        levelJson[hccl::PROP_RANK_ADDR_LIST] = rankAddrListJson;
        levelListJson.push_back(levelJson);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformRankInfo(const RankTable_t &clusterInfo,
    nlohmann::json &perRankJson, u32 rankIndex)
{
    const auto &rankInfo = clusterInfo.rankList[rankIndex];
    perRankJson["local_id"] = std::to_string(rankInfo.localRank);
    perRankJson[PROP_HOST_IP] = std::string(rankInfo.hostIp.GetReadableIP());
    perRankJson[PROP_DEV_ID] = std::to_string(rankInfo.deviceInfo.devicePhyId);
    perRankJson[PROP_DEV_NIC_PORT] = std::to_string(rankInfo.deviceInfo.port);
    perRankJson[PROP_DEV_VNIC_PORT] = std::to_string(rankInfo.deviceInfo.vnicPort);
    perRankJson[PROP_BACKUP_DEV_PORT] = std::to_string(rankInfo.deviceInfo.backupPort);
    perRankJson[PROP_RANK_ID] = std::to_string(rankInfo.rankId);
    perRankJson[PROP_SERVER_ID] = rankInfo.serverId;
    perRankJson[PROP_SUPER_POD_ID] = rankInfo.superPodId;
    perRankJson[PROP_SUPER_DEVICE_ID] = std::to_string(rankInfo.superDeviceId);
    if (clusterInfo.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE && !rankInfo.deviceInfo.deviceIp.empty() &&
        !rankInfo.deviceInfo.deviceIp[0].IsInvalid()) {
        perRankJson[PROP_DEV_IP] = std::string(rankInfo.deviceInfo.deviceIp[0].GetReadableIP());
    }
    if (clusterInfo.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE &&
        !rankInfo.deviceInfo.backupDeviceIp.empty() && !rankInfo.deviceInfo.backupDeviceIp[0].IsInvalid()) {
        perRankJson[PROP_BACKUP_DEV_IP] = std::string(rankInfo.deviceInfo.backupDeviceIp[0].GetReadableIP());
    }
    if (clusterInfo.version == OXC_CLUSTER_VERSION) {
        nlohmann::json levelListJson = nlohmann::json::array();
        CHK_RET(TransformLevelList(rankInfo, levelListJson));
        perRankJson[hccl::PROP_LEVEL_LIST] = levelListJson;
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformServerList(const RankTable_t &clusterInfo,
    nlohmann::json &rankListJson)
{
    for (size_t i = 0; i < clusterInfo.rankList.size(); i++) {
        nlohmann::json perRankJson;
        CHK_RET(TransformRankInfo(clusterInfo, perRankJson, i));
        rankListJson.push_back(perRankJson);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformOxcServerList(const RankTable_t &clusterInfo,
    nlohmann::json &serverListJson)
{
    std::map<std::string, std::vector<const RankInfo_t *>> serverToRanks;
    for (const auto &rankInfo : clusterInfo.rankList) {
        serverToRanks[rankInfo.serverId].push_back(&rankInfo);
    }

    for (const auto &entry : serverToRanks) {
        nlohmann::json serverJson = nlohmann::json::object();
        serverJson[PROP_SERVER_ID] = entry.first;
        if (!entry.second.empty() && !entry.second.front()->hostIp.IsInvalid()) {
            serverJson[PROP_HOST_IP] = std::string(entry.second.front()->hostIp.GetReadableIP());
        }

        auto ranks = entry.second;
        std::sort(ranks.begin(), ranks.end(), [](const RankInfo_t *left, const RankInfo_t *right) {
            return left->localRank < right->localRank;
        });

        nlohmann::json deviceListJson = nlohmann::json::array();
        for (const auto *rankInfo : ranks) {
            nlohmann::json deviceJson = nlohmann::json::object();
            deviceJson[PROP_DEV_ID] = std::to_string(rankInfo->deviceInfo.devicePhyId);
            deviceJson[PROP_SUPER_DEVICE_ID] = std::to_string(rankInfo->superDeviceId);
            deviceJson["rankid"] = std::to_string(rankInfo->rankId);
            if (!rankInfo->deviceInfo.deviceIp.empty() && !rankInfo->deviceInfo.deviceIp[0].IsInvalid()) {
                deviceJson[PROP_DEV_IP] = std::string(rankInfo->deviceInfo.deviceIp[0].GetReadableIP());
            }
            if (!rankInfo->deviceInfo.backupDeviceIp.empty() && !rankInfo->deviceInfo.backupDeviceIp[0].IsInvalid()) {
                deviceJson[PROP_BACKUP_DEV_IP] = std::string(rankInfo->deviceInfo.backupDeviceIp[0].GetReadableIP());
            }
            if (rankInfo->deviceInfo.port != HCCL_INVALID_PORT) {
                deviceJson[PROP_DEV_NIC_PORT] = rankInfo->deviceInfo.port;
            }
            if (rankInfo->deviceInfo.backupPort != HCCL_INVALID_PORT) {
                deviceJson[PROP_BACKUP_DEV_PORT] = rankInfo->deviceInfo.backupPort;
            }
            deviceListJson.push_back(deviceJson);
        }
        serverJson[PROP_DEVICE] = deviceListJson;
        serverListJson.push_back(serverJson);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformOxcSuperPodList(const RankTable_t &clusterInfo,
    nlohmann::json &superPodListJson)
{
    std::map<std::string, std::set<std::string>> superPodToServers;
    for (const auto &rankInfo : clusterInfo.rankList) {
        if (rankInfo.superPodId.empty()) {
            continue;
        }
        superPodToServers[rankInfo.superPodId].insert(rankInfo.serverId);
    }

    for (const auto &entry : superPodToServers) {
        nlohmann::json superPodJson = nlohmann::json::object();
        superPodJson[PROP_SUPER_POD_ID] = entry.first;
        nlohmann::json serverListJson = nlohmann::json::array();
        for (const auto &serverId : entry.second) {
            serverListJson.push_back(nlohmann::json::object({{PROP_SERVER_ID, serverId}}));
        }
        superPodJson[PROP_SERVER_LIST] = serverListJson;
        superPodListJson.push_back(superPodJson);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::TransformOxcGroupList(const RankTable_t &clusterInfo,
    nlohmann::json &groupListJson)
{
    std::map<std::string, std::vector<const RankInfo_t *>> groupToRanks;
    for (const auto &rankInfo : clusterInfo.rankList) {
        groupToRanks[rankInfo.oxcGroupId].push_back(&rankInfo);
    }

    for (const auto &entry : groupToRanks) {
        nlohmann::json groupJson = nlohmann::json::object();
        groupJson["group_id"] = entry.first;

        auto ranks = entry.second;
        std::sort(ranks.begin(), ranks.end(), [](const RankInfo_t *left, const RankInfo_t *right) {
            return left->rankId < right->rankId;
        });

        nlohmann::json rankListJson = nlohmann::json::array();
        for (const auto *rankInfo : ranks) {
            rankListJson.push_back(nlohmann::json::object({{PROP_RANK_ID, std::to_string(rankInfo->rankId)}}));
        }
        groupJson[PROP_RANK_LIST] = rankListJson;
        groupListJson.push_back(groupJson);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktablePartition::Struct2JsonRankTable(const RankTable_t &clusterInfo, const DevType deviceType,
    nlohmann::json &ClusterJson)
{
    ClusterJson[PROP_STATUS] = "completed";
    if (clusterInfo.version == OXC_CLUSTER_VERSION) {
        ClusterJson[PROP_VERSION] = OXC_CLUSTER_VERSION;
        nlohmann::json serverListJson = nlohmann::json::array();
        nlohmann::json superPodListJson = nlohmann::json::array();
        nlohmann::json groupListJson = nlohmann::json::array();
        CHK_RET(TransformOxcServerList(clusterInfo, serverListJson));
        CHK_RET(TransformOxcSuperPodList(clusterInfo, superPodListJson));
        CHK_RET(TransformOxcGroupList(clusterInfo, groupListJson));
        ClusterJson[PROP_SERVER_LIST] = serverListJson;
        ClusterJson[PROP_SUPER_POD_LIST] = superPodListJson;
        ClusterJson["oxc_group_list"] = groupListJson;
        if (!clusterInfo.taskId.empty()) {
            ClusterJson[hccl::PROP_TASK_ID] = clusterInfo.taskId;
        }
    } else {
        ClusterJson[PROP_SERVER_COUNT] = std::to_string(clusterInfo.serverNum);
        ClusterJson[PROP_SUPER_POD_NUM] = std::to_string(clusterInfo.superPodNum);
        ClusterJson[PROP_RANK_NUM] = std::to_string(clusterInfo.rankNum);
        ClusterJson[PROP_DEV_NUM] = std::to_string(clusterInfo.deviceNum);

        nlohmann::json rankListJson;
        CHK_RET(TransformServerList(clusterInfo, rankListJson));
        ClusterJson[PROP_RANK_LIST] = rankListJson;
        ClusterJson[PROP_VERSION] = (deviceType == DevType::DEV_TYPE_910_93) ? SUPERPOD_CLUSTER_VERSION : HCCL_CLUSTER_VERSION;
    }
    return HCCL_SUCCESS;
}
} // namespace hccl
