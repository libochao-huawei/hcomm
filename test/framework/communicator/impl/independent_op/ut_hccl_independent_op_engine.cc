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
#include "hccl_api_base_test.h"
#include "hccl_tbe_task.h"
#include "adapter_hal.h"
#include "dispatcher_ctx.h"

using namespace hccl;
const char* RANKTABLE_FILE_NAME = nullptr;

class HcclIndependentOpEngineTest : public BaseInit {
public:
    void SetUp() override {
        MOCKER(HcclTbeTaskInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        RANKTABLE_FILE_NAME = rankTableFileName;
        EXPECT_EQ(RANKTABLE_FILE_NAME != nullptr, true);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

void CreateCommByConfig(HcclComm *comm, int32_t commEngine, uint32_t threadNum, uint32_t notifyNumPerThread)
{
    Ut_Device_Set(0);
    u32 rankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    commConfig.commEngine = commEngine;
    if (threadNum != 0) {
        commConfig.threadNum = threadNum;
    }
    if (notifyNumPerThread != 0) {
        commConfig.notifyNumPerThread = notifyNumPerThread;
    }
    commConfig.hcclBufferSize = 400;
    HcclResult ret = HcclCommInitClusterInfoConfig(RANKTABLE_FILE_NAME, rankId, &commConfig, comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclIndependentOpEngineTest, Ut_HcclAllocThreadRes_When_Param_Is_Invalid_Expect_Para_Error)
{
    ThreadHandle thread[2] = {0};
    HcclResult ret = HcclAllocThreadRes(nullptr, CommEngine::COMM_ENGINE_HOSTCPU, 2, 1, thread);
    EXPECT_EQ(ret, HCCL_E_PTR);

    CreateCommByConfig(&comm, static_cast<int32_t>(CommEngine::COMM_ENGINE_HOSTCPU), 4, 2);
    ret = HcclAllocThreadRes(comm, CommEngine::COMM_ENGINE_HOSTCPU, 2, 1, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
    ret = HcclAllocThreadRes(comm, CommEngine::COMM_ENGINE_RESERVED, 2, 1, thread);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = HcclAllocThreadRes(comm, CommEngine::COMM_ENGINE_HOSTCPU, 5, 1, thread);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = HcclAllocThreadRes(comm, CommEngine::COMM_ENGINE_HOSTCPU, 2, 3, thread);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
}

// -----HcclAllocThreadRes接口host侧用例-------
// 限定数量

HcclResult hrtDrvGetPlatformInfoStub(uint32_t *info)
{
    *info = 1;
    return HCCL_SUCCESS;
}

void LocalCopyFfts(ThreadHandle thread) {
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .will(invoke(hrtDrvGetPlatformInfoStub));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .with(any())
    .will(returnValue(true));

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(GetExternalInputHcclAicpuUnfold)
    .stubs()
    .with(any())
    .will(returnValue(false));

    MOCKER(GetWorkflowMode)
    .stubs()
    .with(any())
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    DispatcherCtxPtr ctx;
    HcclResult ret = CreateDispatcherCtx(&ctx, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    DispatcherCtx *ctxPtr = static_cast<DispatcherCtx *>(ctx);
    EXPECT_NE(ctxPtr->GetDispatcher(), nullptr);
    EXPECT_NE(GetDispatcherCtx(), nullptr);

    HostMem userIn = HostMem::alloc(1, true);
    HostMem cclIn = HostMem::alloc(1, true);
    HcclMem userInputMem{HcclMemType::HCCL_MEM_TYPE_HOST, userIn.ptr(), 1};
    HcclMem cclInputMem{HcclMemType::HCCL_MEM_TYPE_HOST, cclIn.ptr(), 1};

    ret = HcommLocalCopyOnThread(thread, &userInputMem, &cclInputMem, 1);
    EXPECT_EQ(HCCL_SUCCESS, ret);
    ret = DestroyDispatcherCtx(ctx);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

const CommEngine g_hostEngine = CommEngine::COMM_ENGINE_HOSTCPU;
TEST_F(HcclIndependentOpEngineTest, Ut_HcclAllocThreadRes_When_Alloced_Threads_Morethan_Quota_Expect_Unavailable)
{
    bool isDeviceSide = false;
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));

    u32 info = 1;
    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .with(outBoundP(&info, sizeof(info)))
    .will(returnValue(HCCL_SUCCESS));

    CreateCommByConfig(&comm, static_cast<int32_t>(g_hostEngine), 4, 2);
    ThreadHandle thread1[2] = {0};
    HcclResult ret = HcclAllocThreadRes(comm, g_hostEngine, 2, 2, thread1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < 2; i++) {
        EXPECT_NE(thread1[i], 0);
    }

    LocalCopyFfts(thread1[0]);

    ThreadHandle thread2[1] = {0};
    ret = HcclAllocThreadRes(comm, g_hostEngine, 1, 2, thread2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < 1; i++) {
        EXPECT_NE(thread2[i], 0);
    }

    ThreadHandle thread3[2] = {0};
    ret = HcclAllocThreadRes(comm, g_hostEngine, 2, 1, thread3);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
    Ut_Comm_Destroy(comm);
}

// 默认数量
TEST_F(HcclIndependentOpEngineTest, Ut_HcclAllocThreadRes_When_Alloced_Notify_Morethan_Quota_Expect_Unavailable)
{
    CreateCommByConfig(&comm, static_cast<int32_t>(CommEngine::COMM_ENGINE_HOSTCPU), 0, 0);
    ThreadHandle thread1[2] = {0};
    HcclResult ret = HcclAllocThreadRes(comm, g_hostEngine, 2, 100, thread1);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
    Ut_Comm_Destroy(comm);
}

// -----CommGetNotifyNumInThread接口host侧用例-------
TEST_F(HcclIndependentOpEngineTest, Ut_CommGetNotifyNumInThread_When_Alloced_And_Get_Notify_Success)
{
    bool isDeviceSide = false;
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));
    CreateCommByConfig(&comm, static_cast<int32_t>(CommEngine::COMM_ENGINE_HOSTCPU), 0, 0);
    ThreadHandle thread1[1] = {0};
    uint32_t notifyNum = 2;
    HcclResult ret = HcclAllocThreadRes(comm, g_hostEngine, 1, notifyNum, thread1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    uint32_t getNotifyNum = 0;
    ret = HcclGetNotifyNumInThread(comm, thread1[0], g_hostEngine, &getNotifyNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyNum, getNotifyNum);
    Ut_Comm_Destroy(comm);
}

// -----CommGetNotifyNumInThread接口host侧用例-------
TEST_F(HcclIndependentOpEngineTest, Ut_CommGetNotifyNumInThread_When_Param_Is_Invalid_Expect_Para_Error)
{
    CreateCommByConfig(&comm, static_cast<int32_t>(CommEngine::COMM_ENGINE_HOSTCPU), 0, 0);
    ThreadHandle thread1[1] = {0};
    uint32_t notifyNum = 2;
    uint32_t getNotifyNum = 0;
    HcclResult ret = HcclGetNotifyNumInThread(nullptr, thread1[0], g_hostEngine, &getNotifyNum);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclGetNotifyNumInThread(comm, thread1[0], g_hostEngine, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclGetNotifyNumInThread(comm, thread1[0], g_hostEngine, &getNotifyNum);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclGetNotifyNumInThread(comm, thread1[0], CommEngine::COMM_ENGINE_RESERVED, &getNotifyNum);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
}