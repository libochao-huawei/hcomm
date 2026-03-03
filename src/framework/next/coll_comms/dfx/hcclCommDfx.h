/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCL_COMM_DFX_H
#define HCCL_COMM_DFX_H

#include <memory>
#include "mirror_task_manager.h"
#include "hcclCommProfiling.h"
#include "global_mirror_tasks.h"
#include <unordered_map>
namespace hccl {

class HcclCommDfx {
public:
    // 构造函数（接收CommunicatorImpl中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfx(u32 deviceId);
    
    // 初始化DFX系统
    void Init();
    
    // 注册回调函数
    void AddTaskInfoCallback(u32 streamId, u32 taskId, const TaskParam &taskParam, u64 handle);
    
    // 获取MirrorTaskManager
    Hccl::MirrorTaskManager* GetMirrorTaskManager() const;
    
    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）
    void ReportAllTasks(bool cachedReq = false);
    void ReportOp(u64 beginTime, bool cachedReq, bool opbased);
    // void CallReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo);
    void UpdateProfStat();
    
private:
    u32 deviceId_;
    Hccl::MirrorTaskManager* mirrorTaskManager_;  // 使用原始指针，不管理生命周期
    std::unique_ptr<HcclCommProfiling> profiling_;
    std::unordered_map<CollComm,std::unordered_map<u64, u32 remoteRankId> > channelRemoteRankId_;
};

}

#endif
