/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdlib>

#include "hccl_api_base_test.h"
#include "topoinfo_plane_transformer.h"

namespace {
using namespace hccl;

struct ScopedEnv {
    std::string key;
    std::string oldValue;
    bool hadValue {false};

    ScopedEnv(const char *envKey, const char *envValue) : key(envKey)
    {
        const char *old = std::getenv(envKey);
        if (old != nullptr) {
            hadValue = true;
            oldValue = old;
        }
        setenv(envKey, envValue, 1);
    }

    ~ScopedEnv()
    {
        if (hadValue) {
            setenv(key.c_str(), oldValue.c_str(), 1);
        } else {
            unsetenv(key.c_str());
        }
    }
};
} // namespace

class HcclTopoinfoPlaneTransformerA2SelectionApiTest : public BaseInit {
};

TEST_F(HcclTopoinfoPlaneTransformerA2SelectionApiTest,
    Ut_TopoinfoPlaneTransformer_When_EnableBrokeIsTrue_Expect_AlphaBetaChoosesBestSymmSizeAndMutatesGroups)
{
    std::vector<std::vector<u32>> groups {{0U, 1U, 2U}, {3U, 4U}};
    HcclAlgoType intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    HcclAlgoType interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;

    ASSERT_EQ(TopoinfoPlaneTransformer::RegroupAndSelectAlgo(1U, true, groups, intraAlgType, interAlgType), HCCL_SUCCESS);
    EXPECT_EQ(groups, (std::vector<std::vector<u32>> {{0U}, {1U}, {2U}, {3U}, {4U}}));
    EXPECT_EQ(intraAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
    EXPECT_EQ(interAlgType, HcclAlgoType::HCCL_ALGO_TYPE_RING);
}

TEST_F(HcclTopoinfoPlaneTransformerA2SelectionApiTest,
    Ut_TopoinfoPlaneTransformer_When_EnableBrokeIsFalse_Expect_GcdGroupingAndEnvOverride)
{
    std::vector<std::vector<u32>> groups {{0U, 1U, 2U, 3U}, {4U, 5U, 6U, 7U, 8U, 9U}};
    ScopedEnv intraEnv("HCCL_AHC_ALGO_INTRA", "NB");
    ScopedEnv interEnv("HCCL_AHC_ALGO_INTER", "HD");
    HcclAlgoType intraAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;
    HcclAlgoType interAlgType = HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT;

    ASSERT_EQ(TopoinfoPlaneTransformer::RegroupAndSelectAlgo(2U, false, groups, intraAlgType, interAlgType), HCCL_SUCCESS);
    EXPECT_EQ(groups, (std::vector<std::vector<u32>> {{0U, 1U}, {2U, 3U}, {4U, 5U}, {6U, 7U}, {8U, 9U}}));
    EXPECT_EQ(intraAlgType, HcclAlgoType::HCCL_ALGO_TYPE_NB);
    EXPECT_EQ(interAlgType, HcclAlgoType::HCCL_ALGO_TYPE_HDR);
}
