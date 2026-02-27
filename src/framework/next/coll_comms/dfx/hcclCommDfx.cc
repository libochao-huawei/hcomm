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

HcclCommDfx::HcclCommDfx(uint32_t deviceId) {
    deviceId_ = deviceId;
    mirrorTaskManager_ = new Hccl::MirrorTaskManager(deviceId_, &GlobalMirrorTasks::Instance(), false);
}

void HcclCommDfx::Init() {
    // 1. 如果mirrorTaskManager_为空，则创建新的MirrorTaskManager
    if (mirrorTaskManager_ == nullptr) {
        // 注意：实际实现中应该避免这种情况，CommunicatorImpl应该传入已经存在的MirrorTaskManager
        mirrorTaskManager_ = new Hccl::MirrorTaskManager(deviceId_, &GlobalMirrorTasks::Instance(), false);
    }
    
    // 2. 创建Profiling管理类
    profiling_ = std::make_unique<HcclCommProfiling>(mirrorTaskManager_);
    
    // 3. 注册回调到单例
    RegisterProfilingCallback();
}

// 回调注册实现
void HcclCommDfx::RegisterProfilingCallback() {
    auto callback = [this](const TaskInfo& taskInfo) {
        mirrorTaskManager_->AddTaskInfo(std::make_shared<TaskInfo>(taskInfo));
    };
    // 待后续实现HcomProfiling类 
    // HcomProfiling::GetInstance().RegisterAddTaskInfoCallback(callback);
}

// HcclCommDfx接口实现
void HcclCommDfx::ReportAllTasks(bool cachedReq) {
    if (profiling_) {
        profiling_->ReportAllTasks(cachedReq);
    }
}

void HcclCommDfx::ReportOp(uint64_t beginTime, bool cachedReq, bool opbased) {
    if (profiling_) {
        profiling_->ReportOp(beginTime, cachedReq, opbased);
    }
}

// void HcclCommDfx::CallReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo) {
//     if (profiling_) {
//         profiling->CallReportMc2CommInfo(mc2CommInfo);
//     }
// }

void HcclCommDfx::UpdateProfStat() {
    if (profiling_) {
        profiling_->UpdateProfStat();
    }
}

Hccl::MirrorTaskManager* HcclCommDfx::GetMirrorTaskManager() const {
    return mirrorTaskManager_;
}

}
