/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef GLOBAL_MIRROR_TASKS_H
#define GLOBAL_MIRROR_TASKS_H

#include <map>
#include <memory>
#include <array>
#include "dfx_common.h"
#include "queue.h"
#include "task_info.h"
#include "exception_util.h"
#include "internal_exception.h"
#include "circular_queue.h"
#include "vector_queue.h"
#include <array>

namespace Hccl {

const u32 MAX_CIRCULAR_QUEUE_LENGTH = 2048;

MAKE_ENUM(QueueType, Circular_Queue, Vector_Queue)

using TaskInfoQueue    = Queue<std::shared_ptr<TaskInfo>>;
using TaskInfoQueueMap = std::map<u32, std::unique_ptr<TaskInfoQueue>>;

class GlobalMirrorTasks {
public:
    static GlobalMirrorTasks &Instance();

    u32 DevSize() const;

public:
    TaskInfoQueue            *GetQueue(u32 devId, u32 streamId) const;
    std::shared_ptr<TaskInfo> GetTaskInfo(u32 devId, u32 streamId, u32 taskId) const;

    TaskInfoQueue &CreateQueue(u32 devId, u32 streamId, QueueType type);
    void           DestroyQueue(u32 devId, u32 streamId);

    TaskInfoQueueMap::iterator Begin(u32 devId);
    TaskInfoQueueMap::iterator End(u32 devId);

private:
    static GlobalMirrorTasks                       ins_;
    std::array<TaskInfoQueueMap, DEVICE_MAX_NUM> taskMaps_;

private:
    GlobalMirrorTasks();
    ~GlobalMirrorTasks();
};

} // namespace Hccl

#endif // GLOBAL_MIRROR_TASKS_H