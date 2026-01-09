/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <algorithm>
#include "device_capacity.h"
#include "sal_pub.h"
#include "adapter_hal.h"
#include "dlhal_function.h"
#include "adapter_rts.h"
#include "notify_pool_impl.h"

namespace hccl {
const std::string HCCL_ALLTOALL = "ALLTOALL";

// notify的offset对齐标准
constexpr u32 NOTIFY_OFFSET_ALIGN_EIGHT = 8;
constexpr u32 NOTIFY_OFFSET_ALIGN_FOUR = 4;

NotifyPoolImpl::NotifyPoolImpl(const s32 devicePhyId)
    : devicePhyId_(devicePhyId)
{
}

NotifyPoolImpl::~NotifyPoolImpl()
{
    HcclResult ret = Destroy();
    if (ret != HCCL_SUCCESS) {
        HCCL_WARNING("destroy NotifyPoolImpl resources failed, ret[%d]", ret);
    }
}

HcclResult NotifyPoolImpl::Init()
{
#ifndef HCCD
        CHK_RET(SalGetBareTgid(&pid_)); // 当前进程id
#else
        s32 psPid = 0;
        hrtDrvDeviceGetBareTgid(psPid);
        pid_ = psPid;
#endif
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::Destroy()
{
    HCCL_INFO("NotifyPoolImpl Destroy.");
    CHK_RET(DestroyRegisteredOpMap());

    CHK_RET(DestroyNotifyPoolIPCAsignedMapForA2A());
    CHK_RET(DestroyNotifyPoolDeviceIPCAsignedMapForA2A());
    CHK_RET(DestroyNotifyPoolIPCAsignedMap());
    CHK_RET(DestroyNotifyPoolDeviceIPCAsignedMap());

    CHK_RET(DestroyNotifyPoolNoIPCAsignedMapForA2A());
    CHK_RET(DestroyNotifyPoolDeviceNoIPCAsignedMapForA2A());
    CHK_RET(DestroyNotifyPoolNoIPCAsignedMap());
    CHK_RET(DestroyNotifyPoolDeviceNoIPCAsignedMap());

    HCCL_INFO("NotifyPoolImpl Destroy success.");

    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::CreateNotify(std::shared_ptr<LocalIpcNotify> &localNotify, const s32 localDeviceId,
    const s32 remoteDeviceId, const NotifyLoadType type, bool withIpc, s64 recvId, u32 offsetAlignSize)
{
    std::vector<std::shared_ptr<LocalIpcNotify>> tmpNotifys;
    HcclResult ret = HCCL_SUCCESS;
    bool errorFlag = false;
    while (true) {
        std::shared_ptr<LocalIpcNotify> tmpNotify;
        EXECEPTION_CATCH((tmpNotify = std::make_shared<LocalIpcNotify>()), errorFlag = true);
        CHK_PRT_BREAK(!(tmpNotify) || errorFlag, HCCL_ERROR("[NotifyPoolImpl][CreateNotify]create notify failed, "
            "errorFlag[%d]", errorFlag), errorFlag = true);

        ret = tmpNotify->Init(localDeviceId, remoteDeviceId, type);
        CHK_PRT_BREAK(ret != HCCL_SUCCESS, HCCL_ERROR("[NotifyPoolImpl][CreateNotify]localNotify init failed, "
            "ret[%d]", ret), errorFlag = true);

        // 申请到的notify offset不满足要求，再次申请
        bool isAligned = false;
        ret = IsNotifyOffsetAligned(tmpNotify, offsetAlignSize, isAligned);
        CHK_PRT_BREAK(ret != HCCL_SUCCESS, HCCL_ERROR("[NotifyPoolImpl][CreateNotify]IsNotifyOffsetAligned failed, "
            "ret[%d]", ret), errorFlag = true);
        if (!isAligned) {
            tmpNotifys.push_back(tmpNotify);
            HCCL_DEBUG("CreateNotify id[%u] offset[%llu] is not support atomic write, create again",
                tmpNotify->notifyId_, tmpNotify->offset);
            continue;
        }

        localNotify = tmpNotify;
        HCCL_DEBUG("withIpc[%d], Is310PDevice[%d] recvId[%lld]", withIpc, Is310PDevice(), recvId);
        if (withIpc || Is310PDevice()) {
            ret = localNotify->SetIpc();
            CHK_PRT_BREAK(ret != HCCL_SUCCESS, HCCL_ERROR("[NotifyPoolImpl][CreateNotify]localNotify set ipc failed, "
                "ret[%d]", ret), errorFlag = true);
            }

        if (withIpc) {
            HCCL_DEBUG("withIpc[%d], Is310PDevice[%d] recvId[%lld]", withIpc, Is310PDevice(), recvId);
            ret = localNotify->Grant(recvId);
            CHK_PRT_BREAK(ret != HCCL_SUCCESS, HCCL_ERROR("[NotifyPoolImpl][CreateNotify]localNotify grant failed, "
                "ret[%d]", ret), errorFlag = true);
        }
        break;
    }

    tmpNotifys.clear(); // 释放不满足要求的notify
    if (errorFlag) {
        HCCL_ERROR("[NotifyPoolImpl][CreateNotify]localNotify create failed ,ret[%d]", ret);
        localNotify = nullptr;
        return ret;
    }

    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolIPCAsignedMap()
{
    std::unique_lock<std::mutex> lock(notifyPoolIPCAsignedMutex_);

    for (auto iter = notifyPoolIPCAsignedMap_.begin(); iter != notifyPoolIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolIPCAsignedMap_.clear();
    HCCL_DEBUG("destroy assigned notify pool(ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolDeviceIPCAsignedMap()
{
    std::unique_lock<std::mutex> lock(notifyPoolIPCAsignedMutex_);

    for (auto iter = notifyPoolDeivceIPCAsignedMap_.begin(); iter != notifyPoolDeivceIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolDeivceIPCAsignedMap_.clear();
    HCCL_DEBUG("destroy assigned notify pool(device ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolIPCAsignedMapForA2A()
{
    std::unique_lock<std::mutex> lock(notifyPoolIPCAsignedMutexForA2A_);

    for (auto iter = notifyPoolIPCAsignedMapForA2A_.begin(); iter != notifyPoolIPCAsignedMapForA2A_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolIPCAsignedMapForA2A_.clear();
    HCCL_DEBUG("for a2a destroy assigned notify pool(ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolDeviceIPCAsignedMapForA2A()
{
    std::unique_lock<std::mutex> lock(notifyPoolIPCAsignedMutexForA2A_);

    for (auto iter = notifyPoolDevIPCAsignedMapForA2A_.begin(); iter != notifyPoolDevIPCAsignedMapForA2A_.end();
         iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolDevIPCAsignedMapForA2A_.clear();
    HCCL_DEBUG("for a2a destroy assigned notify pool(ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolNoIPCAsignedMap()
{
    std::unique_lock<std::mutex> lock(notifyPoolNoIPCAsignedMutex_);
    for (auto iter = notifyPoolNoIPCAsignedMap_.begin(); iter != notifyPoolNoIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolNoIPCAsignedMap_.clear();
    HCCL_DEBUG("destroy assigned notify pool(no ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolDeviceNoIPCAsignedMap()
{
    std::unique_lock<std::mutex> lock(notifyPoolNoIPCAsignedMutex_);
    for (auto iter = notifyPoolDeivceNoIPCAsignedMap_.begin(); iter != notifyPoolDeivceNoIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolDeivceNoIPCAsignedMap_.clear();
    HCCL_DEBUG("destroy assigned notify pool(device no ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolNoIPCAsignedMapForA2A()
{
    std::unique_lock<std::mutex> lock(notifyPoolNoIPCAsignedMutexForA2A_);
    for (auto iter = notifyPoolNoIPCAsignedMapForA2A_.begin(); iter != notifyPoolNoIPCAsignedMapForA2A_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolNoIPCAsignedMapForA2A_.clear();
    HCCL_DEBUG("for a2a destroy assigned notify pool(no ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotifyPoolDeviceNoIPCAsignedMapForA2A()
{
    std::unique_lock<std::mutex> lock(notifyPoolNoIPCAsignedMutexForA2A_);
    for (auto iter = notifyPoolDevNoIPCAsignedMapForA2A_.begin(); iter != notifyPoolDevNoIPCAsignedMapForA2A_.end();
         iter++) {
        for (auto &it : iter->second) {
            CHK_RET(DestroyNotify(it));
        }
    }
    notifyPoolDevNoIPCAsignedMapForA2A_.clear();
    HCCL_DEBUG("for a2a destroy assigned notify pool(no ipc).");
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyNotify(std::shared_ptr<LocalIpcNotify> &localNotify)
{
    CHK_PTR_NULL(localNotify);
    CHK_RET(localNotify->Destroy());
    return HCCL_SUCCESS;
}
HcclResult NotifyPoolImpl::RegisterOpMap(
    const std::string &tag, std::map<std::string, NotifyPoolIndicator> &registeredOpMap)
{
    auto iterTag = registeredOpMap.find(tag);
    if (iterTag == registeredOpMap.end()) {
        NotifyPoolIndicator indicator;
        registeredOpMap.insert(std::make_pair(tag, indicator));
    } else {
        HCCL_ERROR(
            "[NotifyPoolImpl][RegisterOp]register op to the notify pool failed, tag[%s] has existed", tag.c_str());
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::RegisterOp(const std::string &tag)
{
    std::string upTag = tag;
    std::transform(upTag.begin(), upTag.end(), upTag.begin(), ::toupper);
    bool hasAlltoAll = upTag.find(HCCL_ALLTOALL) != std::string::npos;
    HCCL_INFO("RegisterOp hasAlltoAll[%d]", hasAlltoAll);
    std::mutex &registeredOpMapMutex = hasAlltoAll ? registeredOpMapMutexForA2A_ : registeredOpMapMutex_;
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap = hasAlltoAll ? registeredOpMapForA2A_ :
        registeredOpMap_;

    /* 此处可能会与并发，加锁 */
    std::unique_lock<std::mutex> lock(registeredOpMapMutex);

    CHK_RET(RegisterOpMap(tag, registeredOpMap));
    auto tagDev = "Dev_" + tag;
    CHK_RET(RegisterOpMap(tagDev, registeredOpMap));
    HCCL_INFO("register op[%s] to the notify pool success.", tag.c_str());

    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::UnregisterOpMap(const std::string &tag,
                                         std::map<std::string, NotifyPoolIndicator> &registeredOpMap)
{
    auto iterTag = registeredOpMap.find(tag);
    if (iterTag == registeredOpMap.end()) {
        HCCL_ERROR("[NotifyPoolImpl][UnregisterOp]unregister op from the notify pool failed, tag[%s] has unregistered",
            tag.c_str());
        return HCCL_E_PARA;
    } else {
        registeredOpMap.erase(tag);
    }
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::UnregisterOp(const std::string &tag)
{
    std::string upTag = tag;
    std::transform(upTag.begin(), upTag.end(), upTag.begin(), ::toupper);
    bool hasAlltoAll = upTag.find(HCCL_ALLTOALL) != std::string::npos;
    HCCL_INFO("UnregisterOp hasAlltoAll[%d]", hasAlltoAll);
    std::mutex &registeredOpMapMutex = hasAlltoAll ? registeredOpMapMutexForA2A_ : registeredOpMapMutex_;
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap = hasAlltoAll ? registeredOpMapForA2A_ :
        registeredOpMap_;

    /* 此处可能会与并发，加锁 */
    std::unique_lock<std::mutex> lock(registeredOpMapMutex);

    CHK_RET(UnregisterOpMap(tag, registeredOpMap));
    auto tagDev = "Dev_" + tag;
    CHK_RET(UnregisterOpMap(tagDev, registeredOpMap));
    HCCL_INFO("unregister op[%s] from the notify pool success.", tag.c_str());
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::DestroyRegisteredOpMap()
{
    registeredOpMap_.clear();
    registeredOpMapForA2A_.clear();
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::IsNotifyOffsetAligned(std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize,
    bool &isAligned)
{
    if (offsetAlignSize == INVALID_UINT) {
        isAligned = true;
    } else if (offsetAlignSize == NOTIFY_OFFSET_ALIGN_EIGHT) { // offset按照8byte对齐
        isAligned = (localNotify->offset % NOTIFY_OFFSET_ALIGN_EIGHT == 0);
    } else if (offsetAlignSize == NOTIFY_OFFSET_ALIGN_FOUR) { // offset按照4byte对齐
        isAligned = (localNotify->offset % NOTIFY_OFFSET_ALIGN_FOUR == 0);
    } else {
        HCCL_ERROR("IsNotifyOffsetAligned offsetAlignSize[%u] is invalid, only support 4 or 8", offsetAlignSize);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::AllocIpc(const std::string &tag, s64 remote, s64 recvId,
    const s32 localDeviceId, const s32 remoteDeviceId, const NotifyLoadType type,
    std::shared_ptr<LocalIpcNotify> &localNotify, std::mutex &registeredOpMapMutex,
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap, std::mutex &notifyPoolIPCAsignedMapMutex,
    std::map<s64, NotifyPoolIPCSub> &notifyPoolIPCAsignedMap, u32 offsetAlignSize)
{
    /* 此处可能会与并发，加锁 */
    std::unique_lock<std::mutex> lock(registeredOpMapMutex);
    auto iterTag = registeredOpMap.find(tag);
    CHK_PRT_RET(iterTag == registeredOpMap.end(), HCCL_ERROR("[NotifyPoolImpl][Alloc]tag[%s] is not registered.",
        tag.c_str()), HCCL_E_PARA);
    auto iterIdx = iterTag->second.notifyPoolIPC.find(remote);
    if (iterIdx == iterTag->second.notifyPoolIPC.end()) {
        iterTag->second.notifyPoolIPC.insert({remote, 0});
        iterIdx = iterTag->second.notifyPoolIPC.find(remote);
        CHK_PRT_RET(iterIdx == iterTag->second.notifyPoolIPC.end(), HCCL_ERROR("[NotifyPoolImpl][Alloc]remote[%d] "
            "is not found.", remote), HCCL_E_PARA);
    }

    std::unique_lock<std::mutex> lockIPC(notifyPoolIPCAsignedMapMutex);
    auto iterRemoteDev = notifyPoolIPCAsignedMap.find(remote);
    if (iterRemoteDev != notifyPoolIPCAsignedMap.end()) {
        // 从资源池中遍历获取offset按照要求对齐的notify
        while (iterIdx->second < iterRemoteDev->second.size()) {
            bool isAligned = false;
            CHK_RET(IsNotifyOffsetAligned(iterRemoteDev->second[iterIdx->second], offsetAlignSize, isAligned));
            if (isAligned) {
                break; // notify满足对齐要求，退出循环
            }
            iterIdx->second++; // 继续遍历
        }

        if (iterIdx->second >= iterRemoteDev->second.size()) {
            CHK_RET(CreateNotify(localNotify, localDeviceId, remoteDeviceId, type, true, recvId, offsetAlignSize));

            iterRemoteDev->second.push_back(localNotify);
            HCCL_INFO("create one notify in notify pool(ipc):tag[%s] remote[%d] total[%zu].", tag.c_str(), remote, \
                iterRemoteDev->second.size());
        } else {
            localNotify = iterRemoteDev->second[iterIdx->second];
            CHK_SMART_PTR_NULL(localNotify);
            CHK_RET(localNotify->Grant(recvId));
            HCCL_INFO("create one notify in notify pool(ipc):tag[%s] remote[%d] total[%zu].", tag.c_str(), remote, \
                iterRemoteDev->second.size());
        }
    } else {
        CHK_RET(CreateNotify(localNotify, localDeviceId, remoteDeviceId, type, true, recvId, offsetAlignSize));

        NotifyPoolIPCSub tmpVec{{localNotify}};
        notifyPoolIPCAsignedMap.insert(std::make_pair(remote, tmpVec));
        HCCL_INFO("create one notify in notify pool(ipc):tag[%s] remote[%d] total[%zu].", tag.c_str(), remote, \
            tmpVec.size());
    }
    iterIdx->second++;

    HCCL_INFO("notify pool ipc alloc: tag[%s] remote[%d] used[%u]", tag.c_str(), remote, \
        iterIdx->second);
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::Alloc(const std::string &tag, s64 remote, s64 recvId, const s32 localDeviceId,
    const s32 remoteDeviceId, const NotifyLoadType type, std::shared_ptr<LocalIpcNotify> &localNotify,
    u32 offsetAlignSize)
{
    std::string upTag = tag;
    std::transform(upTag.begin(), upTag.end(), upTag.begin(), ::toupper);
    bool hasAlltoAll = upTag.find(HCCL_ALLTOALL) != std::string::npos;
    HCCL_INFO("Alloc hasAlltoAll[%d]", hasAlltoAll);
    std::mutex &registeredOpMapMutex = hasAlltoAll ? registeredOpMapMutexForA2A_ : registeredOpMapMutex_;
    std::mutex &notifyPoolIPCAsignedMapMutex = hasAlltoAll ? notifyPoolIPCAsignedMutexForA2A_ : notifyPoolIPCAsignedMutex_;
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap =
        hasAlltoAll ? registeredOpMapForA2A_ : registeredOpMap_;
    if (type == NotifyLoadType::HOST_NOTIFY) {
        std::map<s64, NotifyPoolIPCSub> &notifyPoolIPCAsignedMap = hasAlltoAll ? notifyPoolIPCAsignedMapForA2A_ :
        notifyPoolIPCAsignedMap_;
        CHK_RET(AllocIpc(tag, remote, recvId, localDeviceId, remoteDeviceId, type, localNotify,
            registeredOpMapMutex, registeredOpMap, notifyPoolIPCAsignedMapMutex, notifyPoolIPCAsignedMap,
            offsetAlignSize));
    } else if (type == NotifyLoadType::DEVICE_NOTIFY) { // 申请device上使用的notify资源
        std::map<s64, NotifyPoolIPCSub> &notifyPoolIPCAsignedMap = hasAlltoAll ? notifyPoolDevIPCAsignedMapForA2A_ :
        notifyPoolDeivceIPCAsignedMap_;
        auto tagDev = "Dev_" + tag;
        CHK_RET(AllocIpc(tagDev, remote, recvId, localDeviceId, remoteDeviceId, type, localNotify,
            registeredOpMapMutex, registeredOpMap, notifyPoolIPCAsignedMapMutex, notifyPoolIPCAsignedMap,
            offsetAlignSize));
    }

    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::AllocNoIpc(const std::string &tag, s64 remote, const s32 deviceId,
    const NotifyLoadType type, std::shared_ptr<LocalIpcNotify> &localNotify, std::mutex &registeredOpMapMutex,
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap, std::mutex &notifyPoolNoIPCAsignedMapMutex,
    std::map<s64, NotifyPoolNoIPCSub> &notifyPoolNoIPCAsignedMap, u32 offsetAlignSize)
{
    /* 此处可能会与并发，加锁 */
    std::unique_lock<std::mutex> lock(registeredOpMapMutex);

    auto iterTag = registeredOpMap.find(tag);
    CHK_PRT_RET(iterTag == registeredOpMap.end(), HCCL_ERROR("[NotifyPool][Alloc]tag[%s] is not registered.",
        tag.c_str()), HCCL_E_PARA);

    auto iterIdx = iterTag->second.notifyPoolNoIPC.find(remote);
    if (iterIdx == iterTag->second.notifyPoolNoIPC.end()) {
        iterTag->second.notifyPoolNoIPC.insert({remote, 0});
        iterIdx = iterTag->second.notifyPoolNoIPC.find(remote);
        CHK_PRT_RET(iterIdx == iterTag->second.notifyPoolNoIPC.end(), HCCL_ERROR("[NotifyPool][Alloc]remote[%lld] is "\
            "not found.", remote), HCCL_E_PARA);
    }

    std::unique_lock<std::mutex> lockNoIPC(notifyPoolNoIPCAsignedMapMutex);
    auto iterRemoteDev = notifyPoolNoIPCAsignedMap.find(remote);
    if (iterRemoteDev != notifyPoolNoIPCAsignedMap.end()) {
        // 从资源池中获取offset按照要求对齐的notify
        while (iterIdx->second < iterRemoteDev->second.size()) {
            bool isAligned = false;
            CHK_RET(IsNotifyOffsetAligned(iterRemoteDev->second[iterIdx->second], offsetAlignSize, isAligned));
            if (isAligned) {
                break; // notify满足对齐要求，退出循环
            }
            iterIdx->second++; // 继续遍历
        }

        if (iterIdx->second >= iterRemoteDev->second.size()) {
            CHK_RET(CreateNotify(localNotify, deviceId, deviceId, type, false, -1, offsetAlignSize));
            iterRemoteDev->second.push_back(localNotify);
            HCCL_INFO("create one notify in notify pool(no ipc):tag[%s] remote[%lld] total[%zu].",
                tag.c_str(), remote, iterRemoteDev->second.size());
        } else {
            localNotify = iterRemoteDev->second[iterIdx->second];
            CHK_SMART_PTR_NULL(localNotify);
        }
    } else {
        CHK_RET(CreateNotify(localNotify, deviceId, deviceId, type, false, -1, offsetAlignSize));

        NotifyPoolNoIPCSub tmpVec{localNotify};
        notifyPoolNoIPCAsignedMap.insert(std::make_pair(remote, tmpVec));
        HCCL_INFO("create one notify in notify pool(no ipc):tag[%s] remote[%lld] total[%zu].", tag.c_str(), remote,
            tmpVec.size());
    }
    iterIdx->second++;

    HCCL_INFO("notify pool no ipc alloc: tag[%s] remote[%lld] used[%u]", tag.c_str(), remote,
        iterIdx->second);
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::Alloc(const std::string &tag, s64 remote, const s32 deviceId, const NotifyLoadType type,
    std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize)
{
    std::string upTag = tag;
    std::transform(upTag.begin(), upTag.end(), upTag.begin(), ::toupper);
    bool hasAlltoAll = upTag.find(HCCL_ALLTOALL) != std::string::npos;
    HCCL_INFO("Alloc hasAlltoAll[%d]", hasAlltoAll);
    std::mutex &registeredOpMapMutex = hasAlltoAll ? registeredOpMapMutexForA2A_ : registeredOpMapMutex_;
    std::mutex &notifyPoolNoIPCAsignedMapMutex = hasAlltoAll ? notifyPoolNoIPCAsignedMutexForA2A_ : notifyPoolNoIPCAsignedMutex_;
    std::map<std::string, NotifyPoolIndicator> &registeredOpMap =
        hasAlltoAll ? registeredOpMapForA2A_ : registeredOpMap_;
    if (type == NotifyLoadType::HOST_NOTIFY) {
        std::map<s64, NotifyPoolNoIPCSub> &notifyPoolNoIPCAsignedMap = hasAlltoAll ? notifyPoolNoIPCAsignedMapForA2A_ :
            notifyPoolNoIPCAsignedMap_;
        CHK_RET(AllocNoIpc(tag, remote, deviceId, type, localNotify, registeredOpMapMutex, registeredOpMap,
            notifyPoolNoIPCAsignedMapMutex, notifyPoolNoIPCAsignedMap, offsetAlignSize));
    } else if (type == NotifyLoadType::DEVICE_NOTIFY) { // 申请device上使用的notify资源
        std::map<s64, NotifyPoolNoIPCSub> &notifyPoolNoIPCAsignedMap =
            hasAlltoAll ? notifyPoolDevNoIPCAsignedMapForA2A_ : notifyPoolDeivceNoIPCAsignedMap_;
        auto tagDev = "Dev_" + tag;
        CHK_RET(AllocNoIpc(tagDev, remote, deviceId, type, localNotify, registeredOpMapMutex, registeredOpMap,
            notifyPoolNoIPCAsignedMapMutex, notifyPoolNoIPCAsignedMap, offsetAlignSize));
    }
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::Alloc(const std::string &tag, const RemoteRankInfo &info, const NotifyLoadType type,
    std::shared_ptr<LocalIpcNotify> &localNotify, u32 offsetAlignSize)
{
    HCCL_DEBUG("[Alloc][IpcNotify]localPid[%016llx], remotePid[%016llx], localDeviceId[%d], remoteDeviceId[%d], "
        "remoteSdid[%x], offsetAlignSize[%u]", pid_, info.remotePid, devicePhyId_, info.remoteDeviceId, info.remoteSdid,
        offsetAlignSize);
    // 主从流下标为-1、rdma notify下标是remoteRank
    if (pid_ == info.remotePid && devicePhyId_ == info.remoteDeviceId &&
        info.remoteSdid == INVALID_INT) {
        CHK_RET(Alloc(tag, info.remoteRank, info.remoteDeviceId, type, localNotify, offsetAlignSize));
    } else {
        // 统一使用remoteRank作为notifyPoolMap下标区分
        s64 remoteRank = static_cast<s64>(info.remoteRank);
        // 将(s32)SDID和(s32)Pid拼接成s64作为标志位, 高32位为SDID, 低32位为pid
        s64 recvId =
            ((static_cast<s64>(info.remoteSdid) & 0xFFFFFFFF) << 32) | (static_cast<s64>(info.remotePid) & 0xFFFFFFFF);
        HCCL_INFO("[Alloc][IpcNotify]recvSdid[%016llx], recvPid[%016llx], remoteRank[%u]",
            info.remoteSdid, info.remotePid, info.remoteRank);
        CHK_RET(Alloc(tag, remoteRank, recvId, devicePhyId_, info.remoteDeviceId, type, localNotify, offsetAlignSize));
    }

    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::ResetNotifyForDestRank(s64 destRank) {
    std::unique_lock<std::mutex> lockIPC(notifyPoolIPCAsignedMutex_);
    const auto &notifyPoolDeivceIPCAsignedIt = notifyPoolDeivceIPCAsignedMap_.find(destRank);
    if (notifyPoolDeivceIPCAsignedIt == notifyPoolDeivceIPCAsignedMap_.end()) {
        HCCL_RUN_INFO("[ResetNotify]remoteRank[%d] is not in notifyPoolDeivceIPCAsignedMap_", destRank);
    } else {
        HCCL_RUN_INFO("[ResetNotify]reset notifyPoolDeivceIPCAsignedIt remoteRank=[%d]", destRank);
        for (auto &it : notifyPoolDeivceIPCAsignedIt->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockIPC.unlock();

    // 根据destRank清理对端的notify
    std::unique_lock<std::mutex> lockNoIPC(notifyPoolNoIPCAsignedMutex_);
    const auto &notifyPoolDeivceNoIPCAsignedIt = notifyPoolDeivceNoIPCAsignedMap_.find(destRank);
    if (notifyPoolDeivceNoIPCAsignedIt == notifyPoolDeivceNoIPCAsignedMap_.end()) {
        HCCL_RUN_INFO("[ResetNotify]remoteRank[%d] is not in notifyPoolDeivceNoIPCAsignedMap_", destRank);
    } else {
        HCCL_RUN_INFO("[ResetNotify]reset notifyPoolDeivceNoIPCAsignedIt remoteRank=[%d]", destRank);
        for (auto &it : notifyPoolDeivceNoIPCAsignedIt->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockNoIPC.unlock();
    return HCCL_SUCCESS;
}

HcclResult NotifyPoolImpl::ResetNotify()
{
    HCCL_DEBUG("NotifyPoolImpl ResetNotify");
    std::unique_lock<std::mutex> lockIPC(notifyPoolIPCAsignedMutex_);
    for (auto iter = notifyPoolDeivceIPCAsignedMap_.begin(); iter != notifyPoolDeivceIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockIPC.unlock();
    std::unique_lock<std::mutex> lockNoIPC(notifyPoolNoIPCAsignedMutex_);
    for (auto iter = notifyPoolDeivceNoIPCAsignedMap_.begin(); iter != notifyPoolDeivceNoIPCAsignedMap_.end(); iter++) {
        for (auto &it : iter->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockNoIPC.unlock();
    std::unique_lock<std::mutex> lockIPCA2A(notifyPoolIPCAsignedMutexForA2A_);
    for (auto iter = notifyPoolDevIPCAsignedMapForA2A_.begin(); iter != notifyPoolDevIPCAsignedMapForA2A_.end();
         iter++) {
        for (auto &it : iter->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockIPCA2A.unlock();
    std::unique_lock<std::mutex> lockNoIPCA2A(notifyPoolNoIPCAsignedMutexForA2A_);
    for (auto iter = notifyPoolDevNoIPCAsignedMapForA2A_.begin(); iter != notifyPoolDevNoIPCAsignedMapForA2A_.end();
         iter++) {
        for (auto &it : iter->second) {
            CHK_RET(hrtNotifyReset(it->ptr()));
        }
    }
    lockNoIPCA2A.unlock();
    return HCCL_SUCCESS;
}
}  // namespace hccl
