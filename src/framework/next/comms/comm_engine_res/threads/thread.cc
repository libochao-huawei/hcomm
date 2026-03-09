/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "thread.h"
#include "cpu_ts_thread.h"
#include "aicpu_ts_thread.h"

namespace hccl {
static std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> g_ThreadMap;
static std::unordered_map<ThreadHandle, ThreadHandle> g_ThreadD2HMap;
static std::mutex g_ThreadMapMtx;

HcclResult CreateThread(CommEngine engine, StreamType streamType,
    uint32_t notifyNum, NotifyLoadType loadType, std::shared_ptr<Thread>& out_thread)  
{
    out_thread = nullptr;  // 初始化出参
 
    if (engine == COMM_ENGINE_CPU_TS || engine == COMM_ENGINE_CPU
        || engine == COMM_ENGINE_CCU) {
        EXECEPTION_CATCH(out_thread = std::make_shared<CpuTsThread>(streamType, notifyNum, loadType), return HCCL_E_PTR);
    } else if (engine == COMM_ENGINE_AICPU_TS || engine == COMM_ENGINE_AICPU) {
        EXECEPTION_CATCH(out_thread = std::make_shared<AicpuTsThread>(streamType, notifyNum, loadType), return HCCL_E_PTR);
    } else {
        return HCCL_E_NOT_SUPPORT;
    }
 
    return HCCL_SUCCESS;
}

HcclResult CommHostEngineToNotifyLoadType(CommEngine engine, NotifyLoadType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type =  NotifyLoadType::HOST_NOTIFY;
            break;
        default:
            HCCL_ERROR("[ThreadMgr] Unsupported comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult CommEngineToNotifyLoadType(CommEngine engine, NotifyLoadType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type =  NotifyLoadType::HOST_NOTIFY;
            break;
        case COMM_ENGINE_AICPU:
        case COMM_ENGINE_AICPU_TS:
            type =  NotifyLoadType::DEVICE_NOTIFY;
            break;
        default:
            HCCL_ERROR("[ThreadMgr] Unknown comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult CommEngineToStreamType(CommEngine engine, StreamType &type)
{
    switch (engine) {
        case COMM_ENGINE_CPU:
        case COMM_ENGINE_CPU_TS:
        case COMM_ENGINE_CCU:
            type = StreamType::STREAM_TYPE_ONLINE; // 单算子使用online，图模式使用offine
            break;
        case COMM_ENGINE_AICPU:
        case COMM_ENGINE_AICPU_TS:
            type = StreamType::STREAM_TYPE_DEVICE;
            break;
        // 暂不支持AIV
        case COMM_ENGINE_AIV:
        default:
            HCCL_ERROR("[ThreadMgr] Unknown comm engine type: %d", engine);
            return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult ValidateThreadParams(uint32_t threadNum, uint32_t notifyNumPerThread) 
{
    if (threadNum == 0 || threadNum > HCOMM_THREADNUM_MAX_NUM) {
        HCCL_ERROR("[%s] Validate thread params failed. ThreadNum %u, range (0, %u]",
            __func__, threadNum, HCOMM_THREADNUM_MAX_NUM);
        return HCCL_E_PARA;
    }
    if (notifyNumPerThread > HCOMM_NOTIFY_MAX_NUM) {
        HCCL_ERROR("[%s] Validate thread params failed. notifyNumPerThread %u, range [0, %u]",
            __func__, notifyNumPerThread, HCOMM_NOTIFY_MAX_NUM);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult SaveThreads(const std::vector<std::shared_ptr<hccl::Thread>> &newThreads) {
    std::lock_guard<std::mutex> lk(g_ThreadMapMtx);
    for (const auto &threadPtr : newThreads) {
        ThreadHandle handle = reinterpret_cast<ThreadHandle>(threadPtr.get());

        if (g_ThreadMap.find(handle) != g_ThreadMap.end()) {
            HCCL_ERROR("[%s] thread handle already exists [0x%llx] in ThreadMap", __func__, handle);
            return HCCL_E_INTERNAL;
        }
        if (g_ThreadD2HMap.find(handle) != g_ThreadD2HMap.end()) {
            HCCL_ERROR("[%s] thread handle already exists [0x%llx] in g_ThreadD2HMap", __func__, handle);
            return HCCL_E_INTERNAL;
        }

        g_ThreadMap.emplace(handle, threadPtr);
        g_ThreadD2HMap.emplace(handle, handle);
    }
    return HCCL_SUCCESS;
}

HcclResult CreateAndInitThreads(const ThreadCreateParams& params,
    std::vector<std::shared_ptr<hccl::Thread>>& outThreads) 
{
    HCCL_INFO("[%s] Creating threads with params: engine[%d], threadNum[%u], "
              "notifyNumPerThread[%u], notifyLoadType[%u], streamType[%u]",
              __func__, params.engine, params.threadNum, params.notifyNumPerThread,
              static_cast<int32_t>(params.notifyLoadType), 
              static_cast<int32_t>(params.streamType));
    outThreads.reserve(params.threadNum);

    for (uint32_t i = 0; i < params.threadNum; ++i) {
        std::shared_ptr<hccl::Thread> threadPtr;
        // 创建线程
        HcclResult ret = CreateThread(params.engine, params.streamType, 
                                      params.notifyNumPerThread, 
                                      params.notifyLoadType, threadPtr);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Failed to create thread at index %u, error: %d", 
                      __func__, i, ret);
            return ret;
        }
        // 初始化线程
        ret = threadPtr->Init();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Failed to initialize thread at index %u, error: %d", 
                      __func__, i, ret);
            return ret;
        }
        // 添加到输出列表
        outThreads.emplace_back(std::move(threadPtr));
    }
    HCCL_INFO("[%s] Successfully created and initialized %u threads", 
              __func__, params.threadNum);
    return HCCL_SUCCESS;
}

HcclResult StoreThreadHandles(const std::vector<std::shared_ptr<hccl::Thread>>& newThreads,
    ThreadHandle* threads, CommEngine engine, aclrtBinHandle binHandle)
{
    CHK_PTR_NULL(threads);
    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
        // AICPU引擎处理逻辑
        std::unique_ptr<ThreadHandle[]> hostHandle;
        EXECEPTION_CATCH(hostHandle = std::make_unique<ThreadHandle[]>(newThreads.size()),
                         return HCCL_E_PTR);
        // 确保内核已加载
        CHK_RET(EnsureKernelBinLoaded());
        HcclResult ret = hccl::AicpuLaunchMgr::ThreadKernelLaunchInternal(
            newThreads, hostHandle, g_BinHandle);

        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[StoreThreadHandles] AiCpuKernelLaunch failed, engine[%d], return[%d].", 
                      engine, ret), ret);

        // 保存并映射AICPU线程句柄
        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = hostHandle[i];
            ThreadHandle cpuTsHandle = reinterpret_cast<ThreadHandle>(newThreads[i].get());
            CHK_RET(FillThreadD2HMap(&hostHandle[i], &cpuTsHandle, 1));
            HCCL_INFO("[StoreThreadHandles] AICPU engine[%d] threadArray[%zu] = [%lu]", 
                      engine, i, threads[i]);
        }
    } else {
        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = reinterpret_cast<ThreadHandle>(newThreads[i].get());
            HCCL_INFO("[StoreThreadHandles] Host engine[%d] threadArray[%zu] = [%lu]", 
                      engine, i, threads[i]);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult Thread::AddThreadHandleToMap(CommEngine commEngine, ThreadHandle threadHandle)
{
    if (threadHandleMap_.find(commEngine) != threadHandleMap_.end() && threadHandleMap_[commEngine] != threadHandle) {
        HCCL_ERROR("[Thread][%s]Mapping already exists:commEngine[%d], threadHandle[%lu], new threadHandle[%lu]",
                   __func__, threadHandleMap_[commEngine], threadHandle);
    }

    threadHandleMap_[commEngine] = threadHandle;
    return HCCL_SUCCESS;
}

Thread *Thread::FindThreadByCommEngine(CommEngine commEngine)
{
    if (threadHandleMap_.find(commEngine) != threadHandleMap_.end()) {
        return reinterpret_cast<Thread *>(threadHandleMap_[commEngine]);
    }

    return nullptr;
}
}  // namespace hccl