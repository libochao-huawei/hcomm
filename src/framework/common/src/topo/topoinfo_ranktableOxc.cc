/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#include "topoinfo_ranktableOxc.h"
#include "topoinfo_ranktableParser_pub.h"
#include "log.h"

namespace hccl {

TopoinfoRanktableOxc::TopoinfoRanktableOxc(const std::string &rankTableM, const std::string &identify)
    : TopoinfoRanktableConcise(rankTableM, identify), taskId_("") {
}

HcclResult TopoinfoRanktableOxc::Init()
{
    // 加载 JSON 字符串到 fileContent_
    CHK_RET(LoadRankTableString(rankTableFile_));
    // 解析 OXC RankTable 格式
    CHK_RET(ParseOxcRankTable(rankTable_));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseOxcRankTable(RankTable_t &rankTable)
{
    const std::string funcName = "[TopoinfoRanktableOxc::ParseOxcRankTable]";

    // 解析 task_id（OXC 特有字段）
    CHK_RET(ParseTaskId(fileContent_, taskId_));
    rankTable.taskId = taskId_;

    // 解析 version 字段
    CHK_RET(GetJsonProperty(fileContent_, "version", rankTable.version));

    // 解析 rank_list
    nlohmann::json rankList;
    CHK_RET(GetJsonProperty(fileContent_, "rank_list", rankList));

    HCCL_INFO("[%s.json] -> rank_list: size:[%zu]", fileName_.c_str(), rankList.size());

    // 遍历 rank_list，填充 rankTable.rankList
    for (u32 index = 0; index < rankList.size(); index++) {
        RankInfo_t rankInfo;

        // 直接访问 JSON 数组元素
        nlohmann::json rankObj = rankList[index];

        // 获取 rank_id（直接从 JSON 对象获取 u32 值）
        if (rankObj.contains("rank_id") && rankObj["rank_id"].is_number_unsigned()) {
            rankInfo.rankId = rankObj["rank_id"].get<u32>();
        }

        // 获取 local_id
        if (rankObj.contains("local_id") && rankObj["local_id"].is_number_unsigned()) {
            rankInfo.localRank = rankObj["local_id"].get<u32>();
        }

        // 获取 device_id
        if (rankObj.contains("device_id") && rankObj["device_id"].is_number_unsigned()) {
            u32 deviceId = rankObj["device_id"].get<u32>();
            rankInfo.deviceInfo.devicePhyId = static_cast<s32>(deviceId);
            rankInfo.bindDeviceId = static_cast<s32>(deviceId);
        }

        // 设置默认值
        rankInfo.serverId = "server_" + std::to_string(index);
        rankInfo.serverIdx = index;

        rankTable.rankList.push_back(rankInfo);
        HCCL_DEBUG("[%s.json] -> rank_id[%u], local_id[%u], device_id[%d]",
            fileName_.c_str(), rankInfo.rankId, rankInfo.localRank, rankInfo.deviceInfo.devicePhyId);
    }

    // 设置 rankNum 和 deviceNum
    rankTable.rankNum = rankTable.rankList.size();
    rankTable.deviceNum = rankTable.rankNum;
    rankTable.serverNum = rankTable.rankNum;

    HCCL_INFO("%s OXC ranktable parsed successfully, task_id=%s, rank_num=%u",
        funcName.c_str(), taskId_.c_str(), rankTable.rankNum);

    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseTaskId(const nlohmann::json &obj, std::string &taskId)
{
    const std::string funcName = "[TopoinfoRanktableOxc::ParseTaskId]";

    // task_id 字段是 OXC 2.0 版本特有的
    if (obj.contains("task_id")) {
        CHK_RET(GetJsonProperty(obj, "task_id", taskId));
        HCCL_INFO("%s parse task_id[%s] successfully", funcName.c_str(), taskId.c_str());
    } else {
        HCCL_INFO("%s task_id not found in ranktable, treat as optional field", funcName.c_str());
        taskId = "";
    }

    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::GetClusterInfo(hccl::HcclCommParams &params, hccl::RankTable_t &rankTable)
{
    const std::string funcName = "[TopoinfoRanktableOxc::GetClusterInfo]";

    // 直接返回已经解析好的 rankTable_
    rankTable.nicDeploy = rankTable_.nicDeploy;
    rankTable.deviceNum = rankTable_.deviceNum;
    rankTable.serverNum = rankTable_.serverNum;
    rankTable.superPodNum = rankTable_.superPodNum;
    rankTable.rankNum = rankTable_.rankNum;
    rankTable.rankList = rankTable_.rankList;
    rankTable.serverList = rankTable_.serverList;
    rankTable.version = rankTable_.version;
    rankTable.taskId = rankTable_.taskId;

    // 填充参数信息
    params.deviceType = params_.deviceType;
    params.rank = params_.rank;
    params.userRank = params_.rank;
    params.logicDevId = params_.logicDevId;
    params.totalRanks = params_.totalRanks;
    params.serverId = params_.serverId;

    HCCL_INFO("%s GetClusterInfo success, version=%s, task_id=%s, rank_num=%u",
        funcName.c_str(), rankTable.version.c_str(), rankTable.taskId.c_str(), rankTable.rankNum);

    return HCCL_SUCCESS;
}

} // namespace hccl
