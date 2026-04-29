/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_api_base_test.h"
#include "op_base_v2.h"
#include "task_service.h"

class OpBaseV2Test : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        MOCKER(GetExternalInputHcclEnableEntryLog)
            .stubs()
            .with(any())
            .will(returnValue(true));
    }

    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

// HcclTaskRegisterProfV2 Tests

TEST_F(OpBaseV2Test, Ut_HcclTaskRegisterProfV2_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    Hccl::ProfCallbackTemplate callback = [](const Hccl::TaskParam&, uint64_t) -> HcclResult {
        return HCCL_SUCCESS;
    };
    HcclResult ret = HcclTaskRegisterProfV2(nullptr, callback);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(OpBaseV2Test, Ut_HcclTaskRegisterProfV2_When_CommNotFound_Expect_ReturnIsHCCL_E_NOT_FOUND)
{
    UT_COMM_CREATE_DEFAULT(comm);
    Hccl::ProfCallbackTemplate callback = [](const Hccl::TaskParam&, uint64_t) -> HcclResult {
        return HCCL_SUCCESS;
    };
    // 不注册到g_taskServiceMap，模拟找不到的情况
    MOCKER(HcclCommunicator::GetId).stubs().will(returnValue("nonexistent_comm_id"));
    HcclResult ret = HcclTaskRegisterProfV2(comm, callback);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    Ut_Comm_Destroy(comm);
}

TEST_F(OpBaseV2Test, Ut_HcclTaskRegisterProfV2_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    Hccl::ProfCallbackTemplate callback = [](const Hccl::TaskParam&, uint64_t) -> HcclResult {
        return HCCL_SUCCESS;
    };
    // Mock TaskService::TaskProfRegister返回成功
    Hccl::TaskService taskService;
    MOCKER_CPP_VIRTUAL(taskService, &Hccl::TaskService::TaskProfRegister)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcclTaskRegisterProfV2(comm, callback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
}

TEST_F(OpBaseV2Test, Ut_HcclTaskRegisterProfV2_When_TaskProfRegisterFails_Expect_ErrorCodePropagated)
{
    UT_COMM_CREATE_DEFAULT(comm);
    Hccl::ProfCallbackTemplate callback = [](const Hccl::TaskParam&, uint64_t) -> HcclResult {
        return HCCL_SUCCESS;
    };
    Hccl::TaskService taskService;
    MOCKER_CPP_VIRTUAL(taskService, &Hccl::TaskService::TaskProfRegister)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));
    HcclResult ret = HcclTaskRegisterProfV2(comm, callback);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    Ut_Comm_Destroy(comm);
}

// HcclGetDpuSteamIdV2 Tests

TEST_F(OpBaseV2Test, Ut_HcclGetDpuSteamIdV2_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    u32 dpuStreamId = 0;
    HcclResult ret = HcclGetDpuSteamIdV2(nullptr, dpuStreamId);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(OpBaseV2Test, Ut_HcclGetDpuSteamIdV2_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    u32 dpuStreamId = 0;
    // Mock HcclCommunicator::GetStreamId返回成功
    HcclCommunicator communicator;
    MOCKER_CPP_VIRTUAL(communicator, &HcclCommunicator::GetStreamId)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcclGetDpuSteamIdV2(comm, dpuStreamId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    Ut_Comm_Destroy(comm);
}

TEST_F(OpBaseV2Test, Ut_HcclGetDpuSteamIdV2_When_GetStreamIdFails_Expect_ErrorCodePropagated)
{
    UT_COMM_CREATE_DEFAULT(comm);
    u32 dpuStreamId = 0;
    HcclCommunicator communicator;
    MOCKER_CPP_VIRTUAL(communicator, &HcclCommunicator::GetStreamId)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_E_INTERNAL));
    HcclResult ret = HcclGetDpuSteamIdV2(comm, dpuStreamId);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    Ut_Comm_Destroy(comm);
}
