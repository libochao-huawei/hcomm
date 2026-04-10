/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "comm_kfc_aicpu_server.h"
#include <numeric>
#include "log.h"
#include "common/aicpu_kfc_utils.h"
#include "hccl_mc2_ex.h"
#include "framework/aicpu_communicator.h"
#include "framework/aicpu_hccl_process.h"
#include "hccl_common.h"
#include "../../../../legacy/unified_platform/resource/stream/aicpu/sqe_build_a5.h"

using namespace HcclApi;
namespace {
static constexpr u64 TIMEOUT_ERROR_THRESHOLD = 960UL;
static const std::vector<HcclCMDType> SUPPORT_OP_LIST {
    HCCL_CMD_ALLREDUCE, HCCL_CMD_ALLGATHER, HCCL_CMD_REDUCE_SCATTER, HCCL_CMD_ALLTOALLV, HCCL_CMD_ALLTOALL
};

hccl::HcclCommAicpu *FindCommAicpuByName(const std::string &commName)
{
    std::vector<std::pair<std::string, hccl::HcclCommAicpu *>> aicpuCommInfo;
    if (AicpuHcclProcess::AicpuGetCommAll(aicpuCommInfo) != HCCL_SUCCESS) {
        return nullptr;
    }
    for (auto &commInfo : aicpuCommInfo) {
        if (commInfo.second != nullptr && commInfo.first == commName) {
            return commInfo.second;
        }
    }
    return nullptr;
}
}

HcclResult CommKfcAicpuServer::AddOpContext(const OpResCtx *ctx)
{
    CHK_PTR_NULL(ctx);
    CHK_PRT_RET(msgArea_ != nullptr && reinterpret_cast<u64>(msgArea_) != ctx->workspace,
                HCCL_ERROR("Group %u: message area addr should be %#llx, not %#llx.",
                           groupIdx_, msgArea_, ctx->workspace),
                HCCL_E_PARA);
    if (msgArea_ == nullptr) {
        msgArea_ = reinterpret_cast<HcclMsgArea *>(ctx->workspace);
        turnNumsAddr_ = reinterpret_cast<u64>(msgArea_ + 1);
        rankNum_ = static_cast<u32>(ctx->rankSize);
        std::iota(reinterpret_cast<u32 *>(turnNumsAddr_),
                  reinterpret_cast<u32 *>(turnNumsAddr_) + UINT8_MAX + 1U, 0U);
        KeepAlive();
    }

    for (u32 i = 0U; i < 8U; ++i) {
        const AlgInfo &algInfo = ctx->algInfo[i];
        const u64 opParamKey = algInfo.opParam;
        if (opParamKey == 0U) {
            continue;
        }
        if (MatchExecCtx(opParamKey) != nullptr) {
            HCCL_INFO("Group %u: opParamKey %#llx is already added.", groupIdx_, opParamKey);
            continue;
        }

        std::vector<uint8_t> baseOpParam{};
        std::string commName{};
        CHK_RET(LoadOpenOpParamData(opParamKey, commName, baseOpParam));
        CHK_PRT_RET(commName.empty(),
                    HCCL_ERROR("Group %u: empty comm name for opParamKey %#llx.", groupIdx_, opParamKey),
                    HCCL_E_PARA);
        hccl::HcclCommAicpu *commAicpu = FindCommAicpuByName(commName);
        CHK_PRT_RET(commAicpu == nullptr,
                    HCCL_ERROR("Group %u: failed to find commAicpu for commName[%s], opParamKey %#llx.",
                               groupIdx_, commName.c_str(), opParamKey),
                    HCCL_E_PARA);

        ServerExecCtx execCtx{};
        execCtx.offset = algInfo.offset;
        execCtx.opParamKey = opParamKey;
        execCtx.commName = commName;
        execCtx.baseOpParam = std::move(baseOpParam);
        execCtx.commAicpu = commAicpu;
        execCtx.dispatcher = commAicpu->GetDispatcher();
        execCtx.mainStream = &commAicpu->GetMainStream();
        execCtxList_.push_back(execCtx);
        HCCL_INFO("Group %u: add exec ctx for opParamKey %#llx, commName[%s], message area address %#llx.",
                  groupIdx_, opParamKey, commName.c_str(), msgArea_);
    }

    CHK_PRT_RET(execCtxList_.empty(), HCCL_ERROR("Group %u: no exec ctx is added.", groupIdx_), HCCL_E_PARA);
    syncExecCtx_ = &execCtxList_.front();
    return HCCL_SUCCESS;
}

bool IsSupportedOp(HcclCMDType opType)
{
    return std::find(SUPPORT_OP_LIST.begin(), SUPPORT_OP_LIST.end(), opType) != SUPPORT_OP_LIST.end();
}

const ServerExecCtx *CommKfcAicpuServer::MatchExecCtx(u64 opParamKey) const
{
    for (const auto &execCtx : execCtxList_) {
        if (execCtx.opParamKey == opParamKey) {
            return &execCtx;
        }
    }
    return nullptr;
}

HcclResult CommKfcAicpuServer::LaunchOpenCcoreWait(
    const ServerExecCtx &execCtx, u64 waitAddr, u32 turnNum, u64 turnNumsAddr, bool isLast)
{
    CHK_PTR_NULL(execCtx.mainStream);
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId16 = 0;

    CHK_RET(execCtx.mainStream->GetNextSqeBufferAddr(sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId16));

    auto *sqeCtx = execCtx.mainStream->GetSqeContextPtr();
    CHK_PTR_NULL(sqeCtx);

    const u64 valueAddr = turnNumsAddr + static_cast<u64>(turnNum) * sizeof(u32);
    const u32 a5TaskId = (static_cast<u32>(sqeCtx->buffer.filpNum) << 16) | taskId16;

    Hccl::BuildA5SqeCCoreNotifyWait(0, a5TaskId, waitAddr, valueAddr, isLast, sqeBuffer);
    *sqeTypeAddr = static_cast<uint8_t>(SqeType::A5_CCORE_NOTIFY_WAIT_SQE);
    sqeCtx->buffer.addInfo[taskId16 % hccl::HCCL_SQE_MAX_CNT]
        = (static_cast<u32>(turnNum) << 16) | static_cast<u32>(isLast);

    return LaunchTask(execCtx.dispatcher, *execCtx.mainStream);
}

HcclResult CommKfcAicpuServer::LaunchOpenCcorePost(
    const ServerExecCtx &execCtx, u64 recordAddr, u32 turnNum, u64 turnNumsAddr)
{
    CHK_PTR_NULL(execCtx.mainStream);
    uint8_t *sqeBuffer = nullptr;
    uint8_t *sqeTypeAddr = nullptr;
    uint8_t *sqeDfxInfoAddr = nullptr;
    uint16_t taskId16 = 0;

    CHK_RET(execCtx.mainStream->GetNextSqeBufferAddr(sqeBuffer, sqeTypeAddr, sqeDfxInfoAddr, taskId16));

    auto *sqeCtx = execCtx.mainStream->GetSqeContextPtr();
    CHK_PTR_NULL(sqeCtx);

    const u64 valueAddr = turnNumsAddr + static_cast<u64>(turnNum) * sizeof(u32);
    const u32 a5TaskId = (static_cast<u32>(sqeCtx->buffer.filpNum) << 16) | taskId16;

    Hccl::BuildA5SqeCCoreNotifyRecord(0, a5TaskId, recordAddr, valueAddr, sqeBuffer);
    *sqeTypeAddr = static_cast<uint8_t>(SqeType::A5_CCORE_NOTIFY_RECORD_SQE);
    sqeCtx->buffer.addInfo[taskId16 % hccl::HCCL_SQE_MAX_CNT] = turnNum;

    return LaunchTask(execCtx.dispatcher, *execCtx.mainStream);
}

HcclResult CommKfcAicpuServer::FormatOpParamFromMsg(
    const HcclMsg &msg, HcclMsgExt &extMsg, const ServerExecCtx &execCtx, u32 repeatIdx, std::vector<uint8_t> &opParam)
{
    CHK_PTR_NULL(execCtx.mainStream);
    return FormatOpenOpParamDataFromMsg(
        execCtx.baseOpParam, msg, extMsg, rankNum_, repeatIdx, execCtx.mainStream->ptr(), opParam);
}

HcclResult CommKfcAicpuServer::LaunchOpenAicpuKernelServer(std::vector<uint8_t> &opParam)
{
    return LaunchOpenOpParamData(opParam);
}

HcclResult CommKfcAicpuServer::Orchestrate(const HcclMsg &msg, HcclMsgExt &extMsg, u32 msgPos)
{
    KeepAlive();
    CHK_PTR_NULL(msgArea_);
    const HcclCMDType opType = static_cast<HcclCMDType>(msg.commType.prepareType);
    CHK_PRT_RET(!IsSupportedOp(opType), HCCL_ERROR("Unsupported comm type %u.", static_cast<u32>(opType)), HCCL_E_PARA);

    const u64 opParamKey = msg.addMsg.v1Msg.ccOpTilingData;
    const ServerExecCtx *execCtx = MatchExecCtx(opParamKey);
    CHK_PRT_RET(execCtx == nullptr, HCCL_ERROR("Group %u: exec ctx %#llx is not added by host.", groupIdx_, opParamKey),
        HCCL_E_PARA);

    const HcclHandle handle = msg.addMsg.v1Msg.selfHandleID;
    CHK_PRT_RET(handle < 0, HCCL_ERROR("Group %u: invalid handle id %d.", groupIdx_, handle), HCCL_E_INTERNAL);
    const u32 repeatCnt = static_cast<u32>(msg.addMsg.v1Msg.repeatCnt);
    const u64 waitAddr = reinterpret_cast<u64>(&(msgArea_->commMsg.singleMsg.commitTurnCnt[msgPos].cnt));
    const u64 recordAddr = reinterpret_cast<u64>(&(msgArea_->commMsg.singleMsg.finishedTurnCnt[msgPos].cnt));

    for (u32 i = 0U; i < repeatCnt; ++i) {
        std::vector<uint8_t> runParam{};
        CHK_RET(FormatOpParamFromMsg(msg, extMsg, *execCtx, i, runParam));
        const u32 turnIdx = i + 1U;
        CHK_RET(LaunchOpenCcoreWait(*execCtx, waitAddr, turnIdx, turnNumsAddr_, turnIdx == repeatCnt));
        CHK_RET(LaunchOpenAicpuKernelServer(runParam));
        CHK_RET(LaunchOpenCcorePost(*execCtx, recordAddr, turnIdx, turnNumsAddr_));
    }
    SetMsgPosByHandle(handle, msgPos);
    SetRepeatByHandle(handle, repeatCnt);
    return HCCL_SUCCESS;
}

HcclResult CommKfcAicpuServer::Finalize(u32 msgPos)
{
    KeepAlive();
    return HCCL_SUCCESS;
}

HcclResult CommKfcAicpuServer::IsAllTaskFinished(u32 msgPos, bool &isFinish)
{
    CHK_PTR_NULL(msgArea_);

    isFinish = false;
    // opHandles_能保证不为空，同一个通信域检查任何一个ophandle即可
    void *firstHandle = ctxToOpHandle_.begin()->second;
    if (HcclCheckFinishByStream(firstHandle) != HCCL_SUCCESS) {
        return HCCL_SUCCESS;
    }

    HcclTaskStatus status;
    if (HcclGetTaskStatus(firstHandle, &status) != HCCL_SUCCESS || status != HcclTaskStatus::HCCL_NORMAL_STATUS) {
        HCCL_ERROR("Group %u: abnormal task status %u.", groupIdx_, static_cast<u32>(status));
        return HCCL_E_INTERNAL;
    }

    msgArea_->commMsg.singleMsg.finishedTurnCnt[msgPos].cnt = FINALIZE_FINISH_CNT;
#ifdef __aarch64__
    __asm__ __volatile__("dsb st" : : : "memory");
#endif
    isFinish = true;
    HCCL_INFO("Group %u: all task is finished at message pos %u.", groupIdx_, msgPos);
    for (auto it: ctxToOpHandle_) {
        CHK_RET(HcclReleaseComm(it.second));
        HCCL_INFO("Group %u: Op handle %#llx is released, HCCL context %#llxx.", groupIdx_, it.second, it.first);
    }
    return HCCL_SUCCESS;
}

HcclResult CommKfcAicpuServer::InterGroupSync(const CommKfcAicpuServer &otherServer, HcclHandle handle)
{
    KeepAlive();
    u32 msgPos, repeat;
    HcclResult ret = otherServer.GetServerInfoForSync(handle, msgPos, repeat);
    if (ret != HCCL_SUCCESS) {
        HCCL_INFO("Group %u: group sync info is not obtained, return code %u.", groupIdx_, ret);
        return ret;
    }
    CHK_PRT_RET(msgPos >= HCCL_MSG_CNT, HCCL_ERROR("Group %u: invalid message index %u.", groupIdx_, msgPos),
                HCCL_E_PARA);

    HcclMsgArea *msgArea = otherServer.GetMsgAreaAddr();
    CHK_PTR_NULL(msgArea);
    const u64 waitAddr = reinterpret_cast<u64>(&(msgArea->commMsg.singleMsg.finishedTurnCnt[msgPos].cnt));
    HCCL_INFO("Group %u: group sync for handle %d: message index %u, finish count %u.",
              groupIdx_, handle, msgPos, repeat);
    return HcclLaunchCcoreWait(ctxToOpHandle_.begin()->second, waitAddr, repeat, turnNumsAddr_, false);
}

HcclResult CommKfcAicpuServer::CheckTimeOut(u32 msgPos)
{
    if (!IsTimeout()) {
        return HCCL_SUCCESS;
    }
    const bool error = (timeout_ >= TIMEOUT_ERROR_THRESHOLD);
    HcclResult ret;
    if (error) {
        HCCL_ERROR("Group %u: timeout %u seconds at message pos %u.", groupIdx_, timeout_, msgPos);
        ret = HCCL_E_TIMEOUT;
    } else {
        HCCL_RUN_INFO("Group %u: timeout %u seconds at message pos %u.", groupIdx_, timeout_, msgPos);
        ret = HCCL_E_AGAIN;
    }
    timeout_ *= 2U;
    return ret;
}

HcclResult CommKfcAicpuServer::GetServerInfoForSync(HcclHandle handle, u32 &msgPos, u32 &repeat) const
{
    CHK_PRT_RET(handle < 0, HCCL_ERROR("Group %u: invalid handle id %d.", groupIdx_, handle), HCCL_E_PARA);
    auto it = handleIdToMsgPos_.find(handle);
    if (it == handleIdToMsgPos_.end()) {
        HCCL_INFO("Group %u: handle %d in this group is not ready.", groupIdx_, handle);
        return HCCL_E_AGAIN;
    }
    msgPos = it->second;

    it = handleIdToRepeat_.find(handle);
    CHK_PRT_RET(it == handleIdToRepeat_.end(),
                HCCL_ERROR("Group %u: handle %d in this group is not ready.", groupIdx_, handle),
                HCCL_E_INTERNAL);
    repeat = it->second;
    return HCCL_SUCCESS;
}

HcclResult CommKfcAicpuServer::ErrorDfxProcess(HcclResult errorCode)
{
    void *firstHandle = ctxToOpHandle_.begin()->second;
    if (errorCode == HCCL_SUCCESS) {
        return errorCode;
    } else if (errorCode == HCCL_E_AGAIN) {
        AicpuKfcUtils::PrintAllHcclMsgArea(msgArea_, rankNum_);
        HcclPrintTaskExceptionAllComm(firstHandle);
        errorCode = HCCL_SUCCESS;
    } else {
        AicpuKfcUtils::PrintAllHcclMsgArea(msgArea_, rankNum_, true);
        HcclPrintTaskExceptionAllComm(firstHandle);
    }
    return errorCode;
}
