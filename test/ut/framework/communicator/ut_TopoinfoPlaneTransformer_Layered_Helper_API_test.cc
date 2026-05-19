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
#include "topoinfo_plane_transformer.h"

namespace {
using namespace hccl;

std::vector<std::vector<u32>> BuildCachedSubGroups()
{
    return {{0U, 1U}, {2U, 3U}};
}

std::vector<std::vector<u32>> BuildAsymmetricSubGroups()
{
    return {{0U, 1U, 2U}, {3U}};
}
} // namespace

class HcclTopoinfoPlaneTransformerLayeredHelperApiTest : public BaseInit {
};

TEST_F(HcclTopoinfoPlaneTransformerLayeredHelperApiTest,
    Ut_TopoinfoPlaneTransformer_When_ReparseGroupedPlaneOnSymmetricGroups_Expect_NetPlaneUpdatedByLocalPosition)
{
    std::vector<std::vector<u32>> subGroups = BuildCachedSubGroups();
    u32 netPlaneId = 3U;
    u32 netPlaneNum = 4U;

    ASSERT_EQ(TopoinfoPlaneTransformer::ReparseGroupedPlane(1U, subGroups, netPlaneId, netPlaneNum), HCCL_SUCCESS);

    EXPECT_EQ(netPlaneId, 7U);
    EXPECT_EQ(netPlaneNum, 8U);
}

TEST_F(HcclTopoinfoPlaneTransformerLayeredHelperApiTest,
    Ut_TopoinfoPlaneTransformer_When_AsymmetricGroupsUseBrokeAlgo_Expect_GroupsMutatedByA2CallShape)
{
    std::vector<std::vector<u32>> subGroups = BuildAsymmetricSubGroups();
    HcclAlgoType intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    HcclAlgoType interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;

    ASSERT_EQ(TopoinfoPlaneTransformer::RegroupAndSelectAlgo(1U, true, subGroups, intraAlgType, interAlgType),
        HCCL_SUCCESS);

    EXPECT_EQ(subGroups, (std::vector<std::vector<u32>> {{0U}, {1U}, {2U}, {3U}}));
    EXPECT_EQ(intraAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(interAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
}
