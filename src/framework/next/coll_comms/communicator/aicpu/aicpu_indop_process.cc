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

namespace {
struct CollCommAicpuInfo {
    ReadWriteLockBase commAicpuMgrMapMutex;  // 读写锁单例，维护全局的读写信息
    std::unordered_map<std::string, std::shared_ptr<CollCommAicpuMgr>> commMgrMap;
};
CollCommAicpuInfo g_commAicpuInfo;
// DevType g_devType = DevType::DEV_TYPE_COUNT;
// thread_local HcclCommAicpu *g_hcclComm = nullptr; // 记录当前线程通信域; AicpuGetCommbyGroup赋值，AicpuReleaseCommbyGroup置空
}


HcclResult AicpuIndopProcess::AicpuIndOpCommInit(CommAicpuParam *commAicpuParam) {

    CollCommAicpuMgr *commAicpuMgr = nullptr;
    HcclResult ret = HCCL_SUCCESS;
    std::string group = commAicpuParam->hcomId;
    CHK_RET(AcquireAicpuCommMgr(group, &commAicpuMgr));
    if (commAicpuMgr == nullptr) {
        HCCL_ERROR("[AicpuHcclProcess][AicpuIndOpCommInit]commAicpu is null group[%s]", group.c_str());
        return HCCL_E_PTR;
    }

    ret = commAicpuMgr->InitAicpuIndOp(commAicpuParam);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[AicpuIndopProcess][%s]errNo[0x%016llx] Failed to init independent op comm group[%s]" , __func__,
        HCCL_ERROR_CODE(ret), group.c_str()), ret);

    return HCCL_SUCCESS;
}

HcclResult AicpuIndopProcess::AcquireAicpuCommMgr(const std::string &group, CollCommAicpuMgr **aicpuCommMgrPtr)
{
    ReadWriteLock rwlock(g_commAicpuInfo.commAicpuMgrMapMutex);
    rwlock.writeLock();
    // 查找是否已存在该group的通信实例
    auto iter = g_commAicpuInfo.commMgrMap.find(group);
    if (iter != g_commAicpuInfo.commMgrMap.end()) {
        *aicpuCommMgrPtr = iter->second.get();
        HCCL_INFO("[%s]Reuse existing comm group [%s]", __func__, group.c_str());
        rwlock.writeUnlock();
        return HCCL_SUCCESS;
    }
    
    // 未找到则创建新实例
    std::shared_ptr<CollCommAicpuMgr> aicpuCommMgr;
    try {
        aicpuCommMgr = std::make_shared<CollCommAicpuMgr>();
        aicpuCommMgr->AcquireCollCommAicpu();
    } catch (std::exception& e) {
        HCCL_ERROR("[%s]Failed, exception caught:%s", __func__, e.what());
        rwlock.writeUnlock();
        return HCCL_E_PTR;
    }
    
    if (UNLIKELY(!aicpuCommMgr)) {
        HCCL_ERROR("[%s]errNo[0x%016llx] aicpuComm is nullptr", __func__, HCCL_ERROR_CODE(HCCL_E_PTR));
        rwlock.writeUnlock();
        return HCCL_E_PTR;
    }

    // 将新实例加入映射表
    g_commAicpuInfo.commMgrMap[group] = aicpuCommMgr;
    *aicpuCommMgrPtr = aicpuCommMgr.get();
    HCCL_INFO("[%s]Created new comm group [%s]", __func__, group.c_str());
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