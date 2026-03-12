/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommProfilingLite.h"
#include "profiling_reporter_lite.h"
#include "mirror_task_manager.h"

namespace hccl {
// 构造函数
HcclCommProfilingLite::HcclCommProfilingLite(Hccl::DevId deviceId, Hccl::MirrorTaskManager* mirrorTaskManager) {
    // 获取deviceID
    mirrorTaskManager_ = std::unique_ptr<Hccl::MirrorTaskManager>(mirrorTaskManager);
    profilingReporterLite_ = std::make_unique<Hccl::ProfilingReporterLite>(mirrorTaskManager_.get(), &Hccl::ProfilingHandlerLite::GetInstance());
}

// HcclCommProfilingLite任务上报
void HcclCommProfilingLite::ReportAllTasks(const std::string& group, u32 ranksize) {
    if (profilingReporterLite_) {
        profilingReporterLite_->ReportAllTasks(group, ranksize);
    }
}

// HcclCommProfilingLite::ReportHcclOpInfo实现
// void HcclCommProfilingLite::ReportHcclOpInfo(const DfxOpInfo& hcclOpInfo) {
//     Hccl::ProfilingHandlerLite::GetInstance().ReportHcclOpInfo(hcclOpInfo);todo:输入不一致
// }

// HcclCommProfilingLite::UpdateProfStat实现
void HcclCommProfilingLite::UpdateProfStat() {
    if (profilingReporterLite_) {
        profilingReporterLite_->UpdateProfStat();
    }
}
    
 Hccl::MirrorTaskManager* HcclCommProfilingLite::GetMirrorTaskManager() const{
    return mirrorTaskManager_.get();
 }

}