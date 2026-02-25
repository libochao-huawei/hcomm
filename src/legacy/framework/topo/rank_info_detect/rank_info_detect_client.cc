/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rank_info_detect_client.h"
#include "root_handle_v2.h"
#include "env_config.h"
#include "host_buffer.h"
#include "binary_stream.h"
#include "hccp_peer_manager.h"
#include "orion_adapter_rts.h"
#include "host_socket_handle_manager.h"
#include "socket_manager.h"

namespace Hccl {

void RankInfoDetectClient::Setup(RankTableInfo &rankTable)
{
    // 1. 构造localRankTable
    RankTableInfo localRankTable{};
    ConstructRankTable(localRankTable);

    // 2. 连接root节点
    Connect();

    // 3. 发送本端agentId和rankSize
    SendAgentIdAndRankSize();
    
    // 4. 发送给root节点
    SendLocalRankTable(localRankTable);
    
    // 5. 接收完整rankTable
    RecvRankTable();
    rankTable = rankTable_;
}

void RankInfoDetectClient::Update(u32 devicePort, RankTableInfo &rankTable)
{
    // 1. 构造localRankTable
    RankTableInfo localRankTable{};
    ConstructRankTable(localRankTable);
    for(auto &rank : localRankTable.ranks) {
        rank.devicePort = devicePort;
    }

    // 2. 发送给root节点
    SendLocalRankTable(localRankTable);
    
    // 3. 接收完整rankTable
    RecvRankTable();
    rankTable = rankTable_;
}

void RankInfoDetectClient::Connect()
{
    clientSocket_->Connect(); 
    CheckStatus();
}

void RankInfoDetectClient::CheckStatus()
{
    HCCL_DEBUG("[RankInfoDetectClient::%s] start.", __func__);

    auto startTime = std::chrono::steady_clock::now();
    auto timeout   = std::chrono::seconds(EnvConfig::GetInstance().GetSocketConfig().GetLinkTimeOut());

    while (true) {
        CHK_PRT_THROW((std::chrono::steady_clock::now() - startTime) >= timeout,
                  HCCL_ERROR("[RankInfoDetectClient::%s] get connected status socket timeout! timeout[%lld s]", __func__, timeout),
                  TimeoutException, "client get connection timeout");

        if (clientSocket_->GetStatus() == SocketStatus::OK) {
            HCCL_DEBUG("[RankInfoDetectClient::%s] client get socket connection success.", __func__);
            break;
        }
    }

    HCCL_INFO("[RankInfoDetectClient::%s] end, connect ok.", __func__);
}

void RankInfoDetectClient::SendAgentIdAndRankSize()
{
    HCCL_DEBUG("[RankInfoDetectClient::%s] start.", __func__);

    // 发送agentId
    std::string rankID  = std::to_string(rankId_);
    std::string agentID = std::string(16 - rankID.length(), '0') + rankID;
    socketAgent_.SendMsg(agentID.c_str(), agentID.size());

    // 发送rankSize
    socketAgent_.SendMsg(&rankSize_, sizeof(rankSize_));

    HCCL_INFO("[RankInfoDetectClient::%s] send agentID[%s] and rankSize_[%u] end.", 
        __func__, agentID.c_str(), rankSize_);
}

void RankInfoDetectClient::SendLocalRankTable(const RankTableInfo &localRankTable)
{
    HCCL_DEBUG("[RankInfoDetectClient::%s] start.", __func__);

    // 消息格式: [ranktable数据(n字节)][step(4字节)]
    BinaryStream binaryStream;
    localRankTable.GetBinStream(true, binaryStream);
    binaryStream << currentStep_;

    // 字节流转换为vector<char>格式
    vector<char> sendMsg;
    binaryStream.Dump(sendMsg);

    // 发送
    socketAgent_.SendMsg(sendMsg.data(), sendMsg.size());

    HCCL_INFO("[RankInfoDetectClient::%s] end, currentStep_[%u].", __func__, currentStep_);
    currentStep_++;
}

void RankInfoDetectClient::ConstructSingleRank(RankTableInfo &localRankTable)
{
    localRankTable.version = "2.0";
    localRankTable.rankCount = 1;
    NewRankInfo rankInfo{};
    rankInfo.rankId = rankId_;
    rankInfo.rankLevelInfos.emplace_back(RankLevelInfo{});
    localRankTable.ranks.emplace_back(rankInfo);

    // 打印
    localRankTable.Dump();
    HCCL_INFO("[RankInfoDetectClient::%s] end, single rank, localRankTable[%s].", __func__, localRankTable.Describe().c_str());
}

void CheckRootInfoJson(const nlohmann::json &parseJson)
{
    // check version
    std::string version{};
    std::string msgVersion   = "error occurs when parser rootinfo object of propName \"version\"";
    TRY_CATCH_THROW(InvalidParamsException, msgVersion, version = GetJsonProperty(parseJson, "version"););
    CHK_PRT_THROW(version != "2.0", HCCL_ERROR("[%s] failed with version [%s] is not \"2.0\".", __func__ , version.c_str()),
                  InvalidParamsException, "version error");
    
    // parser topo_file_path
    std::string topoFilePath{};
    std::string msgRankTopoFile = "error occurs when parser object of propName \"topo_file_path\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRankTopoFile, topoFilePath = GetJsonProperty(parseJson, "topo_file_path"););
    
    // check topo_file_path
    char resolvedPath[PATH_MAX] = {0};
    CHK_PRT_THROW(realpath(topoFilePath.c_str(), resolvedPath) == nullptr,
            HCCL_ERROR("[%s] topo_file_path[%s] is not a valid real path", __func__, topoFilePath.c_str()),
            InvalidParamsException, "topo_file_path error");

    // parser rank_count
    u32         rankCount{};
    std::string msgRankcount = "error occurs when parser object of propName \"rank_count\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRankcount, rankCount = GetJsonPropertyUInt(parseJson, "rank_count"););
 
    // parser rank_list
    nlohmann::json rankJsons{};
    std::string    msgRanklist = "error occurs when parser object of propName \"rank_list\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRanklist,
                         GetJsonPropertyList(parseJson, "rank_list", rankJsons););
    
    // check rank_count
    CHK_PRT_THROW(rankCount != rankJsons.size(), 
                  HCCL_ERROR("[%s] failed with rankCount is not equal to rank_list size."
                             "rankCount[%u], ranks.size[%u]", __func__, rankCount, rankJsons.size()),
                  InvalidParamsException, "rankCount error");
}

void RankInfoDetectClient::ConstructRankTable(RankTableInfo &localRankTable)
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    // 单P场景处理
    CHK_PRT_RET_NULL((rankSize_ == 1), ConstructSingleRank(localRankTable));

    // 1. 解析文件topoInfo.json
    std::string filePath = "/etc/hccl_rootinfo.json";
    JsonParser jsonParser{};
    nlohmann::json parseJson{};
    jsonParser.ParseFileToJson(filePath, parseJson);
    CheckRootInfoJson(parseJson);

    // 2. 获取当前devPhyId_对应的devInfo
    nlohmann::json localDevInfoJson{};
    GetLocalDevInfoJson(parseJson, localDevInfoJson);

    // 3. 组rankTable的json格式
    nlohmann::json localRankTableJson{};
    GetLocalRankTableJson(parseJson, localRankTableJson);
    localRankTableJson["rank_list"].push_back(localDevInfoJson); // 添加localDevInfoJson

    // 4. 反序列化获得RankTableInfo
    std::string msgDeserialize = "error occurs when localRankTable Deserialize";
    TRY_CATCH_THROW(InvalidParamsException, msgDeserialize, localRankTable.Deserialize(localRankTableJson, false););
    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

void RankInfoDetectClient::GetLocalDevInfoJson(const nlohmann::json &parseJson, nlohmann::json &localDevInfoJson)
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    // rankList字段对应json内容
    nlohmann::json rankJsons;
    std::string    msgRanklist = "error occurs when parser object of propName \"rank_list\"";
    TRY_CATCH_THROW(InvalidParamsException, msgRanklist,
                         GetJsonPropertyList(parseJson, "rank_list", rankJsons););
    
    // 获取localrankJsons, 匹配deviceId字段与当前devPhyId_匹配的内容
    for (auto &rankJson : rankJsons) {
        u32 devId = 0;
        std::string msgDeviceId = "error occurs when parser object of propName \"device_id\"";
        TRY_CATCH_THROW(InvalidParamsException, msgDeviceId,
            devId = GetJsonPropertyUInt(rankJson, "device_id");
        );
        if (devId == devPhyId_) {
            HCCL_INFO("[RankInfoDetectClient::%s] find localDevInfoJson.", __func__);
            localDevInfoJson = rankJson;
            break;
        }
    }

    if (localDevInfoJson.empty()) {
        HCCL_ERROR("[%s] failed, no device_id matches devPhyId_[%u] in rank_list.", __func__, devPhyId_);
    }

    // 添加rankId
    localDevInfoJson["rank_id"] = rankId_;

    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

void RankInfoDetectClient::GetLocalRankTableJson(const nlohmann::json &parseJson, nlohmann::json &localRankTableJson)
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    std::string version;
    std::string msgVersion  = "error occurs when parser object of propName \"version\"";
    TRY_CATCH_THROW(InvalidParamsException, msgVersion, version = GetJsonProperty(parseJson, "version"););
    localRankTableJson["version"] = version;

    std::string detour;
    std::string msgDetour = "error occurs when parser object of propName \"detour\"";
    TRY_CATCH_THROW(InvalidParamsException, msgDetour, detour = GetJsonProperty(parseJson, "detour", false););
    if (detour == "true") {
        localRankTableJson["detour"] = detour;
    }

    localRankTableJson["rank_count"] = rankSize_;
    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

void RankInfoDetectClient::RecvRankTableMsg(vector<char> &rankInfoMsg)
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    // 接收数据
    u64 revMsgLen = 0;
    std::unique_ptr<HostBuffer> msg = std::make_unique<HostBuffer>(MAX_BUFFER_LEN);
    char *msgAddr = reinterpret_cast<char *>(msg->GetAddr());
    CHK_PRT_THROW(!socketAgent_.RecvMsg(msgAddr, revMsgLen),
        HCCL_ERROR("RankInfoDetectClient::%s, recv rankTable error.", __func__),
        SocketException, "client recv fail");

    // 以vector<char>格式保存
    rankInfoMsg.resize(revMsgLen);
    rankInfoMsg.assign(msgAddr, msgAddr + revMsgLen);

    HCCL_INFO("[RankInfoDetectClient::%s] end, revMsgLen[%llu].", __func__, revMsgLen);
}

// 解析接收到的rank table信息
void RankInfoDetectClient::ParseRankTable(vector<char> &rankInfoMsg)
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    // 消息格式: [ranktable大小(u32, 4字节)][ranktable数据(n字节)][step(4字节)][failedAgentIdList]
    BinaryStream binStream(rankInfoMsg);

    // 解析localRankInfo
    rankTable_ = RankTableInfo(binStream);
    rankTable_.Dump();

    // 解析step
    u32 receivedStep;
    binStream >> receivedStep;

    // 解析failedAgentIdList
    std::string failedAgentIdList;
    binStream >> failedAgentIdList;
    if (failedAgentIdList.size() > 0) {
        HCCL_ERROR("[RankInfoDetectClient::%s] failedAgentIdList %s", __func__, failedAgentIdList.c_str());
    }

    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

void RankInfoDetectClient::RecvRankTable()
{
    // 获取rankTable
    vector<char> rankInfoMsg{};
    RecvRankTableMsg(rankInfoMsg);

    // 解析rankTable
    ParseRankTable(rankInfoMsg);

    // 校验
    VerifyRankTable();
}

void RankInfoDetectClient::VerifyRankTable()
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);

    // 校验rankCount符合预期
    if (rankTable_.rankCount != rankSize_) {
        THROW<InvalidParamsException>(StringFormat("[RankInfoDetectClient::%s] rank_count[%u] does not match"
            " rankSize_[%u].", __func__, rankTable_.rankCount, rankSize_));
    }

    // 校验rankTable内容
    rankTable_.Check();

    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

void RankInfoDetectClient::TearDown()
{
    HCCL_INFO("[RankInfoDetectClient::%s] start.", __func__);
    
    // close socket
    clientSocket_->Close();
    
    // deinit handle
    HostSocketHandleManager::GetInstance().Destroy(devPhyId_, clientSocket_->GetLocalIp());

    // deinit ra
    s32 deviceLogicId = HrtGetDevice();
    HccpPeerManager::GetInstance().DeInit(deviceLogicId);

    HCCL_INFO("[RankInfoDetectClient::%s] end.", __func__);
}

RankInfoDetectClient::~RankInfoDetectClient()
{
    DECTOR_TRY_CATCH("RankInfoDetectClient", TearDown());
}

}
