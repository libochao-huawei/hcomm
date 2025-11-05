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
#include "common/aicpu_kfc_def.h"
#include "framework/aicpu_hccl_process.h"
#include "framework/aicpu_communicator.h"
#include "framework/aicpu_kfc_rpc_server.h"
#include "framework/aicpu_kfc_deprecated_process.h"
#include "framework/aicpu_hdc.h"
#include "framework/aicpu_kfc_process.h"
#include "aicpu_kfc/aicpu_kfc_interface.h"
#include "algorithm/aicpu_allreduce.h"
#include "algorithm/task_orchestrator.h"
#include "algorithm/aicpu_dmy_cal_allreduce.h"
#include "dlhal_function.h"
#include "utils/aicpu_hdc_utils.h"
#include "executor_tracer.h"
#include "dfx/aicpu_executor_tracer.h"
#include "hccl_communicator.h"
#include "../stub/llt_hccl_stub_mc2.h"
#include "../stub/llt_hccl_stub.h"
#include "env_config.h"
#undef private
#undef protected

using namespace std;

constexpr u32 h2dBufferSize = sizeof(KfcExecControl);
constexpr u32 d2hBufferSize = sizeof(KfcExecStatus);

class MC2AicpuRetry_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2AicpuRetry_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MC2AicpuRetry_ST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        g_stubDevType = DevType::DEV_TYPE_910B;
        MOCKER(halGetDeviceInfo)
            .stubs()
            .with(any())
            .will(invoke(StubhalGetDeviceInfo));

        DlHalFunction::GetInstance().DlHalFunctionInit();
        hrtSetDevice(0);
        set_chip_type_stub(0, static_cast<s32>(DevType::DEV_TYPE_910B));
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "MC2AicpuRetry_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        ResetMC2Context();
        GlobalMockObject::verify();
        std::cout << "MC2AicpuRetry_ST Test TearDown" << std::endl;
    }
};

#define init_kfc_args(initTask)                                                             \
    StubHccCommRes commRes;                                                                 \
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();                      \
    AicpuKfcRpcServer::RpcMsgBody msgBody;                                                     \
    (void)memset_s(&msgBody, sizeof(msgBody), 0, sizeof(msgBody));                          \
    paramTask.mc2WorkSpace.workSpace = uint64_t(&msgBody);                                  \
                                                                                            \
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;                                       \
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;                                       \
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));        \
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));        \
    h2dTransfer->InitHost();                                                                \
    d2hTransfer->InitHost();                                                                \
    paramTask.kfcControlTransferH2DParams = h2dTransfer->GetCommunicateParams();                      \
    paramTask.kfcStatusTransferD2HParams = d2hTransfer->GetCommunicateParams();                      \
    paramTask.config.retryEnable = 1;                                                       \
    initTask.context = uint64_t(&paramTask);                                                \

#define init_tiling_data(tilingData)                                                        \
    tilingData.commOrder = 0;                                                               \
    tilingData.commType = HCCL_CMD_ALLREDUCE;                                               \
    tilingData.dataType = HCCL_DATA_TYPE_FP16;                                              \
    tilingData.turnNum = 1;                                                                 \
    tilingData.sendCnt = 256;                                                               \
    tilingData.totalCnt = 1;                                                                \
    tilingData.rspPolicy = 0;                                                               \
    tilingData.waitPolicy = 0;                                                              \
    tilingData.taskType = HCCL_KFC_TASK_HCCL_ONLY_EXE;                                      \
    tilingData.useBufferType = 0;                                                           \
    tilingData.debugMode = MC2_DEBUG_WAIT_COMM;                                             \
    tilingData.preparePosition = TASK_PREPARE_HOST;                                         \
    tilingData.notifyOff = 0;                                                               \

#define init_kfc_task(kfcTask)                                                              \
    u64* a = (u64*)malloc(1024*1024);                                                       \
    u64* b = (u64*)malloc(1024*1024);                                                       \
    kfcTask.inputA = uint64_t(a);                                                           \
    kfcTask.outputC = 0;                                                                    \
    kfcTask.commOut = uint64_t(b);                                                          \
    kfcTask.context = uint64_t(&paramTask);                                                 \
    kfcTask.tilingData = uint64_t(&tilingData);                                             \

#define deinit()                                                                            \
    free(a);                                                                                \
    free(b);                                                                                \


uint8_t sqBuffer[64 * 32 * 64];
drvError_t halSqCpQueryStub_1(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (info == nullptr) {
        return DRV_ERROR_INNER_ERR;
    }
    static u32 counter = 1;
    auto queryinfo = *info;
    switch (queryinfo.prop) {
        case DRV_SQCQ_PROP_SQ_HEAD: {
            info->value[0] = 1;
            return DRV_ERROR_NONE;
        }
        case DRV_SQCQ_PROP_SQ_DEPTH: {
            info->value[0] = 4096;
            return DRV_ERROR_NONE;
        }
        case DRV_SQCQ_PROP_SQ_TAIL: {
            info->value[0] = counter;
            counter++;
            return DRV_ERROR_NONE;
        };
        case DRV_SQCQ_PROP_SQ_BASE: {
            uint8_t *buffer = sqBuffer;
            info->value[0] = reinterpret_cast<uintptr_t>(buffer) & 0xFFFFFFFF;
            info->value[1] = reinterpret_cast<uintptr_t>(buffer) >> 32;
        }
        default:return DRV_ERROR_NONE;
    }
}

drvError_t halSqCpQueryStub_2(uint32_t devId, struct halSqCqQueryInfo *info)
{
    if (info == nullptr) {
        return DRV_ERROR_INNER_ERR;
    }
    static u32 counter = 1;
    auto queryinfo = *info;
    switch (queryinfo.prop) {
        case DRV_SQCQ_PROP_SQ_HEAD: {
            info->value[0] = 2;
            return DRV_ERROR_NONE;
        }
        case DRV_SQCQ_PROP_SQ_DEPTH: {
            info->value[0] = 4096;
            return DRV_ERROR_NONE;
        }
        case DRV_SQCQ_PROP_SQ_TAIL: {
            info->value[0] = counter;
            counter++;
            counter++;
            return DRV_ERROR_NONE;
        };
        case DRV_SQCQ_PROP_SQ_BASE: {
            uint8_t *buffer = sqBuffer;
            info->value[0] = reinterpret_cast<uintptr_t>(buffer) & 0xFFFFFFFF;
            info->value[1] = reinterpret_cast<uintptr_t>(buffer) >> 32;
        }
        default:return DRV_ERROR_NONE;
    }
}
#if 0
//host在N秒快恢的场景下进行的suspend, stopExec和 clean coredump
TEST_F(MC2AicpuRetry_ST, st_suspend_in_once_stopLaunch)
{
    
   HcclCommunicator* hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes{};
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request{};
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response{};
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kStoplaunch;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsStopLaunch);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);

    MOCKER_CPP(&OpRetryManager::SetRetryStateToWaitResume)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&OpRetryManager::ExitWaitResumeState)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = hcclCommunicator->Suspend();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_SUSPENDING, ret);
    hcclCommunicator->isNsRecovery_ = false; //保证析构的时候不会调用到相关命令
    delete hcclCommunicator;
}

TEST_F(MC2AicpuRetry_ST, st_suspend_in_once_Error)
{
    
    HcclCommunicator* hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kRuning状态
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kError;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsStopLaunch);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    MOCKER_CPP(&OpRetryManager::SetRetryStateToWaitResume)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
 
    MOCKER_CPP(&OpRetryManager::ExitWaitResumeState)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = hcclCommunicator->Suspend();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;  
}

TEST_F(MC2AicpuRetry_ST, st_stopExec_in_once_stopExec)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
 
 
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStoplaunch;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kStopExec;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsStopExec);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->StopExec();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_SUSPENDING, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;  
}

TEST_F(MC2AicpuRetry_ST, st_stopExec_in_once_End)
{
    
     HcclCommunicator* hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
   hcclCommunicator->SetMC2EnvFlag();
    //printf("环境是[%d]",hcclCommunicator->kfcControlTransferH2D_->GetEnv());
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStoplaunch;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kEnd;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsStopExec);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
        
    });
    usleep(10000);
    auto ret = hcclCommunicator->StopExec();
    threadHandle.join();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}

TEST_F(MC2AicpuRetry_ST, st_stopExec_in_once_Error)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
 
 
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStoplaunch;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kError;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsStopExec);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->StopExec();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator; 
}

TEST_F(MC2AicpuRetry_ST, st_clean_in_once_clean)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
 
 
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStopExec;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kClear;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsClear);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->Clean();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_SUSPENDING, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}

TEST_F(MC2AicpuRetry_ST, st_clean_in_once_End)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    //printf("环境是[%d]",hcclCommunicator->kfcControlTransferH2D_->GetEnv());
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStopExec;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kEnd;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsClear);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
        
    });
    usleep(10000);
    auto ret = hcclCommunicator->Clean();
    threadHandle.join();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}

TEST_F(MC2AicpuRetry_ST, st_clean_in_once_Error)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
 
 
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kStopExec;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kError;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsClear);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->Clean();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}

TEST_F(MC2AicpuRetry_ST, st_cleanEnd_in_once_End)
{

    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    // 这个里面就是需要有两个内容来保证device侧和host侧是一个共享内存
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    //printf("环境是[%d]",hcclCommunicator->kfcControlTransferH2D_->GetEnv());
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    thread threadHandle([&] {
        response.execStatus.kfcStatus = KfcStatus :: kEnd;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // 给host侧下发kStopLaunch
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.kfcCmd != KfcCommand::kNone) {
                break;
            }
        }
        response.execStatus.kfcStatus = KfcStatus ::kEnd;
        d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        EXPECT_EQ(request.kfcCmd, KfcCommand ::NsClear);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
        
    });
    usleep(10000);
    auto ret = hcclCommunicator->Clean();
    threadHandle.join();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
} 
 
TEST_F(MC2AicpuRetry_ST, st_UnRegisterBackGroundThread_success){
    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_310P1;
    hcclCommunicator->UnRegisterBackGroundThread();
 
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910;
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.bgCmd == BackgroundCommand::kStop) {
                break;
            }
            response.execStatus.backgroundStatus = BackgroundStatus ::kStop;
            d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        }
        EXPECT_EQ(request.bgCmd, BackgroundCommand::kStop);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->UnRegisterBackGroundThread();
    threadHandle.join();
    EXPECT_EQ(HCCL_SUCCESS, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}
 
TEST_F(MC2AicpuRetry_ST, st_UnRegisterBackGroundThread_fail){
    MOCKER_CPP(&HcclCommunicator::HcclGetCmdTimeout)
    .stubs()
    .will(returnValue(50));
    HcclCommunicator *hcclCommunicator = new HcclCommunicator();
    hcclCommunicator->deviceType_ = DevType::DEV_TYPE_910;
    StubHccCommRes commRes;
    HccCommResParamTask paramTask = commRes.StubHccCommResParamTask();
    std::unique_ptr<AicpuKfcRpcServer::RpcMsgBody> msgBody;
    paramTask.mc2WorkSpace.workSpace = reinterpret_cast<uint64_t>(msgBody.get());
    hcclCommunicator->kfcControlTransferH2D_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    hcclCommunicator->kfcStatusTransferD2H_.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    hcclCommunicator->kfcControlTransferH2D_->InitHost();
    hcclCommunicator->kfcStatusTransferD2H_->InitHost();
    paramTask.kfcControlTransferH2DParams = hcclCommunicator->kfcControlTransferH2D_->GetCommunicateParams();
    paramTask.kfcStatusTransferD2HParams = hcclCommunicator->kfcStatusTransferD2H_->GetCommunicateParams();
    hcclCommunicator->SetMC2EnvFlag();
    // 现在就是实现device侧对应的内容->保证device侧共享内存和host侧共享内存是一个
    std::shared_ptr<hccl::HDCommunicate> h2dTransfer;
    std::shared_ptr<hccl::HDCommunicate> d2hTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    d2hTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_D2H, d2hBufferSize));
    h2dTransfer->InitDevice(paramTask.kfcControlTransferH2DParams);
    d2hTransfer->InitDevice(paramTask.kfcStatusTransferD2HParams);
    KfcExecControl request;
    memset_s(&request, sizeof(KfcExecControl), 0, sizeof(KfcExecControl));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    // 这里是模拟对应的内容
    thread threadHandle([&] {
        while (true) {
            h2dTransfer->Get(0, sizeof(KfcExecControl), (u8 *)&request);  // 从host侧拿到KstopLaunch的命令字
            if (request.bgCmd == BackgroundCommand::kStop) {
                break;
            }
            response.execStatus.backgroundStatus = BackgroundStatus ::kNone;
            d2hTransfer->Put(0, sizeof(KfcExecStatus), (u8 *)&response);  // device就会把状态改为KStopLaunch
        }
        EXPECT_EQ(request.bgCmd, BackgroundCommand::kStop);  // 这个时候就是希望从host侧拿到的命令字NsStopLaunch
    });
    usleep(10000);
    auto ret = hcclCommunicator->UnRegisterBackGroundThread();
    threadHandle.join();
    EXPECT_EQ(HCCL_E_INTERNAL, ret);
    hcclCommunicator->isNsRecovery_ = false;
    delete hcclCommunicator;
}
#endif 
KfcCommand stub_kfcCmd = KfcCommand::kNone;
HcclResult GetStubCmd(hccl::HDCommunicate*This,unsigned int offset, unsigned int length, unsigned char* value){
    KfcCommand *cmdTmp = reinterpret_cast<KfcCommand*> (value);
    *cmdTmp = stub_kfcCmd;
    return HCCL_SUCCESS;
}

TEST_F(MC2AicpuRetry_ST, st_aicpuHdc)
{
    AicpuHdc aicpuHdc;

    MOCKER_CPP(&HDCommunicate::Get)
    .stubs()
    .will(invoke(GetStubCmd));

    std::shared_ptr<HDCommunicate> h2dTransfer;
    h2dTransfer.reset(new (std::nothrow) hccl::HDCommunicate(0, HCCL_HDC_TYPE_H2D, h2dBufferSize));
    h2dTransfer->InitHost();

    HcclResult ret = HCCL_SUCCESS;
    KfcCommand cmd = KfcCommand::kNone;;
    ret = aicpuHdc.GetOpExecCtrlCmd(nullptr, cmd);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    stub_kfcCmd = KfcCommand::kExit;
    ret = aicpuHdc.GetOpExecCtrlCmd(h2dTransfer, cmd);
    EXPECT_EQ(cmd, KfcCommand::kExit);

    stub_kfcCmd = KfcCommand::kRetry;
    ret = aicpuHdc.GetOpExecCtrlCmd(h2dTransfer, cmd);
    EXPECT_EQ(cmd, KfcCommand::kRetry);
}

HcclResult HcclOpExecFsmInitProcess_Stub(hccl::HcclCommAicpu*This, const std::string &newTag, OpParam &param, 
    AlgResourceResponse &algResource, HcclOpExecFSM &fsmState, KfcError &errorCode) {
    fsmState = HcclOpExecFSM::HCCL_OP_EXEC_FSM_RETRY;
    return HCCL_SUCCESS;
}

HcclResult GetKfcCommand_stub(hccl::HcclCommAicpu*This, KfcCommand &cmd)
{
    cmd = KfcCommand::kDestroyComm;
    return HCCL_SUCCESS;
}

HcclResult ResponseBackGroundStatus_stub(hccl::HcclCommAicpu*This, KfcExecStatus &status)
{
    status.execStatus.kfcStatus = KfcStatus::kDestroyComm;
    return HCCL_SUCCESS;
}

hccl::HcclCommAicpu g_commForTest;
HcclResult AicpuGetCommAll_stub(std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> &aicpuCommInfo)
{
    g_commForTest.InitCommInfoStatus(true);
    aicpuCommInfo.push_back(std::make_pair("group1", &g_commForTest));
    return HCCL_SUCCESS;
}

TEST_F(MC2AicpuRetry_ST, st_destroy_aicpu_comm)
{
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
    AicpuComContext *ctx = AicpuGetComContext();
    ctx->isStopLaunch = false;
    KfcCommand kCmd = KfcCommand::kDestroyComm;
    memset_s(&kCmd, sizeof(KfcCommand), 0, sizeof(KfcCommand));
    KfcExecStatus response;
    memset_s(&response, sizeof(KfcExecStatus), 0, sizeof(KfcExecStatus));
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));
    MOCKER_CPP(&HcclCommAicpu::GetKfcCommand)
        .stubs()
        .will(invoke(GetKfcCommand_stub));
    MOCKER_CPP(&HcclCommAicpu::ResponseBackGroundStatus)
        .stubs()
        .will(invoke(ResponseBackGroundStatus_stub));
    MOCKER_CPP(&HcclCommAicpu::FlushUtraceInfo)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    h2dTransfer->Put(0, sizeof(KfcCommand), (u8 *)&kCmd);
    dfx_tracer::ExecutorTracer::HandleDestroyComm(ctx);
    d2hTransfer->Get(0, sizeof(KfcExecStatus), (u8 *)&response);
    EXPECT_EQ(response.execStatus.kfcStatus, KfcStatus::kNull);
}

TEST_F(MC2AicpuRetry_ST, st_Handle_Resume_ChangeLink)
{
    KFCResInitTask initTask;
    init_kfc_args(initTask);
    EXPECT_EQ(0, RunAicpuKfcResInit(&initTask));
    AicpuComContext *ctx = AicpuGetComContext();
    MOCKER(AicpuHcclProcess::AicpuGetCommAll)
        .stubs()
        .will(invoke(AicpuGetCommAll_stub));
    KfcCommand kCmd = KfcCommand::NsChangeLink;
    ChangeLinkInfo changeLinkInfo;
    changeLinkInfo.remoteRankNum = 1;
    changeLinkInfo.remoteRankList[0] = 0;
    changeLinkInfo.isUseDefaultPort[0] = true;
    MOCKER_CPP(&AicpuHdc::GetOpExecChangeLink)
    .stubs()
    .with(any(), outBound(changeLinkInfo))
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::RefreshCommResponseTransportRes)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::BackGroundGetCmd).stubs().with(outBound(kCmd)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HcclCommAicpu::ResponseBackGroundStatus)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    dfx_tracer::ExecutorTracer::HandleResumeChangeLink(ctx);
}