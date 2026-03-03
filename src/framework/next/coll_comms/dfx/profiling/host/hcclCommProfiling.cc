/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hcclCommProfiling.h"
#include "profiling_reporter.h"
namespace hccl {
    
explicit HcclCommProfiling(DevId deviceId) {
    mirrorTaskManager_ = std::make_unique<MirrorTaskManager>(deviceId, &GlobalMirrorTasks::Instance(), false);
    profilingReporter_ = std::make_unique<ProfilingReporter>(mirrorTaskManager_, &ProfilingHandler::GetInstance());
}

// HcclCommProfiling任务上报
void HcclCommProfiling::ReportAllTasks(bool cachedReq) {
    if (profilingReporter_) {
        profilingReporter_->ReportAllTasks(cachedReq);
    }
}

// HcclCommProfiling::ReportOp实现
void HcclCommProfiling::ReportOp(uint64_t beginTime, bool cachedReq, bool opbased) {
    if (profilingReporter_) {
        profilingReporter_->ReportOp(beginTime, cachedReq, opbased);
    }
}

// HcclCommProfiling::CallReportMc2CommInfo实现
// void HcclCommProfiling::CallReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo) {
//     if (profilingReporter_) {
//         profilingReporter_->CallReportMc2CommInfo(mc2CommInfo);
//     }
// }

// HcclCommProfiling::UpdateProfStat实现
void HcclCommProfiling::UpdateProfStat() {
    if (profilingReporter_) {
        profilingReporter_->UpdateProfStat();
    }
}
Hccl::MirrorTaskManager* HcclCommProfiling::GetMirrorTaskManager() const {
    return mirrorTaskManager_;
}
}// namespace hccl
