/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef SUB_RESOURCE_MGRS_H
#define SUB_RESOURCE_MGRS_H

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

#include "hccl_types.h"
#include "endpoint.h"
#include "endpoint_pair.h"
#include "channel.h"
#include "hccl_mem_defs.h"

namespace hcomm {

class EndpointsMgr {
public:
    EndpointsMgr() = default;
    ~EndpointsMgr();

    HcclResult Get(EndpointDesc epDesc, EndpointHandle& handle);
    HcclResult RegisterMemory(EndpointHandle epHandle, const std::vector<std::string>& memTag,
        const std::vector<HcclMem>& memVec, std::vector<MemHandle>& memHandleVec);
    HcclResult GetAllRegisteredMemory(EndpointHandle epHandle, std::vector<MemHandle>& memHandleVec);
    HcclResult Destroy(EndpointHandle handle);
    HcclResult DestroyAll();
    HcclResult DestroyAllMem();
    size_t Size() const;
    size_t GetEndpointCount() const;
    size_t GetMemHandleCount() const;
    bool IsEmpty() const;

private:
    HcclResult AddMemHandle(EndpointHandle endpointHandle, const std::vector<MemHandle>& memHandleVec);
    bool IsMemExist(EndpointHandle epHandle);
    bool IsDescExist(EndpointDesc epDesc);

private:
    std::unordered_map<EndpointDesc, EndpointHandle> endpointMap_{};
    std::unordered_map<EndpointHandle, std::vector<MemHandle>> endpointMemMap_{};
    mutable std::mutex mutex_{};
    std::atomic<size_t> endpointCount_{0};
    std::atomic<size_t> memHandleCount_{0};
};

class ChannelsMgr {
public:
    ChannelsMgr() = default;
    ~ChannelsMgr() = default;

    HcclResult Create(EndpointHandle endpointHandle, CommEngine engine,
        HcommChannelDesc* channelDescs, uint32_t channelNum,
        ChannelHandle* outHandles);
    HcclResult Destroy(const ChannelHandle* channels, uint32_t channelNum);
    HcclResult DestroyAll();
    HcclResult Get(ChannelHandle handle, Channel** channel);
    size_t Size() const;
    size_t GetChannelCount() const;
    bool IsEmpty() const;

private:
    std::unordered_map<ChannelHandle, std::unique_ptr<Channel>> channelMap_{};
    std::unordered_map<ChannelHandle, std::vector<MemHandle>> channelMemMap_{};
    mutable std::mutex mutex_{};
    std::atomic<size_t> channelCount_{0};
};

struct DeviceThreadKey {
    int32_t deviceId;
    ThreadHandle handle;

    bool operator==(const DeviceThreadKey& other) const {
        return deviceId == other.deviceId && handle == other.handle;
    }
};

struct DeviceThreadKeyHash {
    std::size_t operator()(const DeviceThreadKey& key) const {
        return std::hash<int32_t>()(key.deviceId) ^
               (std::hash<ThreadHandle>()(key.handle) << 1);
    }
};

class ThreadsMgr {
public:
    ThreadsMgr() = default;
    ~ThreadsMgr() = default;

    HcclResult Alloc(CommEngine engine, uint32_t threadNum,
        const uint32_t* notifyNumPerThread, ThreadHandle* threads);
    HcclResult Free(const ThreadHandle* threads, uint32_t threadNum);
    HcclResult DestroyAll();
    size_t Size() const;
    size_t GetThreadCount() const;
    bool IsEmpty() const;

private:
    std::vector<std::shared_ptr<hccl::Thread>> threads_{};
    std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> threadHandleMap_{};
    mutable std::mutex mutex_{};
    std::atomic<size_t> threadCount_{0};
};

class CcuInstancesMgr {
public:
    CcuInstancesMgr() = default;
    ~CcuInstancesMgr() = default;

    HcclResult GetCcuInstance(int32_t deviceLogicId, void*& ccu);
    HcclResult DestroyAll();
    size_t GetCcuInstanceCount() const;
    bool IsEmpty() const;

private:
    std::unordered_map<int32_t, std::unique_ptr<void, std::function<void(void*)>>> ccuInstances_{};
    mutable std::mutex mutex_{};
    std::atomic<size_t> ccuInstanceCount_{0};
};

class HdcProcess {
public:
    HdcProcess() = default;
    ~HdcProcess();

    HcclResult Init();
    HcclResult Deinit();
    HcclResult GetRdmaHandleManager(void*& manager);
    HcclResult GetHccpHdcManager(void*& manager);
    bool IsInitialized() const { return initialized_.load(); }
    size_t GetRdmaHandleCount() const;

private:
    std::unique_ptr<void> rdmaHandleMgr_;
    std::unique_ptr<void> hccpHdcMgr_;
    std::unique_ptr<void> hccpPeerMgr_;
    std::unique_ptr<void> hccpTlvHdcMgr_;
    std::unique_ptr<void> tpMgr_;
    std::atomic<bool> initialized_{false};
    mutable std::mutex mutex_{};
};

} // namespace hcomm

#endif // SUB_RESOURCE_MGRS_H
