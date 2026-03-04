/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommDfx.h"

namespace hccl {

HcclCommDfx::HcclCommDfx() {
}

HcclResult HcclCommDfx::Init(u32 deviceId, std::string comTag) {
    deviceId_ = deviceId;
    comTag_ = comTag;
    // 1. 如果mirrorTaskManager_为空，则创建新的MirrorTaskManager
    if (!mirrorTaskManager_) {
        mirrorTaskManager_ = std::make_unique<Hccl::MirrorTaskManager>(deviceId_, &Hccl::GlobalMirrorTasks::Instance(), false);
    }
    
    // 2. 创建Profiling管理类
    EXECEPTION_CATCH(profiling_ = std::make_unique<HcclCommProfiling>(mirrorTaskManager_), return HCCL_E_PTR);
    
    // 3. 注册回调到单例
    // RegisterProfilingCallback();
    return HCCL_SUCCESS; // 初始化成功返回成功码
}

// 回调注册实现
HcclResult HcclCommDfx::AddTaskInfoCallback(u32 streamId, u32 taskId, const TaskParam &taskParam, u32 handle) {
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

/ HcclCommDfx接口实现 - 修改为返回HcclResult类型
HcclResult HcclCommDfx::ReportAllTasks(bool cachedReq) {
    if (profiling_) {
        return profiling_->ReportAllTasks(cachedReq);
    }
    return HCCL_E_PTR; // profiling_为空返回指针错误码
}

HcclResult HcclCommDfx::ReportOp(u64 beginTime, bool cachedReq, bool opbased) {
    if (profiling_) {
        return profiling_->ReportOp(beginTime, cachedReq, opbased);
    }
    return HCCL_E_PTR; // profiling_为空返回指针错误码
}

// void HcclCommDfx::CallReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo) {
//     if (profiling_) {
//         profiling->CallReportMc2CommInfo(mc2CommInfo);
//     }
// }

HcclResult HcclCommDfx::UpdateProfStat() {
    if (profiling_) {
        return profiling_->UpdateProfStat();
    }
    return HCCL_E_PTR; // profiling_为空返回指针错误码
}

Hccl::MirrorTaskManager* HcclCommDfx::GetMirrorTaskManager() const {
    return mirrorTaskManager_;
}

// 将remoteRankId添加到channelRemoteRankId_表中 - 修改为返回HcclResult类型
HcclResult HcclCommDfx::AddChannelRemoteRankId(const std::string& commTag, u64 handle, u32 remoteRankId) {
    rwLock_.writelock();
    try {
        HCCL_INFO("[HcclCommDfx][AddChannelRemoteRankId] commTag:[%s], handle:[%lu], remoteRankId:[%u]", commTag.c_str(), handle, remoteRankId);
        channelRemoteRankId_[commTag][handle] = remoteRankId;
        rwLock_.writeUnLock();
        return HCCL_SUCCESS; // 添加成功返回成功码
    } catch (...) {
        rwLock_.writeUnLock(); // 异常时解锁，避免死锁
        return HCCL_E_INTERNAL; // 内部错误返回对应错误码
    }
}

// 在channelRemoteRankId_表中对remoteRankId进行查找（原有逻辑补充返回值）
HcclResult HcclCommDfx::GetChannelRemoteRankId(const std::string& commTag, u64 handle, u32& remoteRankId) {
    rwLock_.readlock();
    if(channelRemoteRankId_.find(commTag) == channelRemoteRankId_.end()) {
        rwLock_.readUnLock();
        HCCL_ERROR("[HcclCommDfx]commTag:[%s] not found", commTag.c_str());
        return HCCL_RESULT_INVALID_PARAM;
    }
    if(channelRemoteRankId_[commTag].find(handle) == channelRemoteRankId_.end()) {
         HCCL_ERROR("[HcclCommDfx]handle not found,commTag:[%s],handle:[%lu]", commTag.c_str(), handle);
        rwLock_.readUnLock();
        return HCCL_RESULT_INVALID_PARAM;
    }
    remoteRankId = channelRemoteRankId_[commTag][handle];
    rwLock_.readUnLock();
    return HCCL_SUCCESS; // 查找成功补充返回成功码
}

}
