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

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "hccl_types.h"
#include "hcomm_res_defs.h"

namespace hcomm {

class Endpoint;
class Channel;
struct MemHandle;

class EndpointsMgr {
public:
    EndpointsMgr() = default;
    ~EndpointsMgr();

    HcclResult Get(EndpointDesc epDesc, EndpointHandle& handle);
    HcclResult RegisterMemory(EndpointHandle epHandle,
        const std::vector<std::string>& memTag,
        const std::vector<HcclMem>& memVec,
        std::vector<MemHandle>& memHandleVec);
    HcclResult Destroy(EndpointHandle handle);
    HcclResult DestroyAll();
    HcclResult DestroyAllMem();
    size_t Size() const;
    size_t GetEndpointCount() const;
    size_t GetMemHandleCount() const;
    bool IsEmpty() const;

    Endpoint* FindEndpointByHandle(EndpointHandle handle);
    HcclResult RemoveOwnedEndpoint(EndpointHandle handle);
    void InstallOwnedEndpoint(EndpointHandle handle, std::unique_ptr<Endpoint> endpoint);

private:
    std::unordered_map<EndpointHandle, std::unique_ptr<Endpoint>> endpointObjMap_;
    std::unordered_map<EndpointHandle, std::vector<MemHandle>> endpointMemMap_;
    mutable std::mutex mutex_;
    std::atomic<size_t> endpointCount_{0};
    std::atomic<size_t> memHandleCount_{0};
};

class ChannelsMgr {
public:
    friend class ChannelProcess;

    ChannelsMgr() = default;
    ~ChannelsMgr() = default;

    HcclResult AddChannel(ChannelHandle handle, std::unique_ptr<Channel> channel);
    HcclResult RemoveChannel(ChannelHandle handle);
    HcclResult FindChannel(ChannelHandle handle, Channel** channel);
    HcclResult Destroy(const ChannelHandle* channels, uint32_t channelNum);
    HcclResult DestroyAll();
    size_t Size() const { return channelCount_.load(); }
    size_t GetChannelCount() const { return channelCount_.load(); }
    bool IsEmpty() const { return channelCount_.load() == 0; }

private:
    std::unordered_map<ChannelHandle, std::unique_ptr<Channel>> channelMap_;
    mutable std::mutex mutex_;
    std::atomic<size_t> channelCount_{0};
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

#ifndef CCL_KERNEL_AICPU
    HcclResult SaveThreadsBatch(const std::vector<std::shared_ptr<hccl::Thread>>& newThreads);
    HcclResult FillThreadD2HMapBatch(ThreadHandle* deviceThreadHandles,
        ThreadHandle* hostThreadHandles, uint32_t listNum);
    HcclResult StoreThreadHandlesBatch(std::vector<std::shared_ptr<hccl::Thread>>& newThreads,
        ThreadHandle* threads, CommEngine engine, aclrtBinHandle binHandle);
    HcclResult FreeThreadHandlesBatch(const ThreadHandle* threads, uint32_t threadNum,
        std::vector<ThreadHandle>& deviceHandles);
#endif
    HcclResult RegisterStreamThread(ThreadHandle handle,
        std::shared_ptr<hccl::Thread> threadPtr);

private:
#ifndef CCL_KERNEL_AICPU
    std::unordered_map<ThreadHandle, std::shared_ptr<hccl::Thread>> threadMap_;
    std::unordered_map<DeviceThreadKey, ThreadHandle, DeviceThreadKeyHash> threadD2HMap_;
#endif
    mutable std::mutex mutex_;
    std::atomic<size_t> threadCount_{0};
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

} // namespace hcomm

#endif // SUB_RESOURCE_MGRS_H