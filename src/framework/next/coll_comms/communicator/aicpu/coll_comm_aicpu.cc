/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_aicpu.h"
#include "aicpu_operator_pub.h"
#include "adapter_hal_pub.h"

constexpr u32 NOTIFY_SIZE_EIGHT = 8;

HcclResult CollCommAicpu::InitAicpuIndOp(CommAicpuParam *commAicpuParam)
{
    if (indOpCommInitialized_) {
        HCCL_RUN_INFO("[%s][InitAicpuIndOp]Group[%s] already initialized, skip reinit", __func__,
            identifier_.c_str());
        return HCCL_SUCCESS;
    }
    CHK_PTR_NULL(commAicpuParam);
    topoInfo_.deviceLogicId = commAicpuParam->deviceLogicId;
    topoInfo_.devicePhyId = commAicpuParam->devicePhyId;
    topoInfo_.deviceType = static_cast<DevType>(commAicpuParam->deviceType);
    identifier_ = std::string(commAicpuParam->hcomId);
    topoInfo_.userRankSize = commAicpuParam->userRankSize;
    topoInfo_.userRank = commAicpuParam->userRank; 
    notifys_.reserve(LOCAL_NOTIFY_MAX_NUM);
    notifySize_ = NOTIFY_SIZE_EIGHT;

    CHK_RET(hrtSetWorkModeAicpu(true)); // ??
    CHK_RET(hrtSetlocalDevice(topoInfo_.deviceLogicId)); // ??
    CHK_RET(hrtSetlocalDeviceType(topoInfo_.deviceType)); // ??
    CHK_RET(hrtDrvGetLocalDevIDByHostDevID(topoInfo_.devicePhyId, &devId_)); // ??
    CHK_RET(taskExecption_.Init(devId_, localUserRank_, identifier_)); // ??
    // CHK_RET(RegisterProfCallBack());

    HCCL_INFO("[HcclCommAicpu][InitAicpuIndOp] InitAicpuIndOpV2 start");
    indOpCommInitialized_ = true;
    return HCCL_SUCCESS;
}

HcclResult CollCommAicpu::InitThreads(ThreadMgrAicpuParam *param)
{
    u32 threadNum = param->threadNum;
    std::vector<std::shared_ptr<Thread>> outThreads;
    outThreads.reserve(threadNum);
    std::string hcomId(param->hcomId);
    for (u32 i = 0; i < threadNum; ++i) {
        std::string thdUniqueId(param->threadParam[i], THREAD_UNIQUE_ID_MAX_SIZE);
        if (UNLIKELY(HcclCheckLogLevel(HCCL_LOG_INFO))) {
            std::ostringstream oss;
            oss << "threadParam[" << i << "] raw bytes: ";
            for (u32 j = 0; j < THREAD_UNIQUE_ID_MAX_SIZE; ++j) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<unsigned int>(static_cast<unsigned char>(param->threadParam[i][j])) << " ";
            }
            HCCL_INFO("[HcclCommAicpu][%s] %s", __func__, oss.str().c_str());
        }
        std::shared_ptr<AicpuTsThread> thread;
        EXECEPTION_CATCH((thread = std::make_shared<AicpuTsThread>(thdUniqueId)), return HCCL_E_PTR);
        HcclResult ret = thread->Init();
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[HcclCommAicpu][%s] comm identifier[%s], init threads num[%u] failed at index %u",
                __func__, hcomId.c_str(), param->threadNum, i);
            return ret;
        }
        outThreads.emplace_back(thread);
    }

    ThreadHandle *threadArray = static_cast<ThreadHandle*>(param->deviceHandle);
    // 空指针校验
    CHK_PTR_NULL(threadArray);
    for (size_t i = 0; i < threadNum; ++i) {
        threadArray[i] = reinterpret_cast<ThreadHandle>(outThreads[i].get());  // 拷贝裸指针
        HCCL_INFO("[HcclCommAicpu][%s] threadArray[%u] = [%lu]", __func__, i, threadArray[i]);
    }
    threads_.insert(threads_.end(), std::make_move_iterator(outThreads.begin()),
        std::make_move_iterator(outThreads.end()));
    HCCL_INFO("[HcclCommAicpu][%s] comm identifier[%s], init threads num[%u] success",
        __func__, hcomId.c_str(), threadNum);
    return HCCL_SUCCESS;
}