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
#include "hccl_common.h"
#include "endpoint.h"
#include "thread.h"
#include <cstring>

namespace hcomm {

namespace {

bool IsEndpointDescEqual(const EndpointDesc& lhs, const EndpointDesc& rhs)
{
    if (lhs.loc.locType != rhs.loc.locType) {
        return false;
    }

    if (lhs.loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        if (lhs.loc.device.devPhyId != rhs.loc.device.devPhyId) {
            return false;
        }
    } else if (lhs.loc.locType == ENDPOINT_LOC_TYPE_HOST) {
        if (lhs.loc.host.id != rhs.loc.host.id) {
            return false;
        }
    }

    if (lhs.protocol != rhs.protocol) {
        return false;
    }

    if (lhs.commAddr.type != rhs.commAddr.type) {
        return false;
    }

    if (lhs.commAddr.type == COMM_ADDR_TYPE_IP_V4) {
        return lhs.commAddr.addr.s_addr == rhs.commAddr.addr.s_addr;
    } else if (lhs.commAddr.type == COMM_ADDR_TYPE_ID) {
        return lhs.commAddr.id == rhs.commAddr.id;
    }

    return memcmp(lhs.commAddr.raws, rhs.commAddr.raws, sizeof(lhs.commAddr.raws)) == 0;
}

} // anonymous namespace

EndpointsMgr::~EndpointsMgr()
{
    HCCL_INFO("[EndpointsMgr][%s] destroying, endpointCount_=%zu", __func__, endpointCount_.load());
    DestroyAll();
    DestroyAllMem();
}

HcclResult EndpointsMgr::Get(EndpointDesc epDesc, EndpointHandle& handle)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& pair : endpointObjMap_) {
        const Endpoint* ep = pair.second.get();
        if (ep != nullptr && IsEndpointDescEqual(ep->GetEndpointDesc(), epDesc)) {
            handle = pair.first;
            return HCCL_SUCCESS;
        }
    }

    return HCCL_E_NOT_FOUND;
}

HcclResult EndpointsMgr::RegisterMemory(EndpointHandle epHandle,
    const std::vector<std::string>& memTag,
    const std::vector<HcclMem>& memVec,
    std::vector<MemHandle>& memHandleVec)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (epHandle == 0 || memVec.empty()) {
        HCCL_ERROR("[EndpointsMgr][%s] invalid parameters", __func__);
        return HCCL_E_PARA;
    }

    auto it = endpointObjMap_.find(epHandle);
    if (it == endpointObjMap_.end()) {
        HCCL_WARNING("[EndpointsMgr][%s] endpoint not found, epHandle=0x%llx", __func__, epHandle);
        return HCCL_E_NOT_FOUND;
    }

    std::vector<MemHandle> handles;
    handles.reserve(memVec.size());
    for (size_t i = 0; i < memVec.size(); ++i) {
        MemHandle handle = reinterpret_cast<MemHandle>(memVec[i].addr);
        handles.push_back(handle);
    }

    endpointMemMap_[epHandle] = handles;
    memHandleVec = std::move(handles);
    memHandleCount_ += memVec.size();

    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::Destroy(EndpointHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = endpointObjMap_.find(handle);
    if (it != endpointObjMap_.end()) {
        endpointObjMap_.erase(it);
        endpointCount_--;
        return HCCL_SUCCESS;
    }
    return HCCL_E_NOT_FOUND;
}

HcclResult EndpointsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    HCCL_INFO("[EndpointsMgr][%s] destroying all endpoints, count=%zu", __func__, endpointCount_.load());
    endpointObjMap_.clear();
    endpointCount_ = 0;
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::DestroyAllMem()
{
    std::lock_guard<std::mutex> lock(mutex_);
    HCCL_INFO("[EndpointsMgr][%s] destroying all mem handles, count=%zu", __func__, memHandleCount_.load());
    endpointMemMap_.clear();
    memHandleCount_ = 0;
    return HCCL_SUCCESS;
}

size_t EndpointsMgr::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return endpointObjMap_.size();
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
    return endpointObjMap_.empty();
}

Endpoint* EndpointsMgr::FindEndpointByHandle(EndpointHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = endpointObjMap_.find(handle);
    if (it != endpointObjMap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

HcclResult EndpointsMgr::RemoveOwnedEndpoint(EndpointHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = endpointObjMap_.find(handle);
    if (it == endpointObjMap_.end()) {
        return HCCL_E_NOT_FOUND;
    }

    auto memIt = endpointMemMap_.find(handle);
    if (memIt != endpointMemMap_.end()) {
        endpointMemMap_.erase(memIt);
    }

    endpointObjMap_.erase(it);
    endpointCount_--;
    return HCCL_SUCCESS;
}

void EndpointsMgr::InstallOwnedEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint)
{
    std::lock_guard<std::mutex> lock(mutex_);
    endpointObjMap_.emplace(handle, std::move(endpoint));
    endpointCount_++;
}

HcclResult ChannelsMgr::AddChannel(ChannelHandle handle, std::unique_ptr<Channel> channel)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (channelMap_.find(handle) != channelMap_.end()) {
        HCCL_ERROR("[ChannelsMgr][%s] channel handle already exists [0x%llx]", __func__, handle);
        return HCCL_E_INTERNAL;
    }

    channelMap_.emplace(handle, std::move(channel));
    channelCount_++;
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::RemoveChannel(ChannelHandle handle)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = channelMap_.find(handle);
    if (it == channelMap_.end()) {
        return HCCL_E_NOT_FOUND;
    }

    channelMap_.erase(it);
    channelCount_--;
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::FindChannel(ChannelHandle handle, Channel** channel)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = channelMap_.find(handle);
    if (it == channelMap_.end()) {
        *channel = nullptr;
        return HCCL_E_NOT_FOUND;
    }

    *channel = it->second.get();
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::Destroy(const ChannelHandle* channels, uint32_t channelNum)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (uint32_t i = 0; i < channelNum; i++) {
        auto it = channelMap_.find(channels[i]);
        if (it != channelMap_.end()) {
            channelMap_.erase(it);
            channelCount_--;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ChannelsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    HCCL_INFO("[ChannelsMgr][%s] destroying all channels, count=%zu", __func__, channelCount_.load());
    channelMap_.clear();
    channelCount_ = 0;
    return HCCL_SUCCESS;
}

ThreadsMgr::~ThreadsMgr() = default;

HcclResult ThreadsMgr::Alloc(CommEngine engine, uint32_t threadNum,
    const uint32_t* notifyNumPerThread, ThreadHandle* threads)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (threads == nullptr) {
        HCCL_ERROR("[ThreadsMgr][%s] threads is null", __func__);
        return HCCL_E_PARA;
    }

    if (threadMap_.size() < threadNum) {
        HCCL_WARNING("[ThreadsMgr][%s] not enough threads, available=%zu, requested=%u",
            __func__, threadMap_.size(), threadNum);
        return HCCL_E_NOT_FOUND;
    }

    (void)engine;
    (void)notifyNumPerThread;

    size_t idx = 0;
    for (auto& pair : threadMap_) {
        if (idx >= threadNum) {
            break;
        }
        threads[idx] = pair.first;
        idx++;
    }

    HCCL_INFO("[ThreadsMgr][%s] allocated %u threads", __func__, threadNum);
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::Free(const ThreadHandle* threads, uint32_t threadNum)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (threads == nullptr || threadNum == 0) {
        HCCL_ERROR("[ThreadsMgr][%s] invalid parameters", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < threadNum; ++i) {
        auto it = threadMap_.find(threads[i]);
        if (it != threadMap_.end()) {
            threadMap_.erase(it);
            threadCount_--;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    HCCL_INFO("[ThreadsMgr][%s] destroying all threads, count=%zu", __func__, threadCount_.load());
#ifndef CCL_KERNEL_AICPU
    threadMap_.clear();
    threadD2HMap_.clear();
#endif
    threadCount_ = 0;
    return HCCL_SUCCESS;
}

size_t ThreadsMgr::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef CCL_KERNEL_AICPU
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
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef CCL_KERNEL_AICPU
    return threadMap_.empty();
#else
    return true;
#endif
}

#ifndef CCL_KERNEL_AICPU
HcclResult ThreadsMgr::SaveThreadsBatch(
    const std::vector<std::shared_ptr<hccl::Thread>>& newThreads)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& threadPtr : newThreads) {
        ThreadHandle handle = reinterpret_cast<ThreadHandle>(threadPtr.get());

        if (threadMap_.find(handle) != threadMap_.end()) {
            HCCL_ERROR("[%s] thread handle already exists [0x%llx]", __func__, handle);
            return HCCL_E_INTERNAL;
        }
        DeviceThreadKey key{deviceId, handle};
        if (threadD2HMap_.find(key) != threadD2HMap_.end()) {
            HCCL_ERROR("[%s] thread handle already exists in D2H map", __func__);
            return HCCL_E_INTERNAL;
        }

        threadMap_.emplace(handle, threadPtr);
        threadD2HMap_.emplace(key, handle);
        threadCount_++;
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::FillThreadD2HMapBatch(ThreadHandle* deviceThreadHandles,
    ThreadHandle* hostThreadHandles, uint32_t listNum)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t idx = 0; idx < listNum; idx++) {
        auto deviceThreadHandle = deviceThreadHandles[idx];
        auto hostThreadHandle = hostThreadHandles[idx];
        DeviceThreadKey key{deviceId, deviceThreadHandle};
        threadD2HMap_.emplace(key, hostThreadHandle);
    }
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::StoreThreadHandlesBatch(
    std::vector<std::shared_ptr<hccl::Thread>>& newThreads,
    ThreadHandle* threads, CommEngine engine, aclrtBinHandle binHandle)
{
    CHK_PTR_NULL(threads);

    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < newThreads.size(); ++i) {
        const auto& threadPtr = newThreads[i];
        ThreadHandle handle = reinterpret_cast<ThreadHandle>(threadPtr.get());

        if (threadMap_.find(handle) != threadMap_.end()) {
            HCCL_ERROR("[ThreadsMgr][%s] thread handle already exists [0x%llx]", __func__, handle);
            return HCCL_E_INTERNAL;
        }

        DeviceThreadKey key{deviceId, handle};
        if (threadD2HMap_.find(key) != threadD2HMap_.end()) {
            HCCL_ERROR("[ThreadsMgr][%s] thread handle already exists in D2H map", __func__);
            return HCCL_E_INTERNAL;
        }

        threadMap_.emplace(handle, threadPtr);
        threadD2HMap_.emplace(key, handle);
        threadCount_++;

        if (threads != nullptr) {
            threads[i] = handle;
        }
    }
    (void)engine;
    (void)binHandle;
    return HCCL_SUCCESS;
}

HcclResult ThreadsMgr::FreeThreadHandlesBatch(
    const ThreadHandle* threads, uint32_t threadNum,
    std::vector<ThreadHandle>& deviceHandles)
{
    int32_t deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));

    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < threadNum; ++i) {
        const ThreadHandle inHandle = threads[i];

        DeviceThreadKey key{deviceId, inHandle};
        auto itH = threadD2HMap_.find(key);
        if (itH == threadD2HMap_.end()) {
            return HCCL_E_NOT_FOUND;
        }
        const ThreadHandle mappedHandle = itH->second;

        auto itC = threadMap_.find(mappedHandle);
        if (itC == threadMap_.end()) {
            return HCCL_E_NOT_FOUND;
        }

        if (inHandle != mappedHandle) {
            deviceHandles.push_back(inHandle);
        }

        threadMap_.erase(itC);
        for (auto it = threadD2HMap_.begin(); it != threadD2HMap_.end();) {
            if (it->second == mappedHandle && it->first.deviceId == deviceId) {
                it = threadD2HMap_.erase(it);
            } else {
                ++it;
            }
        }
        threadCount_--;
    }
    return HCCL_SUCCESS;
}
#endif

HcclResult ThreadsMgr::RegisterStreamThread(ThreadHandle handle,
    std::shared_ptr<hccl::Thread> threadPtr)
{
    std::lock_guard<std::mutex> lock(mutex_);
#ifndef CCL_KERNEL_AICPU
    threadMap_.emplace(handle, threadPtr);
    threadCount_++;
#endif
    return HCCL_SUCCESS;
}

} // namespace hcomm