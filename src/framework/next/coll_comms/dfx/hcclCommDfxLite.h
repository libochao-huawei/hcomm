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
#include "read_write_lock.h"

namespace hccl {
class HcclCommDfxLite {
public:
     // 构造函数（接收CommunicatorImplLite中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfxLite();
    
    // 初始化DFX系统 - 修改为返回HcclResult类型
    HcclResult Init(u32 deviceId, std::string comTag);
    // 注册回调到单例
    HcclResult AddTaskInfoCallback(u32 streamId, u32 taskId, const TaskParam &taskParam, u32 handle);
    // 获取MirrorTaskManager
    Hccl::MirrorTaskManager* GetMirrorTaskManager() const;
    
    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）- 全部修改为返回HcclResult类型
    HcclResult ReportAllTasks();
    HcclResult ReportHcclOpInfo(const HcclOpInfo& hcclOpInfo);
    HcclResult UpdateProfStat();
    std::function<HcclResult(u32, u32, const TaskParam&, u32)> GetCallback() {
        return setAddTaskCallback_;
    }
    // 将remoteRankId添加到channelRemoteRankId_表中
    static void AddChannelRemoteRankId(const std::string& commTag, u64 handle, u32 remoteRankId);
    // 在channelRemoteRankId_表中对remoteRankId进行查找
    static HcclResult GetChannelRemoteRankId(const std::string& commTag, u64 handle, u32& remoteRankId);
private:
    std::unique_ptr<Hccl::MirrorTaskManager> mirrorTaskManager_; 
    std::unique_ptr<HcclCommProfilingLite> profilingImpl_;
    static std::unordered_map<std::string,std::unordered_map<u64, u32> > channelRemoteRankIdLite_;
    std::string comTag_;
    u32 deviceId_;
    std::function<HcclResult(u32, u32, const TaskParam&, u32)> setAddTaskCallback_;
    static ReadWriteLockBase baseLock_; // 基类锁成员
    static ReadWriteLock rwLock_; // 读写锁
};
    ReadWriteLockBase HcclCommDfxLite::baseLock_; // 基类锁成员
    ReadWriteLock HcclCommDfxLite::rwLock_(HcclCommDfxLite::baseLock_); // 读写锁

}