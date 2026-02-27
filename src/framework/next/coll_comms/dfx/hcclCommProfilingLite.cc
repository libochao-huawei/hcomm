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

namespace hccl {
HcclCommProfilingLite::HcclCommProfilingLite(Hccl::MirrorTaskManager* mirrorTaskManager) {
    mirrorTaskManager_ = mirrorTaskManager;
    profilingReporterLite_ = std::make_shared<ProfilingReporterLite>();
}

// HcclCommProfilingLite任务上报
void HcclCommProfilingLite::ReportAllTasks() {
    if (profilingReporterLite_) {
        profilingReporterLite_->ReportAllTasks();
    }
}

// HcclCommProfilingLite::ReportHcclOpInfo实现
void HcclCommProfilingLite::ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo) {
    ProfilingHandlerLite::GetInstance().ReportHcclOpInfo(hcclOpInfo);
}

// HcclCommProfilingLite::UpdateProfStat实现
void HcclCommProfilingLite::UpdateProfStat() {
    if (profilingReporterLite_) {
        profilingReporterLite_->UpdateProfStat();
    }
}
    
 Hccl::MirrorTaskManager* GetMirrorTaskManager() const{
    return mirrorTaskManager_;
 }

}