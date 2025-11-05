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

#ifndef private
#define private public
#define protected public
#endif
#include "common/aicpu_hccl_common.h"
#include "common/aicpu_kfc_def.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "framework/aicpu_hccl_process.h"

#include "llt_hccl_stub_mc2.h"
#include "llt_hccl_stub.h"
#include "env_config.h"
#undef private
#undef protected

using namespace std;
using namespace HcclApi;

class MC2SqeContext_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2SqeContext_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MC2SqeContext_UT TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        g_stubDevType = DevType::DEV_TYPE_910B;
        MOCKER(halGetDeviceInfo).stubs().with(any()).will(invoke(StubhalGetDeviceInfo));
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MC2SqeContext_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "MC2SqeContext_UT Test TearDown" << std::endl;
    }
};

TEST_F(MC2SqeContext_UT, TestQuerySqeInfo)
{
    AicpuSqeContext::InitSqeContext();

    SqeInfo sqeInfo;
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, NOTIFY_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, WRITE_VALUE_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, EVENT_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, MEMCPY_ASYNC_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, CCORE_WAIT_START_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, CCORE_WRITE_VALUE_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, NOTIFY_SQE_V2, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, WRITE_VALUE_SQE_V2, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, EVENT_SQE_V2, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, MEMCPY_ASYNC_SQE_V2, 0, &sqeInfo),
        HCCL_SUCCESS);
    EXPECT_EQ(SqeContextUtils::QuerySqeInfo(GetSqeContext()->buffPtr[0].localBuff, FLIP_PLACEHOLDER_SQE, 0, &sqeInfo),
        HCCL_SUCCESS);
}

TEST_F(MC2SqeContext_UT, TestRtsqTaskTypeToStr)
{
    // Given
    uint8_t type1 = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
    uint8_t type2 = RT_STARS_SQE_TYPE_NOTIFY_RECORD;
    uint8_t type3 = RT_STARS_SQE_TYPE_WRITE_VALUE;
    uint8_t type4 = RT_STARS_SQE_TYPE_SDMA;
    uint8_t type5 = 0xFF; // unknown type

    // When
    std::string result1 = SqeContextUtils::RtsqTaskTypeToStr(type1);
    std::string result2 = SqeContextUtils::RtsqTaskTypeToStr(type2);
    std::string result3 = SqeContextUtils::RtsqTaskTypeToStr(type3);
    std::string result4 = SqeContextUtils::RtsqTaskTypeToStr(type4);
    std::string result5 = SqeContextUtils::RtsqTaskTypeToStr(type5);

    // Then
    EXPECT_EQ(result1, "NOTIFY WAIT");
    EXPECT_EQ(result2, "NOTIFY RECORD");
    EXPECT_EQ(result3, "WRITE VALUE");
    EXPECT_EQ(result4, "SDMA");
    EXPECT_EQ(result5, "255"); // 0xFF in decimal
}