/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "comm_utils.h"
#include "rank_graph.h"
#include "topo_info_extractor.h"

namespace {
using namespace hccl;

RankInfo BuildLayeredAlgoRankInfo(u32 userRank, u32 localRank, u32 serverIdx, s32 devicePhyId, const char *serverId)
{
    RankInfo rankInfo;
    rankInfo.userRank = userRank;
    rankInfo.worldRank = userRank;
    rankInfo.localRank = localRank;
    rankInfo.devicePhyId = devicePhyId;
    rankInfo.deviceType = DevType::DEV_TYPE_910B;
    rankInfo.serverId = serverId;
    rankInfo.serverIdx = serverIdx;
    rankInfo.superPodId = (serverIdx < 2) ? "superpod0" : "superpod1";
    rankInfo.superPodIdx = (serverIdx < 2) ? 0 : 1;
    rankInfo.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;
    rankInfo.nicIdx.push_back(static_cast<u32>(devicePhyId));
    rankInfo.nicIp.push_back(HcclIpAddress(std::string("10.0.0.") + std::to_string(userRank + 1)));
    return rankInfo;
}

std::vector<RankInfo> BuildLayeredRankVectorForGraph()
{
    auto rankInfos = std::vector<RankInfo>{
        BuildLayeredAlgoRankInfo(0, 0, 0, 0, "server0"),
        BuildLayeredAlgoRankInfo(1, 1, 0, 1, "server0"),
        BuildLayeredAlgoRankInfo(2, 0, 1, 0, "server1"),
        BuildLayeredAlgoRankInfo(3, 1, 1, 1, "server1"),
        BuildLayeredAlgoRankInfo(4, 0, 2, 0, "server2"),
        BuildLayeredAlgoRankInfo(5, 1, 2, 1, "server2"),
        BuildLayeredAlgoRankInfo(6, 0, 3, 0, "server3"),
        BuildLayeredAlgoRankInfo(7, 1, 3, 1, "server3"),
    };
    for (auto &rankInfo : rankInfos) {
        rankInfo.oxcGroupId = (rankInfo.serverIdx < 2) ? "group0" : "group1";
    }
    return rankInfos;
}

std::vector<RankInfo> BuildLayeredBrokenRankVectorForGraph()
{
    std::vector<RankInfo> rankInfos;
    rankInfos.reserve(16);
    for (u32 serverIdx = 0; serverIdx < 8; ++serverIdx) {
        const char *serverId = nullptr;
        switch (serverIdx) {
            case 0: serverId = "server0"; break;
            case 1: serverId = "server1"; break;
            case 2: serverId = "server2"; break;
            case 3: serverId = "server3"; break;
            case 4: serverId = "server4"; break;
            case 5: serverId = "server5"; break;
            case 6: serverId = "server6"; break;
            default: serverId = "server7"; break;
        }

        RankInfo rankInfo0 = BuildLayeredAlgoRankInfo(serverIdx * 2, 0, serverIdx, 0, serverId);
        RankInfo rankInfo1 = BuildLayeredAlgoRankInfo(serverIdx * 2 + 1, 1, serverIdx, 1, serverId);
        rankInfo0.superPodId = (serverIdx < 5) ? "superpod0" : "superpod1";
        rankInfo0.superPodIdx = (serverIdx < 5) ? 0 : 1;
        rankInfo1.superPodId = rankInfo0.superPodId;
        rankInfo1.superPodIdx = rankInfo0.superPodIdx;
        rankInfo0.oxcGroupId = rankInfo0.superPodId;
        rankInfo1.oxcGroupId = rankInfo1.superPodId;
        rankInfos.push_back(rankInfo0);
        rankInfos.push_back(rankInfo1);
    }
    return rankInfos;
}

/**
 * @brief 以指定 user rank 视角准备 OXC layered rankGraph 通信域。
 * @param comm 待初始化的通信域句柄。
 * @param userRank 当前视角 user rank。
 */
void PrepareLayeredRankGraphComm(hccl::hcclComm &comm, u32 userRank)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.userRank = userRank;
    topoAttr.realUserRank = userRank;
    topoAttr.userRankSize = 8;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = BuildLayeredRankVectorForGraph();
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0;
    topoAttr.netPlaneNum = 1;
    algoAttr.identifier = "rankgraph_layered_ut";

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    ASSERT_EQ(extractor.Init(algoAttr.commAlgoConfig), HCCL_SUCCESS);
    ASSERT_EQ(extractor.GetCommPlaneRanks(topoAttr.commPlaneRanks), HCCL_SUCCESS);

    CommConfig config("rankgraph_layered_ut");
    comm.communicator_.reset(new HcclCommunicator(config));
    ASSERT_NE(comm.communicator_, nullptr);
    comm.communicator_->identifier_ = "rankgraph_layered_ut";
    ASSERT_EQ(comm.communicator_->rankGraph_.Init(topoAttr), HCCL_SUCCESS);
}

/**
 * @brief 准备默认的 rank=1 layered rankGraph 通信域。
 * @param comm 待初始化的通信域句柄。
 */
void PrepareLayeredRankGraphComm(hccl::hcclComm &comm)
{
    PrepareLayeredRankGraphComm(comm, 1U);
}

void PrepareLayeredBrokenRankGraphComm(hccl::hcclComm &comm)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.userRank = 3;
    topoAttr.realUserRank = 3;
    topoAttr.userRankSize = 16;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = BuildLayeredBrokenRankVectorForGraph();
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0;
    topoAttr.netPlaneNum = 1;
    algoAttr.identifier = "rankgraph_layered_broke_ut";

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    ASSERT_EQ(extractor.Init(algoAttr.commAlgoConfig), HCCL_SUCCESS);
    ASSERT_EQ(extractor.GetCommPlaneRanks(topoAttr.commPlaneRanks), HCCL_SUCCESS);

    CommConfig config("rankgraph_layered_broke_ut");
    comm.communicator_.reset(new HcclCommunicator(config));
    ASSERT_NE(comm.communicator_, nullptr);
    comm.communicator_->identifier_ = "rankgraph_layered_broke_ut";
    ASSERT_EQ(comm.communicator_->rankGraph_.Init(topoAttr), HCCL_SUCCESS);
}

/**
 * @brief 提取指定 layer / topo instance 的成员列表。
 * @param comm 通信域句柄。
 * @param layer 需要查询的 layer。
 * @param topoInstId topo instance 编号。
 * @return std::vector<uint32_t>
 */
std::vector<uint32_t> GetLayeredRanksByTopoInst(HcclComm comm, uint32_t layer, uint32_t topoInstId)
{
    uint32_t *ranks = nullptr;
    uint32_t rankNum = 0;
    if (HcclRankGraphGetRanksByTopoInst(comm, layer, topoInstId, &ranks, &rankNum) != HCCL_SUCCESS || ranks == nullptr) {
        ADD_FAILURE() << "Get layered ranks failed, layer=" << layer << ", topoInstId=" << topoInstId;
        return {};
    }
    return std::vector<uint32_t>(ranks, ranks + rankNum);
}
}  // namespace

class HcclRankGraphLayeredApiTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }
};

TEST_F(HcclRankGraphLayeredApiTest, Ut_HcclRankGraphApi_When_LayeredPlanesInjected_Expect_LayersRanksAndLinksVisible)
{
    hccl::hcclComm comm;
    PrepareLayeredRankGraphComm(comm);

    uint32_t *layers = nullptr;
    uint32_t layerNum = 0;
    ASSERT_EQ(HcclRankGraphGetLayers(reinterpret_cast<HcclComm>(&comm), &layers, &layerNum), HCCL_SUCCESS);
    ASSERT_NE(layers, nullptr);
    ASSERT_GE(layerNum, 3U);
    std::vector<uint32_t> layerVec(layers, layers + layerNum);
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL1), layerVec.end());
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL2), layerVec.end());

    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);

    std::vector<uint32_t> level1Ranks = GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm),
        HCCL_NETLAYER_LAYERED_LEVEL1, 0);
    ASSERT_EQ(level1Ranks.size(), 2U);
    EXPECT_EQ(level1Ranks[0], 1U);
    EXPECT_EQ(level1Ranks[1], 3U);

    uint32_t *instSizes = nullptr;
    uint32_t listSize = 0;
    ASSERT_EQ(HcclRankGraphGetInstSizeListByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1,
        &instSizes, &listSize), HCCL_SUCCESS);
    ASSERT_EQ(listSize, 1U);
    EXPECT_EQ(instSizes[0], level1Ranks.size());

    ASSERT_EQ(HcclRankGraphGetInstSizeListByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &instSizes, &listSize), HCCL_SUCCESS);
    ASSERT_EQ(listSize, 1U);
    EXPECT_EQ(instSizes[0], 2U);

    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);
    std::vector<uint32_t> level2Ranks = GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm),
        HCCL_NETLAYER_LAYERED_LEVEL2, 0);
    ASSERT_EQ(level2Ranks.size(), 2U);
    EXPECT_EQ(level2Ranks[0], 1U);
    EXPECT_EQ(level2Ranks[1], 5U);

    CommLink *links = nullptr;
    uint32_t linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 1, 3,
        &links, &linkNum), HCCL_SUCCESS);
    ASSERT_NE(links, nullptr);
    EXPECT_GT(linkNum, 0U);
    EXPECT_EQ(links[0].srcEndpointDesc.commAddr.id, 1U);
    EXPECT_EQ(links[0].dstEndpointDesc.commAddr.id, 3U);

    links = nullptr;
    linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 1, 5,
        &links, &linkNum), HCCL_SUCCESS);
    EXPECT_EQ(links, nullptr);
    EXPECT_EQ(linkNum, 0U);

    links = nullptr;
    linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 1, 5,
        &links, &linkNum), HCCL_SUCCESS);
    ASSERT_NE(links, nullptr);
    EXPECT_GT(linkNum, 0U);
    EXPECT_EQ(links[0].srcEndpointDesc.commAddr.id, 1U);
    EXPECT_EQ(links[0].dstEndpointDesc.commAddr.id, 5U);
}

TEST_F(HcclRankGraphLayeredApiTest,
    Ut_HcclRankGraphApi_When_LayeredPlanesInjectedFromRank4_Expect_OxcRepresentativeMembersRemainStable)
{
    hccl::hcclComm comm;
    PrepareLayeredRankGraphComm(comm, 4U);

    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 0),
        std::vector<uint32_t>({4U, 6U}));

    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 0),
        std::vector<uint32_t>({0U, 4U}));

    CommLink *links = nullptr;
    uint32_t linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 4, 0,
        &links, &linkNum), HCCL_SUCCESS);
    EXPECT_EQ(links, nullptr);
    EXPECT_EQ(linkNum, 0U);
}

TEST_F(HcclRankGraphLayeredApiTest,
    Ut_HcclRankGraphApi_When_OxcBrokenTailLevel2HasShortTail_Expect_RankGraphSkipsTailGroupRanks)
{
    hccl::hcclComm comm;
    PrepareLayeredBrokenRankGraphComm(comm);

    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(topoInsts[0], 0U);

    uint32_t *instSizes = nullptr;
    uint32_t listSize = 0;
    ASSERT_EQ(HcclRankGraphGetInstSizeListByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &instSizes, &listSize), HCCL_SUCCESS);
    ASSERT_EQ(listSize, 1U);
    EXPECT_EQ(instSizes[0], 2U);

    std::vector<uint32_t> level2Ranks = GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm),
        HCCL_NETLAYER_LAYERED_LEVEL2, 0);
    ASSERT_EQ(level2Ranks.size(), 2U);
    EXPECT_EQ(level2Ranks[0], 3U);
    EXPECT_EQ(level2Ranks[1], 13U);

    CommLink *links = nullptr;
    uint32_t linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 3, 13,
        &links, &linkNum), HCCL_SUCCESS);
    ASSERT_NE(links, nullptr);
    EXPECT_GT(linkNum, 0U);
    EXPECT_EQ(links[0].srcEndpointDesc.commAddr.id, 3U);
    EXPECT_EQ(links[0].dstEndpointDesc.commAddr.id, 13U);

    links = nullptr;
    linkNum = 0;
    ASSERT_EQ(HcclRankGraphGetLinks(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 3, 11,
        &links, &linkNum), HCCL_SUCCESS);
    EXPECT_EQ(links, nullptr);
    EXPECT_EQ(linkNum, 0U);
}
