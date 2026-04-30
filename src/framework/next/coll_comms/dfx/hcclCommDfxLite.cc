/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcclCommDfxLite.h"
#include "hccl_common.h"

namespace hccl {
ReadWriteLockBase HcclCommDfxLite::baseLockLite_; // 基类锁成员
ReadWriteLock HcclCommDfxLite::rwLockLite_(HcclCommDfxLite::baseLockLite_); // 读写锁
std::unordered_map<std::string,std::unordered_map<u64, u32> > HcclCommDfxLite::channelRemoteRankIdLite_;
// HcclCommDfxLite构造函数实现
HcclCommDfxLite::HcclCommDfxLite() {
}

// HcclCommDfxLite初始化流程 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::Init(u32 deviceId, const std::string& commTag) {
    HCCL_INFO("[HcclCommDfxLite][Init] Init begin deviceId[%u], commTag[%s]", deviceId, commTag.c_str());
    deviceId_ = deviceId;
    commTag_ = commTag;
    /*1. 如果mirrorTaskManager_为空，则创建新的MirrorTaskManager
    注意：实际实现中应该避免这种情况，CommunicatorImplLite应该传入已经存在的MirrorTaskManager*/
    EXECEPTION_CATCH(mirrorTaskManager_ = std::make_unique<Hccl::MirrorTaskManager>(
            deviceId_, &Hccl::GlobalMirrorTasks::Instance(), true), return HCCL_E_PTR);

    // 2. 创建Profiling管理类
    EXECEPTION_CATCH(profilingImpl_ = std::make_unique<HcclCommProfilingLite>(deviceId_, mirrorTaskManager_.get()), return HCCL_E_PTR);

    // 3. 注册回调到单例
    addTaskCallback_ = [this](u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle) {
        return this->AddTaskInfoCallback(streamId, taskId, taskParam, handle);
    };
    HCCL_INFO("[HcclCommDfxLite][Init] Init success");
    return HCCL_SUCCESS; // 初始化成功返回成功码
}

HcclResult HcclCommDfxLite::AddTaskInfoCallback(u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle)
{
    CHK_SMART_PTR_NULL(mirrorTaskManager_);
    u32 remoteRankId = INVALID_UINT;
    if (handle != INVALID_U64) {
        CHK_RET(GetChannelRemoteRankId(commTag_, handle, remoteRankId));
    }
    std::shared_ptr<Hccl::TaskInfo> taskInfo{nullptr};
    EXECEPTION_CATCH(taskInfo = std::make_shared<Hccl::TaskInfo>(streamId, taskId,
        remoteRankId, taskParam, mirrorTaskManager_->GetCurrDfxOpInfo()), return HCCL_E_PTR);
    EXECEPTION_CATCH(mirrorTaskManager_->AddTaskInfo(taskInfo), return HCCL_E_PTR);
    HCCL_INFO("[%s]taskInfo: %s", __func__, taskInfo->Describe().c_str());
    return HCCL_SUCCESS;
}

// HcclCommDfxLite接口实现 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::ReportAllTasks() {
    CHK_SMART_PTR_NULL(profilingImpl_);
    profilingImpl_->ReportAllTasks();
    return HCCL_SUCCESS;
}

HcclResult HcclCommDfxLite::UpdateProfStat() {
    CHK_SMART_PTR_NULL(profilingImpl_);
    profilingImpl_->UpdateProfStat();
    return HCCL_SUCCESS;
}

void HcclCommDfxLite::AddChannelRemoteRankId(const std::string& commTag, u64 handle, u32 remoteRankId) {
    rwLockLite_.writeLock();
    HCCL_INFO("[HcclCommDfxLite][AddChannelRemoteRankId] commTag:[%s], handle:[%lu], remoteRankId:[%u]",
        commTag.c_str(), handle, remoteRankId);
    channelRemoteRankIdLite_[commTag][handle] = remoteRankId;
    rwLockLite_.writeUnlock();
}
// 在channelRemoteRankIdLite_表中对remoteRankId进行查找
HcclResult HcclCommDfxLite::GetChannelRemoteRankId(const std::string& commTag, u64 handle, u32& remoteRankId) {
    rwLockLite_.readLock();
    if (channelRemoteRankIdLite_.find(commTag) == channelRemoteRankIdLite_.end()) {
        rwLockLite_.readUnlock();
        HCCL_ERROR("[HcclCommDfxLite]commTag:[%s] not found", commTag.c_str());
        return HCCL_E_PARA;
    }
    if (channelRemoteRankIdLite_[commTag].find(handle) == channelRemoteRankIdLite_[commTag].end()) {
        HCCL_ERROR("[HcclCommDfxLite]handle not found,commTag:[%s], handle:[%lu]", commTag.c_str(), handle);
        rwLockLite_.readUnlock();
        return HCCL_E_PARA;
    }
    remoteRankId = channelRemoteRankIdLite_[commTag][handle];
    rwLockLite_.readUnlock();
    return HCCL_SUCCESS;
}

Hccl::MirrorTaskManager* HcclCommDfxLite::GetMirrorTaskManager() const {
    return mirrorTaskManager_.get();
}
}