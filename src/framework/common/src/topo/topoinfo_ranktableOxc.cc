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

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "config.h"
#include "log.h"
#include "sal_pub.h"
#include "workflow_pub.h"

using namespace hccl;

namespace {
constexpr char PROP_LEVEL[] = "level";
constexpr char PROP_ID[] = "id";
constexpr char PROP_FABRIC_TYPE[] = "fabric_type";
constexpr char PROP_RANK_ADDR_TYPE[] = "rank_addr_type";

// 统一处理 OXC 2.0 中“可选字符串字段”的读取。
// 返回 false 表示字段缺失或类型不匹配，由调用方决定是否接受缺省行为。
bool GetStringField(const nlohmann::json &obj, const char *propName, std::string &value, bool optional = false)
{
    auto iter = obj.find(propName);
    if (iter == obj.end()) {
        return optional;
    }
    if (!iter->is_string()) {
        return false;
    }
    value = *iter;
    return true;
}

// OXC 2.0 的部分字段可能是数字，也可能被序列化成字符串。
// 这里统一做 u32 归一化，避免每个字段单独处理类型分支。
HcclResult ParseU32FromJsonValue(const nlohmann::json &value, const char *propName, u32 &result)
{
    if (value.is_number_unsigned()) {
        u64 tmp = value.get<u64>();
        CHK_PRT_RET(tmp > std::numeric_limits<u32>::max(),
            HCCL_ERROR("[OXC_HCOMM][Parse][%s] value[%llu] exceeds u32 range.", propName, tmp), HCCL_E_PARA);
        result = static_cast<u32>(tmp);
        return HCCL_SUCCESS;
    }

    if (value.is_number_integer()) {
        s64 tmp = value.get<s64>();
        CHK_PRT_RET(tmp < 0 || tmp > static_cast<s64>(std::numeric_limits<u32>::max()),
            HCCL_ERROR("[OXC_HCOMM][Parse][%s] value[%lld] exceeds u32 range.", propName, tmp), HCCL_E_PARA);
        result = static_cast<u32>(tmp);
        return HCCL_SUCCESS;
    }

    if (value.is_string()) {
        return SalStrToULong(value.get<std::string>(), HCCL_BASE_DECIMAL, result);
    }

    HCCL_ERROR("[OXC_HCOMM][Parse][%s] value type is invalid.", propName);
    return HCCL_E_PARA;
}

// 用于 rank_count / rank_id / device_id 这类必填字段的读取。
HcclResult GetRequiredU32Field(const nlohmann::json &obj, const char *propName, u32 &value)
{
    auto iter = obj.find(propName);
    CHK_PRT_RET(iter == obj.end(), HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is missing.", propName), HCCL_E_NOT_FOUND);
    return ParseU32FromJsonValue(*iter, propName, value);
}

// 用于 task_id / net_type / plane_id 这类必填字符串字段的读取。
HcclResult GetRequiredStringField(const nlohmann::json &obj, const char *propName, std::string &value)
{
    auto iter = obj.find(propName);
    CHK_PRT_RET(iter == obj.end(), HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is missing.", propName),
        HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!iter->is_string(), HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is not string.", propName),
        HCCL_E_PARA);
    value = *iter;
    return HCCL_SUCCESS;
}

// Optional 数值字段统一入口；字段不存在时由调用方决定是否采用默认值。
HcclResult GetOptionalU32Field(const nlohmann::json &obj, const char *propName, u32 &value)
{
    auto iter = obj.find(propName);
    if (iter == obj.end()) {
        return HCCL_E_NOT_FOUND;
    }
    return ParseU32FromJsonValue(*iter, propName, value);
}

// OXC 2.0 第一阶段仍缺少正式的“server 分组”消费链，
// 因此这里通过 level=1 的 netInstanceId 生成一个稳定的内部 serverId，
// 让现有 ranktable 校验和后续 params 填充能够继续工作。
std::string DeriveServerId(const RankInfo_t &rankInfo)
{
    for (const auto &levelInfo : rankInfo.levelList) {
        if (levelInfo.netLayer == 1 && !levelInfo.netInstanceId.empty()) {
            return levelInfo.netInstanceId;
        }
    }
    return "default_server";
}
}

TopoinfoRanktableOxc::TopoinfoRanktableOxc(const std::string &rankTableM, const std::string &identify)
    : TopoInfoRanktableParser(rankTableM, identify)
{
}

TopoinfoRanktableOxc::~TopoinfoRanktableOxc()
{
}

HcclResult TopoinfoRanktableOxc::Init()
{
    // 与其他 ranktable parser 保持一致：先把字符串载入 json，再执行一次完整解析。
    CHK_RET(LoadRankTableString(rankTableFile_));
    CHK_RET(ParserClusterInfo(params_, rankTable_));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::GetClusterInfo(RankTable_t &clusterInfo)
{
    clusterInfo = rankTable_;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::GetClusterInfo(HcclCommParams &params, RankTable_t &rankTable)
{
    params = params_;
    rankTable = rankTable_;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParserClusterInfo(HcclCommParams &params, RankTable_t &rankTable)
{
    // 第一阶段在 parser 内部直接完成 deviceType 获取和 identify 校验，
    // 目标是对齐 Concise/Heterog parser 的输出行为，确保能产出 params + rankTable。
    if (!IsTaskNumCalMode()) {
        CHK_RET(hrtGetDeviceType(params.deviceType));
    }

    rankTable.version = OXC_CLUSTER_VERSION;
    CHK_RET(ParseTaskId(rankTable));
    CHK_RET(ParseRankList(rankTable));

    // 后续校验逻辑默认 rankList 已按 rankId 升序排列，因此这里在 parser 末尾统一排序。
    std::sort(rankTable.rankList.begin(), rankTable.rankList.end(),
        [](const RankInfo_t &left, const RankInfo_t &right) { return left.rankId < right.rankId; });
    CHK_RET(CheckRankListInfo(rankTable.rankList));

    if (IsTaskNumCalMode()) {
        return HCCL_SUCCESS;
    }

    // 与 legacy parser 一样，当前 identify 仍按“当前 rank 的字符串编号”解释，
    // 第一阶段不改变这一路径的外部语义。
    u32 rankId = INVALID_VALUE_RANKID;
    CHK_PRT_RET(IsAllDigit(identify_.c_str()) != HCCL_SUCCESS ||
        SalStrToULong(identify_, HCCL_BASE_DECIMAL, rankId) != HCCL_SUCCESS,
        HCCL_ERROR("[OXC_HCOMM][ParserClusterInfo] rank_id[%s] is invalid.", identify_.c_str()), HCCL_E_PARA);
    CHK_PRT_RET(rankId >= rankTable.rankList.size(),
        HCCL_ERROR("[OXC_HCOMM][ParserClusterInfo] rank_id[%u] exceeds rank size[%zu].",
            rankId, rankTable.rankList.size()),
        HCCL_E_PARA);

    CHK_RET(hrtGetDevice(&params.logicDevId));
    u32 devicePhyId = 0;
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(params.logicDevId), devicePhyId));
    CHK_PRT_RET(devicePhyId != static_cast<u32>(rankTable.rankList[rankId].deviceInfo.devicePhyId),
        HCCL_ERROR("[OXC_HCOMM][ParserClusterInfo] ranktable devId[%d] mismatch local devId[%u].",
            rankTable.rankList[rankId].deviceInfo.devicePhyId, devicePhyId), HCCL_E_UNAVAIL);

    params.rank = rankId;
    params.userRank = rankId;
    params.serverId = rankTable.rankList[rankId].serverId;
    params.totalRanks = rankTable.rankNum;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseTaskId(RankTable_t &rankTable)
{
    auto iter = fileContent_.find(PROP_TASK_ID);
    if (iter == fileContent_.end()) {
        // task_id 是 OXC 2.0 的可选字段；缺失时不阻断 parser 完成。
        rankTable.taskId.clear();
        return HCCL_SUCCESS;
    }
    CHK_PRT_RET(!iter->is_string(), HCCL_ERROR("[OXC_HCOMM][Parse][task_id] field is not string."), HCCL_E_PARA);
    rankTable.taskId = *iter;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseRankList(RankTable_t &rankTable)
{
    u32 rankCount = 0;
    CHK_RET(GetRequiredU32Field(fileContent_, "rank_count", rankCount));

    auto rankListIter = fileContent_.find(PROP_RANK_LIST);
    CHK_PRT_RET(rankListIter == fileContent_.end(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_list] field is missing."),
        HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!rankListIter->is_array(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_list] field is not array."),
        HCCL_E_PARA);

    rankTable.rankList.clear();
    rankTable.rankList.reserve(rankListIter->size());
    std::set<std::string> serverIds;

    for (const auto &rankObj : *rankListIter) {
        CHK_PRT_RET(!rankObj.is_object(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_list] rank item is not object."),
            HCCL_E_PARA);
        RankInfo_t rankInfo;
        rankInfo.deviceInfo.deviceIp.push_back(HcclIpAddress());
        CHK_RET(ParseSingleRank(rankObj, rankInfo));
        // 如果输入中没有显式 server_id，就从 level_list 推导一个稳定 server 标识，
        // 以兼容现有 RankTable_t 的 server 维度校验逻辑。
        rankInfo.serverId = rankInfo.serverId.empty() ? DeriveServerId(rankInfo) : rankInfo.serverId;
        GenerateServerIdx(rankInfo.serverId, rankInfo.serverIdx);
        serverIds.insert(rankInfo.serverId);
        rankTable.rankList.push_back(rankInfo);
    }

    // rank_count 与 rank_list 大小必须一致；这是 OXC 2.0 parser 的关键一致性校验。
    CHK_PRT_RET(rankTable.rankList.size() != rankCount,
        HCCL_ERROR("[OXC_HCOMM][Parse][rank_count] expect[%u] actual[%zu] mismatch.",
            rankCount, rankTable.rankList.size()),
        HCCL_E_PARA);

    rankTable.rankNum = static_cast<u32>(rankTable.rankList.size());
    rankTable.serverNum = serverIds.empty() ? 1U : static_cast<u32>(serverIds.size());
    rankTable.superPodNum = 0;
    rankTable.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    CHK_RET(GetDevNum(rankTable.rankList, rankTable.deviceNum));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseSingleRank(const nlohmann::json &rankObj, RankInfo_t &rankInfo)
{
    CHK_RET(GetRequiredU32Field(rankObj, PROP_RANK_ID.c_str(), rankInfo.rankId));
    CHK_RET(GetRequiredU32Field(rankObj, "local_id", rankInfo.localRank));

    u32 deviceId = 0;
    CHK_RET(GetRequiredU32Field(rankObj, PROP_DEV_ID.c_str(), deviceId));
    rankInfo.deviceInfo.devicePhyId = static_cast<s32>(deviceId);
    rankInfo.deviceInfo.deviceType = params_.deviceType;

    if (GetStringField(rankObj, PROP_SERVER_ID.c_str(), rankInfo.serverId, true) == false &&
        rankObj.find(PROP_SERVER_ID) != rankObj.end()) {
        return HCCL_E_PARA;
    }

    // 设备端口在第一阶段不是必填项；如果输入提供则保存，不提供则保留默认值。
    u32 devicePort = 0;
    if (GetOptionalU32Field(rankObj, PROP_DEV_NIC_PORT.c_str(), devicePort) == HCCL_SUCCESS) {
        rankInfo.deviceInfo.port = devicePort;
    }

    CHK_RET(ParseLevelList(rankObj, rankInfo));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseLevelList(const nlohmann::json &rankObj, RankInfo_t &rankInfo)
{
    auto iter = rankObj.find(PROP_LEVEL_LIST);
    CHK_PRT_RET(iter == rankObj.end(), HCCL_ERROR("[OXC_HCOMM][Parse][level_list] field is missing."),
        HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!iter->is_array() || iter->empty(), HCCL_ERROR("[OXC_HCOMM][Parse][level_list] field is invalid."),
        HCCL_E_PARA);

    // level_list 是 OXC 2.0 与 legacy ranktable 的主要差异之一；
    // 这里逐层解析，但暂不在 parser 阶段引入 netplane/CommPlane 消费逻辑。
    rankInfo.levelList.clear();
    rankInfo.levelList.reserve(iter->size());
    for (const auto &levelObj : *iter) {
        CHK_PRT_RET(!levelObj.is_object(), HCCL_ERROR("[OXC_HCOMM][Parse][level_list] level item is not object."),
            HCCL_E_PARA);
        RankLevelInfoOxc levelInfo;
        if (GetOptionalU32Field(levelObj, PROP_NET_LAYER.c_str(), levelInfo.netLayer) != HCCL_SUCCESS) {
            CHK_RET(GetRequiredU32Field(levelObj, PROP_LEVEL, levelInfo.netLayer));
        }

        if (!GetStringField(levelObj, PROP_NET_INSTANCE_ID.c_str(), levelInfo.netInstanceId, true) &&
            !GetStringField(levelObj, PROP_NET_INST_ID.c_str(), levelInfo.netInstanceId, true) &&
            !GetStringField(levelObj, PROP_ID, levelInfo.netInstanceId, true)) {
            HCCL_ERROR("[OXC_HCOMM][Parse][net_instance_id] field is missing.");
            return HCCL_E_NOT_FOUND;
        }

        if (!GetStringField(levelObj, PROP_NET_TYPE.c_str(), levelInfo.netType, true) &&
            !GetStringField(levelObj, PROP_FABRIC_TYPE, levelInfo.netType, true)) {
            HCCL_ERROR("[OXC_HCOMM][Parse][net_type] field is missing.");
            return HCCL_E_NOT_FOUND;
        }

        if (!GetStringField(levelObj, PROP_NET_ATTR.c_str(), levelInfo.netAttr, true) &&
            levelObj.find(PROP_NET_ATTR) != levelObj.end()) {
            HCCL_ERROR("[OXC_HCOMM][Parse][net_attr] field is invalid.");
            return HCCL_E_PARA;
        }

        std::string defaultAddrType;
        if (!GetStringField(levelObj, PROP_RANK_ADDR_TYPE, defaultAddrType, true) &&
            levelObj.find(PROP_RANK_ADDR_TYPE) != levelObj.end()) {
            HCCL_ERROR("[OXC_HCOMM][Parse][rank_addr_type] field is invalid.");
            return HCCL_E_PARA;
        }

        // rank_addr_list 内的每个地址端点都完整保存在 levelInfo 中，
        // 后续是否映射到平面/拓扑接口由下一阶段处理。
        CHK_RET(ParseRankAddrList(levelObj, levelInfo, defaultAddrType));
        rankInfo.levelList.push_back(levelInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseRankAddrList(const nlohmann::json &levelObj, RankLevelInfoOxc &levelInfo,
    const std::string &defaultAddrType)
{
    auto addrIter = levelObj.find(PROP_RANK_ADDR_LIST);
    if (addrIter == levelObj.end()) {
        addrIter = levelObj.find(PROP_RANK_ADDRS);
    }
    CHK_PRT_RET(addrIter == levelObj.end(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_addr_list] field is missing."),
        HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!addrIter->is_array(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_addr_list] field is not array."),
        HCCL_E_PARA);

    levelInfo.rankAddrList.clear();
    levelInfo.rankAddrList.reserve(addrIter->size());
    for (const auto &addrObj : *addrIter) {
        CHK_PRT_RET(!addrObj.is_object(), HCCL_ERROR("[OXC_HCOMM][Parse][rank_addr_list] addr item is not object."),
            HCCL_E_PARA);
        RankAddrInfoOxc addrInfo;
        CHK_RET(GetRequiredStringField(addrObj, PROP_ADDR.c_str(), addrInfo.addr));
        if (!GetStringField(addrObj, PROP_ADDR_TYPE.c_str(), addrInfo.addrType, true)) {
            CHK_PRT_RET(defaultAddrType.empty(), HCCL_ERROR("[OXC_HCOMM][Parse][addr_type] field is missing."),
                HCCL_E_NOT_FOUND);
            addrInfo.addrType = defaultAddrType;
        }
        // plane_id 是后续 netplane/CommPlane 计算的重要输入，因此在 parser 阶段必须完整保留。
        CHK_RET(GetRequiredStringField(addrObj, PROP_NETWORK_PLANEID.c_str(), addrInfo.planeId));

        auto portsIter = addrObj.find(PROP_PORTS);
        CHK_PRT_RET(portsIter == addrObj.end(), HCCL_ERROR("[OXC_HCOMM][Parse][ports] field is missing."),
            HCCL_E_NOT_FOUND);
        CHK_PRT_RET(!portsIter->is_array(), HCCL_ERROR("[OXC_HCOMM][Parse][ports] field is not array."),
            HCCL_E_PARA);
        for (const auto &portItem : *portsIter) {
            CHK_PRT_RET(!portItem.is_string(), HCCL_ERROR("[OXC_HCOMM][Parse][ports] port item is not string."),
                HCCL_E_PARA);
            addrInfo.ports.push_back(portItem.get<std::string>());
        }
        levelInfo.rankAddrList.push_back(std::move(addrInfo));
    }
    return HCCL_SUCCESS;
}
