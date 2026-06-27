/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <atomic>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <set>
#include <unistd.h>
#include <vector>

#include "acl/acl_base.h"
#include "acl/acl_rt.h"
#include "sim_log.h"
#include "store_sim_memory_manager.h"
#include "db_sim_op_db_ops.h"
#include "db_sim_runner_ops.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct ArgsBuffer {
    void *data;
    uint64_t size;
};

pid_t g_devicePid = 0;

static bool CheckDeviceProcStatus()
{
    if (g_devicePid == 0) {
        return false;
    }
    int status = 0;
    pid_t result = waitpid(g_devicePid, &status, WNOHANG);
    if (result == 0) {
        return true;
    }
    if (result == g_devicePid) {
        if (WIFSIGNALED(status)) {
            HCCL_VM_ERROR("device process[{}] killed by signal {}", g_devicePid, WTERMSIG(status));
        } else {
            HCCL_VM_ERROR("device process[{}] exited with status {}", g_devicePid, WEXITSTATUS(status));
        }
    } else {
        HCCL_VM_ERROR("waitpid failed for pid {}, errno: {} ({})", g_devicePid, errno, strerror(errno));
    }
    FlushLog();
    exit(EXIT_FAILURE);
    g_devicePid = 0;
    return false;
}

namespace sim {
constexpr size_t MAX_ARGS_BUFF_SIZE = 64 * 1024U;

struct FuncArgsDetail {
    uint8_t *argsData{nullptr};
    size_t argsDataSize{0};
    bool  isHold{false};
};

struct FuncArgs
{
    uint8_t *argsBuff{nullptr};
    size_t argsBufferSize{0};
    bool isSysMem{true};
    size_t useOffset{0};
    std::vector<FuncArgsDetail*> argDetail;

    FuncArgs()
    {
        argsBuff = new uint8_t[MAX_ARGS_BUFF_SIZE];
        argsBufferSize = MAX_ARGS_BUFF_SIZE;
        isSysMem = true;
    }
    ~FuncArgs()
    {
        if (isSysMem && argsBuff) {
            delete[] argsBuff;
        }

        for (auto& arg : argDetail) {
            delete arg;
        }
    }

    void ResetArgsBuff()
    {
        if (isSysMem && argsBuff) {
            delete[] argsBuff;
            argsBuff = nullptr;
            argsBufferSize = 0;
        }
    }
};

struct FuncHandle
{
    std::string funcName{""};
    std::string kernelName{""};
    std::vector<FuncArgs*> funArgs;
    ~FuncHandle() {
        for (auto& funcArg : funArgs) {
            delete funcArg;
        }
    }
};

struct DevBinary;
struct Program
{
    DevBinary* bin{nullptr};
    std::map<std::string, FuncHandle*> funcs;
    ~Program()
    {
        for (auto& func : funcs) {
            delete func.second;
        }
    }
};

struct DevBinary
{
    std::string binPath{""};
    void* data{nullptr};
    size_t dataLen{0};
    Program prog;

    ~DevBinary()
    {
        if (data != nullptr) {
            delete [] (char *)data;
        }
    }
};

std::set<DevBinary*> g_kernelBinary;
}

aclrtBinary aclrtCreateBinary(const void *data, size_t dataLen)
{
    sim::DevBinary* binPtr = new sim::DevBinary();
 
    auto res = sim::g_kernelBinary.insert(binPtr);
    if (!res.second) {
        HCCL_VM_ERROR("failed");
        return 0;
    }

    binPtr->data = reinterpret_cast<void *>(new char[dataLen]);
    memcpy(binPtr->data, data, dataLen);
    binPtr->dataLen = dataLen;
    HCCL_VM_INFO("[KERNEL] stub dataLen{:d} binary{:p}", dataLen, (aclrtBinary)(binPtr));
    return (aclrtBinary)(binPtr);
}

aclError aclrtDestroyBinary(aclrtBinary binary)
{
    sim::DevBinary* binPtr = (sim::DevBinary*)binary;
    if (auto search = sim::g_kernelBinary.find(binPtr); search == sim::g_kernelBinary.end()){
        HCCL_VM_ERROR("[aclrtDestroyBinary] can not find this binary");
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }
    sim::g_kernelBinary.erase(binPtr);

    HCCL_VM_INFO("[KERNEL] stub  binPtr{:p}", binary);
    delete binPtr;
    return ACL_SUCCESS;
}

aclError aclrtBinaryLoad(const aclrtBinary binary, aclrtBinHandle *binHandle)
{
    sim::DevBinary* binPtr = (sim::DevBinary*)binary;
    // 需要解析binary
    *binHandle = (aclrtBinHandle)&(binPtr->prog);
    HCCL_VM_INFO("[KERNEL] stub  binHandle:{:p}", *binHandle);
    return ACL_SUCCESS;
}

aclError aclrtBinaryUnLoad(aclrtBinHandle binHandle)
{
    (void) binHandle;
    HCCL_VM_INFO("[KERNEL] stub not support.");
    return ACL_SUCCESS;
}

aclError aclrtBinaryLoadFromFile(const char* binPath, aclrtBinaryLoadOptions *options, aclrtBinHandle *binHandle)
{
    (void) options;
    sim::DevBinary* binPtr = new sim::DevBinary();
    binPtr->binPath = binPath;
    binPtr->prog.bin = binPtr;

    auto res = sim::g_kernelBinary.insert(binPtr);
    if (!res.second) {
        HCCL_VM_ERROR("[aclrtBinaryLoadFromFile] file:{} insert failed", binPath);
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }

    *binHandle = (aclrtBinHandle)&(binPtr->prog);
    HCCL_VM_INFO("[KERNEL] stub  binPath:{} binHandle{:p}", binPath, *binHandle);
    return ACL_SUCCESS;
}

aclError aclrtBinaryLoadFromData(const void *data, size_t length, const aclrtBinaryLoadOptions *options, aclrtBinHandle *binHandle)
{
    (void) data;
    (void) length;
    (void) options;
    (void) binHandle;
    HCCL_VM_INFO("not support.");
    return ACL_SUCCESS;
}

aclError aclrtBinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName, aclrtFuncHandle *funcHandle)
{
    sim::Program* prog = (sim::Program*)(uintptr_t)binHandle;

    std::string funcName(kernelName);
    auto funcIter = prog->funcs.find(funcName);
    if (funcIter == prog->funcs.end()) {
        HCCL_VM_WARN("kernelName:{} not register insert it", kernelName);

        sim::FuncHandle* func = new sim::FuncHandle;
        func->funcName = funcName;
        func->kernelName = kernelName;
        auto res = prog->funcs.insert(std::pair<std::string, sim::FuncHandle*>(func->funcName, func));
        if (!res.second) {
            HCCL_VM_ERROR("[aclrtRegisterCpuFunc] func:{} kernelName:{} insert failed", funcName, kernelName);
            return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
        }
        *funcHandle = reinterpret_cast<aclrtFuncHandle>(func);
    } else {
        *funcHandle = reinterpret_cast<aclrtFuncHandle>(funcIter->second);
    }

    HCCL_VM_INFO("[KERNEL] stub  funcHandle:{:p}", *funcHandle);
    return ACL_SUCCESS;
}

aclError aclrtBinaryGetFunctionByEntry(aclrtBinHandle binHandle, uint64_t funcEntry, aclrtFuncHandle *funcHandle)
{
    (void) binHandle;
    (void) funcEntry;
    (void) funcHandle;
    HCCL_VM_INFO("[KERNEL] stub not support.");
    return ACL_SUCCESS;
}

aclError aclrtGetFunctionAddr(aclrtFuncHandle funcHandle, void **aicAddr, void **aivAddr)
{
    (void) funcHandle;
    (void) aicAddr;
    (void) aivAddr;
    HCCL_VM_INFO("stub not support.");
    return ACL_SUCCESS;
}

aclError aclrtGetFunctionName(aclrtFuncHandle funcHandle, uint32_t maxLen, char *name)
{
    (void) maxLen;
    sim::FuncHandle* funcHandlePtr = (sim::FuncHandle *)(uintptr_t)funcHandle;

    memcpy(name, funcHandlePtr->funcName.data(), funcHandlePtr->funcName.length());
    HCCL_VM_INFO("[KERNEL] stub  funcName{}", funcHandlePtr->funcName.data());
    return ACL_SUCCESS;
}

aclError aclrtRegisterCpuFunc(const aclrtBinHandle handle, const char *funcName, const char *kernelName, aclrtFuncHandle *funcHandle)
{
    sim::Program* prog = (sim::Program*)(uintptr_t)handle;

    sim::FuncHandle* func = new sim::FuncHandle;
    func->funcName = funcName;
    func->kernelName = kernelName;
    auto res = prog->funcs.insert(std::pair<std::string, sim::FuncHandle*>(func->funcName, func));
    if (!res.second) {
        HCCL_VM_ERROR("func:{} kernelName:{} insert failed", funcName, kernelName);
        return ACL_ERROR_RT_FEATURE_NOT_SUPPORT;
    }
    *funcHandle = reinterpret_cast<aclrtFuncHandle>(func);
    HCCL_VM_INFO("[KERNEL] stub  funcHandle:{:p}", *funcHandle);
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsInit(aclrtFuncHandle funcHandle, aclrtArgsHandle *argsHandle)
{
    sim::FuncHandle* func = (sim::FuncHandle*)(uintptr_t)funcHandle;
    sim::FuncArgs* args = new sim::FuncArgs;
    func->funArgs.push_back(args);

    auto iter = func->funArgs.rbegin();
    *argsHandle = (aclrtArgsHandle)args;
    HCCL_VM_INFO("[KERNEL] FuncHandle:{:p} ArgsHandle:{:p} ", funcHandle, *argsHandle);
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsInitByUserMem(aclrtFuncHandle funcHandle, aclrtArgsHandle argsHandle, void *userHostMem, size_t actualArgsSize)
{
    (void) funcHandle;
    HCCL_VM_INFO("[KERNEL] stub  argsHandle:{:p} userHostMem:{:p},actualArgsSize:{:d}", argsHandle, userHostMem, actualArgsSize);
    sim::FuncArgs* args = (sim::FuncArgs*)argsHandle;
    args->ResetArgsBuff();
    args->argsBuff = (uint8_t*)userHostMem;
    args->argsBufferSize = actualArgsSize;
    args->isSysMem = false;
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsGetMemSize(aclrtFuncHandle funcHandle, size_t userArgsSize, size_t *actualArgsSize)
{
    (void) funcHandle;
    HCCL_VM_INFO("[KERNEL] userArgsSize {:d}.", userArgsSize);
    // 
    *actualArgsSize = userArgsSize;
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsGetHandleMemSize(aclrtFuncHandle funcHandle, size_t *memSize)
{
    HCCL_VM_INFO("[KERNEL] funcHandle:{:p} userArgsSize 64k", funcHandle);
    // 句柄 + 参数的内存大小
    *memSize = sim::MAX_ARGS_BUFF_SIZE;
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsAppend(aclrtArgsHandle argsHandle, void *param, size_t paramSize,
    aclrtParamHandle *paramHandle)
{
    sim::FuncArgs* args = (sim::FuncArgs*)(uintptr_t)argsHandle;
    HCCL_VM_INFO("[KERNEL] stub  argsHandle:{:p} paramSize:{:d}", argsHandle, paramSize);
    sim::FuncArgsDetail* argsDetail = new sim::FuncArgsDetail;
    argsDetail->argsData = args->argsBuff + args->useOffset;
    argsDetail->argsDataSize = paramSize;

    memcpy(argsDetail->argsData, param, paramSize);
    args->useOffset += paramSize;
    args->argDetail.push_back(argsDetail);
    *paramHandle = (aclrtParamHandle)argsDetail;
    HCCL_VM_INFO("[KERNEL] argsHandle:{:p} paramHandle:{:p} ", argsHandle, *paramHandle);
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsAppendPlaceHolder(aclrtArgsHandle argsHandle, aclrtParamHandle *paramHandle)
{
    sim::FuncArgs* args = (sim::FuncArgs*)(uintptr_t)argsHandle;

    sim::FuncArgsDetail* argsDetail = new sim::FuncArgsDetail;
    argsDetail->isHold = true;

    args->argDetail.push_back(argsDetail);
    *paramHandle = (aclrtParamHandle)argsDetail;
    HCCL_VM_INFO("[KERNEL] paramHandle:{:p} ", *paramHandle);
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsGetPlaceHolderBuffer(aclrtArgsHandle argsHandle, aclrtParamHandle paramHandle, size_t dataSize, void **bufferAddr)
{
    HCCL_VM_INFO("[KERNEL] argsHandle:{:p} ParamHandle:{:p} dataSize:{:d}", argsHandle, paramHandle, dataSize);
    sim::FuncArgs* args = (sim::FuncArgs*)(uintptr_t)argsHandle;

    sim::FuncArgsDetail* detail = (sim::FuncArgsDetail*)(uintptr_t)paramHandle;
    detail->argsData = args->argsBuff + args->useOffset;
    detail->argsDataSize = dataSize;
    *bufferAddr = reinterpret_cast<void *>(detail->argsData);
    HCCL_VM_INFO("[KERNEL] paramHandle:{:p} ", *bufferAddr);
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsParaUpdate(aclrtArgsHandle argsHandle, aclrtParamHandle paramHandle, void *param, size_t paramSize)
{
    HCCL_VM_INFO("[KERNEL] argsHandle:{:p} ParamHandle:{:p} paramSize:{:d}", argsHandle, paramHandle, paramSize);
    sim::FuncArgs* args = (sim::FuncArgs*)(uintptr_t)argsHandle;

    sim::FuncArgsDetail* detail = (sim::FuncArgsDetail*)(uintptr_t)paramHandle;
    if (detail->isHold || detail->argsDataSize != paramSize) {
        HCCL_VM_ERROR("invalid param handle type:{:d}", detail->isHold);
        return ACL_ERROR_INTERNAL_ERROR;
    }

    memcpy(detail->argsData, param, paramSize);
    HCCL_VM_INFO("[KERNEL] argsData:{:p} ", reinterpret_cast<void *>(detail->argsData));
    return ACL_SUCCESS;
}

aclError aclrtKernelArgsFinalize(aclrtArgsHandle argsHandle)
{
    (void) argsHandle;
    HCCL_VM_INFO("[KERNEL]stub not support.");
    return ACL_SUCCESS;
}

aclError aclrtLaunchKernel(aclrtFuncHandle funcHandle, uint32_t blockDim, const void *argsData, size_t argsSize, aclrtStream stream)
{
    (void) funcHandle;
    (void) blockDim;
    (void) argsData;
    (void) argsSize;
    (void) stream;
    HCCL_VM_INFO("[KERNEL] stub not support.");
    
    return ACL_SUCCESS;
}

void ForkAndStartAicpuProcess(int32_t rankId, uint8_t* devState)
{
    pid_t pid = fork();
    if (pid == -1) {
        HCCL_VM_ERROR("[ForkAndStartAicpuProcess] fork aicpu process failed.");
        exit(EXIT_FAILURE);
    }

    const char* installDir = getenv("HCCL_VM_INSTALL_DIR");
    std::string args = std::to_string(rankId);
    std::string devicePath = installDir ? std::string(installDir) + "/bin/device" : "./bin/device";
    std::string libPath = installDir ? std::string(installDir) + "/lib/aarch64" : "./lib/aarch64";
    std::string preloadPath = libPath + "/libhccl-device-proxy.so";
    if (pid == 0) {
        g_logger = nullptr;
        setenv("QEMU_LD_PREFIX", "/usr/aarch64-linux-gnu", 1);
        setenv("LD_PRELOAD", preloadPath.c_str(), 1);
        setenv("LD_LIBRARY_PATH", libPath.c_str(), 1);
        execlp("qemu-aarch64-static", "qemu-aarch64-static", devicePath.c_str(), args.c_str(), nullptr);
        HCCL_VM_ERROR("[ForkAndStartAicpuProcess] execlp aicpu process failed.");
        exit(EXIT_FAILURE);
    } else {
        g_devicePid = pid;  // 记录device进程id用于host结束时杀掉device进程
        while (*devState == DEVICE_RUN) {
            CheckDeviceProcStatus();
            sleep(1);
        }
    }
}

void LaunchAICPUKernelFunc(std::string kernelName, aclrtArgsHandle argsHandle)
{
    uint32_t rankId = (uint32_t)sim::GetCurrRankId();
    HCCL_VM_INFO("[LaunchAICPUKernelFunc] rankId:{}, kernelName:{}.", rankId, kernelName);
    sim::FuncArgs* args = (sim::FuncArgs*)argsHandle;
    uint64_t size = args->useOffset;
    void *ptr = nullptr;
    aclrtMalloc(&ptr, size, aclrtMemMallocPolicy::ACL_MEM_MALLOC_NORMAL_ONLY);
    if (ptr == nullptr) {
        HCCL_VM_ERROR("[LaunchAICPUKernelFunc] malloc device memory failed, size:{}", size);
        return;
    }

    aclrtMemcpy(ptr, size, args->argsBuff, size, aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE);
    void *shmptr = sim::MemoryManager::GetInstance().AcquireMemByName("HcclAicpuData");
    if (shmptr == nullptr) {
        HCCL_VM_ERROR("[LaunchAICPUKernelFunc] acquire shm failed.");
        return;
    }

    HcclAicpuData *aicpuData = static_cast<HcclAicpuData *>(shmptr);
    if (kernelName == "RunAicpuIndOpCommInit") {
        // 第一次调用KernelLaunch需要启动aicpu进程
        aicpuData->task[rankId].devState = DEVICE_RUN;
        ForkAndStartAicpuProcess(rankId, &aicpuData->task[rankId].devState);

        // CCU退化为AICPU模式时，更新模型中展开模式为AICPU
        const char *expanEnv = std::getenv("HCCL_OP_EXPANSION_MODE");
        bool ccuEnabled = expanEnv && (std::string(expanEnv) == "CCU_SCHED" || std::string(expanEnv) == "CCU_MS");
        if (ccuEnabled) {
            HCCL_VM_INFO("[LaunchAICPUKernelFunc] Switch the expansion mode[CCU -> AICPU].");
            constexpr uint8_t kAicpuMode = static_cast<uint8_t>(sim::SimOpExpansionMode::SIM_OP_EXPANSION_MODE_AICPU);
            sim::UpdateOpExpansionMode(kAicpuMode);
        }
    }

    // 填充Kernel执行所需参数通知Device侧执行，Proxy进入等待状态
    memcpy(aicpuData->task[rankId].kernelName, kernelName.c_str(), kernelName.length());
    aicpuData->task[rankId].kernelName[kernelName.length()] = '\0';
    aicpuData->task[rankId].args = reinterpret_cast<uint64_t>(ptr);
    std::atomic_thread_fence(std::memory_order_release);  // 内存屏障强制内存操作的顺序性
    aicpuData->task[rankId].devState = DEVICE_RUN;
    while (aicpuData->task[rankId].devState == DEVICE_RUN) {
        CheckDeviceProcStatus();
        sleep(1);
    }
}

aclError aclrtLaunchKernelWithConfig(aclrtFuncHandle funcHandle, uint32_t blockDim, aclrtStream stream, aclrtLaunchKernelCfg *cfg, aclrtArgsHandle argsHandle, void *reserve)
{
    (void) blockDim;
    (void) cfg;
    (void) reserve;
    sim::FuncHandle* func = (sim::FuncHandle*)(uintptr_t)funcHandle;
    HCCL_VM_INFO("[aclrtLaunchKernelWithConfig] funcName:{}, kernelName:{}, func:{:p} stream:{:p} args:{:p}", func->funcName, func->kernelName, (void*)funcHandle, (void*)stream, (void*)argsHandle);
    if (argsHandle == nullptr || stream == nullptr) {
        HCCL_VM_ERROR("[ERROR] [aclrtLaunchKernelWithConfig] invalid input argsHandle or stream");
        return ACL_ERROR_INVALID_PARAM;
    }

    // AICPU或CCU退化为AICPU模式时调用
    LaunchAICPUKernelFunc(func->kernelName, argsHandle);
    HCCL_VM_INFO("[aclrtLaunchKernelWithConfig] kernel:{} execute finished.", func->kernelName);

    return ACL_SUCCESS;
}

#ifdef __cplusplus
}
#endif  // __cplusplus
