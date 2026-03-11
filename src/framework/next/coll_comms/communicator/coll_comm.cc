/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "coll_comm.h"
// #include "rank_graphs/rank_graph.h"
#include "exception_handler.h"
#include "rank_graph_v2.h"
#include "coll_comm_mgr.h"
#include "kfc.h"
#include "dlhal_function.h"

namespace hccl {
CollComm::CollComm(void * comm, uint32_t rankId, const std::string &commName, const ManagerCallbacks& callbacks)
    : comm_(comm), rankId_(rankId), commId_ (commName), callbacks_(callbacks)
{
}

CollComm::~CollComm()
{
    CollCommMgr::GetInstance()->UnRegisteCollComm(this); 
    HCCL_INFO("[CollComm][~CollComm] collComm deinit");
}

HcclResult CollComm::Init(void * rankGraph, aclrtBinHandle binHandle, HcclMem cclBuffer, HcclCommConfig *config)
{
    EXCEPTION_HANDLE_BEGIN

    CHK_RET(DlHalFunction::GetInstance().DlHalFunctionInit());
    EXECEPTION_CATCH(rankgraph_ = std::make_unique<RankGraphV2>(rankGraph), return HCCL_E_PTR);
    uint32_t rankNum = 0;
    CHK_PTR_NULL(rankgraph_);
    CHK_RET(rankgraph_->GetRankSize(&rankNum));
    u32 threadNum = 0xffffffff;
    u32 notifyNumPerThread = 0xffffffff;
    if (!commEngineResMgr_) {
        EXECEPTION_CATCH(commEngineResMgr_ = std::make_unique<CommEngineResMgr>(),
            return HCCL_E_PTR);
        CHK_PRT(commEngineResMgr_->Init(threadNum, notifyNumPerThread, commId_, binHandle, callbacks_));
    }

    if (!contextMgr_) {
        EXECEPTION_CATCH(contextMgr_ = std::make_unique<ContextManager>(), return HCCL_E_PTR);
    }

    EXECEPTION_CATCH(myRank_ = std::make_shared<MyRank>(binHandle, rankId_, config_, callbacks_), return HCCL_E_PTR);
    uint32_t opExpansionMode = 0;
    if (config) {
        opExpansionMode = config->hcclOpExpansionMode;
    }
    CHK_RET(myRank_->Init(cclBuffer, opExpansionMode, rankNum));
    s32 deviceId = 0;
    CHK_RET(hrtGetDevice(&deviceId));
    CHK_RET(hrtGetDevice(&deviceLogicId_));

    CHK_RET(InitHDCommunicate());
    
    CollCommMgr::GetInstance()->RegisteCollComm(this); 
    commStatus_ = HcclCommStatus::HCCL_COMM_READY;

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

uint32_t CollComm::GetMyRankId() const
{
    return rankId_;
}

HcclResult CollComm::InitHDCommunicate()
{
    // 初始化aicpu进程 host-device 共享内存
    EXECEPTION_CATCH((kfcControlTransferH2D_ = 
        std::make_shared<hccl::HDCommunicate>(deviceLogicId_, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand))),
        return HCCL_E_PTR);
    CHK_RET(kfcControlTransferH2D_->InitHost());

    EXECEPTION_CATCH((kfcStatusTransferD2H_ = 
        std::make_shared<hccl::HDCommunicate>(deviceLogicId_, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus))),
        return HCCL_E_PTR);
    CHK_RET(kfcStatusTransferD2H_->InitHost());
    return HCCL_SUCCESS;

    return HCCL_SUCCESS;
}

HcclResult CollComm::GetHDCommunicate(
    HDCommunicateParams &kfcControlTransferH2DParams, HDCommunicateParams &kfcStatusTransferD2HParams)
{
    CHK_SMART_PTR_NULL(kfcControlTransferH2D_);
    CHK_SMART_PTR_NULL(kfcStatusTransferD2H_);
    kfcControlTransferH2DParams = kfcControlTransferH2D_->GetCommunicateParams();
    kfcStatusTransferD2HParams = kfcStatusTransferD2H_->GetCommunicateParams();
    HCCL_INFO("%s success, group[%s]", __func__, commId_.c_str());
    return HCCL_SUCCESS;
}

std::string CollComm::GetCollCommName()
{
    return commId_;
}

HcclCommStatus CollComm::GetCommStatus()
{
    return commStatus_;
}

HcclResult CollComm::Suspend()
{
    if (commStatus_ == HcclCommStatus::HCCL_COMM_SUSPENDING) {
        HCCL_WARNING("[NsRecovery][Suspend] The current communication has been suspended, no need to suspend again.");
        return HcclResult::HCCL_SUCCESS;
    }
    commStatus_ = HcclCommStatus::HCCL_COMM_SUSPENDING;

    return myRank_->StopLaunch();
}

HcclResult CollComm::Clean()
{
    if (commStatus_ != HcclCommStatus::HCCL_COMM_SUSPENDING) {
        HCCL_ERROR("[NsRecovery][Clean] The current communication is not suspended, cannot clean.");
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    if (isCleaned_) {
        HCCL_WARNING("[NsRecovery][Clean] The current communication has been cleaned, no need to clean again.");
        return HcclResult::HCCL_SUCCESS;
    }
    isCleaned_ = true;

    // 先清理Host
    return myRank_->Clean();
}

HcclResult CollComm::Resume()
{
    if (commStatus_ == HcclCommStatus::HCCL_COMM_UNKNOWN) {
        HCCL_ERROR("[NsRecovery][Resume] Comm has been error, can not resume now!");
        return HcclResult::HCCL_E_INTERNAL;
    }
    if (commStatus_ != HcclCommStatus::HCCL_COMM_SUSPENDING) {
        HCCL_WARNING("[NsRecovery][Resume] The current communication is normal, no need to resume.");
        return HcclResult::HCCL_SUCCESS;
    }
    
    HCCL_INFO("[NsRecovery][Resume] start to Resume.");
    if (myRank_) {
        myRank_->Resume();
    } else {
        HCCL_WARNING("[NsRecovery][Resume] myRank nullptr, no need to resume.");
    }
    commStatus_ = HcclCommStatus::HCCL_COMM_READY;
    isCleaned_ = false;
    HCCL_INFO("[NsRecovery][Resume] Resume success.");
    return HcclResult::HCCL_SUCCESS;
}

}  // namespace hccl