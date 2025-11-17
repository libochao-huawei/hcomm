/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef FFTS_COMMON_H
#define FFTS_COMMON_H

#include <vector>
#include <map>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <memory>
#include <algorithm>
#include "hccl/base.h"
#include "rt_external.h"

namespace GraphMgr {
enum class HcclFftsTaskType : s32 {
    RESERVED = -1,
    INCHIP_NOTIFY_RECORD = 0,
    INCHIP_NOTIFY_WAIT = 1,
    REMOTE_NOTIFY_RECORD = 2,
    REMOTE_NOTIFY_WAIT = 3,
    SDMA = 4,
    RDMA_SEND = 5,
    REDUCE_INLINE = 6,
    REDUCE_TBE = 7,
    PLACE_HOLDER = 8
};

constexpr s32 PRE_LINE_SUCCESSORNUM = 8; // 每行打印的successor num
// 单个task最多有2个后继task; 
// 1. 对于in chip notify record, 只有2个后继task,  后继task为其它流上对应record/wait以及当前流的后续task
// 2. 对于其它task, 只有一个后继task, 后继task为当前流的后续task
constexpr u32 HCCL_TASK_SUCCESSOR_NUM = 2;
constexpr u16 CONTEXT_MAX_NUM = 128;
constexpr s64 HCCL_DMA_MAX_COUNT_4GB = 0x100000000;
constexpr u32 INVALID_UINT = 0xFFFFFFFF;
constexpr u64 INVALID_U64 = 0xFFFFFFFFFFFFFFFF;
constexpr u32 HCCL_FFTS_CAPACITY = 65535;           // FFTS+子图最大容量
constexpr uint32_t SDMA_FP32_ATOMIC_ADD_SQE = 0x1E71;
constexpr uint32_t SDMA_FP32_ATOMIC_MOVE_SQE = 0x1E70;

using HcclFfftsTaskInfo = struct HcclFfftsTaskDef {
    u32 taskIndex = INVALID_UINT;
    HcclFftsTaskType taskType = HcclFftsTaskType::RESERVED;
    u32 ctxId = INVALID_UINT;
    s32 streamId = INVALID_UINT;
    u32 notifyId = INVALID_UINT;
    u32 predCnt = 0;
    u32 successorNum = 0;
    u32 successorList[HCCL_TASK_SUCCESSOR_NUM] = {0};
    HcclFfftsTaskDef() {}
    HcclFfftsTaskDef(u32 id, s32 type, u32 ctxId, s32 streamid, u32 notifyid, u32 predCnt, u32 successornum, u32 successor0, u32 succesor1)
        : taskIndex(id), taskType(static_cast<HcclFftsTaskType>(type)), ctxId(ctxId), streamId(streamid), notifyId(notifyid), predCnt(predCnt), successorNum(successornum)
    {
        successorList[0] = successor0;
        successorList[1] = succesor1;
    }
    HcclFfftsTaskDef(u32 id, HcclFftsTaskType type, u32 ctxId, s32 streamid, u32 notifyid, u32 predCnt, u32 successornum, u32 successor0, u32 succesor1)
        : taskIndex(id), taskType(type), ctxId(ctxId), streamId(streamid), notifyId(notifyid), predCnt(predCnt), successorNum(successornum)
    {
        successorList[0] = successor0;
        successorList[1] = succesor1;
    }
    HcclFfftsTaskDef(const HcclFfftsTaskDef &that)
        : taskIndex(that.taskIndex), taskType(that.taskType), ctxId(that.ctxId), streamId(that.streamId),
        notifyId(that.notifyId), predCnt(that.predCnt), successorNum(that.successorNum)
    {
        successorList[0] = that.successorList[0];
        successorList[1] = that.successorList[1];
    }
    HcclFfftsTaskDef(const HcclFfftsTaskDef &&that)
        : taskIndex(that.taskIndex), taskType(that.taskType), ctxId(that.ctxId), streamId(that.streamId),
        notifyId(that.notifyId), predCnt(that.predCnt), successorNum(that.successorNum)
    {
        successorList[0] = that.successorList[0];
        successorList[1] = that.successorList[1];
    }
    HcclFfftsTaskDef& operator=(const HcclFfftsTaskDef&& that) {
        if (&that != this) {
            taskIndex = that.taskIndex;
            taskType = that.taskType;
            ctxId = that.ctxId;
            streamId = that.streamId;
            notifyId = that.notifyId;
            predCnt = that.predCnt;
            successorNum = that.successorNum;
            successorList[0] = that.successorList[0];
            successorList[1] = that.successorList[1];
        }
        return *this;
    }
    HcclFfftsTaskDef& operator=(const HcclFfftsTaskDef& that) {
        if (&that != this) {
            taskIndex = that.taskIndex;
            taskType = that.taskType;
            ctxId = that.ctxId;
            streamId = that.streamId;
            notifyId = that.notifyId;
            predCnt = that.predCnt;
            successorNum = that.successorNum;
            successorList[0] = that.successorList[0];
            successorList[1] = that.successorList[1];
        }
        return *this;
    }
    bool operator==(const HcclFfftsTaskDef &that) const
    {
        return taskIndex == that.taskIndex && taskType == that.taskType && ctxId == that.ctxId && streamId == that.streamId && notifyId == that.notifyId
           && predCnt == that.predCnt && successorNum == that.successorNum && successorList[0] == that.successorList[0] && successorList[1] == that.successorList[1];
    }
};

using HcclFftsContextsInfo = struct HcclFftsCtxsDef {
    bool completed = false;
    u32 refreshIndex = 0;
    u32 ctxNum = 0;
    std::vector<u32> ignoredSuccessorStart; // 如果流的第一个task为record，则忽略并存入
    std::unordered_map<u32, u32> unrelatedSuccessorEnd;
    std::unordered_map<u32, u32> unrelatedSuccessorStart;
    std::unordered_map<s32, std::vector<u32>> unassignedSuccessorEnd;
    std::unordered_map<s32, u32> lastThreadIndex; // 存放每个streamid 最后一个待更新节点
    std::vector<rtFftsPlusComCtx_t> contexts;

    // Begin: GraphV2 去除空拷贝使用的信息
    std::vector<HcclFfftsTaskInfo> taskInfos; // 所有task信息, 包括片内的notify record/wait
    std::unordered_map<s32, u32> lastTaskIndex; // 存放每个streamid 最后一个待更新节点 包含 片内的notifywait和record
    u32 refreshTaskIndex = 0; // 待更新的taskId
    std::vector<std::vector<bool>> isVisitedTasks; // 两个task间是否被访问过
    std::queue<HcclFfftsTaskInfo> inDegree0TaskQueue; // 原图中所有入度为0的task
    u32 inchipwaitPlaceHolderNum = 0; // 片内wait后面没有ctx, 补充的placeholder数目
    std::unordered_map<s32, std::vector<HcclFfftsTaskInfo>> streamTaskMap; // 根据流存放放每条流对应的task
    std::map<u32, std::set<u32>> ctxExpasionSuccessorMap; // 存放successor数不够的ctx以及该ctx的所有successor;
    u32 expasionSuccessorCtxNum = 0; // 用来扩充successor的ctx数量
    // End: GraphV2 去除空拷贝使用的信息
    HcclFftsCtxsDef()
    {
        contexts.resize(100); // 100: 默认可存放context个数，可动态扩张
        taskInfos.resize(100); // 100: 默认可存放task个数, 可动态扩张
    }
};
}
#endif // !FFTS_COMMON_H