/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <set>

#include "hccl_api_base_test.h"
#include "plane_transformer/plane_transformer_factory.h"
#include "topoinfo_plane_transformer.h"

namespace {
using namespace hccl;

RankInfo BuildFactoryTestRankInfo(u32 userRank)
{
    RankInfo rankInfo;
    rankInfo.userRank = userRank;
    rankInfo.worldRank = userRank;
    rankInfo.serverId = "server";
    rankInfo.serverIdx = 0U;
    rankInfo.devicePhyId = static_cast<s32>(userRank);
    return rankInfo;
}

void ExpectPermutationRow(const std::vector<u32> &row, u32 width)
{
    ASSERT_EQ(row.size(), width);
    std::vector<bool> seen(width, false);
    for (u32 value : row) {
        ASSERT_LT(value, width);
        ASSERT_FALSE(seen[value]);
        seen[value] = true;
    }
}

void ExpectIdentityMatrix(const TransformMatrix &matrix, u32 width)
{
    ASSERT_EQ(matrix.size(), 1U);
    ASSERT_EQ(matrix.front().size(), width);
    for (u32 i = 0; i < width; ++i) {
        EXPECT_EQ(matrix.front()[i], i);
    }
}
}  // namespace

class PlaneTransformerFactoryApiTest : public BaseInit {
};

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerFactory_When_GetKnownAlgo_Expect_AllRegisteredAlgorithmsMatrixCapable)
{
    const auto &factory = PlaneTransformerFactory::Instance();
    const std::vector<HcclAlgoType> registered {
        HcclAlgoType::HCCL_ALGO_TYPE_RING,
        HcclAlgoType::HCCL_ALGO_TYPE_AHC,
        HcclAlgoType::HCCL_ALGO_TYPE_AHC_BROKE,
        HcclAlgoType::HCCL_ALGO_TYPE_NB,
        HcclAlgoType::HCCL_ALGO_TYPE_NHR,
        HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1,
        HcclAlgoType::HCCL_ALGO_TYPE_HDR,
    };

    for (const auto algoType : registered) {
        const IPlaneTransformer *transformer = factory.Get(algoType);
        ASSERT_NE(transformer, nullptr);
        ASSERT_NE(transformer->Clone(), nullptr);
        TransformMatrix matrix;
        ASSERT_EQ(transformer->CalcPlaneTransform(5U, 2U, matrix), HCCL_SUCCESS);
        ASSERT_FALSE(matrix.empty());
        for (const auto &row : matrix) {
            ExpectPermutationRow(row, 5U);
        }
    }
    EXPECT_EQ(factory.Get(HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT), nullptr);
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerNB_When_OddPlaneSize_Expect_A2MatrixSemantics)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NB);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(5U, 2U, matrix), HCCL_SUCCESS);
    ASSERT_EQ(matrix.size(), 4U);
    for (const auto &row : matrix) {
        ExpectPermutationRow(row, 5U);
    }
    EXPECT_EQ(matrix[0], std::vector<u32>({0U, 1U, 2U, 3U, 4U}));
    EXPECT_EQ(matrix[1], std::vector<u32>({0U, 3U, 1U, 4U, 2U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerNB_When_PlaneNumIsOne_Expect_IdentitySingleRow)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NB);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(5U, 1U, matrix), HCCL_SUCCESS);
    ASSERT_EQ(matrix.size(), 1U);
    EXPECT_EQ(matrix[0], std::vector<u32>({0U, 1U, 2U, 3U, 4U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerRing_When_TransformRequested_Expect_IdentityMatrix)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_RING);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(5U, 3U, matrix), HCCL_SUCCESS);
    ExpectIdentityMatrix(matrix, 5U);
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerNB_When_EvenPlaneSize_Expect_IdentityFallback)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NHR);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(4U, 2U, matrix), HCCL_SUCCESS);
    ExpectIdentityMatrix(matrix, 4U);
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerRecursiveHD_When_PowerOfTwo_Expect_A2RecursiveMatrix)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(4U, 2U, matrix), HCCL_SUCCESS);
    ASSERT_EQ(matrix.size(), 2U);
    for (const auto &row : matrix) {
        ExpectPermutationRow(row, 4U);
    }
    EXPECT_EQ(matrix[0], std::vector<u32>({0U, 1U, 2U, 3U}));
    EXPECT_EQ(matrix[1], std::vector<u32>({0U, 2U, 1U, 3U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerRecursiveHD_When_PlaneNumIsOne_Expect_IdentitySingleRow)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(4U, 1U, matrix), HCCL_SUCCESS);
    ASSERT_EQ(matrix.size(), 1U);
    EXPECT_EQ(matrix[0], std::vector<u32>({0U, 1U, 2U, 3U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_PlaneTransformerRecursiveHD_When_NonPowerOfTwo_Expect_IdentityFallback)
{
    TransformMatrix matrix;
    const IPlaneTransformer *transformer = PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_HDR);
    ASSERT_NE(transformer, nullptr);
    ASSERT_EQ(transformer->CalcPlaneTransform(6U, 2U, matrix), HCCL_SUCCESS);
    ExpectIdentityMatrix(matrix, 6U);
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_TopoinfoPlaneTransformer_When_TransformPlaneByAlgoUsesHdr_Expect_A2IndexPathReorders)
{
    std::vector<std::vector<u32>> groups {{0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}};
    std::vector<u32> indexList(8U, 0U);

    ASSERT_EQ(TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_HDR, 1U, 5U,
        groups, indexList), HCCL_SUCCESS);

    EXPECT_EQ(indexList, std::vector<u32>({0U, 1U, 2U, 3U, 4U, 6U, 5U, 7U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_TopoinfoPlaneTransformer_When_TransformPlaneByAlgoUsesNb_Expect_A2IndexPathReorders)
{
    std::vector<std::vector<u32>> groups {{0U, 3U}, {1U, 4U}, {2U, 5U}};
    std::vector<u32> indexList(6U, 0U);

    ASSERT_EQ(TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_NB, 1U, 4U,
        groups, indexList), HCCL_SUCCESS);

    EXPECT_EQ(indexList, std::vector<u32>({0U, 1U, 2U, 3U, 5U, 4U}));
}

TEST_F(PlaneTransformerFactoryApiTest, Ut_TopoinfoPlaneTransformer_When_TransformPlaneByAlgoUsesUnsupportedAlgo_Expect_IdentityFallback)
{
    std::vector<std::vector<u32>> groups {{0U, 1U, 2U}};
    std::vector<u32> indexList(3U, 0U);

    ASSERT_EQ(TopoinfoPlaneTransformer::TransformPlaneByAlgo(HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT, 0U, 0U,
        groups, indexList), HCCL_SUCCESS);

    EXPECT_EQ(indexList, std::vector<u32>({0U, 1U, 2U}));
}
