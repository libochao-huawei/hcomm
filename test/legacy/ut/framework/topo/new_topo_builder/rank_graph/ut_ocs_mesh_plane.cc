/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "rank_gph.h"
#include "rank_table_info.h"
#include "net_instance.h"

using namespace Hccl;

class OcsMeshPlaneTest : public testing::Test {
protected:
    // 构造 RankTableInfo，ranks[index].rankLevelInfos 中包含一个 OCS_MESH 层
    // elecGroupIds 按 rankId 顺序指定每个 rank 的 elecGroupId
    RankTableInfo BuildRankTable(const std::vector<u32> &elecGroupIds)
    {
        RankTableInfo table;
        table.rankCount = static_cast<u32>(elecGroupIds.size());
        table.ranks.resize(elecGroupIds.size());

        for (size_t i = 0; i < elecGroupIds.size(); ++i) {
            auto &rank = table.ranks[i];
            rank.rankId = static_cast<u32>(i);
            rank.deviceId = static_cast<u32>(i);

            RankLevelInfo levelInfo;
            levelInfo.netLayer = 0;
            levelInfo.netType = NetType::OCS_MESH;
            levelInfo.netInstId = "ocs_mesh_0";
            levelInfo.elecGroupId = elecGroupIds[i];

            rank.rankLevelInfos.push_back(levelInfo);
        }

        return table;
    }

    void AddPeers(RankGraph &graph, u32 rankNum)
    {
        for (u32 rankId = 0; rankId < rankNum; ++rankId) {
            graph.AddPeer(std::make_shared<NetInstance::Peer>(rankId, rankId, rankId, rankId));
        }
    }

    // 向 RankGraph 添加一个 OCS_MESH NetInstance，包含指定 rankIds
    void AddOcsMeshNetInst(RankGraph &graph, u32 netLayer, const std::string &instId,
                           const std::vector<RankId> &rankIds)
    {
        auto netInst = std::make_shared<OcsMeshNetInstance>(netLayer, instId);
        for (RankId rankId : rankIds) {
            netInst->AddRankId(rankId);
        }
        graph.AddNetInstance(netInst);
    }

    void AddPathCapableOcsMeshNetInst(RankGraph &graph, u32 netLayer, const std::string &instId,
                                      const std::vector<RankId> &rankIds)
    {
        auto netInst = std::make_shared<OcsMeshNetInstance>(netLayer, instId);
        auto fabric = std::make_shared<NetInstance::Fabric>(0, "ocs_plane_0");
        std::set<LinkProtocol> protocols = {LinkProtocol::UB_CTP};

        netInst->AddNode(fabric);
        for (RankId rankId : rankIds) {
            auto peer = graph.GetPeer(rankId);
            std::set<std::string> ports = {std::to_string(rankId)};
            auto peerIface = std::make_shared<NetInstance::ConnInterface>(
                IpAddress(rankId), ports, AddrPosition::HOST, LinkType::PEER2NET, protocols);

            netInst->AddRankId(rankId);
            netInst->AddNode(peer);
            peer->AddNetInstance(netInst);
            netInst->AddLink(std::make_shared<NetInstance::Link>(
                peer, fabric, peerIface, nullptr, LinkType::PEER2NET, protocols));
            netInst->AddLink(std::make_shared<NetInstance::Link>(
                fabric, peer, nullptr, peerIface, LinkType::PEER2NET, protocols));
        }
        graph.AddNetInstance(netInst);
    }

    void AddClosNetInst(RankGraph &graph, u32 netLayer, const std::string &instId,
                        const std::vector<RankId> &rankIds)
    {
        auto netInst = std::make_shared<ClosNetInstance>(netLayer, instId);
        for (RankId rankId : rankIds) {
            netInst->AddRankId(rankId);
        }
        graph.AddNetInstance(netInst);
    }
};

// Tracer bullet: 多 elecGroup 主通信域场景
// ranks 0,1 → elecGroupId=0; ranks 2,3 → elecGroupId=1
// 期望: ocsPlaneNum=2, ranks 0,1 → planeId=0, ranks 2,3 → planeId=1
TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_MultiElecGroup_MainComm)
{
    RankGraph graph(0);

    // 添加 Peers（AddNetInstance 依赖 HasRank 检查）
    AddPeers(graph, 4);

    // 添加一个 OCS_MESH NetInstance 包含全部 4 个 rank
    AddOcsMeshNetInst(graph, 0, "ocs_mesh_0", {0, 1, 2, 3});

    // 构造 rankTable: rank 0,1 → elecGroupId=0; rank 2,3 → elecGroupId=1
    RankTableInfo table = BuildRankTable({0, 0, 1, 1});

    // 主通信域：globalRankIds 为 nullptr
    graph.ReparseGroupedPlaneForOcsMesh(table, nullptr);

    // 验证分组结果
    EXPECT_EQ(graph.GetOcsPlaneId(0), 0u);
    EXPECT_EQ(graph.GetOcsPlaneNum(0), 2u);
    EXPECT_EQ(graph.GetOcsPlaneId(1), 0u);
    EXPECT_EQ(graph.GetOcsPlaneNum(1), 2u);
    EXPECT_EQ(graph.GetOcsPlaneId(2), 1u);
    EXPECT_EQ(graph.GetOcsPlaneNum(2), 2u);
    EXPECT_EQ(graph.GetOcsPlaneId(3), 1u);
    EXPECT_EQ(graph.GetOcsPlaneNum(3), 2u);
}

TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_SingleElecGroup_MainComm)
{
    RankGraph graph(0);
    AddPeers(graph, 4);
    AddOcsMeshNetInst(graph, 0, "ocs_mesh_0", {0, 1, 2, 3});

    RankTableInfo table = BuildRankTable({7, 7, 7, 7});

    graph.ReparseGroupedPlaneForOcsMesh(table);

    for (RankId rankId = 0; rankId < 4; ++rankId) {
        EXPECT_EQ(graph.GetOcsPlaneId(rankId), 0u);
        EXPECT_EQ(graph.GetOcsPlaneNum(rankId), 1u);
    }
}

TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_NoOcsMeshNetInst_DoesNotSetAttrs)
{
    RankGraph graph(0);
    AddPeers(graph, 4);
    AddClosNetInst(graph, 0, "clos_0", {0, 1, 2, 3});

    RankTableInfo table = BuildRankTable({0, 1, 2, 3});

    graph.ReparseGroupedPlaneForOcsMesh(table);

    for (RankId rankId = 0; rankId < 4; ++rankId) {
        EXPECT_EQ(graph.GetOcsPlaneId(rankId), 0u);
        EXPECT_EQ(graph.GetOcsPlaneNum(rankId), 0u);
    }
}

TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_AllZeroElecGroup_MainComm)
{
    RankGraph graph(0);
    AddPeers(graph, 4);
    AddOcsMeshNetInst(graph, 0, "ocs_mesh_0", {0, 1, 2, 3});

    RankTableInfo table = BuildRankTable({0, 0, 0, 0});

    graph.ReparseGroupedPlaneForOcsMesh(table);

    for (RankId rankId = 0; rankId < 4; ++rankId) {
        EXPECT_EQ(graph.GetOcsPlaneId(rankId), 0u);
        EXPECT_EQ(graph.GetOcsPlaneNum(rankId), 1u);
    }
}

TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_UsesGlobalRankIdsForSubComm)
{
    RankGraph graph(0);
    AddPeers(graph, 3);
    AddOcsMeshNetInst(graph, 0, "ocs_mesh_0", {0, 1, 2});

    RankTableInfo table = BuildRankTable({7, 7, 0, 1, 7, 1});
    std::vector<u32> globalRankIds{5, 2, 3};

    graph.ReparseGroupedPlaneForOcsMesh(table, &globalRankIds);

    EXPECT_EQ(graph.GetOcsPlaneId(0), 1u);
    EXPECT_EQ(graph.GetOcsPlaneNum(0), 2u);
    EXPECT_EQ(graph.GetOcsPlaneId(1), 0u);
    EXPECT_EQ(graph.GetOcsPlaneNum(1), 2u);
    EXPECT_EQ(graph.GetOcsPlaneId(2), 1u);
    EXPECT_EQ(graph.GetOcsPlaneNum(2), 2u);
}

TEST_F(OcsMeshPlaneTest, ReparseGroupedPlaneForOcsMesh_SubRankGraphUsesRankIdMapping)
{
    RankGraph graph(5);
    AddPeers(graph, 6);
    AddPathCapableOcsMeshNetInst(graph, 0, "ocs_mesh_0", {0, 1, 2, 3, 4, 5});

    RankTableInfo table = BuildRankTable({4, 0, 8, 3, 9, 3});
    std::vector<u32> rankIds{5, 2, 3};
    std::unique_ptr<RankGraph> subGraph = graph.CreateSubRankGraph(rankIds);

    subGraph->ReparseGroupedPlaneForOcsMesh(table, &rankIds);

    EXPECT_EQ(subGraph->GetOcsPlaneId(0), 0u);
    EXPECT_EQ(subGraph->GetOcsPlaneNum(0), 2u);
    EXPECT_EQ(subGraph->GetOcsPlaneId(1), 1u);
    EXPECT_EQ(subGraph->GetOcsPlaneNum(1), 2u);
    EXPECT_EQ(subGraph->GetOcsPlaneId(2), 0u);
    EXPECT_EQ(subGraph->GetOcsPlaneNum(2), 2u);
}
