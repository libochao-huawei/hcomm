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
#include "topo_info_extractor.h"

namespace {
using namespace hccl;

RankInfo BuildExtractorIntegrationRankInfo(u32 userRank, u32 localRank, u32 serverIdx, s32 devicePhyId, const char *serverId)
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
    rankInfo.nicIp.push_back(HcclIpAddress(std::string("10.0.1.") + std::to_string(userRank + 1)));
    rankInfo.oxcGroupId = (serverIdx < 2) ? "group0" : "group1";
    return rankInfo;
}

std::vector<RankInfo> BuildExtractorIntegrationRankVector()
{
    return {
        BuildExtractorIntegrationRankInfo(0U, 0U, 0U, 0, "server0"),
        BuildExtractorIntegrationRankInfo(1U, 1U, 0U, 1, "server0"),
        BuildExtractorIntegrationRankInfo(2U, 0U, 1U, 0, "server1"),
        BuildExtractorIntegrationRankInfo(3U, 1U, 1U, 1, "server1"),
        BuildExtractorIntegrationRankInfo(4U, 0U, 2U, 0, "server2"),
        BuildExtractorIntegrationRankInfo(5U, 1U, 2U, 1, "server2"),
        BuildExtractorIntegrationRankInfo(6U, 0U, 3U, 0, "server3"),
        BuildExtractorIntegrationRankInfo(7U, 1U, 3U, 1, "server3"),
    };
}
} // namespace

class TopoInfoExtractorPlaneTransformerIntegrationTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }
};

TEST_F(TopoInfoExtractorPlaneTransformerIntegrationTest,
    Ut_TopoInfoExtractor_When_OxcLayeredPathRuns_Expect_HelperStateWritesAndDensePlanesRestored)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    topoAttr.userRank = 1U;
    topoAttr.realUserRank = 1U;
    topoAttr.userRankSize = 8U;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = BuildExtractorIntegrationRankVector();
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0U;
    topoAttr.netPlaneNum = 1U;
    algoAttr.identifier = "extractor_transformer_integration_ut";

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    ASSERT_EQ(extractor.Init(algoAttr.commAlgoConfig), HCCL_SUCCESS);

    std::vector<std::vector<std::vector<u32>>> commPlaneRanks;
    ASSERT_EQ(extractor.GetCommPlaneRanks(commPlaneRanks), HCCL_SUCCESS);
    ASSERT_GT(commPlaneRanks.size(), COMM_LAYERED_LEVEL2);
    ASSERT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL1].size(), 2U);
    ASSERT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL2].size(), 1U);
    EXPECT_TRUE(commPlaneRanks[COMM_LAYERED_LEVEL1][0].empty());
    EXPECT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL1][1], std::vector<u32>({1U, 3U}));
    EXPECT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL2][0], std::vector<u32>({1U, 5U}));

    EXPECT_EQ(algoAttr.intraAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(algoAttr.interAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(topoAttr.groupId, 0U);
    EXPECT_EQ(topoAttr.groupSize, 0U);
    EXPECT_EQ(topoAttr.netPlaneId, 0U);
    EXPECT_EQ(topoAttr.netPlaneNum, 2U);
    EXPECT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL1][1].size(), 2U);
    EXPECT_EQ(commPlaneRanks[COMM_LAYERED_LEVEL2][0].size(), 2U);
    EXPECT_EQ(extractor.layeredIndexVector_, std::vector<u32>({0U, 2U}));
}
