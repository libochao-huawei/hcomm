/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <chrono>
#include <climits>
#include <memory>
#include "config_log.h"
#include "launch_device.h"
#include "op_task_config.h"
#include "hccl_common_v2.h"
#include "orion_adapter_rts.h"
#include "hccl_dpu_manager.h"

namespace hccl {
constexpr uint8_t DEVICE_SIGNAL_SECOND = 2;
constexpr uint8_t DEVICE_SIGNAL_THIRD = 3;
constexpr uint32_t TEMP_DEV_TYPE_DPU = 0; // 临时适配，后续rts接口上库之后使用rts的定义

DpuManager::DpuManager()
{
}

DpuManager::~DpuManager()
{
    DeInitDpuKernel();
}

HcclResult DpuManager::CreateWorkspaceBuf(const char *memTag, uint64_t *size, bool *newCreated)
{
    std::string tag = memTag != nullptr ? std::string(memTag) : "";
    if (tagWorkspaceMap_.find(tag) == tagWorkspaceMap_.end()) {
        std::shared_ptr<Hccl::DevBuffer> workspace = std::make_shared<Hccl::DevBuffer>(*size);
        tagWorkspaceMap_.insert(make_pair(tag, workspace));
        HCCL_INFO("[DpuManager::%s] Create tagMem[%s] WorkspaceBuf success, WorkspaceBuf = %p",
            __func__, tag.c_str(), workspace.get());
        if (newCreated != nullptr) {
            *newCreated = true;
        }
    }
    return HCCL_SUCCESS;
}

std::shared_ptr<Hccl::DevBuffer> DpuManager::GetKFCWorkSpace(const char *memTag)
{
    std::string tag = memTag != nullptr ? std::string(memTag) : "";
    auto it = tagWorkspaceMap_.find(tag);
    return it != tagWorkspaceMap_.end() ? it->second : nullptr;
}

HcclResult DpuManager::GetDevMemWorkSpace(const std::string &memTag, uint64_t *size, void **addr, bool *newCreated)
{
    auto iter = tagWorkspaceMap_.find(memTag);
    if (iter != tagWorkspaceMap_.end()) {
        std::shared_ptr<Hccl::DevBuffer> oldWorkspace = iter->second;
        if (*size != static_cast<uint64_t>(oldWorkspace.get()->GetSize())) {
            HCCL_ERROR("HcclCommunicator::GetDevMemWorkSpace, The size of oldWorkspace %p is non-consistent, "
                "target size compare now size: %llu->%llu", *addr, *size, oldWorkspace.get()->GetSize());
            return HCCL_E_PARA;
        }
        *addr = reinterpret_cast<void *>(oldWorkspace.get()->GetAddr());
        if (newCreated != nullptr) {
            *newCreated = false;
        }
        return HcclResult::HCCL_SUCCESS;
    }

    std::shared_ptr<Hccl::DevBuffer> newWorkspace = std::make_shared<Hccl::DevBuffer>(*size);
    tagWorkspaceMap_.insert(make_pair(memTag, newWorkspace));
    HCCL_INFO("Create tagMem[%s] WorkspaceBuf success, WorkspaceBuf: %p -> %p, size[%llu]",
            memTag.c_str(), newWorkspace.get(), newWorkspace.get()->GetAddr(), *size);
    if (newCreated != nullptr) {
        *newCreated = true;
    }
    *addr = reinterpret_cast<void *>(newWorkspace.get()->GetAddr());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult DpuManager::Init(const std::string& commId, u32 deviceLogicId)
{
    commId_ = commId;
    devLogicId_ = deviceLogicId;
    return InitDpuKernel();
}

HcclResult DpuManager::LaunchDpuKernel(aclrtFuncHandle& funcHandle)
{
    HCCL_INFO("[DpuManager::%s] Launch Dpu Kernel", __func__);
    aclrtLaunchKernelCfg cfg;
    aclrtLaunchKernelAttr kernelAttr;
    kernelAttr.id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
    kernelAttr.value.timeout = Hccl::NOTIFY_DEFAULT_WAIT_TIME > std::numeric_limits<uint16_t>::max() ?
                               std::numeric_limits<uint16_t>::max() : Hccl::NOTIFY_DEFAULT_WAIT_TIME;
    cfg.numAttrs = 1;
    cfg.attrs = &kernelAttr;
    constexpr u32 numBlocks = 1;

    struct DpuKernelLaunchParam {
        std::string commId;
        u32 memorySize;
        void* shareHBM;
        void* hostMem;
        u32 deviceId;
    } hostArgsTemp;

    hostArgsTemp.commId = commId_;
    hostArgsTemp.memorySize = SHARE_HBM_MEMORY_SIZE;
    hostArgsTemp.hostMem = hostShareBuf_;
    auto shMem = GetKFCWorkSpace(DPUTAG);
    hostArgsTemp.shareHBM = reinterpret_cast<void*>(shMem->GetAddr());
    hostArgsTemp.deviceId = devLogicId_;

    HCCL_INFO("[DpuManager::%s] DpuKernelLaunchParam{commId:%s; memorySize:%u; shareHBM:%p; hostMem:%p}",
        __func__, hostArgsTemp.commId.c_str(), hostArgsTemp.memorySize, hostArgsTemp.shareHBM,
        hostArgsTemp.hostMem);

    size_t argsSize = sizeof(hostArgsTemp);
    aclrtPlaceHolderInfo placeHolderArrays;
    size_t placeHolderNum = 0;
    if (aclrtLaunchKernelWithHostArgs(funcHandle, numBlocks, dpuStream_, &cfg, &hostArgsTemp, argsSize,
                                      &placeHolderArrays, placeHolderNum) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Launch Dpu Kernel Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

HcclResult DpuManager::PrepareDpuKernelResource(aclrtFuncHandle& funcHandle)
{
    // 获取二进制文件路径
    std::string jsonPath;
    const char* envPath = getenv("ASCEND_HOME_PATH");
    if (envPath != nullptr && envPath[0] != '\0') {
        jsonPath = envPath;
    } else {
        jsonPath = "/usr/local/Ascend/cann/";
        HCCL_WARNING("[DpuManager::%s] ENV:ASCEND_HOME_PATH is not set", __func__);
    }

    jsonPath += "/opp/built-in/op_impl/dpu/";
    HCCL_DEBUG("[DpuManager::%s] kernel folder path[%s]", __func__, jsonPath.c_str());

    // cpuKernelMode为1时，json命名需与so命名保持一致， 即libccl_dpu.json与libccl_dpu.so
    jsonPath += "libccl_dpu.json";
    char realPath[PATH_MAX] = {0};
    CHK_PRT_RET(realpath(jsonPath.c_str(), realPath) == nullptr,
        HCCL_ERROR("[DpuManager::%s]: %s is not a valid real path, err[%d]", __func__, jsonPath.c_str(), errno),
        HCCL_E_INTERNAL);
    HCCL_INFO("[DpuManager::%s] realPath: %s", __func__, realPath);

    aclrtBinHandle binHandle;
    aclrtBinaryLoadOptions options;
    aclrtBinaryLoadOption option;
    option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
    option.value.cpuKernelMode = 1;
    options.numOpt = 1;
    options.options = &option;
    if (aclrtBinaryLoadFromFile(realPath, &options, &binHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] load binary from file error.", __func__);
        return HCCL_E_OPEN_FILE_FAILURE;
    }

    // 创建dpustream
    if (aclrtCreateStreamWithConfig(&dpuStream_, 0, ACL_STREAM_FAST_LAUNCH) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Create Local Stream Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    // 查找核函数
    if (aclrtBinaryGetFunction(binHandle, "RunDpuRpcSrvLaunch", &funcHandle) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Get Function Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;
}

HcclResult DpuManager::InitAndLaunchDpuKernel()
{
    HCCL_INFO("[DpuManager::%s] Start to Launch Dpu Kernel", __func__);
    // 申请共享内存(需要在npu ctx 下进行)
    bool newCreate = false;
    uint64_t memSize = static_cast<uint64_t>(SHARE_HBM_MEMORY_SIZE);
    HcclResult memRet = CreateWorkspaceBuf(DPUTAG, &memSize, &newCreate);
    if (memRet != HCCL_SUCCESS) {
        HCCL_ERROR("[DpuManager::InitCommResource] Alloc Share HBM Failed");
        return HCCL_E_RUNTIME;
    }
    hostShareBuf_ = malloc(SHARE_HBM_MEMORY_SIZE);

    // 设置XPU
    HCCL_INFO("[DpuManager::%s] Switch to Dpu Ctx", __func__);
    if (aclrtGetCurrentContext(&npuContext_) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Get Npu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    if (Hccl::HrtSetXpuDevice(TEMP_DEV_TYPE_DPU, 0) != HCCL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Switch to Dpu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }
    if (aclrtGetCurrentContext(&dpuContext_) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Get Dpu Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    // 准备资源
    aclrtFuncHandle funcHandle;
    CHK_RET(PrepareDpuKernelResource(funcHandle));

    // 下发
    CHK_RET(LaunchDpuKernel(funcHandle));

    // 切换回当前Ctx
    HCCL_INFO("[DpuManager::%s] Switch to Npu Ctx", __func__);
    if (ACL_SUCCESS != aclrtSetCurrentContext(npuContext_)) {
        HCCL_ERROR("[DpuManager::%s] Reset Current Ctx Failed", __func__);
        return HCCL_E_INTERNAL;
    }

    HCCL_INFO("[DpuManager::%s] Launch Dpu Kernel End", __func__);
    isDpuKernelLaunched_ = true;
    return HCCL_SUCCESS;
}

HcclResult DpuManager::InitDpuKernel()
{
    /* kernel Launch */
    CHK_RET(InitAndLaunchDpuKernel());
    return HCCL_SUCCESS;
}

HcclResult DpuManager::WaitDpuKernelThreadTerminate()
{
    auto shMem = GetKFCWorkSpace(DPUTAG);
    if (shMem == nullptr) {
        HCCL_ERROR("[DpuManager::%s] GetKFCWorkSpace failed, shMem is null", __func__);
        return HCCL_E_MEMORY;
    }
    uint8_t* dstPtr = reinterpret_cast<uint8_t*>(shMem->GetAddr());
    uint8_t  flag   = DEVICE_SIGNAL_SECOND;
    auto     ret = aclrtMemcpy(dstPtr, sizeof(flag), &flag, sizeof(flag), aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Terminate TaskRun Fail", __func__);
        return HCCL_E_RUNTIME;
    }
    HcclUs startTime = std::chrono::steady_clock::now();
    auto   timeout   = std::chrono::milliseconds(waitTransportReadyTimeoutMs_);
    do {
        if (std::chrono::steady_clock::now() - startTime >= timeout) {
            HCCL_ERROR("[DpuManager::%s] Wait Terminate TaskRun TimeOut", __func__);
            return HCCL_E_TIMEOUT;
        }
        if (aclrtMemcpy(&flag, sizeof(flag), dstPtr, sizeof(flag), aclrtMemcpyKind::ACL_MEMCPY_DEVICE_TO_HOST)
            != ACL_SUCCESS) {
            HCCL_ERROR("[DpuManager::%s] Read Terminate TaskRun Signal Fail", __func__);
            return HCCL_E_RUNTIME;
        }
    } while (flag != DEVICE_SIGNAL_THIRD);
    return HCCL_SUCCESS;
}

HcclResult DpuManager::DeInitDpuKernel()
{
    if (hostShareBuf_ != nullptr) {
        free(hostShareBuf_);
        hostShareBuf_ = nullptr;
    }

    if (!isDpuKernelLaunched_) {
        return HCCL_SUCCESS;
    }

    CHK_RET(WaitDpuKernelThreadTerminate());

    // 切换回 dpu ctx
    if (ACL_SUCCESS != aclrtSetCurrentContext(dpuContext_)) {
        HCCL_ERROR("[DpuManager::%s] set dpu Ctx Failed", __func__);
        return HCCL_E_RUNTIME;
    }
    // 销毁局部流
    HCCL_INFO("[DpuManager::%s] Destroy Stream", __func__);
    if (aclrtDestroyStreamForce(dpuStream_) != ACL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] Destroy Stream Failed", __func__);
        return HCCL_E_RUNTIME;
    }
    // reset DPU kernel 线程
    HCCL_INFO("[DpuManager::%s] Start to reset DPU device", __func__);
    if (Hccl::HrtResetXpuDevice(TEMP_DEV_TYPE_DPU, 0) != HCCL_SUCCESS) {
        HCCL_ERROR("[DpuManager::%s] ResetXpuDevice Failed", __func__);
        return HCCL_E_RUNTIME;
    }
    // 切回 npu ctx
    if (ACL_SUCCESS != aclrtSetCurrentContext(npuContext_)) {
        HCCL_ERROR("[DpuManager::%s] set npu Ctx Failed", __func__);
        return HCCL_E_RUNTIME;
    }

    isDpuKernelLaunched_ = false;
    return HCCL_SUCCESS;
}

}  // namespace hccl
