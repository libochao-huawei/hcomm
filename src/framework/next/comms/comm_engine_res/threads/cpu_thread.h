/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_CPU_THREAD_H
#define HCCL_CPU_THREAD_H
#include <memory>
#include <vector>
#include "thread.h"
//-------------------------------------------------------------------------------
typedef enum {
    THREAD_TYPE_RESERVED = -1,    ///< 保留的
    THREAD_TYPE_CPU = 0,      ///< HOST CPU
    THREAD_TYPE_TS = 1,   ///< TS
} ThreadType;
typedef struct {
    uint16_t notifyNumPerThread;
    uint16_t specifiedNotifyTypeNum;
} ThreadConfig;
typedef struct {
    void* args;
    uint32_t argsSize;
    uint16_t timeOut;
    uint16_t reserved;
} ThreadServiceArgs;
typedef HcclResult (ThreadService)(ThreadServiceArgs* args);
typedef uint64_t ThreadServiceHandle;
//-----------------------------------------------------------------------------------------------------------------------------------
HcclResult HcclThreadAcquireWithConfig(HcclComm comm, CommEngine engine, uint32_t threadNum, const ThreadType type, const ThreadConfig config, ThreadHandle *threads) {
    CHK_PRT_RET(comm == nullptr,  HCCL_ERROR("[%s] comm is null", __func__), HCCL_E_PTR);
    CHK_PRT_RET(threads == nullptr,  HCCL_ERROR("[%s] threads is null", __func__), HCCL_E_PTR);
    CHK_PRT_RET(!IsValidCommEngine(engine), 
        HCCL_ERROR("[%s] commEngine[%d] is invalid", __func__, static_cast<int32_t>(engine)), HCCL_E_PARA);

    auto* hcclComm = static_cast<hccl::hcclComm*>(comm);
    std::string commId = hcclComm->GetIdentifier();
    HCCL_RUN_INFO("Entry-%s:comm[%s] engine[%u] reqThreadNum[%u] notifyNumPerThread[%u]",
        __func__, commId.c_str(), engine, threadNum, notifyNumPerThread);

    HcclResult ret = HCCL_SUCCESS;
    std::vector<uint32_t> threadId;
    if (hcclComm->IsCommunicatorV2()) {
        hccl::CollComm* collComm = hcclComm->GetCollComm();
        CHK_PTR_NULL(collComm);
        CommEngineResMgr* engineResMgr = collComm->GetCommEngineResMgr();
        CHK_PTR_NULL(engineResMgr);
        ret = engineResMgr->HcclThreadAcquire(engine, threadNum, notifyNumPerThread, threads, threadId);
    }
    else {
        auto& engineResMgr = hcclComm->GetIndependentOp().GetCommEngineResMgr();
        ret = engineResMgr.HcclThreadAcquire(engine, threadNum, notifyNumPerThread, threads, threadId);
        if (engine == CommEngine::COMM_ENGINE_AICPU_TS || engine == CommEngine::COMM_ENGINE_AICPU) {
            // 上报流
            if (threadNum != threadId.size()) {
                HCCL_ERROR("[%s] threadNum [%u] != threadId.size[%u]", __func__, threadNum, threadId.size());
                return HCCL_E_PARA;
            }
            CHK_RET(HcclStreamProfilingReport(comm, threadNum, threadId.data()));
        }
    }
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to create threads for engine[%d], threadNum[%u], notifyNumPerThread[%u]",
            __func__, engine, threadNum, notifyNumPerThread);
        return ret;
    }

    HCCL_INFO("[%s] Allocated %u threads for engine[%d], notifyPerThread[%u]", __func__,
              threadNum, engine, notifyNumPerThread);
    return HCCL_SUCCESS;
};
HcclResult CommEngineResMgr::HcclThreadAcquireWithType(CommEngine engine, uint32_t threadNum,
    uint32_t notifyNumPerThread, const ThreadType type, ThreadHandle *threads, std::vector<uint32_t> &threadId)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    uint32_t setThreadNum = threadMgr_->GetThreadNum();
    uint32_t setNotifyNumPerThread = threadMgr_->GetNotifyNumPerThread();
    CHK_PRT_RET(threadNum > setThreadNum,  HCCL_ERROR("[%s] Alloced thread num[%u] more than num[%u] in config", 
        __func__, threadNum, setThreadNum), HCCL_E_PARA);
    CHK_PRT_RET(notifyNumPerThread > setNotifyNumPerThread,  HCCL_ERROR("[%s] Alloced preNotify num[%u] more than "
        "num[%u] in config", __func__, notifyNumPerThread, setNotifyNumPerThread), HCCL_E_PARA);
    return threadMgr_->HcclThreadAcquire(engine, threadNum, notifyNumPerThread, threads, threadId);
}

HcclResult ThreadMgr::HcclThreadAcquireWithType(CommEngine engine, uint32_t threadNum,
    uint32_t notifyNumPerThread, const ThreadType type, ThreadHandle *threads, std::vector<uint32_t> &threadId)
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

    // 开始创建thread资源
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
    if (engine == COMM_ENGINE_AICPU) {
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

    if (engine == COMM_ENGINE_AICPU) {
        for (u32 i = 0; i < newThreads.size(); ++i, ++threadsIt) {
            ThreadHandle cpuTsHandle = reinterpret_cast<ThreadHandle>((*threadsIt).get());
            (*threadsIt)->AddThreadHandleToMap(engine, hostHandle[i]);
            threadHandleOthersToCpu_[hostHandle[i]] = cpuTsHandle;
        }
    }

    HCCL_INFO("[ThreadMgr][HcclThreadAcquire] Hcom[%s] HcclThreadAcquire done: engine[%d] threadNum[%u],"
        "notifyPerThread[%u]%s", commId_.c_str(), engine, threadNum, notifyNumPerThread,
        (engine == COMM_ENGINE_AICPU) ? " (AICPU token ready)" : "");
    return HCCL_SUCCESS;
}
HcclResult HcommThreadAllocWithType(CommEngine engine, uint32_t threadNum, uint32_t notifyNumPerThread, const ThreadType type, ThreadHandle *threads) {
    CHK_PTR_NULL(threads);

    HCCL_INFO("[%s]ThreadAcquire begin. need threadNum[%u], notifyPerThread[%u]",
        __func__,
        threadNum,
        notifyNumPerThread);
    if (threadNum <= 0 || threadNum > hccl::HCOMM_THREADNUM_MAX_NUM) {
        HCCL_ERROR("[HcommThreadAlloc]ThreadAlloc failed.ThreadNum %u.threadNum range (0 , %u]", threadNum, hccl::HCOMM_THREADNUM_MAX_NUM);
        return HCCL_E_PARA;
    }

    if (notifyNumPerThread < 0 || notifyNumPerThread > hccl::HCOMM_NOTIFY_MAX_NUM) {
        HCCL_ERROR("[HcommThreadAlloc]ThreadAlloc failed.notifyNumPerThread is %u,notifyNumPerThread range [0 , %u]", notifyNumPerThread, hccl::HCOMM_NOTIFY_MAX_NUM);
        return HCCL_E_PARA;
    }

    hccl::NotifyLoadType notifyLoadType;
    hccl::StreamType streamType;
    CHK_RET(CommEngineToNotifyLoadType(engine, notifyLoadType));
    CHK_RET(CommEngineToStreamType(engine, streamType));

    HcclResult ret = HCCL_SUCCESS;
    for (uint32_t i = 0; i < threadNum; ++i) {
        std::shared_ptr<hccl::Thread> handle;
        HCCL_INFO("[%s] Thread notifyLoadType[%u], streamType[%u]",
            __func__,
            static_cast<int32_t>(notifyLoadType),
            static_cast<int32_t>(streamType));
        ret = CreateThread(engine, streamType, notifyNumPerThread, notifyLoadType, handle);
        if (ret != HCCL_SUCCESS ) {
            HCCL_ERROR("[HcommThreadAlloc] Failed to create thread index %u", i);
            if (i != 0) {
                CHK_RET(HcommThreadFree(threads, i));
            }
            return ret;
        }
        ret = handle->Init();
        if (ret != HCCL_SUCCESS ) {
            HCCL_ERROR("[HcommThreadAlloc] Failed to init thread index %u", i);
            if (i != 0) {
                CHK_RET(HcommThreadFree(threads, i));
            }
            return ret;
        }
        threads[i] = reinterpret_cast<ThreadHandle>(handle.get());
        hcomm::g_ThreadMap.emplace(threads[i], handle);
    }

    HCCL_INFO("[HcommThreadAlloc] ThreadAcquire done: engine[%d] threadNum[%u],"
              "notifyPerThread[%u]", engine, threadNum, notifyNumPerThread);
    return HCCL_SUCCESS;
}

//-------------------------------------------------------------------

namespace hccl {

class MsgQueue {
public:
    HcclResult Init();
    pop();
    push();
    empty();
    
private:
    uint64_t addr_;
    uint64_t capacity_;
    uint64_t msgSize_;
    uint64_t head_;
    uint64_t tail_;
};

class ServiceScheduler {
public:
    ServiceScheduler();
    ~ServiceScheduler();
    HcclResult Run();

    HcclResult ServiceRegister(Service serviceCb, ServiceHandle service);
    HcclResult ServiceUnregister(ServiceHandle service);
private:
    HcclResult executeService(ServiceHandle service); // ?

    std::mutex mtx_;
    std::unordered_map<ThreadService*, ThreadServiceHandle> service2HandleMap_;
    std::unordered_map<ThreadServiceHandle, ThreadService*> handle2ServiceMap_;
    std::shared_ptr<MsgQueue> sendQueue_;
    std::shared_ptr<MsgQueue> completeQueue_;
};

class HostMemNotify {
    
}

class CpuThread : public Thread {
public:
    CpuThread(rtStream_t rtStream, uint32_t notifyNum, const NotifyLoadType notifyLoadType);
    CpuThread(StreamType streamType, uint32_t notifyNum, const NotifyLoadType notifyLoadType);

    HcclResult Init() override;
    HcclResult DeInit() override;

    ~CpuThread();

    HcclResult ServiceRegister(Service serviceCb, ServiceHandle service);
    HcclResult ServiceUnregister(ServiceHandle service);
    HcclResult KernelRun();

private:
    HcclResult PrepareDpuKernelResource(aclrtFuncHandle &funcHandle);
    HcclResult LaunchDpuKernel(aclrtFuncHandle &funcHandle);

    aclrtStream dpuStream_;
    aclrtContext dpuContext_;
    aclrtContext npuContext_;
    std::unique_ptr<ServiceScheduler> ServiceScheduler_;
    uint32_t notifyNum_;
    std::vector<std::unique_ptr<HostMemNotify>> notifys_;
    ThreadServiceHandle notifyServiceHandle_;
    ThreadServiceHandle waitServiceHandle_;
};

__attribute__((visibility("default"))) uint32_t RunDpuRpcSrvLaunch(const uint64_t args)
{
    struct DpuKernelLaunchParam {
        u64      memorySize;
        void*       deviceMem;
        void*       hostMem;
        int32_t    deviceId;
        std::string commId;
    };

    HCCL_INFO("[%s] Launch Dpu Kernel: 0x%lx", __func__, args);
    if (args == 0) {
        HCCL_ERROR("[%s] args is null.", __func__);
        return HCCL_E_PARA;
    }

    // 解析参数信息
    DpuKernelLaunchParam *params = reinterpret_cast<DpuKernelLaunchParam *>(args);

    HCCL_RUN_INFO("[%s] DpuKernelLaunchParam{commId:%s; memorySize:%lu; deviceMem:%p; hostMem:%p; devId:%d}", __func__,
                  params->commId.c_str(), params->memorySize, params->deviceMem, params->hostMem, params->deviceId);

    if (params->memorySize == 0) {
        HCCL_ERROR("[%s] memorySize is 0.", __func__);
        return HCCL_E_PARA;
    }
    if (params->deviceMem == nullptr || params->hostMem == nullptr) {
        HCCL_ERROR("[%s] deviceMem[%p] or hostMem[%p] is nullptr.", __func__, params->deviceMem, params->hostMem);
        return HCCL_E_PARA;
    }

    // 实例化TaskService
    std::unique_ptr<Hccl::TaskService> taskService = std::make_unique<Hccl::TaskService>(params->deviceMem, params->memorySize,
                            params->hostMem, params->memorySize);

    aclError ret = aclrtSetDevice(params->deviceId);
    if (ret != ACL_SUCCESS) {
        HCCL_ERROR("[%s] set device fail. DeviceId: %d.", __func__, params->deviceId);
        return HCCL_E_RUNTIME;
    }

    // 设置到通信域中保存 map<commId, TaskService>
    HCCL_INFO("[%s] save TaskService", __func__);
    g_taskServiceMap.insert(std::make_pair(params->commId, std::move(taskService)));

    // Run
    HCCL_INFO("[%s] start to TaskRun", __func__);
    g_taskServiceMap.at(params->commId)->TaskRun();
    return HCCL_SUCCESS;
}

}  // namespace hccl

#endif  // HCCL_CPU_THREAD_H
