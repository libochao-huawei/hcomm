/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "thread_manager.h"
#include <cstring>
#include "aicpu_launch_manager.h"
#include "aicpu_operator_pub.h"
#include "independent_op.h"

namespace hccl {


ThreadMgr::ThreadMgr(uint32_t threadNum, uint32_t notifyNumPerThread, std::string commId, 
    aclrtBinHandle binHandle, const ManagerCallbacks& callbacks) : threadNum_(threadNum), notifyNumPerThread_(notifyNumPerThread), 
    commId_(commId), binHandle_(binHandle), callbacks_(callbacks){}

HcclResult ThreadMgr::HcclThreadAcquire(CommEngine engine, uint32_t threadNum,
    uint32_t notifyNumPerThread, ThreadHandle *threads, std::vector<uint32_t> &threadId)
{
    CHK_PTR_NULL(threads);
    std::lock_guard<std::mutex> lock(threadMutex_);
    std::lock_guard<std::mutex> lockMap(threadMapMutex_);
    HCCL_INFO("[ThreadMgr][%s] Hcom[%s] HcclThreadAcquire begin, max: engine[%d] threadNum[%u],"
        "notifyPerThread[%u], need: threadNum[%u], notifyPerThread[%u]",
        __func__, commId_.c_str(), engine, threadNum_, notifyNumPerThread_, threadNum, notifyNumPerThread);

    if (threadNum == 0) {
        HCCL_ERROR("[ThreadMgr][HcclThreadAcquire] threadNum is 0");
        return HCCL_E_PARA;
    }
    // 如果没设定最大值，设置一下
    uint64_t maxNotifyTotal = 0;
    if (threadNum_ == HCCL_COMM_THREADNUM_CONFIG_NOT_SET &&
        notifyNumPerThread_ == HCCL_COMM_NOTIFY_NUM_PER_THREAD_CONFIG_NOT_SET) {
        maxNotifyTotal = LOCAL_NOTIFY_MAX_NUM;
        threadNum_ = LOCAL_STREAM_MAX_NUM;
        notifyNumPerThread_ = LOCAL_NOTIFY_MAX_NUM;
    } else {
        maxNotifyTotal = static_cast<uint64_t>(threadNum_) * static_cast<uint64_t>(notifyNumPerThread_);
        maxNotifyTotal = maxNotifyTotal > LOCAL_NOTIFY_MAX_NUM ? LOCAL_NOTIFY_MAX_NUM : maxNotifyTotal;
    }
    uint32_t remainQuota = (threadNum_ > threads_.size()) ? (threadNum_ - threads_.size()) : 0;
    if (remainQuota == 0 || threadNum > remainQuota) {
        HCCL_ERROR("[ThreadMgr][%s] Threads quota exhausted: remainQuota[%u], need[%u].",
            __func__, remainQuota, threadNum);
        return HCCL_E_UNAVAIL;
    }

    const uint64_t used = usedNotifyNum_;
    uint64_t remainNotifyQuota = (maxNotifyTotal > used) ? (maxNotifyTotal - used) : 0;
    uint64_t needNotifyTotal = static_cast<uint64_t>(threadNum) * static_cast<uint64_t>(notifyNumPerThread);
    if (remainNotifyQuota < needNotifyTotal  || notifyNumPerThread > notifyNumPerThread_ ||
        maxNotifyTotal > LOCAL_NOTIFY_MAX_NUM) {
        HCCL_ERROR("[ThreadMgr][%s] Notify quota exhausted: remainQuota[%llu], total[%llu], used[%llu], need[%llu], " 
            "setPreNum[%u], allocPreNum[%u]", __func__, remainNotifyQuota, maxNotifyTotal, used, needNotifyTotal,
            notifyNumPerThread_, notifyNumPerThread);
        return HCCL_E_UNAVAIL;
    }

    HCCL_INFO("[ThreadMgr][%s] Hcom[%s] HcclThreadAcquire quota: engine[%d] threadNum[%llu], "
        "remainNotifyQuota[%u]", __func__, commId_.c_str(), engine, remainQuota, remainNotifyQuota);

    NotifyLoadType notifyLoadType;
    StreamType streamType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(CommEngineToStreamType(engine, streamType));
    std::vector<std::shared_ptr<Thread>> newThreads;
    newThreads.reserve(threadNum);
    HcclResult ret = HCCL_E_INTERNAL;

    for (uint32_t i = 0; i < threadNum; ++i) {
        std::shared_ptr<Thread> handle;
        HCCL_INFO("[ThreadMgr][%s] Hcom[%s] AicpuTsThread notifyLoadType[%u], streamType[%u]",
                __func__, commId_.c_str(), static_cast<int32_t>(notifyLoadType), static_cast<int32_t>(streamType));
        CHK_RET(CreateThread(engine, streamType, notifyNumPerThread, notifyLoadType, handle));
        ret = handle->Init();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[ThreadMgr][HcclThreadAcquire] Failed to init thread index %u", i);
            return ret;
        }
        usedNotifyNum_ += notifyNumPerThread;
        newThreads.emplace_back(std::move(handle));
    }

    // thread资源 AICPU侧展开
    std::unique_ptr<ThreadHandle[]> hostHandle;
    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
        if (!callbacks_.getAicpuCommState()) {
            HCCL_INFO("ThreadMgr::HcclAllocThreadRes kernelLaunchAicpuCommInit start");
            HcclResult ret = callbacks_.kernelLaunchAicpuCommInit();
            CHK_PRT_RET(ret != HCCL_SUCCESS, 
                HCCL_ERROR("[%s] kernelLaunchAicpuCommInit failed, return [%d].", __func__, ret), ret);
            callbacks_.setAicpuCommState(true);
        }

        EXECEPTION_CATCH(hostHandle = std::make_unique<ThreadHandle[]>(newThreads.size()),
            return HCCL_E_PTR);
        HCCL_INFO("ThreadMgr::HcclAllocThreadRes ThreadKernelLaunch start");
        ret = AicpuLaunchMgr::ThreadKernelLaunch(newThreads, commId_, hostHandle, binHandle_);
        HCCL_INFO("ThreadMgr::HcclAllocThreadRes ThreadKernelLaunch end");
        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[ThreadMgr][HcclThreadAcquire] AiCpuKernelLaunch failed, return [%d].", ret), ret);
        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = hostHandle[i];
            HCCL_INFO("[ThreadMgr][%s] aicpu threadArray[%u] = [%lu]", __func__, i, threads[i]);
        }
    } else {
        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = reinterpret_cast<ThreadHandle>(newThreads[i].get());
            HCCL_INFO("[ThreadMgr][%s] host threadArray[%u] = [%lu]", __func__, i, threads[i]);
        }
    }
    for (size_t i = 0; i < newThreads.size(); ++i) {
        uint32_t id = newThreads[i]->GetStream()->id();
        HCCL_DEBUG("[%s] thread id = [%u]", __func__, id);
        threadId.push_back(id);
    }
    threads_.reserve(threads_.size() + newThreads.size());
    auto threadsIt = threads_.end();
    threads_.insert(threads_.end(),
                    std::make_move_iterator(newThreads.begin()),
                    std::make_move_iterator(newThreads.end()));

    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
        for (u32 i = 0; i < newThreads.size(); ++i, ++threadsIt) {
            ThreadHandle cpuTsHandle = reinterpret_cast<ThreadHandle>((*threadsIt).get());
            (*threadsIt)->AddThreadHandleToMap(engine, hostHandle[i]);
            threadHandleOthersToCpu_[hostHandle[i]] = cpuTsHandle;
        }
    }

    HCCL_INFO("[ThreadMgr][HcclThreadAcquire] Hcom[%s] HcclThreadAcquire done: engine[%d] threadNum[%u],"
        "notifyPerThread[%u]%s", commId_.c_str(), engine, threadNum, notifyNumPerThread,
        (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) ? " (AICPU token ready)" : "");
    return HCCL_SUCCESS;
}

HcclResult ThreadMgr::HcclGetNotifyNumInThread(ThreadHandle thread, uint32_t *notifyNum)
{
    CHK_PTR_NULL(notifyNum);
    Thread* hcclThread = reinterpret_cast<Thread*>(thread);
    CHK_PTR_NULL(hcclThread);
    *notifyNum = hcclThread->GetNotifyNum();
    HCCL_INFO("[ThreadMgr] Hcom[%s] HcclGetNotifyNumInThread done: notifyPerThread[%u]",
        commId_.c_str(),  *notifyNum);
    return HCCL_SUCCESS;
}

HcclResult ThreadMgr::HcclThreadAcquireWithStream(CommEngine engine,
    rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread)
{
    CHK_PTR_NULL(thread);

    if (mainThread_.find(stream) != mainThread_.end()) {
        *thread = reinterpret_cast<ThreadHandle>(mainThread_[stream].get());
        return HCCL_SUCCESS;
    }

    NotifyLoadType notifyLoadType;
    CHK_RET(CommHostEngineToNotifyLoadType(engine, notifyLoadType));
    std::shared_ptr<CpuTsThread> handle;
    EXECEPTION_CATCH(handle = std::make_shared<CpuTsThread>(stream, notifyNum, notifyLoadType), return HCCL_E_PTR);
    CHK_RET(handle->Init());

    // 返回第一个句柄
    std::lock_guard<std::mutex> lock(mainThreadMutex_);
    mainThread_.emplace(stream, std::move(handle));
    *thread = reinterpret_cast<ThreadHandle>(mainThread_[stream].get());
    HCCL_INFO("[ThreadMgr] Hcom[%s] HcclThreadAcquireWithStream done: engine[%d] stream[%p],"
        "notifyNum[%u]", commId_.c_str(), engine, stream, notifyNum);
    return HCCL_SUCCESS;
}

HcclResult ThreadMgr::ThreadExportToCommEngineCpu(uint32_t threadNum, const ThreadHandle *threads, ThreadHandle *exportedThreads)
{
    std::lock_guard<std::mutex> lock(threadMapMutex_);
    for (u32 i = 0; i < threadNum; i++) {
        if (threadHandleOthersToCpu_.find(threads[i]) == threadHandleOthersToCpu_.end()) {
            HCCL_ERROR("[CommEngineResMgr]%s Unknown ThreadHandle[%lu]", __func__, threads[i]);
            return HCCL_E_PARA;
        }
        exportedThreads[i] = threadHandleOthersToCpu_[threads[i]];
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadMgr::GetExportedThread(const ThreadHandle threadHandle, CommEngine commEngine, Thread *&exportedThread, std::shared_ptr<Thread> &threadOut)
{
    Thread *threadPtr = reinterpret_cast<Thread *>(threadHandle);
    for (auto &thread : threads_) {
        if (thread.get() == threadPtr) {
            exportedThread = thread->FindThreadByCommEngine(commEngine);
            threadOut = thread;
            return HCCL_SUCCESS;
        }
    }

    for (auto &pair : mainThread_) {
        if (pair.second.get() == threadPtr) {
            exportedThread = pair.second->FindThreadByCommEngine(commEngine);
            threadOut = pair.second;
            return HCCL_SUCCESS;
        }
    }

    HCCL_ERROR("[ThreadMgr][%s]Unknown ThreadHandle[%lu]", __func__, threadHandle);
    return HCCL_E_PARA;
}

HcclResult ThreadMgr::ThreadExportToCommEngineAicpu(uint32_t threadNum, const ThreadHandle *threads, CommEngine dstCommEngine, ThreadHandle *exportedThreads)
{
    std::vector<std::shared_ptr<Thread>> hostThreads;
    std::vector<u32> index;
    Thread *exportedThread;
    for (u32 i = 0; i < threadNum; i++) {
        std::shared_ptr<Thread> handle;
        CHK_RET(GetExportedThread(threads[i], dstCommEngine, exportedThread, handle));
        if (exportedThread != nullptr) {
            exportedThreads[i] = reinterpret_cast<ThreadHandle>(exportedThread);
            continue;
        } else {
            hostThreads.push_back(handle);
            index.push_back(i);
        }
    }
    if (!hostThreads.empty()) {
        std::lock_guard<std::mutex> lock(threadMapMutex_);
        if (!callbacks_.getAicpuCommState()) {
            HcclResult ret = callbacks_.kernelLaunchAicpuCommInit();
            CHK_PRT_RET(ret != HCCL_SUCCESS,
                        HCCL_ERROR("[%s] kernelLaunchAicpuCommInit failed, return [%d].", __func__, ret), ret);
            callbacks_.setAicpuCommState(true);
        }
        std::unique_ptr<ThreadHandle[]> aicpuHandle;
        EXECEPTION_CATCH(aicpuHandle = std::make_unique<ThreadHandle[]>(hostThreads.size()),
                         return HCCL_E_PTR);
        HcclResult ret = AicpuLaunchMgr::ThreadKernelLaunch(hostThreads, commId_, aicpuHandle, binHandle_);
        CHK_PRT_RET(ret != HCCL_SUCCESS,
                    HCCL_ERROR("[ThreadMgr][HcclThreadExportToCommEngine] AiCpuKernelLaunch failed, return [%d].", ret), ret);
        for (size_t i = 0; i < hostThreads.size(); ++i) {
            exportedThreads[index[i]] = aicpuHandle[i];
            CHK_RET(hostThreads[i]->AddThreadHandleToMap(dstCommEngine, aicpuHandle[i]));
            threadHandleOthersToCpu_[aicpuHandle[i]] = threads[index[i]];
            HCCL_INFO("[ThreadMgr][%s] aicpu threadArray[%u] = [%lu]", __func__, i, aicpuHandle[i]);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadMgr::HcclThreadExportToCommEngine(uint32_t threadNum, const ThreadHandle *threads, CommEngine dstCommEngine, ThreadHandle *exportedThreads)
{
    switch (dstCommEngine) {
    case COMM_ENGINE_CPU_TS:
    case COMM_ENGINE_CPU:
    case COMM_ENGINE_CCU:
        CHK_RET(ThreadExportToCommEngineCpu(threadNum, threads, exportedThreads));
        break;
    case COMM_ENGINE_AICPU:
    case COMM_ENGINE_AICPU_TS:
        CHK_RET(ThreadExportToCommEngineAicpu(threadNum, threads, dstCommEngine, exportedThreads));
        break;
    case COMM_ENGINE_AIV:
    default:
        HCCL_ERROR("[ThreadMgr] Unknown comm engine type: %d", dstCommEngine);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}
}