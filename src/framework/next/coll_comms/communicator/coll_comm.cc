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


namespace hccl {
CollComm::CollComm(void * comm, uint32_t rankId, const std::string &commName, const ManagerCallbacks& callbacks)
    : comm_(comm), rankId_(rankId), commId_ (commName), callbacks_(callbacks)
{
    CollCommMgr::GetInstance()->RegisteCollComm(this); 
}

CollComm::~CollComm()
{
    CollCommMgr::GetInstance()->UnRegisteCollComm(this); 
    HCCL_INFO("[CollComm][~CollComm] collComm deinit");
}

HcclResult CollComm::Init(void * rankGraph, aclrtBinHandle binHandle, HcclMem cclBuffer, HcclCommConfig *config)
{
    EXCEPTION_HANDLE_BEGIN

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

    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

uint32_t CollComm::GetMyRankId() const
{
    return rankId_;
}

std::string CollComm::GetCollCommName()
{
    return commId_;
}

void CollComm::SetKfcControlTransfer(std::shared_ptr<hccl::HDCommunicate> kfcControlTransferH2D, 
        std::shared_ptr<hccl::HDCommunicate> kfcStatusTransferD2H)
{
    kfcControlTransferH2D_ = kfcControlTransferH2D;
    kfcStatusTransferD2H_ = kfcStatusTransferD2H;
}

void CollComm::SetCommStatus(const HcclCommStatus& status)
{
    commStatus_ = status;
}
HcclCommStatus CollComm::GetCommStatus()
{
    return commStatus_;
}
HcclResult CollComm::Suspend()
{
    TRY_CATCH_RETURN(
        if (commStatus_ == HcclCommStatus::HCCL_COMM_SUSPENDING) {
            HCCL_WARNING("[NsRecovery][Suspend] The current communication has been suspended, no need to suspend again.");
            return HcclResult::HCCL_SUCCESS;
        }
        commStatus_ = HcclCommStatus::HCCL_COMM_SUSPENDING;
        if (!isAicpuKernelLaunched_) { // 非Aicpu场景
            HCCL_INFO("[NsRecovery][Suspend] Aicpu kernel is not launched yet. Suspend host only.");
            return HcclResult::HCCL_SUCCESS;
        }

        // Aicpu场景
        KfcCommand opCmd = KfcCommand::NS_STOP_LAUNCH;
        CHK_RET(kfcControlTransferH2D_->Put(0, sizeof(KfcCommand), reinterpret_cast<uint8_t *>(&opCmd)));
        HCCL_INFO("[NsRecovery][Suspend] send KfcCommand[%d] success, which is NS_STOP_LAUNCH.", opCmd);
        KfcExecStatus opInfo;
        auto timeout   = std::chrono::milliseconds(WAIT_CMD_TIMEOUT);
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
            if (opInfo.kfcStatus == KfcStatus::STOP_LAUNCH_DONE) {
                HCCL_INFO("[NsRecovery][Suspend] received KfcStatus[%d], which is STOP_LAUNCH_DONE", opInfo.kfcStatus);
                return HcclResult::HCCL_E_SUSPENDING;
            } else if (opInfo.kfcStatus == KfcStatus::ERROR){
                HCCL_ERROR("[NsRecovery][Suspend] received KfcStatus[%d], which is ERROR", opInfo.kfcStatus);
                return HcclResult::HCCL_E_INTERNAL;
            } else {
                if((std::chrono::steady_clock::now() - startTime) >= timeout){
                    HCCL_ERROR("[NsRecovery][Suspend] Wait suspend response status timeout[%u ms] and get the opExecStatus is [%u].", WAIT_CMD_TIMEOUT,
                            opInfo.kfcStatus);
                    return HcclResult::HCCL_E_TIMEOUT;
                }
                continue;
            }
        }
    );
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CollComm::Clean()
{
    TRY_CATCH_RETURN(
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
        myRank_->Clean();

        // 再清理device，后续优化全用host管理
        HCCL_INFO("[NsRecovery][Clean] start to clean device, waiting for device STOP_LAUNCH_DONE");
        KfcExecStatus opInfo;
        CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
        if (opInfo.kfcStatus == KfcStatus::STOP_LAUNCH_DONE) {
            HCCL_INFO("[NsRecovery][Clean] received KfcStatus[%d], which is STOP_LAUNCH_DONE", opInfo.kfcStatus);
            // 通知背景线程清理device侧资源
            KfcCommand opCmd = KfcCommand::NS_CLEAN;
            CHK_RET(kfcControlTransferH2D_->Put(0, sizeof(KfcCommand), reinterpret_cast<uint8_t *>(&opCmd)));
            HCCL_INFO("[NsRecovery][Clean] send KfcCommand [%d] success, which is NS_CLEAN", opCmd);
            // 监听背景线程状态
            auto timeout   = std::chrono::milliseconds(WAIT_CMD_TIMEOUT);
            auto startTime = std::chrono::steady_clock::now();
            while (true) {
                CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
                if (opInfo.kfcStatus == KfcStatus::CLEAN_DONE) {
                    HCCL_INFO("[NsRecovery][Clean] received KfcStatus[%d], which is CLEAN_DONE", opInfo.kfcStatus);
                    return HcclResult::HCCL_E_SUSPENDING;
                } else if (opInfo.kfcStatus == KfcStatus::ERROR){
                    HCCL_ERROR("[NsRecovery][Clean] received KfcStatus[%d], which is ERROR", opInfo.kfcStatus);
                    return HcclResult::HCCL_E_INTERNAL;
                } else {
                    if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                        HCCL_ERROR("[NsRecovery][Clean] Wait clean response status timeout[%u ms] and get the opExecStatus is [%u].", WAIT_CMD_TIMEOUT,
                                opInfo.kfcStatus);
                        return HcclResult::HCCL_E_TIMEOUT;
                    }
                    continue;
                }
            }
        } else {
            std::string msg = StringFormat("[NsRecovery][Clean] Aicpu kernel is not stopped yet. Cannot clean.");
            THROW<InternalException>(msg);
            return HcclResult::HCCL_E_INTERNAL;
        }
    );
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CollComm::Resume()
{
    TRY_CATCH_RETURN(
        if (status == CommStatus::COMM_ERROR) {
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
    );
    return HcclResult::HCCL_SUCCESS;
}

}  // namespace hccl