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
#include "read_write_lock.h"
#include "hccl_common.h"
#include <unordered_map>
#include "buffer.h"
#include "common.h"
#include "hcclCommOp.h"

namespace hccl {

class HcclCommDfx {
public:
    // 构造函数（接收CommunicatorImpl中已经存在的MirrorTaskManager指针）
    explicit HcclCommDfx();

    // 初始化DFX系统
    HcclResult Init(u32 deviceId, const std::string& comTag);

    // 注册回调函数
    HcclResult AddTaskInfoCallback(u32 streamId, u32 taskId, const Hccl::TaskParam &taskParam, u64 handle);

    // 获取MirrorTaskManager
    Hccl::MirrorTaskManager* GetMirrorTaskManager() const;

    // Profiling相关接口（直接暴露，不通过GetProfilingImpl）
    HcclResult ReportAllTasks(bool cachedReq);
    HcclResult ReportOp(u64 beginTime, bool cachedReq, bool opbased);
    void ReportMc2CommInfo(const Mc2CommInfo& mc2CommInfo);
    HcclResult UpdateProfStat();

    // 将remoteRankId添加到channelRemoteRankId_表中
    static void AddChannelRemoteRankId(const std::string& commTag, u64 handle, u32 remoteRankId);
    // 在channelRemoteRankId_表中对remoteRankId进行查找
    static HcclResult GetChannelRemoteRankId(const std::string& commTag, u64 handle, u32& remoteRankId);
    std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> GetCallback() {
        return setAddTaskCallback_;
    }
    HcclResult ReportKernel(uint64_t beginTime, const std::string& commTag, const std::string& kernelName, uint32_t threadId);
private:
    std::unique_ptr<Hccl::MirrorTaskManager> mirrorTaskManager_;
    std::unique_ptr<HcclCommProfiling> profiling_;
    static std::unordered_map<std::string,std::unordered_map<u64, u32> > channelRemoteRankId_;
    static ReadWriteLockBase baseLock_; // 基类锁成员
    static ReadWriteLock rwLock_; // 读写锁
    std::string commTag_;
    u32 deviceId_{0};
    std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> setAddTaskCallback_;
};

} // namesapce hccl

#endif
