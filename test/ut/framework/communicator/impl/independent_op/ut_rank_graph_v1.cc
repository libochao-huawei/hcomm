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
#include <mockcpp/mockcpp.hpp>
#include "rank_graph.h"
#include "rank_graph_base.h"
#include "topoinfo_struct.h"
#include "hccl_common.h"

using namespace hccl;

class RankGraphV1DirectTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RankGraphV1DirectTest SetUp" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "RankGraphV1DirectTest TearDown" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(RankGraphV1DirectTest, Ut_GetTopoInstsByLayer_When_ValidLayer_Expect_Success)
{
    RankGraphV1 rankGraph;

    rankGraph.netLayer_.push_back(0);
    std::vector<u32> instRanks = {0, 1, 2, 3};
    rankGraph.rankSizeList_[0] = instRanks;

    uint32_t netLayer = 0;
    uint32_t *topoInsts = nullptr;
    uint32_t topoInstNum = 0;

    HcclResult ret = rankGraph.GetTopoInstsByLayer(netLayer, &topoInsts, &topoInstNum);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoInstNum, 4u);
    EXPECT_NE(topoInsts, nullptr);
}

TEST_F(RankGraphV1DirectTest, Ut_GetTopoType_When_ValidParams_Expect_Success)
{
    RankGraphV1 rankGraph;

    rankGraph.netLayer_.push_back(0);
    rankGraph.topoAttr_.deviceType = DevType::DEV_TYPE_910_93;

    uint32_t netLayer = 0;
    uint32_t topoInstId = 0;
    CommTopo topoType = CommTopo::COMM_TOPO_CLOS;

    HcclResult ret = rankGraph.GetTopoType(netLayer, topoInstId, &topoType);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(topoType, CommTopo::COMM_TOPO_910_93);
}

TEST_F(RankGraphV1DirectTest, Ut_GetRanksByTopoInst_When_ValidParams_Expect_Success)
{
    RankGraphV1 rankGraph;

    rankGraph.netLayer_.push_back(0);
    rankGraph.devType_ = DevType::DEV_TYPE_910B;
    std::vector<u32> rankList = {0, 1, 2, 3};
    rankGraph.rankList_[0] = rankList;
    std::vector<u32> rankSizeList = {2, 2};
    rankGraph.rankSizeList_[0] = rankSizeList;

    uint32_t netLayer = 0;
    uint32_t topoInstId = 0;
    uint32_t *ranks = nullptr;
    uint32_t rankNum = 0;

    HcclResult ret = rankGraph.GetRanksByTopoInst(netLayer, topoInstId, &ranks, &rankNum);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(ranks, nullptr);
    EXPECT_EQ(rankNum, 2u);
}

TEST_F(RankGraphV1DirectTest, Ut_GetRanksInTopoInst_When_ValidParams_Expect_Success)
{
    RankGraphV1 rankGraph;

    rankGraph.netLayer_.push_back(0);
    rankGraph.netLayer_.push_back(1);
    std::vector<u32> rankList = {0, 1};
    rankGraph.rankList_[1] = rankList;
    std::vector<u32> rankSizeList = {2};
    rankGraph.rankSizeList_[1] = rankSizeList;

    RankInfo_t graphInfo;
    graphInfo.rankId = 0;
    rankGraph.rankGraph_.push_back(graphInfo);
    graphInfo.rankId = 1;
    rankGraph.rankGraph_.push_back(graphInfo);

    uint32_t netLayer = 1;
    uint32_t topoInstId = 0;

    auto ranks = rankGraph.GetRanksInTopoInst(netLayer, topoInstId);

    EXPECT_EQ(ranks.size(), 2u);
}
