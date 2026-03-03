/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm_lite_mgr.h"
#include "daemon/aicpu_daemon_service.h"
#include "ns_resume/aicpu/ns_resume_lite.h"

namespace hccl {

CollCommLiteMgr* CollCommLiteMgr::instance_ = nullptr;
static std::once_flag instanceFlag;

CollCommLiteMgr* CollCommLiteMgr::GetInstance() 
{
    std::call_once(instanceFlag, [&] {
        instance_ = new CollCommLiteMgr();
    });
    return instance_;
}

CollCommLiteMgr::CollCommLiteMgr()
{
    HCCL_INFO("CommunicatorImplLiteMgr:: start");
    static auto commandToBackGroud = CommandToBackGroud::Default;
    HCCL_INFO("CommunicatorImplLiteMgr:: gen daemon service run func");
    static auto daemonServiceRun = [](void *info) {
        AicpuDaemonService::GetInstance().ServiceRun(info);
    };
    HCCL_INFO("CommunicatorImplLiteMgr:: gen daemon service stop func");
    static auto daemonServiceStop = [](void *info) {
        AicpuDaemonService::GetInstance().ServiceStop(info);
    };

    // 注册守护进程函数
    AicpuDaemonService::GetInstance().Register(&NsResumeLiteFunc::GetInstance());

    // 启动背景线程
    if (StartMC2MaintenanceThread != nullptr) {
        StartMC2MaintenanceThread(daemonServiceRun, &commandToBackGroud, daemonServiceStop, &commandToBackGroud);
        HCCL_INFO("[CommunicatorImplLiteMgr] start BackGround thread success.");
    } else {
        HCCL_WARNING("Aicpu api StartMC2MaintenanceThread func is nullptr");
    }
    HCCL_INFO("CollCommLiteMgr::end");
}
CollCommLiteMgr::~CollCommLiteMgr()
{
}

void CollCommLiteMgr::RegisteCollComm(CollCommAicpu* collComm)
{
    allCollCommLites_[collComm->GetIdentifier()] = collComm;
}

void CollCommLiteMgr::UnRegisteCollComm(CollCommAicpu* collComm)
{
    allCollCommLites_.erase(collComm->GetIdentifier());
}

std::unordered_map<std::string, CollCommAicpu*> CollCommLiteMgr::GetAllCollComms()
{
    return allCollCommLites_;
}

}