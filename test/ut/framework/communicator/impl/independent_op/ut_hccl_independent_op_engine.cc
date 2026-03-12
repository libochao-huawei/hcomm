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
#include "hccl/hccl_res.h"
#include "../../hccl_api_base_test.h"
#include "hccl_tbe_task.h"
#include "adapter_hal.h"
#include "dispatcher_ctx.h"
#include "hcomm_primitives.h"
#include "launch_aicpu.h"

using namespace hccl;
static const char* RANKTABLE_FILE_NAME = nullptr;

class HcclIndependentOpEngineTest : public BaseInit {
public:
    void SetUp() override {
        MOCKER(HcclTbeTaskInit)
            .stubs()
            .will(returnValue(HCCL_SUCCESS));
        BaseInit::SetUp();
        bool isDeviceSide = false;
        MOCKER(GetRunSideIsDevice)
            .stubs()
            .with(outBound(isDeviceSide))
            .will(returnValue(HCCL_SUCCESS));
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
        UT_COMM_CREATE_DEFAULT(comm);
        RANKTABLE_FILE_NAME = rankTableFileName;
        EXPECT_EQ(RANKTABLE_FILE_NAME != nullptr, true);
        EXPECT_EQ(comm != nullptr, true);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
        Ut_Comm_Destroy(comm);
    }
};

TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadAcquire_When_Param_Is_Invalid_Expect_Para_Error)
{
    ThreadHandle threads[2] = {0};
    HcclResult ret = HcclThreadAcquire(nullptr, CommEngine::COMM_ENGINE_CPU_TS , 2, 1, threads);
    EXPECT_EQ(ret, HCCL_E_PTR);

    ret = HcclThreadAcquire(comm, CommEngine::COMM_ENGINE_CPU_TS , 2, 1, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
    ret = HcclThreadAcquire(comm, CommEngine::COMM_ENGINE_RESERVED, 2, 1, threads);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = HcclThreadAcquire(comm, CommEngine::COMM_ENGINE_CPU_TS , 2, 1, threads);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcclThreadAcquire(comm, CommEngine::COMM_ENGINE_CPU_TS , 39, 1, threads);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
    ret = HcclThreadAcquire(comm, CommEngine::COMM_ENGINE_CPU_TS , 1, 64, threads);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
}

// -----HcclThreadAcquire接口host侧用例-------
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

    int32_t retCopy = HcommLocalCopyOnThread(thread, &userInputMem, &cclInputMem, 1);
    EXPECT_EQ(retCopy, 0);
    retCopy = DestroyDispatcherCtx(ctx);
    EXPECT_EQ(retCopy, 0);
}

const CommEngine g_hostEngine = CommEngine::COMM_ENGINE_CPU;
TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadAcquire_When_Alloced_Threads_Morethan_Quota_Expect_Unavailable)
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

    ThreadHandle thread1[2] = {0};
    HcclResult ret = HcclThreadAcquire(comm, g_hostEngine, 2, 2, thread1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < 2; i++) {
        EXPECT_NE(thread1[i], 0);
    }

    LocalCopyFfts(thread1[0]);

    ThreadHandle thread2[1] = {0};
    ret = HcclThreadAcquire(comm, g_hostEngine, 1, 2, thread2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < 1; i++) {
        EXPECT_NE(thread2[i], 0);
    }

    ThreadHandle thread3[2] = {0};
    ret = HcclThreadAcquire(comm, g_hostEngine, 1, 61, thread3);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
}

// 默认数量
TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadAcquire_When_Alloced_Notify_Morethan_Quota_Expect_Unavailable)
{
    ThreadHandle thread1[2] = {0};
    HcclResult ret = HcclThreadAcquire(comm, g_hostEngine, 2, 100, thread1);
    EXPECT_EQ(ret, HCCL_E_UNAVAIL);
}

// -----CommGetNotifyNumInThread接口host侧用例-------
TEST_F(HcclIndependentOpEngineTest, Ut_CommGetNotifyNumInThread_When_Alloced_And_Get_Notify_Success)
{
    bool isDeviceSide = false;
    MOCKER(GetRunSideIsDevice)
    .stubs()
    .with(outBound(isDeviceSide))
    .will(returnValue(HCCL_SUCCESS));
    ThreadHandle thread1[1] = {0};
    uint32_t notifyNum = 2;
    HcclResult ret = HcclThreadAcquire(comm, g_hostEngine, 1, notifyNum, thread1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    uint32_t getNotifyNum = 0;
    ret = HcclGetNotifyNumInThread(comm, thread1[0], g_hostEngine, &getNotifyNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyNum, getNotifyNum);
}

// -----CommGetNotifyNumInThread接口host侧用例-------
TEST_F(HcclIndependentOpEngineTest, Ut_CommGetNotifyNumInThread_When_Param_Is_Invalid_Expect_Para_Error)
{
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
}

// -----HcclThreadExportToCommEngine接口host侧用例-------
TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadExportToCommEngine_When_Param_Is_Invalid)
{
    ThreadHandle threads[1] = {0};
    ThreadHandle exportedThreads[1] = {0};

    // 通信域为空
    HcclResult ret = HcclThreadExportToCommEngine(nullptr, 1, threads, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // 需要转换的thread为0
    ret = HcclThreadExportToCommEngine(comm, 0, threads, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PARA);
    // 需要转换的thread超过40
    ret = HcclThreadExportToCommEngine(comm, 41, threads, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // 需要导入的地址为空
    ret = HcclThreadExportToCommEngine(comm, 1, nullptr, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PTR);

    // 目标引擎非法
    ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_RESERVED, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // 导出地址为空
    ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_CPU, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadExportToCommEngine_When_Engine_Is_Cpu)
{
    ThreadHandle threads[1] = {1};
    ThreadHandle exportedThreads[1] = {2};

    // 未知threadHandle
    HcclResult ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // 添加thread 
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto &threadMgr = hcclComm->GetIndependentOp().GetCommEngineResMgr().threadMgr_;
    threadMgr->threadHandleOthersToCpu_[threads[0]] = exportedThreads[0];
    ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_CPU, exportedThreads);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclIndependentOpEngineTest, Ut_HcclThreadExportToCommEngine_When_Engine_Is_Aicpu)
{
    ThreadHandle threads[1] = {1};
    ThreadHandle exportedThreads[1] = {2};

    // 未知threadHandle
    HcclResult ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_AICPU_TS, exportedThreads);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // 添加thread 
    MOCKER(AicpuAclKernelLaunch).stubs().will(returnValue(HCCL_SUCCESS));

    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    auto &threadMgr = hcclComm->GetIndependentOp().GetCommEngineResMgr().threadMgr_;
    threadMgr->HcclThreadAcquireWithStream(CommEngine::COMM_ENGINE_CPU, nullptr, 1, threads);
    ret = HcclThreadExportToCommEngine(comm, 1, threads, CommEngine::COMM_ENGINE_AICPU_TS, exportedThreads);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
