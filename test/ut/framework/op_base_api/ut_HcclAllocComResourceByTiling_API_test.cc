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

class HcclAllocComResourceByTilingTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
    }
    
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
protected:
    void *commContext = nullptr;
    void *mc2Tiling = nullptr;
    rtStream_t stream = nullptr;
};

TEST_F(HcclAllocComResourceByTilingTest, Ut_HcclAllocComResourceByTiling_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    
    int dd = 0;
    mc2Tiling = static_cast<void *>(&dd);
    
    MOCKER(hrtGetHcclV2Support)
        .stubs()
        .with(outBound(true))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCcuMc2ServerNum).stubs().with().will(returnValue(10));
    MOCKER_CPP(&HcclCommunicator::AllocCommResource).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclAllocComResourceByTilingTest, Ut_HcclAllocComResourceByTiling_When_AllocCommResourceFails_Expect_ReturnIsHCCL_E_INTERNAL)
{
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    
    int dd = 0;
    mc2Tiling = static_cast<void *>(&dd);
    
    MOCKER(hrtGetHcclV2Support)
        .stubs()
        .with(outBound(true))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCcuMc2ServerNum).stubs().with().will(returnValue(10));
    MOCKER_CPP(&HcclCommunicator::AllocCommResource).stubs().with(any(), any()).will(returnValue(HCCL_E_INTERNAL));
    
    HcclResult ret = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HcclAllocComResourceByTilingTest, Ut_HcclAllocComResourceByTiling_When_ServerNumExceeds20_Expect_ReturnIsHCCL_E_INTERNAL)
{
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    
    int dd = 0;
    mc2Tiling = static_cast<void *>(&dd);
    
    MOCKER(hrtGetHcclV2Support)
        .stubs()
        .with(outBound(true))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCcuMc2ServerNum).stubs().with().will(returnValue(21));
    MOCKER_CPP(&HcclCommunicator::AllocCommResource).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HcclAllocComResourceByTilingTest, Ut_HcclAllocComResourceByTiling_When_ServerNumIs10_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    
    int dd = 0;
    mc2Tiling = static_cast<void *>(&dd);
    
    MOCKER(hrtGetHcclV2Support)
        .stubs()
        .with(outBound(true))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCcuMc2ServerNum).stubs().with().will(returnValue(10));
    MOCKER_CPP(&HcclCommunicator::AllocCommResource).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclAllocComResourceByTilingTest, Ut_HcclAllocComResourceByTiling_With_Log_Expect_ReturnIsHCCL_SUCCESS)
{
    EnvConfig::GetInstance().logCfg.entryLogEnable = CfgField<bool>({"HCCL_ENTRY_LOG_ENABLE", true, CastBin2Bool});
    EnvConfig::GetInstance().logCfg.entryLogEnable.isParsed = true;
    
    UT_COMM_CREATE_DEFAULT(comm);
    UT_STREAM_CREATE_DEFAULT(stream);
    
    int dd = 0;
    mc2Tiling = static_cast<void *>(&dd);
    
    MOCKER(hrtGetHcclV2Support)
        .stubs()
        .with(outBound(true))
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommunicator::GetCcuMc2ServerNum).stubs().with().will(returnValue(10));
    MOCKER_CPP(&HcclCommunicator::AllocCommResource).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = HcclAllocComResourceByTiling(comm, stream, mc2Tiling, &commContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    EnvConfig::GetInstance().logCfg.entryLogEnable.value = false;
}