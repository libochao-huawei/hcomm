/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_indop_process.h"
#include "coll_comm_aicpu_mgr.h"

using namespace hccl;
using namespace HcclApi;

namespace {
struct hcclCommAicpuInfo {
    ReadWriteLockBase ReadWriteLockBase;  // 读写锁单例，维护全局的读写信息
    std::unordered_map<std::string, CollCommAicpuMgr> commMap;
};
hcclCommAicpuInfo g_commAicpuInfo;
DevType g_devType = DevType::DEV_TYPE_COUNT;
thread_local HcclCommAicpu *g_hcclComm = nullptr; // 记录当前线程通信域; AicpuGetCommbyGroup赋值，AicpuReleaseCommbyGroup置空
}


HcclResult AicpuIndopProcess::AicpuIndOpCommInit(CommAicpuParam *commAicpuParam) {

    CollCommAicpuMgr *commAicpuMgr = nullptr;
    HcclResult ret = HCCL_SUCCESS;
    std::string group = commAicpuParam->hcomId;
    CHK_RET(AcquireAicpuComm(group, &commAicpuMgr));
    if (commAicpu == nullptr) {
        HCCL_ERROR("[AicpuHcclProcess][AicpuIndOpCommInit]commAicpu is null group[%s]", group.c_str());
        return HCCL_E_PTR;
    }


    return HCCL_SUCCESS;
}

HcclResult AicpuIndopProcess::AcquireAicpuComm(const std::string &group, CollCommAicpuMgr **aicpuCommPtr)
{
    ReadWriteLock rwlock(g_commAicpuInfo.commAicpuMapMutex);
    rwlock.writeLock();
    
    rwlock.writeUnlock();
    return HCCL_SUCCESS;
}


HcclResult AicpuIndopProcess::AicpuIndOpThreadInit(ThreadMgrAicpuParam *param)
{
    return HCCL_SUCCESS;
}

HcclResult AicpuIndopProcess::AicpuIndOpChannelInitV2(HcclChannelUrmaRes *commParam)
{
    return HCCL_SUCCESS;
}

HcclResult AicpuIndopProcess::AicpuIndOpNotifyInit(NotifyMgrAicpuParam *param)
{
    return HCCL_SUCCESS;
}