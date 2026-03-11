/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "hccl_api.h"
#include "hccl_communicator.h"
#include "hccl_comm_pub.h"
#include "hccl_types.h"
#include "llt_hccl_stub_pub.h"
#include "param_check_pub.h"
#include "hccl_api_base_test.h"
#include "llt_hccl_stub_GenRankTable.h"

using namespace hccl;

/**
 * @brief HcclGetNetPlaneInfo API 测试类
 */
class HcclGetNetPlaneInfoAPITest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 每个测试用例执行前的初始化
    }

    void TearDown() override
    {
        // 每个测试用例执行后的清理
    }
};

/**
 * @brief TC001: 测试 netPlaneId 空指针场景
 * @预期: 返回 HCCL_E_PTR
 */
TEST_F(HcclGetNetPlaneInfoAPITest, TC001_GetNetPlaneId_NullPtr_When_NullNetPlaneId_Expect_HCCL_E_PTR)
{
    // 执行：传入空指针（不需要创建通信域，直接测试 API）
    HcclComm dummyComm = reinterpret_cast<HcclComm>(0x12345678); // 非空地址
    uint32_t *nullNetPlaneId = nullptr;
    HcclResult ret = HcclGetNetPlaneId(dummyComm, nullNetPlaneId);

    // 验证：应返回空指针错误
    EXPECT_EQ(ret, HCCL_E_PTR);
}

/**
 * @brief TC002: 测试 netPlaneNum 空指针场景
 * @预期: 返回 HCCL_E_PTR
 */
TEST_F(HcclGetNetPlaneInfoAPITest, TC002_GetNetPlaneNum_NullPtr_When_NullNetPlaneNum_Expect_HCCL_E_PTR)
{
    // 执行：传入空指针（不需要创建通信域，直接测试 API）
    HcclComm dummyComm = reinterpret_cast<HcclComm>(0x12345678); // 非空地址
    uint32_t *nullNetPlaneNum = nullptr;
    HcclResult ret = HcclGetNetPlaneNum(dummyComm, nullNetPlaneNum);

    // 验证：应返回空指针错误
    EXPECT_EQ(ret, HCCL_E_PTR);
}

/**
 * @brief TC003: 测试 comm 空指针场景
 * @预期: 返回 HCCL_E_PTR
 */
TEST_F(HcclGetNetPlaneInfoAPITest, TC003_GetNetPlaneId_NullPtr_When_NullComm_Expect_HCCL_E_PTR)
{
    // 执行：传入空通信域
    uint32_t netPlaneId = 0;
    HcclResult ret = HcclGetNetPlaneId(nullptr, &netPlaneId);

    // 验证：应返回空指针错误
    EXPECT_EQ(ret, HCCL_E_PTR);
}
