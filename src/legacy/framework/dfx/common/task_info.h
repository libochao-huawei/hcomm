/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef TASK_INFO_H
#define TASK_INFO_H
#include <string>
#include <memory>
#include "task_param.h"
#include "coll_operator.h"

namespace Hccl {

constexpr u32 INVALID_VALUE_RANKID = 0xFFFFFFFF; // rank id非法值

class DfxOpInfo {
public:
    CollOperator op_;
    std::string  tag_;
    AlgType      algType_;
    u32          index_;
    u64          beginTime_;
    u64          endTime_;
    void        *comm_;
    u32          mainStreamId_;

public:
    std::string Describe() const
    {
        return StringFormat(
            "DfxOpInfo: [collOperator:[%s], tag:[%s], algType:[%u], index:[%u], beginTime:[%llu], endTime:[%llu]",
            CollOpToString(op_).c_str(), tag_.c_str(), algType_, index_, beginTime_, endTime_);
    }
};

class TaskInfo {
public:
    u32                        streamId_;
    u32                        taskId_;
    u32                        remoteRank_{0xffffffff};
    TaskParam                  taskParam_;
    std::shared_ptr<DfxOpInfo> dfxOpInfo_;
    bool                       isMaster_;

public:
    TaskInfo(u32 streamId, u32 taskId, u32 remoteRank, TaskParam taskParam,
              std::shared_ptr<DfxOpInfo> dfxOpInfo = nullptr, bool isMaster = false);

    std::string Describe() const;

    std::string GetAlgTypeName() const;
    std::string GetBaseInfo() const;
    std::string GetConciseBaseInfo() const;
    std::string GetParaInfo() const;
    std::string GetOpInfo() const;

private:
    std::string GetParaDMA() const;
    std::string GetParaReduce() const;
    std::string GetParaNotify() const;
    std::string GetRemoteRankInfo(bool needConcise = false) const;
    std::string GetTaskConciseName() const;
    std::string GetNotifyInfo() const;
};

} // namespace Hccl

#endif