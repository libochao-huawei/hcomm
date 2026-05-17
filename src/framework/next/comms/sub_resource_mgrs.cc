/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "sub_resource_mgrs.h"

#include <algorithm>

#include "log.h"
#include "endpoint.h"
#include "hcomm_c_adpt.h"
#include "adapter_hal_pub.h"

#ifndef CCL_KERNEL_AICPU
#include "thread.h"
#include "aicpu_launch_manager.h"
#include "exception_handler.h"
#endif

namespace hcomm {

EndpointsMgr::~EndpointsMgr()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : endpointMemMap_) {
        const EndpointHandle endpointHandle = kv.first;
        for (auto memHandle : kv.second) {
            (void)HcommMemUnreg(endpointHandle, memHandle);
        }
    }
    endpointMemMap_.clear();
    memHandleCount_ = 0;
    endpointObjMap_.clear();
    endpointMap_.clear();
    endpointCount_ = 0;
}

HcclResult EndpointsMgr::Get(EndpointDesc epDesc, EndpointHandle& handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto iterPtr = endpointMap_.find(epDesc);
    if (iterPtr != endpointMap_.end()) {
        handle = iterPtr->second;
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[EndpointsMgr::Get] create Endpoint");
    std::unique_ptr<Endpoint> endpointPtr = nullptr;
    CHK_RET(Endpoint::CreateEndpoint(epDesc, endpointPtr));
    CHK_PTR_NULL(endpointPtr);
    CHK_RET(endpointPtr->Init());
    const EndpointHandle h = reinterpret_cast<EndpointHandle>(endpointPtr.get());
    endpointObjMap_.emplace(h, std::move(endpointPtr));
    endpointMap_.emplace(epDesc, h);
    endpointCount_++;
    handle = h;
    return HCCL_SUCCESS;
}

void EndpointsMgr::InstallOwnedEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = endpointObjMap_.find(handle);
    if (it != endpointObjMap_.end()) {
        it->second = std::move(endpoint);
        return;
    }
    endpointObjMap_.emplace(handle, std::move(endpoint));
}

Endpoint* EndpointsMgr::FindEndpointByHandle(EndpointHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = endpointObjMap_.find(handle);
    if (it == endpointObjMap_.end() || !it->second) {
        return nullptr;
    }
    return it->second.get();
}

HcclResult EndpointsMgr::RemoveOwnedEndpoint(EndpointHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto itObj = endpointObjMap_.find(handle);
    if (itObj == endpointObjMap_.end()) {
        return HCCL_E_NOT_FOUND;
    }
    auto memIt = endpointMemMap_.find(handle);
    if (memIt != endpointMemMap_.end()) {
        for (auto memHandle : memIt->second) {
            (void)HcommMemUnreg(handle, memHandle);
        }
        endpointMemMap_.erase(memIt);
    }
    endpointObjMap_.erase(itObj);
    for (auto it = endpointMap_.begin(); it != endpointMap_.end();) {
        if (it->second == handle) {
            it = endpointMap_.erase(it);
            if (endpointCount_ > 0) {
                endpointCount_--;
            }
        } else {
            ++it;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::RegisterMemory(EndpointHandle epHandle, const std::vector<std::string>& memTag,
    const std::vector<HcclMem>& memVec, std::vector<MemHandle>& memHandleVec)
{
    memHandleVec.clear();
    uint32_t index = 0;
    for (const auto &mem : memVec) {
        MemHandle memHandle = nullptr;
        CommMem commMem {
            static_cast<CommMemType>(mem.type),
            mem.addr,
            mem.size
        };
        HcclResult ret = static_cast<HcclResult>(HcommMemReg(epHandle, memTag[index].c_str(), &commMem, &memHandle));
        if (ret != HCCL_SUCCESS && ret != HCCL_E_AGAIN) {
            HCCL_ERROR("[%s] HcommMemReg failed, ret=%d", __FUNCTION__, ret);
            return ret;
        }
        CHK_PTR_NULL(memHandle);
        memHandleVec.push_back(memHandle);
        memHandleCount_++;
        index++;
        if (ret == HCCL_E_AGAIN) {
            HCCL_WARNING("This mem has already been registered, addr=%p, size=%llu", mem.addr, mem.size);
        }
    }
    CHK_RET(AddMemHandle(epHandle, memHandleVec));
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::GetAllRegisteredMemory(EndpointHandle epHandle, std::vector<MemHandle>& memHandleVec)
{
    if (!IsMemExist(epHandle)) {
        HCCL_ERROR("EndpointsMgr GetAllRegisteredMemory Fail");
        return HCCL_E_MEMORY;
    }
    memHandleVec = endpointMemMap_.at(epHandle);
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::Destroy(EndpointHandle handle)
{
    return RemoveOwnedEndpoint(handle);
}

HcclResult EndpointsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : endpointMemMap_) {
        for (auto memHandle : kv.second) {
            (void)HcommMemUnreg(kv.first, memHandle);
        }
    }
    endpointMemMap_.clear();
    memHandleCount_ = 0;
    endpointObjMap_.clear();
    endpointMap_.clear();
    endpointCount_ = 0;
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::DestroyAllMem()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : endpointMemMap_) {
        for (auto memHandle : kv.second) {
            (void)HcommMemUnreg(kv.first, memHandle);
        }
    }
    endpointMemMap_.clear();
    memHandleCount_ = 0;
    return HCCL_SUCCESS;
}

size_t EndpointsMgr::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return endpointMap_.size();
}

size_t EndpointsMgr::GetEndpointCount() const
{
    return endpointCount_.load();
}

size_t EndpointsMgr::GetMemHandleCount() const
{
    return memHandleCount_.load();
}

bool EndpointsMgr::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return endpointMap_.empty();
}

HcclResult EndpointsMgr::AddMemHandle(EndpointHandle epHandle, const std::vector<MemHandle>& memHandleVec)
{
    if (memHandleVec.empty()) {
        return HCCL_SUCCESS;
    }

    if (IsMemExist(epHandle)) {
        auto& existMemHandleVec = endpointMemMap_.at(epHandle);
        existMemHandleVec.insert(existMemHandleVec.end(), memHandleVec.begin(), memHandleVec.end());
        return HCCL_SUCCESS;
    }

    endpointMemMap_.emplace(epHandle, std::move(memHandleVec));
    return HCCL_SUCCESS;
}

bool EndpointsMgr::IsMemExist(EndpointHandle epHandle)
{
    return endpointMemMap_.find(epHandle) != endpointMemMap_.end();
}

bool EndpointsMgr::IsDescExist(EndpointDesc epDesc)
{
    return endpointMap_.find(epDesc) != endpointMap_.end();
}

HcclResult ChannelsMgr::Create(EndpointHandle endpointHandle, CommEngine engine,
    HcommChannelDesc* channelDescs, uint32_t channelNum,
    ChannelHandle* outHandles)
{
    (void)endpointHandle;
    (void)engine;
    (void)channelDescs;
    (void)channelNum;
    (void)outHandles;
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::Destroy(const ChannelHandle* channels, uint32_t channelNum)
{
    (void)channels;
    (void)channelNum;
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    channelMap_.clear();
    channelD2HMap_.clear();
    channelCount_ = 0;
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::Get(ChannelHandle handle, Channel** channel)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channelMap_.find(handle);
    if (it != channelMap_.end()) {
        *channel = it->second.get();
        return HCCL_SUCCESS;
    }
    return HCCL_E_NOT_FOUND;
}

size_t ChannelsMgr::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return channelMap_.size();
}

size_t ChannelsMgr::GetChannelCount() const
{
    return channelCount_.load();
}

bool ChannelsMgr::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return channelMap_.empty();
}

HcclResult ThreadsMgr::Alloc(CommEngine engine, uint32_t threadNum,
    const uint32_t* notifyNumPerThread, ThreadHandle* threads)
{
    (void)engine;
    (void)threadNum;
    (void)notifyNumPerThread;
    (void)threads;
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::Free(const ThreadHandle* threads, uint32_t threadNum)
{
    (void)threads;
    (void)threadNum;
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::DestroyAll()
{
#ifndef CCL_KERNEL_AICPU
    std::lock_guard<std::mutex> lock(mutex_);
    threadMap_.clear();
    threadD2HMap_.clear();
    threadCount_ = 0;
#endif
    return HCCL_SUCCESS;
}

size_t ThreadsMgr::Size() const
{
#ifndef CCL_KERNEL_AICPU
    std::lock_guard<std::mutex> lock(mutex_);
    return threadMap_.size();
#else
    return 0;
#endif
}

size_t ThreadsMgr::GetThreadCount() const
{
    return threadCount_.load();
}

bool ThreadsMgr::IsEmpty() const
{
#ifndef CCL_KERNEL_AICPU
    std::lock_guard<std::mutex> lock(mutex_);
    return threadMap_.empty();
#else
    return true;
#endif
}

#ifndef CCL_KERNEL_AICPU
HcclResult ThreadsMgr::SaveThreadsBatch(const std::vector<std::shared_ptr<hccl::Thread>>& newThreads)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& threadPtr : newThreads) {
        ThreadHandle handle = reinterpret_cast<ThreadHandle>(threadPtr.get());

        if (threadMap_.find(handle) != threadMap_.end()) {
            HCCL_ERROR("[%s] thread handle already exists [0x%llx] in ThreadMap", __func__, handle);
            return HCCL_E_INTERNAL;
        }
        DeviceThreadKey key{deviceId, handle};
        if (threadD2HMap_.find(key) != threadD2HMap_.end()) {
            HCCL_ERROR("[%s] thread handle already exists [0x%llx] in threadD2HMap_, deviceId[%d]", __func__, handle,
                deviceId);
            return HCCL_E_INTERNAL;
        }

        threadMap_.emplace(handle, threadPtr);
        threadD2HMap_.emplace(key, handle);
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::FillThreadD2HMapBatch(
    ThreadHandle* deviceThreadHandles, ThreadHandle* hostThreadHandles, uint32_t listNum)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t idx = 0; idx < listNum; idx++) {
        auto deviceThreadHandle = deviceThreadHandles[idx];
        auto hostThreadHandle = hostThreadHandles[idx];
        HCCL_INFO("%s deviceId[%d], deviceThreadHandle[0x%llx], hostThreadHandle[0x%llx]", __func__, deviceId,
            deviceThreadHandle, hostThreadHandle);
        DeviceThreadKey key{deviceId, deviceThreadHandle};
        threadD2HMap_.emplace(key, hostThreadHandle);
    }

    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::StoreThreadHandlesBatch(std::vector<std::shared_ptr<hccl::Thread>>& newThreads,
    ThreadHandle* threads, CommEngine engine, aclrtBinHandle binHandle)
{
    CHK_PTR_NULL(threads);
    if (engine == COMM_ENGINE_AICPU || engine == COMM_ENGINE_AICPU_TS) {
        std::unique_ptr<ThreadHandle[]> aicpuHandle;
        EXECEPTION_CATCH(aicpuHandle = std::make_unique<ThreadHandle[]>(newThreads.size()), return HCCL_E_PTR);
        CHK_PTR_NULL(binHandle);
        HcclResult ret = AicpuLaunchMgr::ThreadKernelLaunchForBase(newThreads, aicpuHandle, binHandle);

        CHK_PRT_RET(ret != HCCL_SUCCESS,
            HCCL_ERROR("[StoreThreadHandles] AiCpuKernelLaunch failed, engine[%d], return[%d].", engine, ret), ret);

        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = aicpuHandle[i];
            ThreadHandle hostHandle = reinterpret_cast<ThreadHandle>(newThreads[i].get());
            CHK_RET(FillThreadD2HMapBatch(&aicpuHandle[i], &hostHandle, 1));
            HCCL_INFO("[StoreThreadHandles] AICPU engine[%d] threadArray[%zu] = [%lu]", engine, i, threads[i]);
        }
    } else {
        for (size_t i = 0; i < newThreads.size(); ++i) {
            threads[i] = reinterpret_cast<ThreadHandle>(newThreads[i].get());
            HCCL_INFO("[StoreThreadHandles] Host engine[%d] threadArray[%zu] = [%lu]", engine, i, threads[i]);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::FreeThreadHandlesBatch(
    const ThreadHandle* threads, uint32_t threadNum, std::vector<ThreadHandle>& deviceHandles)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < threadNum; ++i) {
        const ThreadHandle inHandle = threads[i];

        DeviceThreadKey key{deviceId, inHandle};
        auto itH = threadD2HMap_.find(key);
        if (itH == threadD2HMap_.end()) {
            HCCL_ERROR("[%s] failed to find handle mapping in threadD2HMap_, deviceId[%d], inHandle[0x%llx].", __func__,
                deviceId, inHandle);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        const ThreadHandle mappedHandle = itH->second;

        auto itC = threadMap_.find(mappedHandle);
        if (itC == threadMap_.end()) {
            HCCL_ERROR("[%s] failed to find thread in threadMap_, deviceId[%d], inHandle[0x%llx], mappedHandle[0x%llx].",
                __func__, deviceId, inHandle, mappedHandle);
            return HcclResult::HCCL_E_NOT_FOUND;
        }
        if (inHandle != mappedHandle) {
            deviceHandles.push_back(inHandle);
        }

        HCCL_INFO("[%s] erase thread: deviceId[%d], inHandle[0x%llx], mappedHandle[0x%llx], ptr[%p]", __func__, deviceId,
            inHandle, mappedHandle, itC->second.get());
        threadMap_.erase(itC);

        for (auto it = threadD2HMap_.begin(); it != threadD2HMap_.end();) {
            if (it->second == mappedHandle && it->first.deviceId == deviceId) {
                it = threadD2HMap_.erase(it);
            } else {
                ++it;
            }
        }
    }
    return HCCL_SUCCESS;
}
#endif

HcclResult ThreadsMgr::RegisterStreamThread(ThreadHandle handle, std::shared_ptr<hccl::Thread> threadPtr)
{
#ifndef CCL_KERNEL_AICPU
    std::lock_guard<std::mutex> lock(mutex_);
    threadMap_.emplace(handle, std::move(threadPtr));
#else
    (void)handle;
    (void)threadPtr;
#endif
    return HCCL_SUCCESS;
}

HcclResult CcuInstancesMgr::GetCcuInstance(int32_t deviceLogicId, void*& ccu)
{
    (void)deviceLogicId;
    (void)ccu;
    return HCCL_SUCCESS;
}

HcclResult CcuInstancesMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ccuInstances_.clear();
    ccuInstanceCount_ = 0;
    return HCCL_SUCCESS;
}

size_t CcuInstancesMgr::GetCcuInstanceCount() const
{
    return ccuInstanceCount_.load();
}

bool CcuInstancesMgr::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ccuInstances_.empty();
}

HdcProcess::~HdcProcess()
{
    (void)Deinit();
}

HcclResult HdcProcess::Init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_.load()) {
        return HCCL_SUCCESS;
    }
    initialized_.store(true);
    HCCL_INFO("[HdcProcess][%s] initialized", __func__);
    return HCCL_SUCCESS;
}

HcclResult HdcProcess::Deinit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.load()) {
        return HCCL_SUCCESS;
    }

    rdmaHandleMgr_.reset();
    hccpHdcMgr_.reset();
    hccpPeerMgr_.reset();
    hccpTlvHdcMgr_.reset();
    tpMgr_.reset();

    initialized_.store(false);
    HCCL_INFO("[HdcProcess][%s] deinitialized", __func__);
    return HCCL_SUCCESS;
}

HcclResult HdcProcess::GetRdmaHandleManager(void*& manager)
{
    (void)manager;
    return HCCL_SUCCESS;
}

HcclResult HdcProcess::GetHccpHdcManager(void*& manager)
{
    (void)manager;
    return HCCL_SUCCESS;
}

size_t HdcProcess::GetRdmaHandleCount() const
{
    return 0;
}

} // namespace hcomm
