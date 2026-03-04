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
namespace hccl {
// HcclCommDfxLite构造函数实现
HcclCommDfxLite::HcclCommDfxLite() {
}

// HcclCommDfxLite初始化流程
void HcclCommDfxLite::Init(c) {
    // 1. 如果mirrorTaskManager_为空，则创建新的MirrorTaskManager
    if (!mirrorTaskManager_) {
        // 注意：实际实现中应该避免这种情况，CommunicatorImplLite应该传入已经存在的MirrorTaskManager
        mirrorTaskManager_ = std::make_unique<Hccl::MirrorTaskManager>(
            0, &Hccl::GlobalMirrorTasks::Instance(), true);
    }
    
    // 2. 创建Profiling管理类
    profilingImpl_ = std::make_unique<HcclCommProfilingLite>(mirrorTaskManager_);
    
    // 3. 注册回调到单例
    RegisterProfilingCallback();
}

void HcclCommDfxLite::AddTaskInfoCallback(u32 streamId, u32 taskId, const TaskParam &taskParam, u64 handle) {
    // auto callback = [this](const Hccl::TaskInfo& taskInfo) {
    //     mirrorTaskManager_->AddTaskInfo(std::make_shared<Hccl::TaskInfo>(taskInfo));
    // };
    // HcomProfilingLite::GetInstance().RegisterAddTaskInfoCallback(callback);todo:待后续实现HcomProfilingLite类 
}

// HcclCommDfxLite接口实现
void HcclCommDfxLite::ReportAllTasks() {
    if (profilingImpl_) {
        profilingImpl_->ReportAllTasks();
    }
}

void HcclCommDfxLite::ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo) {
    if (profilingImpl_) {
        profilingImpl_->ReportHcclOpInfo(hcclOpInfo);
    }
}

void HcclCommDfxLite::UpdateProfStat() {
    if (profilingImpl_) {
        profilingImpl_->UpdateProfStat();
    }
}

void HcclCommDfxLite::AddChannelRemoteRankId(const std::string& commTag, u64 handle, u32 remoteRankId) {
    rwLock_.writelock();
    HCCL_INFO("[HcclCommDfxLite][AddChannelRemoteRankId] commTag:[%s], handle:[%lu], remoteRankId:[%u]", commTag.c_str(), handle, remoteRankId);
    channelRemoteRankIdLite_[commTag][handle] = remoteRankId;
    rwLock_.writeUnLock();
}
// 在channelRemoteRankIdLite_表中对remoteRankId进行查找
HcclResult HcclCommDfxLite::GetChannelRemoteRankId(const std::string& commTag, u64 handle, u32& remoteRankId) {
    rwLock_.readlock();
    if(channelRemoteRankIdLite_.find(commTag) == channelRemoteRankIdLite_.end()) {
        rwLock_.readUnLock();
        HCCL_ERROR("[HcclCommDfxLite]commTag:[%s] not found", commTag.c_str());
        return HCCL_RESULT_INVALID_PARAM;
    }
    if(channelRemoteRankIdLite_[commTag].find(handle) == channelRemoteRankIdLite_[commTag].end()) {
         HCCL_ERROR("[HcclCommDfxLite]handle not found,commTag:[%s],handle:[%lu]", commTag.c_str(), handle);
        rwLock_.readUnLock();
        return HCCL_RESULT_INVALID_PARAM;
    }
    remoteRankId = channelRemoteRankIdLite_[commTag][handle];
    rwLock_.readUnLock();
}

Hccl::MirrorTaskManager* HcclCommDfxLite::GetMirrorTaskManager() const {
    return mirrorTaskManager_;
}
}