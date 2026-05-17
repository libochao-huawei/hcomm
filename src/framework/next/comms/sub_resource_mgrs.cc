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
#include "hcomm_c_adpt.h"

namespace hcomm {

EndpointsMgr::~EndpointsMgr()
{
    for (const auto &kv : endpointMemMap_) {
        const EndpointHandle &endpointHandle = kv.first;
        const std::vector<MemHandle> &memHandleVec = kv.second;

        for (auto memHandle : memHandleVec) {
            (void)HcommMemUnreg(endpointHandle, memHandle);
        }
    }

    for (const auto &kv : endpointMap_) {
        const EndpointHandle &endpointHandle = kv.second;
        (void)HcommEndpointDestroy(endpointHandle);
    }
}

HcclResult EndpointsMgr::Get(EndpointDesc epDesc, EndpointHandle &handle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto iterPtr = endpointMap_.find(epDesc);
    if (iterPtr != endpointMap_.end()) {
        handle = iterPtr->second;
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[EndpointsMgr::Get] create Endpoint");
    CHK_RET(static_cast<HcclResult>(HcommEndpointCreate(&epDesc, &handle)));

    endpointMap_.emplace(epDesc, handle);
    endpointCount_++;
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = endpointMap_.begin();
    while (it != endpointMap_.end()) {
        if (it->second == handle) {
            endpointMap_.erase(it);
            endpointCount_--;
            return HCCL_SUCCESS;
        }
        ++it;
    }
    return HCCL_E_NOT_FOUND;
}

HcclResult EndpointsMgr::DestroyAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    endpointMap_.clear();
    endpointCount_ = 0;
    return HCCL_SUCCESS;
}

HcclResult EndpointsMgr::DestroyAllMem()
{
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    threads_.clear();
    threadHandleMap_.clear();
    threadCount_ = 0;
    return HCCL_SUCCESS;
}

size_t ThreadsMgr::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return threads_.size();
}

size_t ThreadsMgr::GetThreadCount() const
{
    return threadCount_.load();
}

bool ThreadsMgr::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return threads_.empty();
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
