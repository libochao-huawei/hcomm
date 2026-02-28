/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "mirror_task_manager.h"
#include "hcclCommProfilingLite.h"


namespace hccl {
class HcclCommDfxLite {
public:
    // 构造函数（接收CommunicatorImplLite中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfxLite(Hccl::MirrorTaskManager* existingMirrorTaskManager = nullptr);
    
    // 初始化DFX系统
    void Init();
    
    // 注册回调到单例
    void RegisterProfilingCallback();
    
    // 获取MirrorTaskManager
    Hccl::MirrorTaskManager* GetMirrorTaskManager() const;
    
    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）
    void ReportAllTasks();
    void ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo);
    void UpdateProfStat();

private:
    Hccl::MirrorTaskManager* mirrorTaskManager_;  // 使用原始指针，不管理生命周期
    std::unique_ptr<HcclCommProfilingLite> profilingImpl_;
};

}