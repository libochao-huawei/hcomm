/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NOTIFY_POOL_IMPL_H
#define NOTIFY_POOL_IMPL_H

#include <mutex>
#include <map>
#include "hccl/base.h"
#include "mem_name_repository_pub.h"
#include "local_ipc_notify.h"
#include "remote_notify.h"
#include "dispatcher.h"

namespace hccl {

using NotifyPoolIPCSub = std::vector<std::shared_ptr<LocalIpcNotify>>;
using NotifyPoolNoIPCSub = std::vector<std::shared_ptr<LocalIpcNotify>>;

using NotifyPoolIndicator = struct NotifyPoolIndicatorDef {
    std::map<s64, u32> notifyPoolIPC;       // key: remote, value: index
    std::map<s64, u32> notifyPoolNoIPC;     // key: remote, value: index
};

class NotifyPoolImpl {
public:
    explicit NotifyPoolImpl(const s32 devicePhyId);
    ~NotifyPoolImpl();
    HcclResult Init();
    HcclResult Destroy();
    HcclResult RegisterOp(const std::string &tag);
    HcclResult UnregisterOp(const std::string &tag);
    // local notify申请
    HcclResult Alloc(const std::string &tag, const RemoteRankInfo &info, const NotifyLoadType type,
        std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize);

    HcclResult ResetNotify();
    HcclResult ResetNotifyForDestRank(s64 destRank);
private:
    HcclResult CreateNotify(std::shared_ptr<LocalIpcNotify> &localNotify, const s32 localDeviceId,
        const s32 remoteDeviceId, const NotifyLoadType type, bool withIpc = false,  s64 recvId = -1,
        u32 offsetAlignSize = INVALID_UINT);

    HcclResult AllocIpc(const std::string &tag, s64 remote, s64 recvId,
        const s32 localDeviceId, const s32 remoteDeviceId, const NotifyLoadType type,
        std::shared_ptr<LocalIpcNotify> &localNotify, std::mutex &registeredOpMapMutex,
        std::map<std::string, NotifyPoolIndicator> &registeredOpMap, std::mutex &notifyPoolIPCAsignedMapMutex,
        std::map<s64, NotifyPoolIPCSub> &notifyPoolIPCAsignedMap, u32 offsetAlignSize);
    HcclResult Alloc(const std::string &tag, s64 remote, s64 recvId, const s32 localDeviceId,
        const s32 remoteDeviceId, const NotifyLoadType type, std::shared_ptr<LocalIpcNotify> &localNotify,
        u32 offsetAlignSize);

    HcclResult AllocNoIpc(const std::string &tag, s64 remote, const s32 deviceId,
        const NotifyLoadType type, std::shared_ptr<LocalIpcNotify> &localNotify, std::mutex &registeredOpMapMutex,
        std::map<std::string, NotifyPoolIndicator> &registeredOpMap, std::mutex &notifyPoolNoIPCAsignedMapMutex,
        std::map<s64, NotifyPoolNoIPCSub> &notifyPoolNoIPCAsignedMap, u32 offsetAlignSize);
    HcclResult Alloc(const std::string &tag, s64 remote, const s32 deviceId, const NotifyLoadType type,
        std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize);
    HcclResult IsNotifyOffsetAligned(std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize, bool &isAligned);

    HcclResult DestroyRegisteredOpMap();
    HcclResult DestroyNotifyPoolIPCAsignedMap();
    HcclResult DestroyNotifyPoolNoIPCAsignedMap();
    HcclResult DestroyNotifyPoolIPCAsignedMapForA2A();
    HcclResult DestroyNotifyPoolNoIPCAsignedMapForA2A();
    HcclResult DestroyNotifyPoolDeviceIPCAsignedMap();
    HcclResult DestroyNotifyPoolDeviceNoIPCAsignedMap();
    HcclResult DestroyNotifyPoolDeviceIPCAsignedMapForA2A();
    HcclResult DestroyNotifyPoolDeviceNoIPCAsignedMapForA2A();
    HcclResult RegisterOpMap(const std::string &tag, std::map<std::string, NotifyPoolIndicator> &registeredOpMap);
    HcclResult UnregisterOpMap(const std::string &tag, std::map<std::string, NotifyPoolIndicator> &registeredOpMap);
    HcclResult DestroyNotify(std::shared_ptr<LocalIpcNotify> &localNotify);
    std::mutex notifyPoolIPCAsignedMutex_;
    std::mutex notifyPoolNoIPCAsignedMutex_;
    std::mutex registeredOpMapMutex_;
    std::mutex hcclSignalPoolAsignedMutex_;
    std::map<std::string, NotifyPoolIndicator> registeredOpMap_;
    std::map<s64, NotifyPoolIPCSub> notifyPoolIPCAsignedMap_;
    std::map<s64, NotifyPoolNoIPCSub> notifyPoolNoIPCAsignedMap_;
    std::map<s64, NotifyPoolIPCSub> notifyPoolDeivceIPCAsignedMap_;
    std::map<s64, NotifyPoolNoIPCSub> notifyPoolDeivceNoIPCAsignedMap_;

    // notify pool for alltoall、alltoallv、alltoallvc
    std::mutex notifyPoolIPCAsignedMutexForA2A_;
    std::mutex notifyPoolNoIPCAsignedMutexForA2A_;
    std::mutex registeredOpMapMutexForA2A_;
    std::map<std::string, NotifyPoolIndicator> registeredOpMapForA2A_;
    std::map<s64, NotifyPoolIPCSub> notifyPoolIPCAsignedMapForA2A_;
    std::map<s64, NotifyPoolNoIPCSub> notifyPoolNoIPCAsignedMapForA2A_;
    std::map<s64, NotifyPoolIPCSub> notifyPoolDevIPCAsignedMapForA2A_;
    std::map<s64, NotifyPoolNoIPCSub> notifyPoolDevNoIPCAsignedMapForA2A_;
    s32 devicePhyId_;
    s32 pid_;
};
}  // namespace hccl
#endif /* NOTIFY_POOL_IMPL_H */
