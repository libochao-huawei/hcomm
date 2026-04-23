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
#include "plane_transformer/plane_transformer_factory.h"
#include "topoinfo_plane_transformer.h"

namespace {
using namespace hccl;

/**
 * @brief 构造最小 layered 闭环所需的 algorithm::RankInfo。
 * @param userRank 全局 user rank。
 * @param localRank server 内 local rank。
 * @param serverIdx server 索引。
 * @param devicePhyId 设备物理 ID。
 * @param serverId server 标识。
 * @return RankInfo
 */
RankInfo BuildAlgoRankInfo(u32 userRank, u32 localRank, u32 serverIdx, s32 devicePhyId, const char *serverId)
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

std::vector<RankInfo> BuildLayeredRankVector()
{
    auto rankInfos = std::vector<RankInfo>{
        BuildAlgoRankInfo(0, 0, 0, 0, "server0"),
        BuildAlgoRankInfo(1, 1, 0, 1, "server0"),
        BuildAlgoRankInfo(2, 0, 1, 0, "server1"),
        BuildAlgoRankInfo(3, 1, 1, 1, "server1"),
        BuildAlgoRankInfo(4, 0, 2, 0, "server2"),
        BuildAlgoRankInfo(5, 1, 2, 1, "server2"),
        BuildAlgoRankInfo(6, 0, 3, 0, "server3"),
        BuildAlgoRankInfo(7, 1, 3, 1, "server3"),
    };
    for (auto &rankInfo : rankInfos) {
        rankInfo.oxcGroupId = (rankInfo.serverIdx < 2) ? "group0" : "group1";
    }
    return rankInfos;
}

std::vector<RankInfo> BuildLayeredBrokenRankVector()
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

        RankInfo rankInfo0 = BuildAlgoRankInfo(serverIdx * 2, 0, serverIdx, 0, serverId);
        RankInfo rankInfo1 = BuildAlgoRankInfo(serverIdx * 2 + 1, 1, serverIdx, 1, serverId);
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
 * @brief 初始化 OXC layered extractor 所需的拓扑属性。
 * @param algoAttr 算法属性。
 * @param topoAttr 拓扑属性。
 * @param userRank 当前视角 user rank。
 * @param identifier extractor 标识符。
 */
void InitOxcLayeredTopoAttr(HcclAlgoAttr &algoAttr, HcclTopoAttr &topoAttr, u32 userRank, const char *identifier)
{
    topoAttr.userRank = userRank;
    topoAttr.realUserRank = userRank;
    topoAttr.userRankSize = 8;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = BuildLayeredRankVector();
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0;
    topoAttr.netPlaneNum = 1;
    algoAttr.identifier = identifier;
}

/**
 * @brief 提取通信平面中的 user rank 列表。
 * @param rankInfos 通信平面成员。
 * @return std::vector<u32>
 */
std::vector<u32> ExtractUserRanks(const std::vector<RankInfo> &rankInfos)
{
    std::vector<u32> userRanks;
    userRanks.reserve(rankInfos.size());
    for (const auto &rankInfo : rankInfos) {
        userRanks.push_back(rankInfo.userRank);
    }
    return userRanks;
}

/**
 * @brief 断言 layeredIndexVector_ 仍保持 identity 映射。
 * @param indexList 待校验的 indexList。
 */
void ExpectIdentityIndexList(const std::vector<u32> &indexList)
{
    ASSERT_EQ(indexList.size(), 4U);
    EXPECT_EQ(indexList[0], 0U);
    EXPECT_EQ(indexList[1], 1U);
    EXPECT_EQ(indexList[2], 2U);
    EXPECT_EQ(indexList[3], 3U);
}

/**
 * @brief 断言 extractor 生成的 OXC layered 平面与组信息符合预期。
 * @param extractor 已初始化的 extractor。
 * @param topoAttr 对应 topoAttr。
 * @param expectedGroupId 期望 groupId。
 * @param expectedLevel1Ranks 期望的 LEVEL1 成员列表。
 * @param expectedLevel2Ranks 期望的 LEVEL2 成员列表。
 */
void ExpectOxcLayeredPlanes(const TopoInfoExtractor &extractor, const HcclTopoAttr &topoAttr, u32 expectedGroupId,
    const std::vector<u32> &expectedLevel1Ranks, const std::vector<u32> &expectedLevel2Ranks)
{
    ASSERT_EQ(topoAttr.groupSize, 2U);
    EXPECT_EQ(topoAttr.groupId, expectedGroupId);
    EXPECT_EQ(topoAttr.netPlaneId, 0U);
    EXPECT_EQ(topoAttr.netPlaneNum, 1U);

    ExpectIdentityIndexList(extractor.layeredIndexVector_);

    ASSERT_FALSE(extractor.CommPlaneSubGroupVector_[COMM_LEVEL1].empty());
    ASSERT_FALSE(extractor.CommPlaneSubGroupVector_[COMM_LEVEL1][0].empty());
    EXPECT_EQ(extractor.CommPlaneSubGroupVector_[COMM_LEVEL1][0].size(), 2U);

    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1].size(), 1U);
    EXPECT_EQ(ExtractUserRanks(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0]), expectedLevel1Ranks);

    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2].size(), 1U);
    EXPECT_EQ(ExtractUserRanks(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2][0]), expectedLevel2Ranks);
}
}  // namespace

class TopoinfoPlaneTransformerLayeredTest : public BaseInit {
public:
    void SetUp() override
    {
        BaseInit::SetUp();
        DevType deviceType = DevType::DEV_TYPE_910B;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    }
};

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoinfoPlaneTransformer_When_RegroupReparseTransform_Expect_ReturnValueIsValid)
{
    std::vector<std::vector<u32>> groups = {{0, 1}, {2, 3}};
    HcclAlgoType intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    HcclAlgoType interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;

    HcclResult ret = TopoinfoPlaneTransformer::RegroupAndSelectAlgo(1, false, groups, intraAlgType, interAlgType);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(groups.size(), 2U);
    EXPECT_EQ(intraAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(interAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);

    u32 netPlaneId = 0;
    u32 netPlaneNum = 1;
    ret = TopoinfoPlaneTransformer::ReparseGroupedPlane(1, groups, netPlaneId, netPlaneNum);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneId, 1U);
    EXPECT_EQ(netPlaneNum, 2U);

    std::vector<u32> indexList;
    ret = TopoinfoPlaneTransformer::TransformPlaneByAlgo(interAlgType, netPlaneId, 1, groups, indexList);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(indexList.size(), 4U);
    EXPECT_EQ(indexList[0], 0U);
    EXPECT_EQ(indexList[1], 1U);
    EXPECT_EQ(indexList[2], 2U);
    EXPECT_EQ(indexList[3], 3U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_PlaneTransformerFactory_When_GetRingNbHdr_Expect_ReturnTransformer)
{
    auto ringTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_RING);
    auto nbTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NB);
    auto hdrTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);

    ASSERT_NE(ringTransformer, nullptr);
    ASSERT_NE(nbTransformer, nullptr);
    ASSERT_NE(hdrTransformer, nullptr);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_PlaneTransformerFactory_When_NbAndHdrCalcMatrix_Expect_MatrixIsValid)
{
    auto ringTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_RING);
    ASSERT_NE(ringTransformer, nullptr);
    auto ringMatrix = ringTransformer->CalcPlaneTransform(4, 1);
    ASSERT_EQ(ringMatrix.size(), 1U);
    EXPECT_EQ(ringMatrix[0][0], 0U);
    EXPECT_EQ(ringMatrix[0][1], 1U);
    EXPECT_EQ(ringMatrix[0][2], 2U);
    EXPECT_EQ(ringMatrix[0][3], 3U);

    auto nbTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NB);
    ASSERT_NE(nbTransformer, nullptr);
    auto nbMatrix = nbTransformer->CalcPlaneTransform(3, 1);
    ASSERT_EQ(nbMatrix.size(), 2U);
    ASSERT_EQ(nbMatrix[0].size(), 3U);
    EXPECT_EQ(nbMatrix[0][0], 0U);
    EXPECT_EQ(nbMatrix[0][1], 1U);
    EXPECT_EQ(nbMatrix[0][2], 2U);
    EXPECT_EQ(nbMatrix[1][0], 0U);
    EXPECT_EQ(nbMatrix[1][1], 2U);
    EXPECT_EQ(nbMatrix[1][2], 1U);

    auto nhrTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NHR);
    ASSERT_NE(nhrTransformer, nullptr);
    auto nhrMatrix = nhrTransformer->CalcPlaneTransform(3, 1);
    ASSERT_EQ(nhrMatrix.size(), 2U);
    EXPECT_EQ(nhrMatrix[0][0], 0U);
    EXPECT_EQ(nhrMatrix[0][1], 1U);
    EXPECT_EQ(nhrMatrix[0][2], 2U);
    EXPECT_EQ(nhrMatrix[1][0], 0U);
    EXPECT_EQ(nhrMatrix[1][1], 2U);
    EXPECT_EQ(nhrMatrix[1][2], 1U);

    auto hdrTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    ASSERT_NE(hdrTransformer, nullptr);
    auto hdrMatrix = hdrTransformer->CalcPlaneTransform(4, 1);
    ASSERT_EQ(hdrMatrix.size(), 2U);
    ASSERT_EQ(hdrMatrix[0].size(), 4U);
    EXPECT_EQ(hdrMatrix[0][0], 0U);
    EXPECT_EQ(hdrMatrix[0][1], 1U);
    EXPECT_EQ(hdrMatrix[0][2], 2U);
    EXPECT_EQ(hdrMatrix[0][3], 3U);
    EXPECT_EQ(hdrMatrix[1][0], 0U);
    EXPECT_EQ(hdrMatrix[1][1], 2U);
    EXPECT_EQ(hdrMatrix[1][2], 1U);
    EXPECT_EQ(hdrMatrix[1][3], 3U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_PlaneTransformerFactory_When_InvalidPlaneSize_Expect_DegradeToIdentity)
{
    auto nbTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NB);
    ASSERT_NE(nbTransformer, nullptr);
    auto nbInvalidMatrix = nbTransformer->CalcPlaneTransform(4, 1);
    ASSERT_EQ(nbInvalidMatrix.size(), 1U);
    EXPECT_EQ(nbInvalidMatrix[0][0], 0U);
    EXPECT_EQ(nbInvalidMatrix[0][1], 1U);
    EXPECT_EQ(nbInvalidMatrix[0][2], 2U);
    EXPECT_EQ(nbInvalidMatrix[0][3], 3U);

    auto hdrTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    ASSERT_NE(hdrTransformer, nullptr);
    auto hdrInvalidMatrix = hdrTransformer->CalcPlaneTransform(3, 1);
    ASSERT_EQ(hdrInvalidMatrix.size(), 1U);
    EXPECT_EQ(hdrInvalidMatrix[0][0], 0U);
    EXPECT_EQ(hdrInvalidMatrix[0][1], 1U);
    EXPECT_EQ(hdrInvalidMatrix[0][2], 2U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoinfoPlaneTransformer_When_FactoryMiss_Expect_DegradeToIdentity)
{
    auto missingTransformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_AHC);
    EXPECT_EQ(missingTransformer, nullptr);

    std::vector<std::vector<u32>> groups = {{0, 1}, {2, 3}};
    std::vector<u32> indexList;
    HcclResult ret = TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_AHC, 0, 0, groups,
        indexList);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(indexList.size(), 4U);
    EXPECT_EQ(indexList[0], 0U);
    EXPECT_EQ(indexList[1], 1U);
    EXPECT_EQ(indexList[2], 2U);
    EXPECT_EQ(indexList[3], 3U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoinfoPlaneTransformer_When_UseNbTransform_Expect_IndexListFollowMatrix)
{
    std::vector<std::vector<u32>> groups = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
    std::vector<u32> indexList;

    HcclResult ret = TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_NB, 1, 4, groups,
        indexList);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(indexList.size(), 9U);
    EXPECT_EQ(indexList[0], 0U);
    EXPECT_EQ(indexList[3], 3U);
    EXPECT_EQ(indexList[6], 6U);
    EXPECT_EQ(indexList[1], 1U);
    EXPECT_EQ(indexList[4], 7U);
    EXPECT_EQ(indexList[7], 4U);
    EXPECT_EQ(indexList[2], 2U);
    EXPECT_EQ(indexList[5], 5U);
    EXPECT_EQ(indexList[8], 8U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoinfoPlaneTransformer_When_UseHdrTransform_Expect_IndexListFollowMatrix)
{
    std::vector<std::vector<u32>> groups = {{0, 1}, {2, 3}, {4, 5}, {6, 7}};
    std::vector<u32> indexList;

    HcclResult ret = TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_HDR, 0, 1, groups,
        indexList);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    ASSERT_EQ(indexList.size(), 8U);
    EXPECT_EQ(indexList[0], 0U);
    EXPECT_EQ(indexList[1], 1U);
    EXPECT_EQ(indexList[2], 4U);
    EXPECT_EQ(indexList[3], 3U);
    EXPECT_EQ(indexList[4], 2U);
    EXPECT_EQ(indexList[5], 5U);
    EXPECT_EQ(indexList[6], 6U);
    EXPECT_EQ(indexList[7], 7U);
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoInfoExtractor_When_OxcRank1ViewEnabled_Expect_LayeredPlanesFollowLevel1SourceSemantics)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    InitOxcLayeredTopoAttr(algoAttr, topoAttr, 1U, "layered_closure_ut");

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    HcclResult ret = extractor.Init(algoAttr.commAlgoConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);

    ExpectOxcLayeredPlanes(extractor, topoAttr, 0U, {1U, 3U}, {1U, 5U});
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoInfoExtractor_When_OxcRank4ViewEnabled_Expect_LayeredPlanesRemainRepresentativeAndStable)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;
    InitOxcLayeredTopoAttr(algoAttr, topoAttr, 4U, "layered_rank4_closure_ut");

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    HcclResult ret = extractor.Init(algoAttr.commAlgoConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);

    ExpectOxcLayeredPlanes(extractor, topoAttr, 1U, {4U, 6U}, {0U, 4U});
}

TEST_F(TopoinfoPlaneTransformerLayeredTest,
    Ut_TopoInfoExtractor_When_OxcBrokenTailHasShortTailGroups_Expect_LayeredLevel2SkipTailGroups)
{
    HcclAlgoAttr algoAttr;
    HcclTopoAttr topoAttr;

    topoAttr.userRank = 3;
    topoAttr.realUserRank = 3;
    topoAttr.userRankSize = 16;
    topoAttr.deviceType = DevType::DEV_TYPE_910B;
    topoAttr.rankInfoList = BuildLayeredBrokenRankVector();
    topoAttr.isOxcMode = true;
    topoAttr.netPlaneId = 0;
    topoAttr.netPlaneNum = 1;
    algoAttr.identifier = "layered_broke_ut";

    TopoInfoExtractor extractor(algoAttr, topoAttr, TopoType::TOPO_TYPE_2P_MESH);
    HcclResult ret = extractor.Init(algoAttr.commAlgoConfig);
    ASSERT_EQ(ret, HCCL_SUCCESS);

    ASSERT_EQ(topoAttr.groupSize, 2U);
    EXPECT_EQ(topoAttr.groupId, 0U);
    EXPECT_EQ(topoAttr.netPlaneId, 0U);
    EXPECT_EQ(topoAttr.netPlaneNum, 1U);

    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1].size(), 1U);
    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0].size(), 5U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0][0].userRank, 1U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0][1].userRank, 3U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0][2].userRank, 5U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0][3].userRank, 7U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL1][0][4].userRank, 9U);

    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2].size(), 1U);
    ASSERT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2][0].size(), 2U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2][0][0].userRank, 3U);
    EXPECT_EQ(extractor.CommPlaneVector_[COMM_LAYERED_LEVEL2][0][1].userRank, 13U);
}
