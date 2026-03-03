/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cpu_thread.h"
#include "orion_adapter_rts.h"

namespace hccl {
struct HostArgs {
    void* cpuThread;
    s32 deviceId;
};
HostArgs hostArgsTemp;

HcclResult CpuThread::PrepareDpuKernelResource(aclrtFuncHandle &funcHandle)
{
    // 获取二进制文件路径
    std::string jsonPath;
    std::string getPath = getenv("ASCEND_HOME_PATH");
    if (!getPath.empty()) {
        jsonPath = getPath;
    } else {
        jsonPath = "/usr/local/Ascend/cann/";
        HCCL_WARNING("[CommunicatorImpl::%s] ENV:ASCEND_HOME_PATH is not set", __func__);
    }

    jsonPath += "/opp/built-in/op_impl/dpu/";
    HCCL_DEBUG("[CommunicatorImpl::%s] kernel folder path[%s]", __func__, jsonPath.c_str());

    // cpuKernelMode为1时，json命名需与so命名保持一致， 即libccl_dpu.json与libccl_dpu.so
    jsonPath += "libccl_dpu.json";
    char realPath[PATH_MAX] = {0};
    CHK_PRT_RET(realpath(jsonPath.c_str(), realPath) == nullptr,
        HCCL_ERROR("[CommunicatorImpl::%s]: %s is not a valid real path, err[%d]", __func__, jsonPath.c_str(), errno),
        HCCL_E_INTERNAL);
    HCCL_INFO("[CommunicatorImpl::%s] realPath: %s", __func__, realPath);

    aclrtBinHandle         binHandle;
    aclrtBinaryLoadOptions options;
    aclrtBinaryLoadOption  option;
    option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
    option.value.cpuKernelMode = 1; // 0 ：仅需要加载json，1 ：加载cpu so & json，2: LoadFromData
    options.numOpt  = 1;
    options.options = &option;
    if (aclrtBinaryLoadFromFile(realPath, &options, &binHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] load binary from file error.", __func__);
        return HCCL_E_OPEN_FILE_FAILURE;
    }

    // 创建dpustream
    if (aclrtCreateStreamWithConfig(&dpuStream_, 0, ACL_STREAM_FAST_LAUNCH) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Create Local Stream Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    // 查找核函数
    if (aclrtBinaryGetFunction(binHandle, "RunDpuRpcSrvLaunch", &funcHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Get Function Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;
}

HcclResult CpuThread::LaunchDpuKernel(aclrtFuncHandle &funcHandle)
{
    // 下发
    HCCL_INFO("[CommunicatorImpl::%s] Launch Dpu Kernel", __func__);
    aclrtLaunchKernelCfg  cfg;
    aclrtLaunchKernelAttr kernelAttr;
    kernelAttr.id            = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    kernelAttr.value.timeout = NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs             = 1;
    cfg.attrs                = &kernelAttr;
    constexpr u32 numBlocks   = 1;
    // 核函数入参
    hostArgsTemp.cpuThread = static_cast<void*>(this);
    size_t               argsSize = sizeof(hostArgsTemp);
    aclrtPlaceHolderInfo placeHolderArrays;
    size_t               placeHolderNum = 0;
    if (aclrtLaunchKernelWithHostArgs(funcHandle, numBlocks, dpuStream_, &cfg, &hostArgsTemp, argsSize,
                                      &placeHolderArrays, placeHolderNum)
        != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Launch Dpu Kernel Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult CpuThread::Init()
{
    // 申请notify
    uint64_t notifySize = 10;
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        auto notify = std::make_unique<MemNotify>();
        notify->Alloc(notifySize);
        notifys_.push_back(std::move(notify));
    }
    
    // 初始化scheduler
    serviceScheduler_ = std::make_unique<ServiceScheduler>();
    CHK_RET(serviceScheduler_->Init());
    // 注册recordService和waitService
    serviceScheduler_->ServiceRegister(RecordService, &recordServiceHandle_);
    serviceScheduler_->ServiceRegister(WaitService, &waitServiceHandle_);

    s32 devId = 0;
    CHK_RET(hrtGetDevice(&devId));
    hostArgsTemp.deviceId = devId;

    //--------------------------------------------------------------------------------
    // 设置XPU
    HCCL_INFO("[CommunicatorImpl::%s] Switch to Dpu Ctx", __func__);
    if (aclrtGetCurrentContext(&npuContext_) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Get Npu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    if (Hccl::rtSetXpuDevice(Hccl::RT_DEV_TYPE_DPU, 0) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Switch to Dpu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    if (aclrtGetCurrentContext(&dpuContext_) != ACL_SUCCESS) {
        HCCL_ERROR("[CommunicatorImpl::%s] Get Dpu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    // 准备资源
    aclrtFuncHandle funcHandle;
    CHK_RET(PrepareDpuKernelResource(funcHandle));

    // 下发
    CHK_RET(LaunchDpuKernel(funcHandle));

    // 切换回当前Ctx
    HCCL_INFO("[CommunicatorImpl::%s] Switch to Npu Ctx", __func__);
    if (ACL_SUCCESS != aclrtSetCurrentContext(npuContext_)) {
        HCCL_ERROR("[CommunicatorImpl::%s] Reset Current Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    HCCL_INFO("[CommunicatorImpl::%s] Launch Dpu Kernel End", __func__);
    return HCCL_SUCCESS;
};

HcclResult CpuThread::DeInit()
{
    return HCCL_SUCCESS;
};

CpuThread::~CpuThread() {
};

HcclResult CpuThread::ServiceRegister(ThreadService serviceCb, ThreadServiceHandle* serviceHandle)
{
    return serviceScheduler_->ServiceRegister(serviceCb, serviceHandle);
};
HcclResult CpuThread::ServiceUnregister(ThreadServiceHandle serviceHandle)
{
    return serviceScheduler_->ServiceUnregister(serviceHandle);
};
HcclResult CpuThread::KernelRun()
{
    //
    return serviceScheduler_->ServiceRun();
}

HcclResult CpuThread::GetThreadEntity(ThreadEntity* threadEntity)
{
    threadEntity->type = THREAD_TYPE_CPU;
    threadEntity->engine = COMM_ENGINE_AICPU;
    threadEntity->cpuRes.sendQueue = serviceScheduler_->GetSendQueue()->GetQueueInfo();
    threadEntity->cpuRes.recordService = recordServiceHandle_;
    threadEntity->cpuRes.waitService = waitServiceHandle_;
    threadEntity->notifyNum = notifyNum_;
    for (size_t i = 0; i < notifys_.size(); i++)
    {
        threadEntity->notifies[i].type = NOTIFY_TYPE_DEVICE_MEM;
        threadEntity->notifies[i].identifier = notifys_[i]->GetIdentifier();
    }
    return HCCL_SUCCESS; 
}

MemNotify *CpuThread::GetNotify(uint32_t notifyIndex)
{
    if (notifyIndex >= notifys_.size()) {
        return nullptr;
    }
    return notifys_[notifyIndex].get();
}
uint32_t CpuThread::GetNotifyNum() const
{
    return notifyNum_;
}
}