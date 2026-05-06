/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hccl_comm_pub.h"
#define private public
#include "hcclCommTaskExceptionLite.h"
#undef private
#include "task_scheduler_error.h"
#include "aicpu_indop_env.h"
#include "adapter_hal_pub.h"
#include "dlhal_function_v2.h"

using namespace hccl;
using namespace hcomm;

constexpr u32 RT_UB_LOCAL_OPERATIOINERR = 0x2;
constexpr u32 RT_UB_REMOTE_OPERATIOINERR = 0x3;
constexpr u32 RT_UB_LINK_FAILEDERR = 0x5;

class hcclCommTaskExceptionLiteTest : public testing::Test
{
protected:
    virtual void SetUp() override {}

    virtual void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SwitchUBCqeErrCodeToTsErrCode_When_Normal_Expect_ReturnIsCorrect)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);
    uint16_t ret = HcclCommTaskExceptionLite::GetInstance().SwitchUBCqeErrCodeToTsErrCode(RT_UB_LOCAL_OPERATIOINERR);
    EXPECT_EQ(ret, TS_ERROR_HCCL_OP_UB_DDRC_FAILED);
    
    ret = HcclCommTaskExceptionLite::GetInstance().SwitchUBCqeErrCodeToTsErrCode(RT_UB_REMOTE_OPERATIOINERR);
    EXPECT_EQ(ret, TS_ERROR_HCCL_OP_UB_POISON_FAILED);

    ret = HcclCommTaskExceptionLite::GetInstance().SwitchUBCqeErrCodeToTsErrCode(RT_UB_LINK_FAILEDERR);
    EXPECT_EQ(ret, TS_ERROR_HCCL_OP_UB_LINK_FAILED);

    ret = HcclCommTaskExceptionLite::GetInstance().SwitchUBCqeErrCodeToTsErrCode(static_cast<u32>(123));
    EXPECT_EQ(ret, TS_ERROR_HCCL_OTHER_ERROR);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SwitchSdmaCqeErrCodeToTsErrCode_When_Normal_Expect_ReturnIsCorrect)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);
    uint16_t ret = HcclCommTaskExceptionLite::GetInstance().SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPERR);
    EXPECT_EQ(ret, TS_ERROR_SDMA_LINK_ERROR);
    
    ret = HcclCommTaskExceptionLite::GetInstance().SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_COMPDATAERR);
    EXPECT_EQ(ret, TS_ERROR_SDMA_POISON_ERROR);

    ret = HcclCommTaskExceptionLite::GetInstance().SwitchSdmaCqeErrCodeToTsErrCode(RT_SDMA_DATAERR);
    EXPECT_EQ(ret, TS_ERROR_SDMA_DDRC_ERROR);

    ret = HcclCommTaskExceptionLite::GetInstance().SwitchSdmaCqeErrCodeToTsErrCode(static_cast<u32>(123));
    EXPECT_EQ(ret, TS_ERROR_HCCL_OTHER_ERROR);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SwitchSdmaCqeErrCodeToTsErrCode_taskexception_disable)
{
    hcomm::SetTaskExceptionEnable(false);
    HcclCommTaskExceptionLite::GetInstance().Init(0);
    rtLogicCqReport_t exceptionInfo;
    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().ProcessCqe(nullptr, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::SetTaskExceptionEnable(true);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_UBSqeType_Expect_ReturnHCCL_SUCCESS)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);

    MOCKER(::getpid)
        .stubs()
        .will(returnValue(12345));

    MOCKER(HrtHalDrvQueryProcessHostPid)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DlHalFunctionV2::dlHalEschedSubmitEvent)
        .stubs()
        .will(returnValue(DRV_ERROR_NONE));

    u32 notifyId = 1;
    u32 tsId = 2;
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 9;
    exceptionInfo.errorCode = RT_UB_LOCAL_OPERATIOINERR;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_SDMASqeType_Expect_ReturnHCCL_SUCCESS)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);

    MOCKER(::getpid)
        .stubs()
        .will(returnValue(12345));

    MOCKER(HrtHalDrvQueryProcessHostPid)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DlHalFunctionV2::dlHalEschedSubmitEvent)
        .stubs()
        .will(returnValue(DRV_ERROR_NONE));

    u32 notifyId = 1;
    u32 tsId = 2;
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 11;
    exceptionInfo.errorCode = RT_SDMA_COMPERR;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_OtherSqeType_Expect_ReturnHCCL_SUCCESS)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);

    MOCKER(::getpid)
        .stubs()
        .will(returnValue(12345));

    MOCKER(HrtHalDrvQueryProcessHostPid)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DlHalFunctionV2::dlHalEschedSubmitEvent)
        .stubs()
        .will(returnValue(DRV_ERROR_NONE));

    u32 notifyId = 1;
    u32 tsId = 2;
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 99;
    exceptionInfo.errorCode = 123;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_DlHalSubmitFailed_Expect_ReturnHCCL_E_DRV)
{
    HcclCommTaskExceptionLite::GetInstance().Init(0);

    MOCKER(::getpid)
        .stubs()
        .will(returnValue(12345));

    MOCKER(HrtHalDrvQueryProcessHostPid)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&DlHalFunctionV2::dlHalEschedSubmitEvent)
        .stubs()
        .will(returnValue(DRV_ERROR_NOT_SUPPORT));

    u32 notifyId = 1;
    u32 tsId = 2;
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 9;
    exceptionInfo.errorCode = RT_UB_LOCAL_OPERATIOINERR;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_E_DRV);
}
