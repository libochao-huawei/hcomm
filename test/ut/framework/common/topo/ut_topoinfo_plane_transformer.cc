/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "llt_hccl_stub_sal_pub.h"

#define private public
#define protected public
#include "topoinfo_plane_transformer.h"
#include "plane_transformer/plane_transformer_factory.h"
#undef private
#undef protected

#include <string>
#include <memory>
#include <vector>

using namespace std;
using namespace hccl;

/**
 * @brief TopoinfoPlaneTransformer 单元测试类
 */
class TopoinfoPlaneTransformerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        cout << "\033[36m--TopoinfoPlaneTransformerTest SetUp--\033[0m" << endl;
    }

    static void TearDownTestCase() {
        cout << "\033[36m--TopoinfoPlaneTransformerTest TearDown--\033[0m" << endl;
    }

    virtual void SetUp() {
        cout << "A Test case SetUp" << endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        cout << "A Test case TearDown" << endl;
    }

    // 辅助函数：创建测试用的 RankInfo
    RankInfo_t CreateRankInfo(u32 rankId, const std::string &serverId, s32 devicePhyId, u32 groupId = INVALID_UINT) {
        RankInfo_t info;
        info.rankId = rankId;
        info.serverId = serverId;
        info.deviceInfo.devicePhyId = devicePhyId;
        info.groupId = groupId;
        return info;
    }
};

/**
 * @brief TC001: 单 rank 通信域测试
 * @details 验证单个 rank 时 netPlaneNum = 1
 * @预期结果 返回 HCCL_SUCCESS，netPlaneNum = 1
 */
TEST_F(TopoinfoPlaneTransformerTest, TC001_SingleRank_When_OneRank_Expect_NetPlaneNumOne)
{
    // Arrange
    std::vector<u32> rankIds = { 0 };
    std::vector<RankInfo_t> globalRankList = {
        CreateRankInfo(0, "server0", 0)
    };
    std::vector<RankInfo_t> subRankList = { globalRankList[0] };
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankIds, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(netPlaneNum, 1u);  // 单 rank 默认 netPlaneNum = 1
}

/**
 * @brief TC002: 空 rankIds 测试
 * @details 验证空 rankIds 时返回成功
 * @预期结果 返回 HCCL_SUCCESS
 */
TEST_F(TopoinfoPlaneTransformerTest, TC002_EmptyRankIds_When_Empty_Expect_Success)
{
    // Arrange
    std::vector<u32> rankIds;
    std::vector<RankInfo_t> globalRankList;
    std::vector<RankInfo_t> subRankList;
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankIds, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC003: 均匀平面测试（等差数列）
 * @details 验证 rankIds 呈等差数列时正确计算
 * @预期结果 返回 HCCL_SUCCESS，netPlaneNum > 0
 */
TEST_F(TopoinfoPlaneTransformerTest, TC003_UniformPlane_When_ArithmeticSequence_Expect_Success)
{
    // Arrange - 创建 4 个 rank，呈等差数列 (0, 1, 2, 3)
    std::vector<u32> rankIds = { 0, 1, 2, 3 };
    std::vector<RankInfo_t> globalRankList;
    for (u32 i = 0; i < 4; ++i) {
        globalRankList.push_back(CreateRankInfo(i, "server0", static_cast<s32>(i), 0));
    }
    std::vector<RankInfo_t> subRankList = globalRankList;
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankIds, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_GT(netPlaneNum, 0u);
}

/**
 * @brief TC004: 非均匀平面测试
 * @details 验证 rankIds 不呈等差数列时降级为 netPlaneNum = 1
 * @预期结果 返回 HCCL_SUCCESS，netPlaneNum = 1
 */
TEST_F(TopoinfoPlaneTransformerTest, TC004_NonUniformPlane_When_NonArithmeticSequence_Expect_FallbackToOne)
{
    // Arrange - 创建非等差数列的 rankIds (0, 2, 5)
    std::vector<u32> rankIds = { 0, 2, 5 };
    std::vector<RankInfo_t> globalRankList;
    for (u32 i = 0; i < 6; ++i) {
        globalRankList.push_back(CreateRankInfo(i, "server0", static_cast<s32>(i), 0));
    }
    std::vector<RankInfo_t> subRankList;
    for (u32 id : rankIds) {
        subRankList.push_back(globalRankList[id]);
    }
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankIds, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 非均匀平面降级为 netPlaneNum = 1
    EXPECT_EQ(netPlaneNum, 1u);
}

/**
 * @brief TC005: Ring 算法变换测试
 * @details 验证 Ring 算法返回单位矩阵
 * @预期结果 返回单位矩阵
 */
TEST_F(TopoinfoPlaneTransformerTest, TC005_RingAlgoTransform_When_RingAlgo_Expect_IdentityMatrix)
{
    // Arrange
    RingPlaneTransformer transformer;
    u32 planeSize = 4;
    u32 planeNum = 2;

    // Act
    std::vector<std::vector<u32>> matrix = transformer.CalcPlaneTransform(planeSize, planeNum);

    // Assert
    EXPECT_EQ(matrix.size(), planeNum);
    for (u32 i = 0; i < planeNum; ++i) {
        EXPECT_EQ(matrix[i].size(), planeSize);
        for (u32 j = 0; j < planeSize; ++j) {
            // Ring 算法返回单位矩阵：matrix[i][j] = j
            EXPECT_EQ(matrix[i][j], j);
        }
    }
}

/**
 * @brief TC008: PlaneTransformerFactory Ring 算法测试
 * @details 验证工厂能正确创建 Ring 变换器
 * @预期结果 返回非空指针
 */
TEST_F(TopoinfoPlaneTransformerTest, TC008_FactoryRingAlgo_When_RingType_Expect_ValidPointer)
{
    // Act
    std::unique_ptr<IPlaneTransformer> transformer =
        PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_RING);

    // Assert
    EXPECT_NE(transformer, nullptr);

    // 验证变换功能
    auto matrix = transformer->CalcPlaneTransform(4, 2);
    EXPECT_EQ(matrix.size(), 2u);
    EXPECT_EQ(matrix[0].size(), 4u);
}

/**
 * @brief TC009: PlaneTransformerFactory 不支持算法测试
 * @details 验证不支持的算法返回 nullptr
 * @预期结果 返回 nullptr
 */
TEST_F(TopoinfoPlaneTransformerTest, TC009_FactoryUnsupportedAlgo_When_UnsupportedType_Expect_NullPointer)
{
    // Act - 使用一个可能不支持的算法类型
    std::unique_ptr<IPlaneTransformer> transformer =
        PlaneTransformerFactory::Instance().Get(HcclAlgoType::HCCL_ALGO_TYPE_NHR);

    // Assert - 对于不支持的算法，应该返回 nullptr 或空实现
    // 根据当前实现，NHR 算法返回 nullptr
    EXPECT_EQ(transformer, nullptr);
}

/**
 * @brief TC010: ParsePlane 数组版本测试
 * @details 验证数组版本的 ParsePlane 正确工作
 * @预期结果 返回 HCCL_SUCCESS
 */
TEST_F(TopoinfoPlaneTransformerTest, TC010_ParsePlaneArrayVersion_When_ValidArray_Expect_Success)
{
    // Arrange
    u32 rankIds[] = { 0, 1, 2 };
    u32 rankNum = 3;
    std::vector<RankInfo_t> globalRankList;
    for (u32 i = 0; i < 3; ++i) {
        globalRankList.push_back(CreateRankInfo(i, "server0", static_cast<s32>(i), 0));
    }
    std::vector<RankInfo_t> subRankList = globalRankList;
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankNum, rankIds, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

/**
 * @brief TC011: ParsePlane 空指针测试
 * @details 验证空指针输入返回错误
 * @预期结果 返回 HCCL_E_PARA
 */
TEST_F(TopoinfoPlaneTransformerTest, TC011_ParsePlaneNullPointer_When_NullPointer_Expect_Error)
{
    // Arrange
    u32 rankNum = 3;
    std::vector<RankInfo_t> globalRankList;
    std::vector<RankInfo_t> subRankList;
    u32 netPlaneNum = 0;

    // Act
    HcclResult ret = TopoinfoPlaneTransformer::ParsePlane(rankNum, nullptr, globalRankList, subRankList, netPlaneNum);

    // Assert
    EXPECT_EQ(ret, HCCL_E_PARA);
}
