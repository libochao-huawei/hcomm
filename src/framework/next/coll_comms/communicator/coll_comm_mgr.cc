/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm_mgr.h"
#include "ns_recovery/task_abort_handler.h"

namespace hccl {

CollCommMgr* CollCommMgr::instance_ = nullptr;
static std::once_flag instanceFlag;

CollCommMgr* CollCommMgr::GetInstance() 
{
    std::call_once(instanceFlag, [&] {
        instance_ = new CollCommMgr();
    });
    return instance_;
}

void CollCommMgr::RegisteCollComm(CollComm* collComm)
{
    std::lock_guard<std::mutex> lock(mutex_);
    allCollComms_[collComm->GetCommId()] = collComm;
    // 注册到需要的地方
    HcclTaskAbortHandler::GetInstance().Register(collComm);
}

void CollCommMgr::UnRegisteCollComm(CollComm* collComm)
{
    std::lock_guard<std::mutex> lock(mutex_);
    allCollComms_.erase(collComm->GetCommId());
    // 从通信域里面注销
    HcclTaskAbortHandler::GetInstance().UnRegister(collComm);
}

std::unordered_map<std::string, CollComm*> CollCommMgr::GetAllCollComms()
{
    return allCollComms_;
}

}
