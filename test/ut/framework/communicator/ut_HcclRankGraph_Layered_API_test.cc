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

std::vector<RankInfo> BuildLayeredPerRingDifferentGroupVectorForGraph()
{
    auto rankInfos = BuildLayeredRankVectorForGraph();
    rankInfos[0].oxcGroupId = "group0";
    rankInfos[2].oxcGroupId = "group0";
    rankInfos[4].oxcGroupId = "group1";
    rankInfos[6].oxcGroupId = "group1";

    rankInfos[1].oxcGroupId = "ring1_group0";
    rankInfos[3].oxcGroupId = "ring1_group1";
    rankInfos[5].oxcGroupId = "ring1_group0";
    rankInfos[7].oxcGroupId = "ring1_group1";
    return rankInfos;
}

void PrepareLayeredRankGraphComm(hccl::hcclComm &comm, u32 userRank, const std::vector<RankInfo> &rankInfos,
    const char *identifier)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.userRank = userRank;
    topoAttr.realUserRank = userRank;
    topoAttr.userRankSize = 8;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = rankInfos;
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0;
    topoAttr.netPlaneNum = 1;
    algoAttr.identifier = identifier;

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    ASSERT_EQ(extractor.Init(algoAttr.commAlgoConfig), HCCL_SUCCESS);
    ASSERT_EQ(extractor.GetCommPlaneRanks(topoAttr.commPlaneRanks), HCCL_SUCCESS);

    CommConfig config(identifier);
    comm.communicator_.reset(new HcclCommunicator(config));
    ASSERT_NE(comm.communicator_, nullptr);
    comm.communicator_->identifier_ = identifier;
    ASSERT_EQ(comm.communicator_->rankGraph_.Init(topoAttr), HCCL_SUCCESS);
}

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

TEST_F(HcclRankGraphLayeredApiTest,
    Ut_HcclRankGraphApi_When_OxcLayeredRankVectorInjected_Expect_LayeredLayersVisibleAgain)
{
    hccl::hcclComm comm;
    PrepareLayeredRankGraphComm(comm, 1U, BuildLayeredRankVectorForGraph(), "rankgraph_layered_ut");

    uint32_t *layers = nullptr;
    uint32_t layerNum = 0;
    ASSERT_EQ(HcclRankGraphGetLayers(reinterpret_cast<HcclComm>(&comm), &layers, &layerNum), HCCL_SUCCESS);
    ASSERT_NE(layers, nullptr);
    std::vector<uint32_t> layerVec(layers, layers + layerNum);
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL1), layerVec.end());
    EXPECT_NE(std::find(layerVec.begin(), layerVec.end(), HCCL_NETLAYER_LAYERED_LEVEL2), layerVec.end());

    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 0),
        std::vector<uint32_t>({1U, 3U}));

    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 0),
        std::vector<uint32_t>({1U, 5U}));
}

TEST_F(HcclRankGraphLayeredApiTest,
    Ut_HcclRankGraphApi_When_OxcPerRingDifferentGroupsInjected_Expect_LayeredLayersVisibleFromCachedSubgroups)
{
    hccl::hcclComm comm;
    PrepareLayeredRankGraphComm(comm, 5U, BuildLayeredPerRingDifferentGroupVectorForGraph(),
        "rankgraph_layered_ring_specific_ut");

    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;
    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL1, 0),
        std::vector<uint32_t>({5U, 7U}));

    ASSERT_EQ(HcclRankGraphGetTopoInstsByLayer(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2,
        &topoInsts, &topoInstNum), HCCL_SUCCESS);
    ASSERT_EQ(topoInstNum, 1U);
    EXPECT_EQ(GetLayeredRanksByTopoInst(reinterpret_cast<HcclComm>(&comm), HCCL_NETLAYER_LAYERED_LEVEL2, 0),
        std::vector<uint32_t>({1U, 5U}));
}
