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

// HcclCommDfxLite初始化流程 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::Init(u32 deviceId, std::string comTag) {
    deviceId_ = deviceId;
    
    // 1. 如果mirrorTaskManager_为空，则创建新的MirrorTaskManager
    if (!mirrorTaskManager_) {
        // 注意：实际实现中应该避免这种情况，CommunicatorImplLite应该传入已经存在的MirrorTaskManager
        mirrorTaskManager_ = std::make_unique<Hccl::MirrorTaskManager>(
            deviceId_,, &Hccl::GlobalMirrorTasks::Instance(), true);
        // 检查内存分配是否成功
        if (mirrorTaskManager_ == nullptr) {
            return HCCL_E_MEMORY; // 返回内存分配失败错误码
        }
    }
    
    // 2. 创建Profiling管理类
    
    EXECEPTION_CATCH(profilingImpl_ = std::make_unique<HcclCommProfilingLite>(mirrorTaskManager_), return HCCL_E_PTR);

    
    // 3. 注册回调到单例
    // RegisterProfilingCallback();
    setAddTaskCallback_ = [this](u32 streamId, u32 taskId, const TaskParam &taskParam, u32 handle) {
        return this->AddTaskInfoCallback(streamId, taskId, taskParam, handle)
    };
    
    return HCCL_SUCCESS; // 初始化成功返回成功码
}


HcclResult HcclCommDfxLite::AddTaskInfoCallback(u32 streamId, u32 taskId, const TaskParam &taskParam, u32 handle) {
    if (handle == 0xffffff) {
        remoteRankId = 0xffffff;
    } else {
        CHK_RET(GetChannelRemoteRankId(commTag_, handle, &remoteRankId));
    }
    shared_ptr<TaskInfo> taskInfo{nullptr};
    EXECEPTION_CATCH(taskInfo = std::make_shared<TaskInfo>(streamId, taskId,
    remoteRankId, taskParam, mirrorTaskManager_->GetCurrDfxOpInfo()), return HCCL_E_PTR);
    EXECEPTION_CATCH(mirrorTaskManager_->AddTaskInfo(taskInfo), return HCCL_E_PTR);
    return HCCL_SUCCESS;
}

// HcclCommDfxLite接口实现 - 修改为返回HcclResult类型
HcclResult HcclCommDfxLite::ReportAllTasks() {
    if (profilingImpl_) {
        profilingImpl_->ReportAllTasks();
    }
    return HCCL_E_PTR; // 指针为空返回错误码
}

HcclResult HcclCommDfxLite::ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo) {
    if (profilingImpl_) {
        profilingImpl_->ReportHcclOpInfo(hcclOpInfo);
    }
    return HCCL_E_PTR; // 指针为空返回错误码
}

HcclResult HcclCommDfxLite::UpdateProfStat() {
    if (profilingImpl_) {
        profilingImpl_->UpdateProfStat();
    }
    return HCCL_E_PTR; // 指针为空返回错误码
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