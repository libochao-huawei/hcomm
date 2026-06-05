/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "roce_cqe_err_handler_lite.h"
#include "aicpu_indop_process.h"
#include "coll_comm_aicpu.h"
#include "coll_comm_aicpu_mgr.h"
#include "read_write_lock.h"
#include "error_message_v2.h"
#include "log.h"

namespace hcomm {

RoceCqeErrHandlerLite &RoceCqeErrHandlerLite::GetInstance()
{
    static RoceCqeErrHandlerLite instance;
    return instance;
}

RoceCqeErrHandlerLite::RoceCqeErrHandlerLite()
{
}

RoceCqeErrHandlerLite::~RoceCqeErrHandlerLite()
{
    initFlag_ = false;
}

void RoceCqeErrHandlerLite::Init(u32 devId)
{
    if (initFlag_) {
        HCCL_DEBUG("[%s] already initialized", __func__);
        return;
    }
    initFlag_ = true;
    devId_ = devId;
    HCCL_INFO("[%s] success, devId_[%u]", __func__, devId_);
}

void RoceCqeErrHandlerLite::Call()
{
    if (stopCall_) {
        return;
    }

    HcclResult ret = HandleRoceCqeErr();
    if (ret != HCCL_SUCCESS) {
        stopCall_ = true;
        HCCL_ERROR("[%s] HandleRoceCqeErr fail, set stopCall_[%d]", __func__, stopCall_);
    }
}

HcclResult RoceCqeErrHandlerLite::HandleRoceCqeErr()
{
    ReadWriteLockBase &commAicpuMapMutex = AicpuIndopProcess::AicpuGetCommMutex();
    ReadWriteLock rwlock(commAicpuMapMutex);
    rwlock.readLock();

    std::vector<std::pair<std::string, CollCommAicpuMgr *>> aicpuCommInfo;
    HcclResult ret = AicpuIndopProcess::AicpuGetCommAll(aicpuCommInfo);
    if (ret != HCCL_SUCCESS) {
        rwlock.readUnlock();
        HCCL_ERROR("[%s] AicpuGetCommAll fail, ret[%d]", __func__, ret);
        return ret;
    }

    for (auto &commInfo : aicpuCommInfo) {
        CollCommAicpu *aicpuComm = commInfo.second->GetCollCommAicpu();
        if (aicpuComm == nullptr) {
            continue;
        }

        if (aicpuComm->GetCommmStatus() == HcclCommStatus::HCCL_COMM_STATUS_INVALID) {
            continue;
        }

        const auto &roceTransportMap = aicpuComm->GetRoceTransportMap();
        for (const auto &kv : roceTransportMap) {
            if (kv.second == nullptr) {
                continue;
            }

            std::vector<RoceCqeErrInfo> errInfos;
            ret = kv.second->PollErrorCqe(errInfos);
            if (ret != HCCL_SUCCESS) {
                HCCL_ERROR("[%s] PollErrorCqe fail for group[%s], ret[%d]",
                    __func__, aicpuComm->GetIdentifier().c_str(), ret);
                continue;
            }

            for (const auto &errInfo : errInfos) {
                ret = ProcessRoceCqeErr(aicpuComm, errInfo);
                if (ret != HCCL_SUCCESS) {
                    HCCL_ERROR("[%s] ProcessRoceCqeErr fail for group[%s]",
                        __func__, aicpuComm->GetIdentifier().c_str());
                }
            }
        }
    }
    rwlock.readUnlock();
    return HCCL_SUCCESS;
}

HcclResult RoceCqeErrHandlerLite::ProcessRoceCqeErr(CollCommAicpu *aicpuComm,
    const RoceCqeErrInfo &errInfo)
{
    if (aicpuComm == nullptr) {
        return HCCL_E_PARA;
    }

    if (errInfo.status == 0) {
        return HCCL_SUCCESS;
    }

    return ReportErrorToHost(aicpuComm, errInfo);
}

HcclResult RoceCqeErrHandlerLite::ReportErrorToHost(CollCommAicpu *aicpuComm,
    const RoceCqeErrInfo &errInfo)
{
    if (aicpuComm == nullptr) {
        return HCCL_E_PARA;
    }

    Hccl::ErrorMessageReport errMsgInfo{};

    // 填入通信域标识信息
    errMsgInfo.rankId = aicpuComm->GetTopoInfo().userRank;
    errMsgInfo.rankSize = aicpuComm->GetTopoInfo().userRankSize;
    CHK_SAFETY_FUNC_RET(memcpy_s(errMsgInfo.group, sizeof(errMsgInfo.group),
        aicpuComm->GetIdentifier().c_str(), aicpuComm->GetIdentifier().size()));

    // 填入 CQE 错误信息: status 和 qpn
    // 使用 ErrorMessageReport 的 rtCqErrorType / rtCqErrorCode 字段来传递 RDMA CQE 错误
    errMsgInfo.rtCqErrorCode = errInfo.status;
    errMsgInfo.taskType = Hccl::TaskParamType::TASK_SDMA;  // 标记为 RoCE 相关

    HCCL_ERROR("[RoCE][CQE_ERR] group[%s], qpn[%u], cqe_status[%u], "
        "rankId[%u], rankSize[%u], "
        "time[%ld.%06ld]",
        aicpuComm->GetIdentifier().c_str(),
        errInfo.qpn,
        errInfo.status,
        errMsgInfo.rankId,
        errMsgInfo.rankSize,
        static_cast<long>(errInfo.time.tv_sec),
        static_cast<long>(errInfo.time.tv_usec));

    // 通过 HDC D2H 通道上报到 Host 端
    HcclResult ret = aicpuComm->SendErrorMessageReportToHost(errMsgInfo);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] SendErrorMessageReportToHost fail, group[%s], ret[%d]",
            __func__, aicpuComm->GetIdentifier().c_str(), ret);
        return ret;
    }

    HCCL_RUN_INFO("[%s] RoCE CQE error reported to host, group[%s], qpn[%u], status[%u]",
        __func__, aicpuComm->GetIdentifier().c_str(), errInfo.qpn, errInfo.status);
    return HCCL_SUCCESS;
}

} // namespace hcomm
