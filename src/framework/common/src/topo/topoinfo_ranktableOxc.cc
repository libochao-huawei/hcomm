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
#include <array>
#include <limits>
#include <map>
#include <sstream>
#include <set>
#include <unordered_map>
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
constexpr char PROP_RANKID[] = "rankid";
constexpr char PROP_OXC_GROUP_LIST[] = "oxc_group_list";
constexpr char PROP_GROUP_ID[] = "group_id";

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

/**
 * @brief 读取可选字符串字段，并在字段存在但类型非法时返回显式错误。
 * @param obj 输入 json 对象。
 * @param propName 字段名。
 * @param value 输出字符串值。
 * @param found 输出：字段是否存在。
 * @return HcclResult
 */
HcclResult GetOptionalStringFieldChecked(const nlohmann::json &obj, const char *propName, std::string &value, bool &found)
{
    auto iter = obj.find(propName);
    if (iter == obj.end()) {
        found = false;
        value.clear();
        return HCCL_SUCCESS;
    }

    found = true;
    CHK_PRT_RET(!iter->is_string(), HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is not string.", propName),
        HCCL_E_PARA);
    value = *iter;
    return HCCL_SUCCESS;
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

HcclResult GetRequiredU32FieldAny(const nlohmann::json &obj, const char *propNameA, const char *propNameB, u32 &value)
{
    auto iter = obj.find(propNameA);
    if (iter != obj.end()) {
        return ParseU32FromJsonValue(*iter, propNameA, value);
    }

    iter = obj.find(propNameB);
    CHK_PRT_RET(iter == obj.end(),
        HCCL_ERROR("[OXC_HCOMM][Parse][%s|%s] field is missing.", propNameA, propNameB), HCCL_E_NOT_FOUND);
    return ParseU32FromJsonValue(*iter, propNameB, value);
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

std::string FormatPortsForLog(const std::vector<std::string> &ports)
{
    std::ostringstream oss;
    oss << "[";
    for (u32 index = 0; index < ports.size(); ++index) {
        if (index != 0) {
            oss << ",";
        }
        oss << ports[index];
    }
    oss << "]";
    return oss.str();
}

u32 CountOxcGroupNum(const RankTable_t &rankTable)
{
    std::set<std::string> groupIds;
    for (const auto &rankInfo : rankTable.rankList) {
        if (!rankInfo.oxcGroupId.empty()) {
            groupIds.insert(rankInfo.oxcGroupId);
        }
    }
    return static_cast<u32>(groupIds.size());
}

void LogParsedOxcRankTableObservability(const RankTable_t &rankTable)
{
    const bool enableInfo = HcclCheckLogLevel(HCCL_LOG_INFO);
    const bool enableDebug = HcclCheckLogLevel(HCCL_LOG_DEBUG);
    if (!enableInfo && !enableDebug) {
        return;
    }

    const u32 groupNum = CountOxcGroupNum(rankTable);
    if (enableInfo) {
        HCCL_INFO("[OXC_HCOMM][Parse][Summary] rank_num[%u] server_num[%u] super_pod_num[%u] group_num[%u].",
            rankTable.rankNum, rankTable.serverNum, rankTable.superPodNum, groupNum);
        for (const auto &rankInfo : rankTable.rankList) {
            HCCL_INFO("[OXC_HCOMM][Parse][Summary][Rank] rank_id[%u] server_id[%s] super_pod_id[%s] super_device_id[%u] group_id[%s].",
                rankInfo.rankId, rankInfo.serverId.c_str(), rankInfo.superPodId.c_str(),
                rankInfo.superDeviceId, rankInfo.oxcGroupId.c_str());
        }
    }

    if (enableDebug) {
        for (const auto &rankInfo : rankTable.rankList) {
            HCCL_DEBUG("[OXC_HCOMM][Parse][Detail][Rank] rank_id[%u] server_id[%s] super_pod_id[%s] super_device_id[%u] group_id[%s] level_source[synthesized].",
                rankInfo.rankId, rankInfo.serverId.c_str(), rankInfo.superPodId.c_str(), rankInfo.superDeviceId,
                rankInfo.oxcGroupId.c_str());
            for (const auto &levelInfo : rankInfo.levelList) {
                HCCL_DEBUG("[OXC_HCOMM][Parse][Detail][Level] rank_id[%u] net_layer[%u] net_type[%s] net_instance_id[%s].",
                    rankInfo.rankId, levelInfo.netLayer, levelInfo.netType.c_str(), levelInfo.netInstanceId.c_str());
                for (const auto &addrInfo : levelInfo.rankAddrList) {
                    HCCL_DEBUG("[OXC_HCOMM][Parse][Detail][RankAddr] rank_id[%u] net_layer[%u] addr[%s] addr_type[%s] plane_id[%s] ports%s.",
                        rankInfo.rankId, levelInfo.netLayer, addrInfo.addr.c_str(), addrInfo.addrType.c_str(),
                        addrInfo.planeId.c_str(), FormatPortsForLog(addrInfo.ports).c_str());
                }
            }
            HCCL_DEBUG("[OXC_HCOMM][Parse][Detail][Ports] rank_id[%u] device_port[%u] backup_device_port[%u].",
                rankInfo.rankId, rankInfo.deviceInfo.port, rankInfo.deviceInfo.backupPort);
        }
    }
}

void SynthesizeLevelList(RankInfo_t &rankInfo)
{
    rankInfo.levelList.clear();

    RankLevelInfoOxc level0;
    level0.netLayer = 0;
    level0.netInstanceId = rankInfo.serverId + "-l0";
    level0.netType = "TOPO_FILE_DESC";
    level0.netAttr.clear();
    level0.rankAddrList.push_back({rankInfo.serverId, "IPV4",
        rankInfo.serverId + "-dev" + std::to_string(rankInfo.localRank), {}});
    rankInfo.levelList.push_back(level0);

    RankLevelInfoOxc level1;
    level1.netLayer = 1;
    level1.netInstanceId = rankInfo.oxcGroupId;
    level1.netType = "CLOS";
    level1.netAttr.clear();
    level1.rankAddrList.push_back({rankInfo.serverId, "IPV4", rankInfo.oxcGroupId, {}});
    rankInfo.levelList.push_back(level1);

    RankLevelInfoOxc level2;
    level2.netLayer = 2;
    level2.netInstanceId = "0";
    level2.netType = "OXC_Mesh";
    level2.netAttr.clear();
    level2.rankAddrList.push_back({rankInfo.serverId, "IPV4", rankInfo.oxcGroupId, {}});
    rankInfo.levelList.push_back(level2);

    RankLevelInfoOxc level3;
    level3.netLayer = 3;
    level3.netInstanceId = "CLUSTER1";
    level3.netType = "CLOS";
    level3.netAttr.clear();
    level3.rankAddrList.push_back({rankInfo.serverId, "IPV4", "CLUSTER", {}});
    rankInfo.levelList.push_back(level3);
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

HcclResult TopoinfoRanktableOxc::SetIsInterSuperPodRetryEnable(bool isRetryEnable)
{
    isInterSuperPodRetryEnable_ = isRetryEnable;
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
        LogParsedOxcRankTableObservability(rankTable);
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
    LogParsedOxcRankTableObservability(rankTable);
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
    rankTable.rankList.clear();
    CHK_RET(ParseServerList(rankTable));
    CHK_RET(ParseSuperPodList(rankTable));
    CHK_RET(ParseOxcGroupList(rankTable));
    for (auto &rankInfo : rankTable.rankList) {
        SynthesizeLevelList(rankInfo);
    }

    std::sort(rankTable.rankList.begin(), rankTable.rankList.end(),
        [](const RankInfo_t &left, const RankInfo_t &right) { return left.rankId < right.rankId; });
    CHK_RET(CheckRankListInfo(rankTable.rankList));

    u32 rankCount = 0;
    HcclResult rankCountRet = GetOptionalU32Field(fileContent_, "rank_count", rankCount);
    CHK_PRT_RET(rankCountRet != HCCL_SUCCESS && rankCountRet != HCCL_E_NOT_FOUND,
        HCCL_ERROR("[OXC_HCOMM][Parse][rank_count] field is invalid."), rankCountRet);
    if (rankCountRet == HCCL_SUCCESS) {
        CHK_PRT_RET(rankTable.rankList.size() != rankCount,
            HCCL_ERROR("[OXC_HCOMM][Parse][rank_count] expect[%u] actual[%zu] mismatch.",
                rankCount, rankTable.rankList.size()),
            HCCL_E_PARA);
    }

    if (params_.deviceType == DevType::DEV_TYPE_910_93) {
        CHK_RET(ValidateAndFinalizeA3RankInfo(rankTable));
    }

    rankTable.rankNum = static_cast<u32>(rankTable.rankList.size());
    rankTable.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    CHK_RET(GetDevNum(rankTable.rankList, rankTable.deviceNum));
    CHK_RET(ValidateA3ParameterPlaneContract(rankTable));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseServerList(RankTable_t &rankTable)
{
    auto serverListIter = fileContent_.find(PROP_SERVER_LIST);
    CHK_PRT_RET(serverListIter == fileContent_.end(),
        HCCL_ERROR("[OXC_HCOMM][Parse][server_list] field is missing."), HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!serverListIter->is_array() || serverListIter->empty(),
        HCCL_ERROR("[OXC_HCOMM][Parse][server_list] field is invalid."), HCCL_E_PARA);

    rankTable.serverNum = static_cast<u32>(serverListIter->size());
    for (u32 serverIndex = 0; serverIndex < serverListIter->size(); ++serverIndex) {
        const auto &serverObj = (*serverListIter)[serverIndex];
        CHK_PRT_RET(!serverObj.is_object(),
            HCCL_ERROR("[OXC_HCOMM][Parse][server_list] server item is not object."), HCCL_E_PARA);
        CHK_RET(ParseSingleServer(serverObj, serverIndex, rankTable));
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseSingleServer(const nlohmann::json &serverObj, u32 serverIndex,
    RankTable_t &rankTable)
{
    std::string serverId;
    CHK_RET(GetRequiredStringField(serverObj, PROP_SERVER_ID.c_str(), serverId));
    CHK_PRT_RET(serverId.empty(),
        HCCL_ERROR("[OXC_HCOMM][Parse][server_id] server[%u] is empty.", serverIndex), HCCL_E_PARA);
    CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_SERVER_ID, serverId,
        JsonCheckOpType::CHECK_OP_TYPE_INSERT));

    HcclIpAddress hostIp;
    std::string hostIpStr;
    bool hostFieldFound = false;
    CHK_RET(GetOptionalStringFieldChecked(serverObj, PROP_HOST_IP.c_str(), hostIpStr, hostFieldFound));
    if (hostFieldFound && !hostIpStr.empty()) {
        CHK_RET(ConvertIpAddress(hostIpStr, hostIp));
    } else {
        HcclResult hostRet = ConvertIpAddress(serverId, hostIp);
        if (hostRet != HCCL_SUCCESS) {
            hostIp = HcclIpAddress();
        }
    }

    return ParseDeviceList(serverObj, serverId, serverIndex, hostIp, rankTable);
}

HcclResult TopoinfoRanktableOxc::ParseDeviceList(const nlohmann::json &serverObj, const std::string &serverId,
    u32 serverIndex, const HcclIpAddress &hostIp, RankTable_t &rankTable)
{
    auto deviceIter = serverObj.find(PROP_DEVICE);
    CHK_PRT_RET(deviceIter == serverObj.end(),
        HCCL_ERROR("[OXC_HCOMM][Parse][device] field is missing for server[%s].", serverId.c_str()),
        HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!deviceIter->is_array() || deviceIter->empty(),
        HCCL_ERROR("[OXC_HCOMM][Parse][device] field is invalid for server[%s].", serverId.c_str()),
        HCCL_E_PARA);

    for (const auto &deviceObj : *deviceIter) {
        CHK_PRT_RET(!deviceObj.is_object(),
            HCCL_ERROR("[OXC_HCOMM][Parse][device] device item is not object for server[%s].", serverId.c_str()),
            HCCL_E_PARA);
        RankInfo_t rankInfo;
        CHK_RET(ParseSingleDevice(deviceObj, serverId, serverIndex, hostIp, rankInfo));
        rankTable.rankList.push_back(rankInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseSingleDevice(const nlohmann::json &deviceObj, const std::string &serverId,
    u32 serverIndex, const HcclIpAddress &hostIp, RankInfo_t &rankInfo)
{
    CHK_RET(GetRequiredU32FieldAny(deviceObj, PROP_RANKID, PROP_RANK_ID.c_str(), rankInfo.rankId));
    CHK_RET(GetRequiredU32Field(deviceObj, PROP_DEV_ID.c_str(), rankInfo.localRank));
    rankInfo.deviceInfo.devicePhyId = static_cast<s32>(rankInfo.localRank);
    rankInfo.deviceInfo.deviceType = params_.deviceType;
    rankInfo.serverId = serverId;
    rankInfo.hostIp = hostIp;
    rankInfo.originalSuperPodId.clear();
    rankInfo.superPodId.clear();
    rankInfo.serverIdx = serverIndex;

    u32 superDeviceId = INVALID_UINT;
    CHK_RET(GetRequiredU32Field(deviceObj, PROP_SUPER_DEVICE_ID.c_str(), superDeviceId));
    rankInfo.superDeviceId = superDeviceId;

    std::string deviceIp;
    bool fieldFound = false;
    CHK_RET(GetOptionalStringFieldChecked(deviceObj, PROP_DEV_IP.c_str(), deviceIp, fieldFound));
    if (fieldFound && !deviceIp.empty()) {
        HcclIpAddress deviceIpAddr;
        CHK_RET(ConvertIpAddress(deviceIp, deviceIpAddr));
        rankInfo.deviceInfo.deviceIp.push_back(deviceIpAddr);
        CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_DEVICE_IP,
            deviceIp, JsonCheckOpType::CHECK_OP_TYPE_INSERT));
    } else {
        rankInfo.deviceInfo.deviceIp.push_back(HcclIpAddress());
    }

    std::string backupDeviceIp;
    CHK_RET(GetOptionalStringFieldChecked(deviceObj, PROP_BACKUP_DEV_IP.c_str(), backupDeviceIp, fieldFound));
    if (fieldFound && !backupDeviceIp.empty()) {
        HcclIpAddress backupDeviceIpAddr;
        CHK_RET(ConvertIpAddress(backupDeviceIp, backupDeviceIpAddr));
        rankInfo.deviceInfo.backupDeviceIp.push_back(backupDeviceIpAddr);
        CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_BACKUP_DEVICE_IP,
            backupDeviceIp, JsonCheckOpType::CHECK_OP_TYPE_INSERT));
    }

    u32 devicePort = 0;
    HcclResult devicePortRet = GetOptionalU32Field(deviceObj, PROP_DEV_NIC_PORT.c_str(), devicePort);
    CHK_PRT_RET(devicePortRet != HCCL_SUCCESS && devicePortRet != HCCL_E_NOT_FOUND,
        HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is invalid.", PROP_DEV_NIC_PORT.c_str()), devicePortRet);
    if (devicePortRet == HCCL_SUCCESS) {
        rankInfo.deviceInfo.port = devicePort;
        rankInfo.deviceInfo.vnicPort = devicePort;
    }

    u32 backupPort = 0;
    HcclResult backupPortRet = GetOptionalU32Field(deviceObj, PROP_BACKUP_DEV_PORT.c_str(), backupPort);
    CHK_PRT_RET(backupPortRet != HCCL_SUCCESS && backupPortRet != HCCL_E_NOT_FOUND,
        HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is invalid.", PROP_BACKUP_DEV_PORT.c_str()), backupPortRet);
    if (backupPortRet == HCCL_SUCCESS) {
        rankInfo.deviceInfo.backupPort = backupPort;
    }

    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseSuperPodList(RankTable_t &rankTable)
{
    auto superPodIter = fileContent_.find(PROP_SUPER_POD_LIST);
    CHK_PRT_RET(superPodIter == fileContent_.end(),
        HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] field is missing."), HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!superPodIter->is_array() || superPodIter->empty(),
        HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] field is invalid."), HCCL_E_PARA);

    std::unordered_map<std::string, std::string> serverToSuperPod;
    for (const auto &superPodObj : *superPodIter) {
        CHK_PRT_RET(!superPodObj.is_object(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] super pod item is not object."), HCCL_E_PARA);
        std::string superPodId;
        CHK_RET(GetRequiredStringField(superPodObj, PROP_SUPER_POD_ID.c_str(), superPodId));
        CHK_PRT_RET(superPodId.empty(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_id] field is empty."), HCCL_E_PARA);
        CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_SUPER_POD_ID, superPodId,
            JsonCheckOpType::CHECK_OP_TYPE_INSERT));

        auto serverListIter = superPodObj.find(PROP_SERVER_LIST);
        CHK_PRT_RET(serverListIter == superPodObj.end() || !serverListIter->is_array() || serverListIter->empty(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list.server_list] field is invalid."), HCCL_E_PARA);
        for (const auto &serverObj : *serverListIter) {
            CHK_PRT_RET(!serverObj.is_object(),
                HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list.server_list] server item is not object."),
                HCCL_E_PARA);
            std::string serverId;
            CHK_RET(GetRequiredStringField(serverObj, PROP_SERVER_ID.c_str(), serverId));
            auto emplaceRet = serverToSuperPod.emplace(serverId, superPodId);
            CHK_PRT_RET(!emplaceRet.second,
                HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] duplicated server_id[%s].", serverId.c_str()),
                HCCL_E_PARA);
        }
    }

    CHK_PRT_RET(serverToSuperPod.size() != rankTable.serverNum,
        HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] server coverage[%zu] != serverNum[%u].",
            serverToSuperPod.size(), rankTable.serverNum), HCCL_E_PARA);

    std::set<std::string> superPodIds;
    for (auto &rankInfo : rankTable.rankList) {
        auto iter = serverToSuperPod.find(rankInfo.serverId);
        CHK_PRT_RET(iter == serverToSuperPod.end(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_list] server_id[%s] is not covered.", rankInfo.serverId.c_str()),
            HCCL_E_PARA);
        rankInfo.superPodId = iter->second;
        rankInfo.originalSuperPodId = rankInfo.superPodId;
        GenerateSuperPodIdx(rankInfo.superPodId, rankInfo.superPodIdx);
        superPodIds.insert(rankInfo.superPodId);
    }
    rankTable.superPodNum = static_cast<u32>(superPodIds.size());
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseOxcGroupList(RankTable_t &rankTable)
{
    auto groupIter = fileContent_.find(PROP_OXC_GROUP_LIST);
    CHK_PRT_RET(groupIter == fileContent_.end(),
        HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] field is missing."), HCCL_E_NOT_FOUND);
    CHK_PRT_RET(!groupIter->is_array() || groupIter->empty(),
        HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] field is invalid."), HCCL_E_PARA);

    std::unordered_map<u32, RankInfo_t *> rankIdToInfo;
    for (auto &rankInfo : rankTable.rankList) {
        rankIdToInfo.emplace(rankInfo.rankId, &rankInfo);
    }

    std::set<u32> groupedRankIds;
    for (const auto &groupObj : *groupIter) {
        CHK_PRT_RET(!groupObj.is_object(),
            HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] group item is not object."), HCCL_E_PARA);
        std::string groupId;
        CHK_RET(GetRequiredStringField(groupObj, PROP_GROUP_ID, groupId));
        CHK_PRT_RET(groupId.empty(),
            HCCL_ERROR("[OXC_HCOMM][Parse][group_id] field is empty."), HCCL_E_PARA);

        auto rankListIter = groupObj.find(PROP_RANK_LIST);
        CHK_PRT_RET(rankListIter == groupObj.end() || !rankListIter->is_array() || rankListIter->empty(),
            HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list.rank_list] field is invalid."), HCCL_E_PARA);
        for (const auto &rankObj : *rankListIter) {
            CHK_PRT_RET(!rankObj.is_object(),
                HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list.rank_list] rank item is not object."), HCCL_E_PARA);
            u32 rankId = 0;
            CHK_RET(GetRequiredU32FieldAny(rankObj, PROP_RANK_ID.c_str(), PROP_RANKID, rankId));
            CHK_PRT_RET(!groupedRankIds.insert(rankId).second,
                HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] duplicated rank_id[%u].", rankId), HCCL_E_PARA);
            auto infoIter = rankIdToInfo.find(rankId);
            CHK_PRT_RET(infoIter == rankIdToInfo.end(),
                HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] rank_id[%u] is not in server_list.", rankId),
                HCCL_E_PARA);
            infoIter->second->oxcGroupId = groupId;
        }
    }

    CHK_PRT_RET(groupedRankIds.size() != rankTable.rankList.size(),
        HCCL_ERROR("[OXC_HCOMM][Parse][oxc_group_list] groupedRankCount[%zu] != rankNum[%zu].",
            groupedRankIds.size(), rankTable.rankList.size()), HCCL_E_PARA);
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
    HcclResult devicePortRet = GetOptionalU32Field(rankObj, PROP_DEV_NIC_PORT.c_str(), devicePort);
    CHK_PRT_RET(devicePortRet != HCCL_SUCCESS && devicePortRet != HCCL_E_NOT_FOUND,
        HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is invalid.", PROP_DEV_NIC_PORT.c_str()), devicePortRet);
    if (devicePortRet == HCCL_SUCCESS) {
        rankInfo.deviceInfo.port = devicePort;
    }

    CHK_RET(ParseA3RankExtensions(rankObj, rankInfo));

    CHK_RET(ParseLevelList(rankObj, rankInfo));
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ParseA3RankExtensions(const nlohmann::json &rankObj, RankInfo_t &rankInfo)
{
    bool fieldFound = false;
    std::string hostIp;
    CHK_RET(GetOptionalStringFieldChecked(rankObj, PROP_HOST_IP.c_str(), hostIp, fieldFound));
    if (fieldFound && !hostIp.empty()) {
        CHK_RET(ConvertIpAddress(hostIp, rankInfo.hostIp));
    }

    std::string deviceIp;
    CHK_RET(GetOptionalStringFieldChecked(rankObj, PROP_DEV_IP.c_str(), deviceIp, fieldFound));
    if (fieldFound && !deviceIp.empty()) {
        HcclIpAddress deviceIpAddr;
        CHK_RET(ConvertIpAddress(deviceIp, deviceIpAddr));
        rankInfo.deviceInfo.deviceIp.clear();
        rankInfo.deviceInfo.deviceIp.push_back(deviceIpAddr);
        CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_DEVICE_IP,
            deviceIp, JsonCheckOpType::CHECK_OP_TYPE_INSERT));
    }

    std::string backupDeviceIp;
    CHK_RET(GetOptionalStringFieldChecked(rankObj, PROP_BACKUP_DEV_IP.c_str(), backupDeviceIp, fieldFound));
    if (fieldFound && !backupDeviceIp.empty()) {
        HcclIpAddress backupDeviceIpAddr;
        CHK_RET(ConvertIpAddress(backupDeviceIp, backupDeviceIpAddr));
        rankInfo.deviceInfo.backupDeviceIp.clear();
        rankInfo.deviceInfo.backupDeviceIp.push_back(backupDeviceIpAddr);
        CHK_RET(CheckUniqueAndInsertPool(JsonUniqueInfoType::UNIQUE_INFO_TYPE_BACKUP_DEVICE_IP,
            backupDeviceIp, JsonCheckOpType::CHECK_OP_TYPE_INSERT));
    }

    u32 backupPort = 0;
    HcclResult backupPortRet = GetOptionalU32Field(rankObj, PROP_BACKUP_DEV_PORT.c_str(), backupPort);
    CHK_PRT_RET(backupPortRet != HCCL_SUCCESS && backupPortRet != HCCL_E_NOT_FOUND,
        HCCL_ERROR("[OXC_HCOMM][Parse][%s] field is invalid.", PROP_BACKUP_DEV_PORT.c_str()), backupPortRet);
    if (backupPortRet == HCCL_SUCCESS) {
        rankInfo.deviceInfo.backupPort = backupPort;
    }

    if (params_.deviceType != DevType::DEV_TYPE_910_93) {
        return HCCL_SUCCESS;
    }

    CHK_RET(GetRequiredStringField(rankObj, PROP_SUPER_POD_ID.c_str(), rankInfo.superPodId));
    rankInfo.originalSuperPodId = rankInfo.superPodId;
    GenerateSuperPodIdx(rankInfo.superPodId, rankInfo.superPodIdx);
    u32 superDeviceId = INVALID_UINT;
    CHK_RET(GetRequiredU32Field(rankObj, PROP_SUPER_DEVICE_ID.c_str(), superDeviceId));
    rankInfo.superDeviceId = superDeviceId;
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
    const u32 invalidPreviousNetLayer = std::numeric_limits<u32>::max();
    u32 previousNetLayer = invalidPreviousNetLayer;
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

        // OXC 2.0 当前阶段继续把“层级顺序 + L2 语义”留在 parser 层兜底，
        // 这样可以尽早拦住 schema 级错误，避免把非法 level_list 传播到后续 params/rankTable。
        CHK_RET(ValidateLevelInfo(levelInfo, previousNetLayer));
        rankInfo.levelList.push_back(levelInfo);
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ValidateLevelInfo(const RankLevelInfoOxc &levelInfo, u32 &previousNetLayer)
{
    constexpr u32 MAX_OXC_NET_LAYER = 3;
    CHK_PRT_RET(levelInfo.netLayer > MAX_OXC_NET_LAYER,
        HCCL_ERROR("[OXC_HCOMM][Parse][net_layer] value[%u] exceeds OXC range[0,%u].",
            levelInfo.netLayer, MAX_OXC_NET_LAYER), HCCL_E_PARA);

    if (previousNetLayer != std::numeric_limits<u32>::max()) {
        CHK_PRT_RET(levelInfo.netLayer <= previousNetLayer,
            HCCL_ERROR("[OXC_HCOMM][Parse][level_list] net_layer[%u] is not strictly increasing after [%u].",
                levelInfo.netLayer, previousNetLayer), HCCL_E_PARA);
    }

    if (levelInfo.netLayer == 2) {
        CHK_PRT_RET(levelInfo.netType != "OXC_Mesh",
            HCCL_ERROR("[OXC_HCOMM][Parse][net_type] net_layer[2] must be OXC_Mesh, got[%s].",
                levelInfo.netType.c_str()), HCCL_E_NOT_SUPPORT);
        CHK_PRT_RET(levelInfo.netInstanceId != "0",
            HCCL_ERROR("[OXC_HCOMM][Parse][net_instance_id] net_layer[2] must be 0, got[%s].",
                levelInfo.netInstanceId.c_str()), HCCL_E_PARA);
    }

    previousNetLayer = levelInfo.netLayer;
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ValidateRankLevelLayouts(const std::vector<RankInfo_t> &rankList)
{
    constexpr u32 EXPECTED_OXC_LEVEL_COUNT = 4;
    std::array<std::string, EXPECTED_OXC_LEVEL_COUNT> baselineNetTypes;
    bool baselineInitialized = false;

    for (u32 rankIndex = 0; rankIndex < rankList.size(); ++rankIndex) {
        const auto &rankInfo = rankList[rankIndex];
        CHK_PRT_RET(rankInfo.levelList.size() != EXPECTED_OXC_LEVEL_COUNT,
            HCCL_ERROR("[OXC_HCOMM][Parse][level_list] rank[%u] expect[%u] levels, actual[%zu].",
                rankInfo.rankId, EXPECTED_OXC_LEVEL_COUNT, rankInfo.levelList.size()), HCCL_E_PARA);

        for (u32 levelIndex = 0; levelIndex < EXPECTED_OXC_LEVEL_COUNT; ++levelIndex) {
            const auto &levelInfo = rankInfo.levelList[levelIndex];
            CHK_PRT_RET(levelInfo.netLayer != levelIndex,
                HCCL_ERROR("[OXC_HCOMM][Parse][level_list] rank[%u] levelIndex[%u] expect net_layer[%u], actual[%u].",
                    rankInfo.rankId, levelIndex, levelIndex, levelInfo.netLayer), HCCL_E_PARA);

            if (!baselineInitialized) {
                baselineNetTypes[levelIndex] = levelInfo.netType;
                continue;
            }

            CHK_PRT_RET(levelInfo.netType != baselineNetTypes[levelIndex],
                HCCL_ERROR("[OXC_HCOMM][Parse][net_type] rank[%u] level[%u] expect[%s], actual[%s].",
                    rankInfo.rankId, levelIndex, baselineNetTypes[levelIndex].c_str(), levelInfo.netType.c_str()),
                HCCL_E_PARA);
        }
        baselineInitialized = true;
    }
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ValidateAndFinalizeA3RankInfo(RankTable_t &rankTable)
{
    std::map<std::string, std::set<u32>> superPodToSdid;
    for (auto &rankInfo : rankTable.rankList) {
        CHK_PRT_RET(rankInfo.superPodId.empty(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_pod_id] rank[%u] field is missing.", rankInfo.rankId),
            HCCL_E_NOT_FOUND);
        CHK_PRT_RET(rankInfo.superDeviceId == INVALID_UINT,
            HCCL_ERROR("[OXC_HCOMM][Parse][super_device_id] rank[%u] field is missing.", rankInfo.rankId),
            HCCL_E_NOT_FOUND);
        auto &sdidSet = superPodToSdid[rankInfo.superPodId];
        CHK_PRT_RET(sdidSet.find(rankInfo.superDeviceId) != sdidSet.end(),
            HCCL_ERROR("[OXC_HCOMM][Parse][super_device_id] rank[%u] conflicts in super_pod_id[%s] with duplicate super_device_id[%u].",
                rankInfo.rankId, rankInfo.superPodId.c_str(), rankInfo.superDeviceId), HCCL_E_PARA);
        sdidSet.insert(rankInfo.superDeviceId);
    }
    rankTable.superPodNum = static_cast<u32>(superPodToSdid.size());
    return HCCL_SUCCESS;
}

HcclResult TopoinfoRanktableOxc::ValidateA3ParameterPlaneContract(const RankTable_t &rankTable)
{
    if (params_.deviceType != DevType::DEV_TYPE_910_93) {
        return HCCL_SUCCESS;
    }

    const bool isCrossServerA3 = rankTable.serverNum > 1;
    const bool needDeviceParameterPlane = isCrossServerA3 &&
        rankTable.nicDeploy == NICDeployment::NIC_DEPLOYMENT_DEVICE;
    for (const auto &rankInfo : rankTable.rankList) {
        if (isCrossServerA3) {
            CHK_PRT_RET(rankInfo.hostIp.IsInvalid(),
                HCCL_ERROR("[OXC_HCOMM][Parse][host_ip] rank[%u] is required for A3 cross-server route.",
                    rankInfo.rankId),
                HCCL_E_NOT_FOUND);
        }

        if (!needDeviceParameterPlane) {
            continue;
        }

        const bool deviceIpMissing = rankInfo.deviceInfo.deviceIp.empty() ||
            ((rankInfo.deviceInfo.deviceIp.size() == 1) && rankInfo.deviceInfo.deviceIp[0].IsInvalid());
        CHK_PRT_RET(deviceIpMissing,
            HCCL_ERROR("[OXC_HCOMM][Parse][device_ip] rank[%u] is required for A3 device-parameter-plane route.",
                rankInfo.rankId),
            HCCL_E_NOT_FOUND);
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
