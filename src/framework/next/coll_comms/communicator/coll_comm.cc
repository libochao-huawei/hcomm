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
#include "kfc.h"
#include "dlhal_function.h"
#include "hcclCommTaskException.h"

namespace hccl {
CollComm::CollComm(void * comm, uint32_t rankId, const std::string &commName, const ManagerCallbacks& callbacks)
    : comm_(comm), rankId_(rankId), commId_ (commName), callbacks_(callbacks)
{}

CollComm::~CollComm()
{
    HCCL_INFO("[CollComm][~CollComm] collComm deinit");
    (void)DestroyAicpuComm();
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

 	if (!hcclCommDfx_) {
        EXECEPTION_CATCH(hcclCommDfx_ = std::make_unique<HcclCommDfx>(), return HCCL_E_PTR);
 	}
 	CHK_RET(hcclCommDfx_->Init(deviceLogicId_, commId_));
    CHK_RET(InitTaskExceptionHandler());
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

HcclResult CollComm::DestroyAicpuComm()
{
    if (callbacks_.getAicpuCommState()) {
        CHK_SMART_PTR_NULL(kfcControlTransferH2D_);
        CHK_SMART_PTR_NULL(kfcStatusTransferD2H_);

        Hccl::KfcCommand opCmd = Hccl::KfcCommand::DESTROY_AICPU_COMM;
        CHK_RET(kfcControlTransferH2D_->Put(0, sizeof(Hccl::KfcCommand), reinterpret_cast<uint8_t *>(&opCmd)));
        HCCL_RUN_INFO("[%s]group[%s] send Hccl::KfcCommand[%d] success", __func__, commId_.c_str(), opCmd);

        Hccl::KfcExecStatus opInfo;
        constexpr u32 WAIT_CMD_TIMEOUT = 10 * 1000; // 最大等待10秒
        auto timeout = std::chrono::milliseconds(WAIT_CMD_TIMEOUT);
        auto startTime = std::chrono::steady_clock::now();

        while (true) {
            CHK_RET(kfcStatusTransferD2H_->Get(0, sizeof(Hccl::KfcExecStatus), reinterpret_cast<uint8_t *>(&opInfo)));
            if (opInfo.kfcStatus == Hccl::KfcStatus::DESTROY_AICPU_COMM_DONE) {
                HCCL_RUN_INFO("[%s]get Hccl::KfcStatus[%d] success", __func__, opInfo.kfcStatus);
                return HCCL_SUCCESS;
            } else if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
                HCCL_ERROR("[%s]timeout, maxTime[%u ms] and get the opExecStatus is [%u].",
                    __func__, WAIT_CMD_TIMEOUT, opInfo.kfcStatus);
                return HCCL_E_TIMEOUT;
            }
            usleep(TEN_MILLISECOND_OF_USLEEP);
        }
    }
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

HcclResult CollComm::InitTaskExceptionHandler()
{
    hcomm::TaskExceptionHost* handler = hcomm::TaskExceptionHostManager::GetHandler(static_cast<size_t>(deviceLogicId_));
    CHK_PTR_NULL(handler);
    CHK_RET(handler->Register());
    return HCCL_SUCCESS;
}

void CollComm::RegisterAicpuTaskExceptionCallback(u32 streamId)
{
    HCCL_INFO("[%s] start, commId[%s], streamId[%u]", __func__, commId_.c_str(), streamId);
    auto getAicpuTaskExceptionCallBack = [this]() {return this->GetAicpuTaskException();};
    hcomm::TaskExceptionHostManager::RegisterGetAicpuTaskExceptionCallBack(streamId, deviceLogicId_,
        getAicpuTaskExceptionCallBack);
    return ;
}

Hccl::ErrorMessageReport CollComm::GetAicpuTaskException()
{
    Hccl::ErrorMessageReport errorMessage;
    CHK_PRT_RET(kfcStatusTransferD2H_ == nullptr, HCCL_ERROR("[%s]fail, d2h is nullptr", __func__), errorMessage);
    
    HcclResult ret = kfcStatusTransferD2H_->Get(sizeof(Hccl::KfcStatus) + sizeof(Hccl::KfcErrType),
       sizeof(errorMessage),reinterpret_cast<uint8_t *>(&errorMessage));
   
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[%s]fail, group [%s], ret[%u]", __func__, commId_.c_str() ,ret), errorMessage);
    HCCL_INFO("[%s]group[%s] success", __func__, commId_.c_str());
   return errorMessage;
}

uint32_t CollComm::UpdateIndex()
{
    return index_ += 1;
}

}  // namespace hccl
