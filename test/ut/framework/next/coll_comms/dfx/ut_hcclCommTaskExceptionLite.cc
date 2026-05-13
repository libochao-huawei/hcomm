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
#include "hcclCommTaskException.h"

using namespace hccl;
using namespace hcomm;

constexpr u32 MAX_MODULE_DEVICE_NUM_V2 = 65;
extern std::array<std::map<s32, GetAicpuTaskExceptionCallBackHcomm>, MAX_MODULE_DEVICE_NUM_V2> g_communicatorCallbackMapV2;

constexpr u32 RT_UB_LOCAL_OPERATIOINERR = 0x2;
constexpr u32 RT_UB_REMOTE_OPERATIOINERR = 0x3;
constexpr u32 RT_UB_LINK_FAILEDERR = 0x5;

class hcclCommTaskExceptionLiteTest : public testing::Test
{
protected:
    virtual void SetUp() override
    {
        g_communicatorCallbackMapV2.fill({});
        MOCKER(::getpid)
            .stubs()
            .will(returnValue(12345));
        MOCKER(HrtHalDrvQueryProcessHostPid)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        Hccl::DlHalFunctionV2::GetInstance().dlHalEschedSubmitEvent = [](unsigned int, struct event_summary *) -> drvError_t {
            return DRV_ERROR_NONE;
        };
        HcclCommTaskExceptionLite::GetInstance().Init(0);
    }

    virtual void TearDown() override
    {
        g_communicatorCallbackMapV2.fill({});
        GlobalMockObject::verify();
    }
private:
    u32 notifyId = 1;
    u32 tsId = 2;
};

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SwitchUBCqeErrCodeToTsErrCode_When_Normal_Expect_ReturnIsCorrect)
{
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
    rtLogicCqReport_t exceptionInfo;
    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().ProcessCqe(nullptr, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::SetTaskExceptionEnable(true);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_UBSqeType_Expect_ReturnHCCL_SUCCESS)
{
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 9;
    exceptionInfo.errorCode = RT_UB_LOCAL_OPERATIOINERR;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_SDMASqeType_Expect_ReturnHCCL_SUCCESS)
{
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 11;
    exceptionInfo.errorCode = RT_SDMA_COMPERR;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_SendTaskExceptionByMBox_When_OtherSqeType_Expect_ReturnHCCL_SUCCESS)
{
    rtLogicCqReport_t exceptionInfo;
    exceptionInfo.sqeType = 8;
    exceptionInfo.errorCode = 123;

    HcclResult ret = HcclCommTaskExceptionLite::GetInstance().SendTaskExceptionByMBox(notifyId, tsId, exceptionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_RegisterGetAicpuTaskExceptionCallBack_When_Normal_Expect_SUCCESS)
{
    s32 streamId = 1;
    u32 deviceLogicId = 0;
    auto callback = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };

    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback);
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_UnregisterGetAicpuTaskExceptionCallBack_When_Registered_Expect_Removed)
{
    s32 streamId = 2;
    u32 deviceLogicId = 1;
    auto callback = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };

    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback);
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
    TaskExceptionHostManager::UnregisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId);
    EXPECT_FALSE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId) !=
                 g_communicatorCallbackMapV2[deviceLogicId].end());
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_UnregisterGetAicpuTaskExceptionCallBack_When_NoRegistered_Expect_NoCrash)
{
    s32 streamId = 999;
    u32 deviceLogicId = 0;

    TaskExceptionHostManager::UnregisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId);
    EXPECT_TRUE(true);
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_RegisterMultipleCallBacks_SameDevice_DifferentStream_Expect_AllStored)
{
    u32 deviceLogicId = 2;
    s32 streamId1 = 10;
    s32 streamId2 = 20;
    s32 streamId3 = 30;
    auto callback1 = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };
    auto callback2 = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };
    auto callback3 = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };

    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId1, deviceLogicId, callback1);
    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId2, deviceLogicId, callback2);
    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId3, deviceLogicId, callback3);

    EXPECT_EQ(g_communicatorCallbackMapV2[deviceLogicId].size(), 3u);
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId1) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId2) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId3) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_UnregisterOneCallBack_OtherCallbacksPreserved_Expect_Correct)
{
    u32 deviceLogicId = 3;
    s32 streamId1 = 100;
    s32 streamId2 = 200;
    auto callback1 = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };
    auto callback2 = []() -> Hccl::ErrorMessageReport {
        Hccl::ErrorMessageReport report;
        return report;
    };

    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId1, deviceLogicId, callback1);
    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId2, deviceLogicId, callback2);
    EXPECT_EQ(g_communicatorCallbackMapV2[deviceLogicId].size(), 2u);
    TaskExceptionHostManager::UnregisterGetAicpuTaskExceptionCallBack(streamId1, deviceLogicId);
    EXPECT_EQ(g_communicatorCallbackMapV2[deviceLogicId].size(), 1u);
    EXPECT_TRUE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId2) !=
                g_communicatorCallbackMapV2[deviceLogicId].end());
    EXPECT_FALSE(g_communicatorCallbackMapV2[deviceLogicId].find(streamId1) !=
                 g_communicatorCallbackMapV2[deviceLogicId].end());
}

TEST_F(hcclCommTaskExceptionLiteTest, Ut_CallbackOverwrite_SameStreamId_Expect_Updated)
{
    u32 deviceLogicId = 4;
    s32 streamId = 500;
    bool callback1Called = false;
    bool callback2Called = false;

    auto callback1 = [&callback1Called]() -> Hccl::ErrorMessageReport {
        callback1Called = true;
        Hccl::ErrorMessageReport report;
        return report;
    };
    auto callback2 = [&callback2Called]() -> Hccl::ErrorMessageReport {
        callback2Called = true;
        Hccl::ErrorMessageReport report;
        return report;
    };

    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback1);
    TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId, callback2);
    EXPECT_EQ(g_communicatorCallbackMapV2[deviceLogicId].size(), 1u);
    auto it = g_communicatorCallbackMapV2[deviceLogicId].find(streamId);
    ASSERT_TRUE(it != g_communicatorCallbackMapV2[deviceLogicId].end());
    it->second();
    EXPECT_TRUE(callback2Called);
    EXPECT_FALSE(callback1Called);
}

