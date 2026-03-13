/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm_aicpu_destroy_func.h"
#include "aicpu_indop_process.h"
#include "read_write_lock.h"

namespace hccl {
CollCommAicpuDestroyFunc &CollCommAicpuDestroyFunc::GetInstance()
{
    static CollCommAicpuDestroyFunc func;
    return func;
}

void CollCommAicpuDestroyFunc::Call()
{
    if (stopCall_ == true) {
        return;
    }

    HcclResult ret = Process();
    if (ret != HCCL_SUCCESS) {
        stopCall_ = true;
        HCCL_ERROR("[%s]Process fail, set stopCall_[%d]", __func__, stopCall_); // 函数调用失败，停止调用避免刷屏
    }
}

HcclResult CollCommAicpuDestroyFunc::Process()
{
    ReadWriteLockBase &commAicpuMapMutex = AicpuIndopProcess::AicpuGetCommMutex();
    ReadWriteLock rwlock(commAicpuMapMutex);
    rwlock.readLock();

    std::vector<std::pair<std::string, CollCommAicpuMgr *>> aicpuCommInfo;
    CHK_RET(AicpuIndopProcess::AicpuGetCommAll(aicpuCommInfo));
    std::vector<CollCommAicpu *> destroyComm;

    for (auto &commInfo : aicpuCommInfo) {
        CollCommAicpu *aicpuComm = commInfo.second->GetCollCommAicpu();
        CHK_PTR_NULL(aicpuComm);

        Hccl::KfcCommand cmd = Hccl::KfcCommand::NONE;
        CHK_RET(aicpuComm->BackGroundGetCmd(cmd));
        if (cmd != Hccl::KfcCommand::DESTROY_AICPU_COMM) {
            continue;
        }
        HCCL_RUN_INFO("[%s]Recv DESTROY_AICPU_COMM cmd, group[%s]", __func__, aicpuComm->GetIdentifier().c_str());
        destroyComm.push_back(aicpuComm);
    }
    rwlock.readUnlock();

    rwlock.writeLock();
    for (CollCommAicpu *comm: destroyComm) {
        CHK_RET(comm->BackGroundSetStatus(Hccl::KfcStatus::DESTROY_AICPU_COMM_DONE));
        CHK_RET(AicpuIndopProcess::AicpuDestroyCommbyGroup(comm->GetIdentifier().c_str()));
        HCCL_RUN_INFO("[%s]group[%s] destroy success", __func__, comm->GetIdentifier().c_str());
    }
    rwlock.readUnlock();
    return HCCL_SUCCESS;
}
}  // namespace hccl