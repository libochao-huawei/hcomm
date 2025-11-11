/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "comm_engine_res_manager.h"
#include "launch_aicpu.h"
#include "launch_device.h"

namespace hccl {
HcclResult CommEngineResMgr::LoadAICPUKernel(void)
{
    std::string jsonPath;
    CHK_RET(GetKernelFilePath(jsonPath));
    jsonPath += "ccl_kernel.json";

    HcclResult ret = LoadBinaryFromFile(jsonPath.c_str(), ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE, 0,
        binHandle_);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[LoadAICPUKernel]errNo[0x%016llx]load aicpu file fail, path[%s] optionType[%u]"
        "cpuKernelMode[%u].", ret, jsonPath.c_str(), ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE, 0), ret);
    return HCCL_SUCCESS;
}

HcclResult CommEngineResMgr::Init(uint32_t threadNum, uint32_t notifyNumPerThread,
    const std::string& commId, const aclrtBinHandle binHandle)
{
    std::lock_guard<std::mutex> lock(mtx_);
    HCCL_INFO("[CommEngineResMgr][%s] Hcom[%s] threadNum[%u], notifyPerThread[%u]", 
        __func__, commId.c_str(), threadNum, notifyNumPerThread);
    if (!threadMgr_) {
        EXECEPTION_CATCH(threadMgr_ = std::make_unique<ThreadMgr>(threadNum, notifyNumPerThread, commId, binHandle),
            return HCCL_E_PTR);
    }
    if (!notifyMgr_) {
        EXECEPTION_CATCH(notifyMgr_ = std::make_unique<NotifyManager>(commId, binHandle),
            return HCCL_E_PTR);
    }
    if (!binHandle_) {
        CHK_RET(LoadAICPUKernel());
    }
    return HCCL_SUCCESS;
}

HcclResult CommEngineResMgr::CommAllocThreadRes(CommEngine engine, uint32_t threadNum,
    uint32_t notifyNumPerThread, ThreadHandle *thread)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    uint32_t setThreadNum = threadMgr_->GetThreadNum();
    uint32_t setNotifyNumPerThread = threadMgr_->GetNotifyNumPerThread();
    CHK_PRT_RET(threadNum > setThreadNum,  HCCL_ERROR("[%s] Alloced thread num[%u] more than num[%u] in config", 
        __func__, threadNum, setThreadNum), HCCL_E_PARA);
    CHK_PRT_RET(notifyNumPerThread > setNotifyNumPerThread,  HCCL_ERROR("[%s] Alloced preNotify num[%u] more than "
        "num[%u] in config", __func__, notifyNumPerThread, setNotifyNumPerThread), HCCL_E_PARA);
    return threadMgr_->CommAllocThreadRes(binHandle_, engine, threadNum, notifyNumPerThread, thread);
}

HcclResult CommEngineResMgr::CommAllocThreadResByStream(CommEngine engine,
        rtStream_t stream, uint32_t notifyNum, ThreadHandle *thread)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    return threadMgr_->CommAllocThreadResByStream(engine, stream, notifyNum, thread);
}

HcclResult CommEngineResMgr::CommGetNotifyNumInThread(ThreadHandle thread, CommEngine engine, uint32_t *notifyNum)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    return threadMgr_->CommGetNotifyNumInThread(thread, notifyNum);
}

HcclResult CommEngineResMgr::HcommAllocNotify(CommEngine commEngine, NotifyType notifyType, uint32_t notifyNum,
    NotifyHandle **notifyHandleList)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    return notifyMgr_->HcommAllocNotify(commEngine, notifyType, notifyNum, notifyHandleList);
}

HcclResult CommEngineResMgr::HcommFreeNotify(uint32_t notifyNum, NotifyHandle *notifyHandleList)
{
    CHK_SMART_PTR_NULL(threadMgr_);
    return notifyMgr_->HcommFreeNotify(notifyNum, notifyHandleList);
}
}