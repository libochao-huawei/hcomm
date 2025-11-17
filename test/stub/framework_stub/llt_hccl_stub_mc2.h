/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef LLT_HCCL_STUB_MC2_H
#define LLT_HCCL_STUB_MC2_H

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include <cstring>
#include "common/aicpu_hccl_def.h"
#include "common/sqe_context.h"
#include "framework/aicpu_rpc_server.h"
#include "framework/aicpu_hccl_process.h"
#include "utils/mc2_aicpu_utils.h"
#undef private
#undef protected

extern DevType g_stubDevType;

inline void ResetMC2Context()
{
    AicpuComContext *ctx = AicpuGetComContext();
    ctx->kfcStatusTransferD2H = nullptr;
    ctx->kfcControlTransferH2D = nullptr;
    std::vector<struct hccl::TransportDeviceNormalData> temp;
    ctx->ibversData.swap(temp);
    memset(ctx, 0, sizeof(AicpuComContext));
    SqeContextUtils::InitSqeContext();
}

class StubHccCommRes {
public:
    StubHccCommRes()
    {
        workSpaceSize = 16 * 1024 * 1024;
        workSpace = (u64 *)malloc(workSpaceSize);
        winSize = 200 * 1024 * 1024;
        windows = (u64 *)malloc(winSize);
    }
    ~StubHccCommRes()
    {
        if (workSpace != nullptr) {
            free(workSpace);
        }
        if (windows != nullptr) {
            free(windows);
        }
    }
    HccCommResParamTask StubHccCommResParamTask()
    {
        HccCommResParamTask paramTask;
        std::memset(&paramTask, 0, sizeof(HccCommResParamTask));
        paramTask.mc2WorkSpace.workSpaceSize = workSpaceSize;
        paramTask.mc2WorkSpace.workSpace = (u64)workSpace;
        paramTask.rankId = 0;
        paramTask.rankNum = 8;
        paramTask.winSize = winSize;
        strcpy(paramTask.hcomId, "hcom\0");
        for (uint32_t i = 0; i < AC_MAX_RANK_NUM; i++) {
            paramTask.windowsIn[i] = (u64)windows * (i + 1);
            paramTask.windowsOut[i] = (u64)windows * (i + 1);
            paramTask.streamInfo[i].streamIds = 52 + i;
            paramTask.streamInfo[i].sqIds = 52 + i;
            paramTask.streamInfo[i].cqIds = 52 + i;
            paramTask.streamInfo[i].logicCqids = 52 + i;
        }
        uint64_t resId = 0;
        for (uint32_t i = 0; i < AC_MAX_RANK_NUM * 2; i++) {
            paramTask.signalInfo.noIpcNotifys[i].resId = ++resId;
            paramTask.signalInfo.noIpcNotifys[i].addr = 0x10 + i;
            paramTask.signalInfo.noIpcNotifys[i].devId = 0;
            paramTask.signalInfo.noIpcNotifys[i].tsId = 0;
            paramTask.signalInfo.noIpcNotifys[i].rankId = i % AC_MAX_RANK_NUM;
        }
        for (uint32_t i = 0; i < AC_MAX_RANK_NUM * 4; i++) {
            paramTask.signalInfo.ipcNotifys[i].resId = ++resId;
            paramTask.signalInfo.ipcNotifys[i].addr = 0x100 + i;
            paramTask.signalInfo.ipcNotifys[i].devId = 0;
            paramTask.signalInfo.ipcNotifys[i].tsId = 0;
            paramTask.signalInfo.ipcNotifys[i].rankId = i % AC_MAX_RANK_NUM;
        }
        for (uint32_t i = 0; i < AC_MAX_RANK_NUM; i++) {
            paramTask.signalInfo.noIpcEvents[i].resId = ++resId;
            paramTask.signalInfo.noIpcEvents[i].addr = 0x1000 + i;
            paramTask.signalInfo.noIpcEvents[i].devId = 0;
            paramTask.signalInfo.noIpcEvents[i].tsId = 0;
            paramTask.signalInfo.noIpcEvents[i].rankId = i;
        }
        paramTask.signalInfo.aicpuNotify.resId = ++resId;
        paramTask.signalInfo.aicpuNotify.addr = 0x10000;
        paramTask.signalInfo.aicpuNotify.devId = 0;
        paramTask.signalInfo.aicpuNotify.tsId = 0;
        paramTask.signalInfo.aicpuNotify.rankId = 0;
        paramTask.config.deterministic = 0;
        paramTask.config.notifyWaitTime = 1;
        paramTask.overFlowAddr = 0;
        return paramTask;
    }

private:
    u64 *workSpace;
    u64 workSpaceSize;
    u64 *windows;
    u64 winSize;
};

class StubSqeBuffer {
public:
    StubSqeBuffer()
    {
        buffer = new uint8_t[64 * 64 * 64];
        AicpuComContext *ctx = AicpuGetComContext();
        for (auto &info : ctx->streamInfo) {
            info.sqDepth = 4096;
            info.sqBaseAddr = buffer;
        }
    }
    ~StubSqeBuffer()
    {
        if (buffer != nullptr) {
            delete[] buffer;
            buffer = nullptr;
        }
    }

private:
    uint8_t *buffer = nullptr;
};


drvError_t StubhalGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
class MC2AicpuProcessStub: public AicpuHcclProcess {
public:
    HcclResult InitTimeOutConfig(HccCommResParamTask *commParam, AicpuComContext *ctx) override{
        (void) commParam;
        return HCCL_SUCCESS;
    }
};

inline uint32_t RunAicpuKfcResInitStub(void *args)
{
    if (args == nullptr) {
        return HCCL_E_PTR;
    }
    KFCResInitTask *ctxArgs = reinterpret_cast<KFCResInitTask *>(args);
    MC2AicpuProcessStub mc2AicpuProcessStub;
    return mc2AicpuProcessStub.AicpuRpcResInit(reinterpret_cast<HccCommResParamTask *>(ctxArgs->context));
}

inline uint32_t RunAicpuKfcResInitV2Stub(void *args)
{
    if (args == nullptr) {
        HCCL_ERROR("args is null.");
        return HCCL_E_PARA;
    }
    KFCResInitTask *ctxArgs = reinterpret_cast<KFCResInitTask *>(args);
    MC2AicpuProcessStub mc2AicpuProcessStub;
    return mc2AicpuProcessStub.AicpuRpcResInitV2(reinterpret_cast<HcclOpResParam *>(ctxArgs->context), false);
}

inline void MockGetSendRecvCnt()
{
    MOCKER(MC2AicpuUtils::GetSendCnt)
        .stubs()
        .will(returnValue(0));
    MOCKER(MC2AicpuUtils::GetRecvCnt)
        .stubs()
        .will(returnValue(0));
}
extern "C" {
__attribute__((default)) int32_t AdprofReportAdditionalInfo(uint32_t agingFlag, const void *data, uint32_t length);
__attribute__((default)) int32_t MsprofReportAdditionalInfo(uint32_t agingFlag, const VOID_PTR data, uint32_t length);
__attribute__((default)) int32_t AdprofCheckFeatureIsOn(uint64_t feature);
__attribute__((default)) uint64_t AdprofGetHashId(const char *hashInfo, size_t length);
__attribute__((default)) uint64_t MsprofStr2Id(const char *hashInfo, size_t length);
__attribute__((default)) uint32_t AicpuGetStreamId();
__attribute__((default)) uint64_t AicpuGetTaskId();
}
namespace aicpu {
extern __attribute__((default)) status_t GetTaskAndStreamId(uint64_t &taskId, uint32_t &streamId);
}

inline uint32_t GenXorStub(HcclMsg *msg) {
    if (msg == nullptr) {
        return UINT32_MAX;
    }
    DataBlock* block = reinterpret_cast<DataBlock*>(msg);
    uint32_t xorVal = 0;
    for (int i = 0; i < 15; i++) {
        xorVal ^= block->data[i];
    }
    return xorVal;
}
#endif // __MC2_AICPU_STUB_HPP__
