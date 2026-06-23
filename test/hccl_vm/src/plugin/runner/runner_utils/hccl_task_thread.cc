/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccl_task_thread.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

#include "aiv_graph_executor_mgr.h"
#include "aiv_task_snapshot_loader.h"
#include "ccu_resource_manager.h"
#include "sim_common_macro.h"
#include "store_sim_store_pub.h"
#include "hccl_task_reduce_process.h"
#include "hccl_types.h"
#include "sim_log.h"

using namespace HcclSim;

// 本地任务定义
namespace VirtualRunTime {
std::unordered_map<uint64_t, bool> g_notifyStatus;

HcclVmResult TransformAddr(uint64_t srcOffset, uint64_t dstOffset, VmUniquePtr &src, VmUniquePtr &dst)
{
    HCCLVM_CHK_RET(GetAddrByOffset(srcOffset, src));
    HCCLVM_CHK_RET(GetAddrByOffset(dstOffset, dst));
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

// Memory copy task
HcclVmResult TaskMemcpy(const HcclTaskMetaData &task) {
    HCCL_VM_DEBUG("Memory copy task from src offset= {} to dst offset= {} len= {} started",
                task.taskData.transMem.srcOffset, task.taskData.transMem.dstOffset, task.taskData.transMem.len);
    // 拷贝数据
    VmUniquePtr src;
    VmUniquePtr dst;
    HCCLVM_CHK_RET(TransformAddr(task.taskData.transMem.srcOffset, task.taskData.transMem.dstOffset, src, dst));
    memcpy(dst.get(), src.get(), task.taskData.transMem.len);
    HCCL_VM_DEBUG("Memory copy task completed.");
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

// Reduce add task
HcclVmResult TaskReduceAdd(const HcclTaskMetaData &task) {
    HCCL_VM_DEBUG("Reduce Add task from src offset= {} to dst offset= {} started",
                task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset);
    // Reduce add
    VmUniquePtr src;
    VmUniquePtr dst;
    HCCLVM_CHK_RET(TransformAddr(task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset, src, dst));
    auto dataType = static_cast<HcclDataType>(task.taskData.reduce.dataType);
    MemReduceSum(src.get(), dst.get(), task.taskData.reduce.dataCount, dataType);
    HCCL_VM_DEBUG("Reduce add task completed.");
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

// Reduce max task
HcclVmResult TaskReduceMax(const HcclTaskMetaData &task) {
    HCCL_VM_DEBUG("Reduce Max task from src offset= {} to dst offset= {} started",
                task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset);
    // Reduce max
    VmUniquePtr src;
    VmUniquePtr dst;
    HCCLVM_CHK_RET(TransformAddr(task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset, src, dst));
    auto dataType = static_cast<HcclDataType>(task.taskData.reduce.dataType);
    MemReduceMax(src.get(), dst.get(), task.taskData.reduce.dataCount, dataType);
    HCCL_VM_DEBUG("Reduce max task completed.");
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

// Reduce min task
HcclVmResult TaskReduceMin(const HcclTaskMetaData &task) {
    HCCL_VM_DEBUG("Reduce Min task from src offset= {} to dst offset= {} started",
                task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset);
    // Reduce min
    VmUniquePtr src;
    VmUniquePtr dst;
    HCCLVM_CHK_RET(TransformAddr(task.taskData.reduce.srcOffset, task.taskData.reduce.dstOffset, src, dst));
    auto dataType = static_cast<HcclDataType>(task.taskData.reduce.dataType);
    MemReduceMin(src.get(), dst.get(), task.taskData.reduce.dataCount, dataType);
    HCCL_VM_DEBUG("Reduce min task completed.");
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

HcclVmResult TaskReduce(const HcclTaskMetaData &task)
{
    switch ((HcclReduceOp)(task.taskData.reduce.reduceOp))
    {
        case HcclReduceOp::HCCL_REDUCE_MIN:
            return TaskReduceMin(task);
        case HcclReduceOp::HCCL_REDUCE_MAX:
            return TaskReduceMax(task);
        case HcclReduceOp::HCCL_REDUCE_SUM:
            return TaskReduceAdd(task);
        default:
            break;
    }
    HCCL_VM_DEBUG("Reduce task completed.");
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

HcclVmResult TaskNotifyRecord(const HcclTaskMetaData &task)
{
    uint64_t notify = task.taskData.notify.notifyId;
    g_notifyStatus[notify] = true;
    HCCL_VM_DEBUG("Record {} task start.", notify);
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

HcclVmResult TaskNotifyWait(const HcclTaskMetaData &task)
{
    uint64_t notify = task.taskData.notify.notifyId;
    HCCL_VM_DEBUG("Wait {} task start.", notify);
    if (g_notifyStatus[notify]) {
        g_notifyStatus[notify] = false;
        return HcclVmResult::HCCL_SIM_SUCCESS;
    }

    return HcclVmResult::HCCL_SIM_VRT_HOLD_CMD;
}

HcclSim::HcclVmResult TaskCcuGraph(const HcclTaskMetaData &task)
{
    auto rankId = task.rankId;
    auto dieId = task.taskData.ccu.dieId;
    auto instrStartId = task.taskData.ccu.instStartId;
    auto instCnt = task.taskData.ccu.instCnt;
    uint16_t endInstrId = instrStartId + instCnt;

    auto &ccuResMgr = CcuResourceManager::GetInstance();
    ccuResMgr.AddTaskInfo(rankId, task);
    auto simulator = ccuResMgr.InitSimulator(rankId, dieId, instrStartId, endInstrId, instCnt);
    HCCL_VM_INFO("TaskCcuGraph simulator start, rankId={}, dieId={}, instrStartId={}, endInstrId={}, instCnt={}, simulator_ptr: {:p}",
                rankId, dieId, instrStartId, endInstrId, instCnt, (void*)(simulator.get()));

    if (simulator->GetState() == CcuExecState::EXEC_FAIL) {
        return HcclVmResult::HCCL_SIM_E_INTERNAL;
    }
    
    if (simulator->Execute() == false) {
        HCCL_VM_WARN("TaskCcuGraph Execute Hold");
        return HcclVmResult::HCCL_SIM_VRT_HOLD_CMD;
    }
    return HcclVmResult::HCCL_SIM_SUCCESS;
}

HcclVmResult TaskAivGraph(const HcclTaskMetaData &task)
{
    const uint32_t rankId = task.rankId;
    const uint32_t streamId = static_cast<uint32_t>(task.streamId);
    const uint32_t launchIndex = static_cast<uint32_t>(task.taskData.aiv.launchIdx);
    auto aivGraphExecutor = AivGraphExecutorMgr::GetInstance().GetAivGraphExecutor(rankId, launchIndex);
    if (!aivGraphExecutor->IsInitialized()) {
        if (!aivGraphExecutor->Init(rankId, launchIndex)) {
            HCCL_VM_ERROR("Failed to init AivGraphExecutor, rankId={}, streamId={}, launchIndex={}",
                        rankId, streamId, launchIndex);
            return HcclVmResult::HCCL_SIM_E_INTERNAL;
        }
    }

    HCCL_VM_DEBUG("AivGraphExecutor running, rankId={}, streamId={}, launchIndex={}",
                rankId, streamId, launchIndex);
    auto ret = aivGraphExecutor->Execute();
    if (ret != HcclVmResult::HCCL_SIM_SUCCESS) {
        if (ret != HcclVmResult::HCCL_SIM_VRT_HOLD_CMD) {
            HCCL_VM_ERROR("AivGraphExecutor failed, rankId={}, streamId={}, launchIndex={}, ret={}",
                        rankId, streamId, launchIndex, static_cast<int>(ret));
        }
        return ret;
    }
    return HcclVmResult::HCCL_SIM_SUCCESS;
}
}
