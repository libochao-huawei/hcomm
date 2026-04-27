/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_kernel.h"
#include "ccu_rep_v1.h"
#include "ccu_kernel_resource.h"
#include "ccu_assist_v1.h"
#include "ccu_microcode_v1.h"

#include "exception_util.h"
#include "ccu_api_exception.h"
#include "ccu_dev_mgr_imp.h"
#include "env_config.h"
#include "ccu_rep_type_v1.h"

#include "hcomm_c_adpt.h"

#include "ccu_rep_context_v1.h"
#include "../../endpoint_pairs/channels/ccu/ccu_urma_channel.h"
#include "ccu_jetty_mgr.h"
#include "ccu_assist.h"
#include "hccl_comm_pub.h"
#include "hcclCommDfx.h"
#include "task_info.h"

#include "task_param.h"
#include "ccu_ctx.h"


namespace hcomm {

constexpr uint32_t TOKEN_VALUE_INDEX = 2;
constexpr uint16_t INVALID_U16 = 65535;

template <typename T> T CcuKernel::CreateResAssist(std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord)
{
    // 获取DieId
    uint32_t dieId = GetDieId(); // 外部检查避免越界
    resRecord[dieId].emplace_back(this);

    auto& item = resRecord[dieId].back();
    item.Reset(resRecord[dieId].size(), dieId);
    return item;
}

template <typename T> std::vector<T> CcuKernel::CreateBlockResAssist(
    const uint32_t count, std::array<std::vector<T>, CCU_MAX_IODIE_NUM> &resRecord)
{
    std::vector<T> block;
    // 获取DieId
    uint32_t dieId = GetDieId(); // 外部检查避免越界
    block.reserve(count);
    for (size_t i = 0; i < count; i++) {
        block.emplace_back(this);
        block.back().Reset(resRecord[dieId].size() + i, dieId);
    }
    resRecord[dieId].insert(resRecord[dieId].end(), block.begin(), block.end());
    return block;
}

CcuKernel::CcuKernel(const CcuKernelArg &arg)
{
    HCCL_INFO("Construct CcuKernel: %s", arg.GetKernelSignature().GetData().c_str());
    channels_ = arg.channels;
}

CcuKernel::~CcuKernel()
{
}

static HcclResult GetDieIdByChannel(const ChannelHandle channel, uint32_t &dieId)
{
    void *channelPtr{nullptr};
    CHK_RET(static_cast<HcclResult>(HcommChannelGet(channel, &channelPtr)));
    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
    if (channelImpl == nullptr) {
        HCCL_ERROR("[%s] failed to cast channel[0x%llx] to CcuUrmaChannel", __func__, channel);
        return HcclResult::HCCL_E_PTR;
    }
    dieId = channelImpl->GetDieId();
    HCCL_INFO("[%s]channelHandle[0x%llx], dieId[%u]", __func__, channel, dieId);
    return HcclResult::HCCL_SUCCESS;
}

static HcclResult GetDieIdByChannels(const std::vector<ChannelHandle> &channels, uint32_t &dieId)
{
    if (channels.empty()) {
        dieId = 0;
        return HcclResult::HCCL_SUCCESS;
    }

    uint32_t firstDieId = 0;
    CHK_RET(GetDieIdByChannel(channels[0], firstDieId));
    for (const auto channel : channels) {
        uint32_t nextDieId = 0;
        CHK_RET(GetDieIdByChannel(channel, nextDieId));
        if (firstDieId != nextDieId) {
            HCCL_ERROR("[%s] failed, the dies of channels are not same.", __func__);
            return HcclResult::HCCL_E_PARA;
        }
    }

    dieId = firstDieId;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::Init()
{
    // 根据channels 0 判断dieId
    // 当前默认给的所有channelhandle都属于一个die
    uint32_t dieId{0};
    CHK_RET(GetDieIdByChannels(channels_, dieId));
    CHK_PRT_RET(dieId >= CCU_MAX_IODIE_NUM,
        HCCL_ERROR("[CcuKernel][%s] failed, dieId[%u] should be less than [%u].",
            __func__, dieId, CCU_MAX_IODIE_NUM),
        HcclResult::HCCL_E_PARA);

    SetDieId(dieId);
    profilingInfo.clear();
    CHK_RET(Algorithm());
    // 生成SQE粒度profiling信息
    AddSqeProfiling();
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::GeneTaskParam(const CcuTaskArg &arg, std::vector<CcuTaskParam> &taskParams)
{
    auto args    = GeneArgs(arg);
    auto agrsNum = args.size();
    if (agrsNum != loadArgIndex_) {
        HCCL_ERROR("[CcuKernel][%s] failed, args number does not match the Load instruction, "
            "agrsNum = %d, loadArgInstr= %u", __func__, agrsNum, loadArgIndex_);
        return HcclResult::HCCL_E_INTERNAL;
    }

    if (instrInfo_.missionInstrCount == 0 || instrInfo_.instrVec.empty()) {
        HCCL_ERROR("[CcuKernel][%s] failed, mission instructions are empty, "
            "the kernel is not been translated yet.", __func__);
        return HcclResult::HCCL_E_INTERNAL;
    }

    // 如果agrs数量超过sqe arg的最大数量，则返回多个TaskParam，前面几个只从sqe中加载args;
    // args数量大于等于0、小于等于最大值时，返回1个TaskParam
    const uint32_t seqNum
        = (agrsNum / CCU_SQE_ARGS_LEN) + ((agrsNum % CCU_SQE_ARGS_LEN) == 0 ? 0 : 1) + (agrsNum == 0 ? 1 : 0);

    const uint32_t preMissonSqeInsCnt = (seqNum - 1) * CCU_SQE_ARGS_LEN;
    if (instrInfo_.missionInstrCount < preMissonSqeInsCnt) {
        HCCL_ERROR("[CcuKernel][%s] failed, missionInstrCount[%u] should be greater "
            "than preMissonSqeInsCnt[%u].", __func__, instrInfo_.missionInstrCount,
            preMissonSqeInsCnt);
        return HcclResult::HCCL_E_INTERNAL;
    }

    taskParams.resize(seqNum);
    for (uint32_t index = 0; index < seqNum; index++) {
        taskParams[index].dieId       = GetDieId();
        taskParams[index].missionId   = GetMissionId();
        taskParams[index].instStartId = instrInfo_.missionStartInstrId + index * CCU_SQE_ARGS_LEN;
        taskParams[index].key         = GetMissionKey();
        taskParams[index].argSize     = CCU_SQE_ARGS_LEN;
        if (index == seqNum - 1) {
            // index 由计算得出，相乘结果不会溢出
            const uint32_t preMissionInsCnt = index * CCU_SQE_ARGS_LEN;
            taskParams[index].instCnt = instrInfo_.missionInstrCount - preMissionInsCnt;
            std::copy(std::begin(args) + preMissionInsCnt, std::end(args), std::begin(taskParams[index].args));
        } else {
            taskParams[index].instCnt = CCU_SQE_ARGS_LEN;
            std::copy(std::begin(args) + index * CCU_SQE_ARGS_LEN, std::begin(args) + (index + 1) * CCU_SQE_ARGS_LEN,
                      std::begin(taskParams[index].args));
        }

        HCCL_INFO("[GeneTaskParam]task Param, dieId[%u] missionId[%u] instStartId[%u] instCnt[%u], argSize[%u]",
                  taskParams[index].dieId, taskParams[index].missionId, taskParams[index].instStartId,
                  taskParams[index].instCnt, taskParams[index].argSize);
        for (uint32_t i = 0; i < taskParams[index].argSize; i++) {
            if (i == TOKEN_VALUE_INDEX) { continue; }
            HCCL_INFO("[GeneTaskParam]arg[%lu] = %lu", i, taskParams[index].args[i]);
        }
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::CreateVariable(const ChannelHandle channel, uint32_t varIndex, CcuRep::Variable *var) const
{
    void *channelPtr{nullptr};
    CHK_RET(static_cast<HcclResult>(HcommChannelGet(channel, &channelPtr)));
    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
    if (channelImpl == nullptr) {
        HCCL_ERROR("[%s] failed to cast channel[0x%llx] to CcuUrmaChannel", __func__, channel);
        return HcclResult::HCCL_E_PTR;
    }
    uint32_t locXnId{0};
    CHK_RET(channelImpl->GetLocXnByIndex(varIndex, locXnId));
    var->Reset(locXnId, channelImpl->GetDieId());
    return HcclResult::HCCL_SUCCESS;
}

CcuRepResource &CcuKernel::GetResource()
{
    return res_;
}

CcuResReq CcuKernel::GetResourceRequest()
{
    CcuResReq req;
    uint32_t dieId = GetDieId();
    req.msReq[dieId]              = res_.ccubufs[dieId].size();
    req.blockMsReq[dieId]         = res_.blockCcubufs[dieId].size();
    req.ckeReq[dieId]             = res_.completedEvent[dieId].size()
                                    + res_.localNotify[dieId].size();
    req.blockCkeReq[dieId]        = res_.blockCompletedEvent[dieId].size();
    req.loopEngineReq[dieId]      = res_.executor[dieId].size();
    req.blockLoopEngineReq[dieId] = res_.blockExecutor[dieId].size();
    req.gsaReq[dieId]             = res_.address[dieId].size();
    req.xnReq[dieId]              = res_.variable[dieId].size();
    req.continuousXnReq[dieId]    = res_.continuousVariable[dieId].size();

    req.missionReq.reqType           = MissionReqType::FUSION_MULTIPLE_DIE;
    req.missionReq.req[dieId] = 1;

    auto info
        = Hccl::StringFormat("resource request: dieId[%u], ms[%u], blockMs[%u], cke[%u], blockCke[%u], "
                       "loopEngine[%u], blockLoopEngine[%u], gsa[%u], xn[%u], continuous xn[%u], missionId[%u]",
                       dieId, req.msReq[dieId], req.blockMsReq[dieId], req.ckeReq[dieId], req.blockCkeReq[dieId],
                       req.loopEngineReq[dieId], req.blockLoopEngineReq[dieId], req.gsaReq[dieId], req.xnReq[dieId],
                       req.continuousXnReq[dieId], req.missionReq.req[dieId]);

    HCCL_INFO("%s", info.c_str());

    return req;
}

void CcuKernel::Load(const CcuRep::Variable &var)
{
    auto loadArgRep = std::make_shared<CcuRep::CcuRepLoadArg>(var, loadArgIndex_ % CCU_SQE_ARGS_LEN);
    Append(loadArgRep);
    loadArgIndex_++;
}

void CcuKernel::LoadVariable(uint64_t addr, const CcuRep::Variable &var)
{
    Append(std::make_shared<CcuRep::CcuRepLoad>(addr, var));
}

void CcuKernel::LoadVariable(uint64_t addr, const CcuRep::Variable &var, uint32_t num)
{
    Append(std::make_shared<CcuRep::CcuRepLoad>(addr, var, num));
}

void CcuKernel::StoreVariable(const CcuRep::Variable &var, uint64_t addr)
{
    Append(std::make_shared<CcuRep::CcuRepStore>(var, addr));
}

void CcuKernel::LoadVariable(const CcuRep::Variable &src, const CcuRep::Variable &var)
{
    Append(std::make_shared<CcuRep::CcuRepLoadVar>(src, var));
}

HcclResult CcuKernel::LocalNotifyRecord(const uint32_t coreId,
    const uint32_t dstNotifyIdx, const uint32_t mask)
{
    if (CurrentBlock()->Type() == CcuRep::CcuRepType::LOOP_BLOCK) {
        HCCL_ERROR("[CcuKernel][%s] is not supported in loop block, please check.", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    const std::string notifyTag = "Notify_" + std::to_string(coreId) + "_" +
        std::to_string(dstNotifyIdx);

    auto &sharedNotifies = importedRes_.sharedNotifies;
    if (sharedNotifies.find(notifyTag) == sharedNotifies.end()) {
        CcuRep::LocalNotify localNotify;
        sharedNotifies.insert({notifyTag, localNotify});
    }

    Append(std::make_shared<CcuRep::CcuRepRecordSharedNotify>(sharedNotifies.at(notifyTag), mask));

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::LocalNotifyWait(const uint32_t coreId,
    const uint32_t notifyIdx, const uint32_t mask)
{
    const std::string notifyTag = "Notify_" + std::to_string(coreId) + "_"
        + std::to_string(notifyIdx);

    auto &sharedNotifies = exportedRes_.sharedNotifies;
    if (sharedNotifies.find(notifyTag) == sharedNotifies.end()) {
        CcuRep::LocalNotify notify = CreateLocalNotify();
        exportedRes_.sharedNotifies.insert({notifyTag, notify});
    }

    bool isProfiling = CurrentBlock()->Type() != CcuRep::CcuRepType::LOOP_BLOCK;
    Append(std::make_shared<CcuRep::CcuRepLocWaitNotify>(
        exportedRes_.sharedNotifies.at(notifyTag), mask, isProfiling));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::RecordEvent(CcuRep::CompletedEvent event)
{
    if (CurrentBlock()->Type() == CcuRep::CcuRepType::LOOP_BLOCK) {
        HCCL_ERROR("[CcuKernel][%s] is not supported in loop block, please check.", __func__);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }

    Append(std::make_shared<CcuRep::CcuRepLocRecordEvent>(event));
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::WaitEvent(CcuRep::CompletedEvent event)
{
    bool isProfiling = CurrentBlock()->Type() != CcuRep::CcuRepType::LOOP_BLOCK;
    auto rep = std::make_shared<CcuRep::CcuRepLocWaitEvent>(event, isProfiling);
    if (isProfiling) {
        CHK_RET(static_cast<HcclResult>(AddProfiling("WaitEvent", rep->GetMask())));
    }
 	Append(rep);
    return HCCL_SUCCESS;
}

/*RemotePost新接口*/
HcclResult CcuKernel::NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask)
{
    Append(std::make_shared<CcuRep::CcuRepRemPostSem>(channel, remoteNotifyIdx, mask));
    return HCCL_SUCCESS;
}
/*WriteVariableWithSignal新接口*/
HcclResult CcuKernel::NotifyRecord(const ChannelHandle channel, uint32_t remoteNotifyIdx, 
                                        uint32_t remoteVarIdx, const CcuRep::Variable &var, uint32_t mask)
{
    Append(std::make_shared<CcuRep::CcuRepRemPostVar>(var, channel, remoteVarIdx, remoteNotifyIdx, mask));
    return HCCL_SUCCESS;
}

/*RemoteWait新接口*/
HcclResult CcuKernel::NotifyWait(const ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask)
{
    bool isProfiling = CurrentBlock()->Type() != CcuRep::CcuRepType::LOOP_BLOCK;
    if (isProfiling) {
        CHK_RET(static_cast<HcclResult>(AddProfiling(channel, "NotifyWait", localNotifyIdx, mask)));
    }
    Append(std::make_shared<CcuRep::CcuRepRemWaitSem>(channel, localNotifyIdx, mask, isProfiling));
    return HCCL_SUCCESS;
}

/*Read新接口*/
HcclResult CcuKernel::ReadNb(const ChannelHandle channel, const CcuRep::CcuBuf &loc, const CcuRep::RemoteAddr &rem,
                      const CcuRep::Variable &len, CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepBufRead>(channel, rem, loc, len, event, event.mask));
    return HCCL_SUCCESS;
}

/*Write新接口*/
HcclResult CcuKernel::WriteNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::CcuBuf &loc,
                       const CcuRep::Variable &len, CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepBufWrite>(channel, loc, rem, len, event, event.mask));
    return HCCL_SUCCESS;
}

static bool isLowPrecisionIn(Hccl::DataType dataType)
{
    return dataType == Hccl::DataType::INT8 || dataType == Hccl::DataType::HIF8 || dataType == Hccl::DataType::FP8E4M3
           || dataType == Hccl::DataType::FP8E5M2;
}

static bool isLowPrecisionOut(Hccl::DataType dataType)
{
    return dataType == Hccl::DataType::FP16 || dataType == Hccl::DataType::BFP16 || dataType == Hccl::DataType::FP32;
}

constexpr uint32_t MAX_DATA_TYPE = 17;

const Hccl::DataType orionDataTypes[] = {
    Hccl::DataType::INT8,
    Hccl::DataType::INT16,
    Hccl::DataType::INT32,
    Hccl::DataType::FP16,
    Hccl::DataType::FP32,
    Hccl::DataType::INT64,
    Hccl::DataType::UINT64,
    Hccl::DataType::UINT8,
    Hccl::DataType::UINT16,
    Hccl::DataType::UINT32,
    Hccl::DataType::FP64,
    Hccl::DataType::BFP16,
    Hccl::DataType::INT128,
#if !defined (OPEN_BUILD_PROJECT) || defined (ORION_MODE)
    Hccl::DataType::HIF8,
    Hccl::DataType::FP8E4M3,
    Hccl::DataType::FP8E5M2,
    Hccl::DataType::FP8E8M0
#endif
};

static Hccl::DataType HcommDataTypeToHcclDataType(const HcclDataType dataType)
{
    const auto dataTypeNum = static_cast<uint32_t>(dataType);
    if (dataTypeNum > MAX_DATA_TYPE) {
        return Hccl::DataType::INVALID;
    }

    return orionDataTypes[dataTypeNum];
}

constexpr uint32_t MAX_REDUCE_TYPE = 4;
const Hccl::ReduceOp orionReduceOps[] = {
    Hccl::ReduceOp::SUM,
    Hccl::ReduceOp::PROD,
    Hccl::ReduceOp::MAX,
    Hccl::ReduceOp::MIN,
};

static Hccl::ReduceOp HcommReduceOpToHcclReduceOp(const HcclReduceOp reduceOp)
{
    const auto reduceOpNum = static_cast<uint32_t>(reduceOp);
    if (reduceOpNum > MAX_REDUCE_TYPE) {
        return Hccl::ReduceOp::INVALID;
    }

    return orionReduceOps[reduceOpNum];
}

HcclResult CcuKernel::LocalReduceNb(const CcuRep::CcuBuf *bufs, uint32_t count, HcclDataType dataType,
                     HcclDataType outputDataType, HcclReduceOp opType,
                     const CcuRep::Variable &len, CcuRep::CompletedEvent event)
{
    auto opType_ = HcommReduceOpToHcclReduceOp(opType);
    auto dataType_ = HcommDataTypeToHcclDataType(dataType);
    auto outputDataType_ = HcommDataTypeToHcclDataType(outputDataType);

    if ((opType_ == Hccl::ReduceOp::SUM && isLowPrecisionIn(dataType_) && !isLowPrecisionOut(outputDataType_))
        || (opType_ == Hccl::ReduceOp::SUM && !isLowPrecisionIn(dataType_) && dataType_ != outputDataType_)
        || (opType_ != Hccl::ReduceOp::SUM && dataType_ != outputDataType_)) {
        return HCCL_E_NOT_SUPPORT;
    }

    std::vector<CcuRep::CcuBuf> ccuBufs(count);
    for (uint32_t i = 0; i < count; i++) {
        ccuBufs[i] = bufs[i];
    }

    Append(std::make_shared<CcuRep::CcuRepBufReduce>(ccuBufs, count, CcuRep::GetCcuDataType(dataType_, opType_),
                                                     CcuRep::GetCcuDataType(outputDataType_, opType_),
                                                     CcuRep::GetCcuReduceType(opType_), event, len, event.mask));
    return HCCL_SUCCESS;
}


/*Read新接口*/
HcclResult CcuKernel::ReadNb(const ChannelHandle channel, const CcuRep::LocalAddr &loc, const CcuRep::RemoteAddr &rem,
                      const CcuRep::Variable &len, CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepRead>(channel, loc, rem, len, event, event.mask));
    return HCCL_SUCCESS;
}

/*ReadReduce新接口*/
HcclResult CcuKernel::ReadReduceNb(const ChannelHandle channel, const CcuRep::LocalAddr &loc, const CcuRep::RemoteAddr &rem,
                            const CcuRep::Variable &len, HcclDataType dataType, HcclReduceOp opType,
                            CcuRep::CompletedEvent event)
{
    auto opType_ = HcommReduceOpToHcclReduceOp(opType);
    auto dataType_ = HcommDataTypeToHcclDataType(dataType);

    Append(std::make_shared<CcuRep::CcuRepRead>(channel, loc, rem, len, CcuRep::GetUBDataType(dataType_),
                                                CcuRep::GetUBReduceType(opType_), event, event.mask));
    return HCCL_SUCCESS;
}

/*Write新接口*/
HcclResult CcuKernel::WriteNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::LocalAddr &loc,
                       const CcuRep::Variable &len, CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepWrite>(channel, rem, loc, len, event, event.mask));
    return HCCL_SUCCESS;
}

/*WriteReduce新接口*/
HcclResult CcuKernel::WriteReduceNb(const ChannelHandle channel, const CcuRep::RemoteAddr &rem, const CcuRep::LocalAddr &loc,
                             const CcuRep::Variable &len, HcclDataType dataType, HcclReduceOp opType,
                             CcuRep::CompletedEvent event)
{
    auto opType_ = HcommReduceOpToHcclReduceOp(opType);
    auto dataType_ = HcommDataTypeToHcclDataType(dataType);

    Append(std::make_shared<CcuRep::CcuRepWrite>(channel, rem, loc, len, CcuRep::GetUBDataType(dataType_),
                                                 CcuRep::GetUBReduceType(opType_), event, event.mask));
    return HCCL_SUCCESS;
}

/*LocalCopy新接口*/
HcclResult CcuKernel::LocalCopyNb(const CcuRep::LocalAddr &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                           CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepLocCpy>(dst, src, len, event, event.mask));
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::LocalCopyNb(const CcuRep::CcuBuf &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                           CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepBufLocRead>(src, dst, len, event, event.mask));
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::LocalCopyNb(const CcuRep::LocalAddr &dst, const CcuRep::CcuBuf &src, const CcuRep::Variable &len,
                           CcuRep::CompletedEvent event)
{
    Append(std::make_shared<CcuRep::CcuRepBufLocWrite>(src, dst, len, event, event.mask));
    return HCCL_SUCCESS;
}

/*LocalReduce新接口*/
HcclResult CcuKernel::LocalReduceNb(const CcuRep::LocalAddr &dst, const CcuRep::LocalAddr &src, const CcuRep::Variable &len,
                             HcclDataType dataType, HcclReduceOp opType, CcuRep::CompletedEvent event)
{
    auto opType_ = HcommReduceOpToHcclReduceOp(opType);
    auto dataType_ = HcommDataTypeToHcclDataType(dataType);

    Append(std::make_shared<CcuRep::CcuRepLocCpy>(dst, src, len, CcuRep::GetUBDataType(dataType_), CcuRep::GetUBReduceType(opType_),
                                                  event, event.mask));
    return HCCL_SUCCESS;
}

CcuRep::FuncCall CcuKernel::Func(const std::string &label)
{
    return CcuRep::FuncCall(this, label);
}

CcuRep::FuncCall CcuKernel::Func(const CcuRep::Variable &funcAddr)
{
    return CcuRep::FuncCall(this, funcAddr);
}

CcuRep::LoopCall CcuKernel::Loop(const std::string &label)
{
    return CcuRep::LoopCall(this, label);
}

void CcuKernel::SetInstrId(uint32_t instrId)
{
    instrInfo_.startInstrId = instrId;
}

uint32_t CcuKernel::GetInstrId() const
{
    return instrInfo_.startInstrId;
}

uint32_t CcuKernel::GetInstrCount()
{
    uint32_t instrCount = 0;
    for (const auto &rep : GetRepSequence()) {
        instrCount += rep->InstrCount();
    }
    instrInfo_.instrCount = instrCount;
    HCCL_INFO("Kernel inst %u", instrCount);
    return instrCount;
}

void CcuKernel::SetCcuInstrInfo(const CcuRep::CcuInstrInfo &instrInfo)
{
    this->instrInfo_ = instrInfo;
}

CcuRep::Variable CcuKernel::CreateVariable()
{
    return CreateResAssist(res_.variable);
}

CcuRep::Variable CcuKernel::CreateContinuousVariable()
{
    return CreateResAssist(res_.continuousVariable);
}

CcuRep::Address CcuKernel::CreateAddress()
{
    return CreateResAssist(res_.address);
}

CcuRep::LocalNotify CcuKernel::CreateLocalNotify()
{
    return CreateResAssist(res_.localNotify);
}

CcuRep::CompletedEvent CcuKernel::CreateCompletedEvent()
{
    return CreateResAssist(res_.completedEvent);
}

CcuRep::CcuBuf CcuKernel::CreateCcuBuf()
{
    return CreateResAssist(res_.ccubufs);
}

CcuRep::Executor CcuKernel::CreateExecutor()
{
    return CreateResAssist(res_.executor);
}

CcuRep::LocalAddr CcuKernel::CreateLocalAddr()
{
    return CcuRep::LocalAddr(CreateAddress(), CreateVariable());
}

CcuRep::RemoteAddr CcuKernel::CreateRemoteAddr()
{
    return CcuRep::RemoteAddr(CreateAddress(), CreateVariable());
}

CcuRep::RemoteAddr CcuKernel::GetRemoteAddr(const ChannelHandle channel, uint32_t index)
{
    (void)index;
    auto mem = CcuRep::RemoteAddr(CreateAddress(), CreateVariable());
    Append(std::make_shared<CcuRep::CcuRepRemMem>(channel, mem));
    return mem;
}

CcuRep::LocalAddr CcuKernel::CreateLocalAddr(const CcuRep::Variable &token)
{
    return CcuRep::LocalAddr(CreateAddress(), token);
}

HcclResult CcuKernel::CreateBlockCcuBuf(const uint32_t count, CcuRep::CcuBuf *ccuBufs)
{
    CHK_PTR_NULL(ccuBufs);
    auto resources = CreateBlockResAssist(count, res_.blockCcubufs);

    for (uint32_t i = 0; i < count; i++) {
        ccuBufs[i] = resources[i]; // 拷贝虚拟资源，通过shared_ptr链接到物理资源
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::CreateBlockExecutor(const uint32_t count, CcuRep::Executor *ccuExes)
{
    CHK_PTR_NULL(ccuExes);
    auto resources = CreateBlockResAssist(count, res_.blockExecutor);

    for (uint32_t i = 0; i < count; i++) {
        ccuExes[i] = resources[i]; // 拷贝虚拟资源，通过shared_ptr链接到物理资源
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuKernel::CreateBlockCompletedEvent(const uint32_t count, CcuRep::CompletedEvent *ccuEvents)
{
    CHK_PTR_NULL(ccuEvents);
    auto resources = CreateBlockResAssist(count, res_.blockCompletedEvent);

    for (uint32_t i = 0; i < count; i++) {
        ccuEvents[i] = resources[i]; // 拷贝虚拟资源，通过shared_ptr链接到物理资源
    }

    return HcclResult::HCCL_SUCCESS;
}

void CcuKernel::SetResRepository(const CcuResRepository &resRepo)
{
    resRepo_ = resRepo;
}

CcuResRepository  &CcuKernel::GetResRepository()
{
    return resRepo_;
}

CcuSharedResource &CcuKernel::GetExportedRes()
{
    return exportedRes_;
}

CcuSharedResource &CcuKernel::GetImportedRes()
{
    return importedRes_;
}

HcclResult GetArgIndex(const std::unordered_map<uint16_t, uint16_t> &varId2VarIdMap,
                                 const std::unordered_map<uint16_t, uint32_t> &varId2ArgIndexMap,
                                 const std::vector<uint64_t> &taskArgs, uint16_t varId, uint64_t& argIndex)
{
    HCCL_INFO("[GetArgIndex] Enter varId(%u)", varId);
    auto item = varId2ArgIndexMap.find(varId);
    if (item == varId2ArgIndexMap.end()) {
        uint16_t oriVarId = varId;
        auto iter = varId2VarIdMap.find(varId);
        while (iter != varId2VarIdMap.end()) { // 循环查找中间assign Rep，找到起始varId
            oriVarId = iter->second;
            iter = varId2VarIdMap.find(oriVarId);
        }
        if (oriVarId != varId) { // 起始varId预期通过LoadArg赋值
            item = varId2ArgIndexMap.find(oriVarId);
            if (item == varId2ArgIndexMap.end()) {
                HCCL_ERROR("[%s]fail, Invalid goSize variable id(%u), oriVarId = %u", __func__, varId, oriVarId);
                return HCCL_E_PARA;
            }
        } else {
            HCCL_ERROR("[%s]fail, Invalid goSize variable id(%u)", __func__, varId);
            return HCCL_E_PARA;
        }
    }
    HCCL_INFO("[GetArgIndex] find end");
    if (item->second >= taskArgs.size()) {
        HCCL_ERROR("Invalid goSize variable index(%u).", item->second);
        return HCCL_E_PARA;
    }
    HCCL_INFO(
        "GetArgIndex success: varId(%u) varId2VarIdMapSize(%u) varId2ArgIndexMapSize(%u) taskArgsSize(%u)",
        varId, varId2VarIdMap.size(), varId2ArgIndexMap.size(), taskArgs.size());
    argIndex = taskArgs[item->second];
    return HCCL_SUCCESS;
}

void DumpCcuProfilingInfo(const std::vector<CcuProfilingInfo> &ccuProfilingInfo)
{
    auto dumpLinkInfo = [] (const CcuProfilingInfo &info) -> void {
        for (int i = 0; i < CCU_MAX_CHANNEL_NUM; i++) {
            if (info.channelId[i] == INVALID_VALUE_CHANNELID) {
                continue;
            }
            HCCL_INFO("channelId(%u), remoteRankId(%u).", info.channelId[i], info.remoteRankId[i]);
        }
    };

    for (const auto &profInfo : ccuProfilingInfo) {
        if (profInfo.type == static_cast<uint8_t>(CcuProfilinType::CCU_TASK_PROFILING)) {
            HCCL_INFO("Dump CCU Profiling Info:SQE Profiling Info: ctxSignautre(%s), "
                       "dieId(%d), missionId(%d), instrId(%d).",
                       profInfo.name.c_str(), static_cast<int>(profInfo.dieId), static_cast<int>(profInfo.missionId),
                       static_cast<int>(profInfo.instrId));
        } else if (profInfo.type == static_cast<uint8_t>(CcuProfilinType::CCU_WAITCKE_PROFILING)) {
            HCCL_INFO("Microcode WaitCKE Profiling Info: name(%s), "
                       "dieId(%d), missionId(%d), instrId(%d), ckeId(%u), mask(%u).",
                       profInfo.name.c_str(), static_cast<int>(profInfo.dieId), static_cast<int>(profInfo.missionId),
                       static_cast<int>(profInfo.instrId), profInfo.ckeId, profInfo.mask);
            dumpLinkInfo(profInfo);
        } else if (profInfo.type == static_cast<uint8_t>(CcuProfilinType::CCU_LOOPGROUP_PROFILING)) {
            HCCL_INFO("Microcode LoopGroup Profiling Info: name(%s), "
                       "dieId(%d), missionId(%d), instrId(%d), reduceOpType(%d), inputDataType(%d), "
                       "outputDataType(%d), dataSize(%llu).",
                       profInfo.name.c_str(), static_cast<int>(profInfo.dieId), static_cast<int>(profInfo.missionId),
                       static_cast<int>(profInfo.instrId), static_cast<int>(profInfo.reduceOpType),
                       static_cast<int>(profInfo.inputDataType), static_cast<int>(profInfo.outputDataType),
                       profInfo.dataSize);
            dumpLinkInfo(profInfo);
        }
    }
}
/*
 	* variable/maskSignal等资源变量Id，一定要在获取ccu profiling时才获取；
 	* 原因：在创建context Rep时，其资源Id属于虚拟资源；翻译时，才会绑定固定的物理资源。
*/
HcclResult CcuKernel::GetCcuProfilingInfo(const CcuTaskArg &arg, std::vector<CcuProfilingInfo> &allCcuProfilingInfo)
{
 	HCCL_INFO("[GetCcuProfilingInfo] Enter.");
    allCcuProfilingInfos_.clear();
 	auto &ccuProfilingCache = GetProfilingInfo();
 	 
 	auto taskArgs = GeneArgs(arg);
 	uint32_t count {0};
 	HCCL_INFO("[GetCcuProfilingInfo] Process sqe&waitcke profiling info start.");
    for (auto &profInfo : ccuProfilingCache) {
        profInfo.missionId = GetMissionId();
        if (profInfo.type == static_cast<uint8_t>(hcomm::CcuProfilinType::CCU_TASK_PROFILING)) {
            profInfo.instrId   = GetInstrId();
            allCcuProfilingInfos_.push_back(profInfo);
            continue;
        }
        if (count >= GetWaiteCkeProfilingReps().size()) {
            HCCL_ERROR("count[%u] out of range[0, %u], cache size(%u).", count, GetWaiteCkeProfilingReps().size(), ccuProfilingCache.size());
            return HCCL_E_INTERNAL;
        }
        auto waitCkeRep = GetWaiteCkeProfilingReps()[count];
        profInfo.instrId = waitCkeRep->StartInstrId();
        if (profInfo.ckeId == INVALID_CKE_ID) { // localWait Rep
            if (waitCkeRep.get() == nullptr) {
                HCCL_ERROR("[GetCcuProfilingInfo] localWaitRep is nullptr.");
                return HCCL_E_PTR;
            }
            profInfo.ckeId = waitCkeRep->GetId();
            HCCL_INFO("[CcuKernel][GetCcuProfilingInfo] waitcke[%u]", profInfo.ckeId);
        }
        allCcuProfilingInfos_.push_back(profInfo);
        count++;
    }

    // loopGroup
    auto &lgProfInfo = GetLGProfilingInfo();
    HCCL_INFO("[GetCcuProfilingInfo] create varId2ArgIndexMap start. size=%lu", lgProfInfo.loadRep2ArgIdxMap.size());
    std::unordered_map<uint16_t, uint32_t> varId2ArgIndexMap;
    for (auto &iter : lgProfInfo.loadRep2ArgIdxMap) {
        if (iter.first.get() == nullptr) {
            HCCL_ERROR("[GetCcuProfilingInfo] loadRep is nullptr.");
            return HCCL_E_PTR;
        }
        auto loadRep = dynamic_cast<CcuRep::CcuRepLoadArg*>(iter.first.get());
        varId2ArgIndexMap[loadRep->GetVarId()] = iter.second;
    }

    HCCL_INFO("[GetCcuProfilingInfo] create varId2VarIdMap start. size=%lu", lgProfInfo.assignProfilingReps.size());
    std::unordered_map<uint16_t, uint16_t> varId2VarIdMap;
    for (auto &iter : lgProfInfo.assignProfilingReps) {
        if (iter.get() == nullptr) {
            HCCL_ERROR("[GetCcuProfilingInfo] assignRep is nullptr.");
            return HCCL_E_PTR;
        }
        auto assignRep = dynamic_cast<CcuRep::CcuRepAssign*>(iter.get());
        varId2VarIdMap[assignRep->varB.Id()] = assignRep->varA.Id();
    }

    HCCL_INFO("[GetCcuProfilingInfo] process loop group profiling start: lgsize(%lu), goSize(%lu)", lgProfInfo.lgProfilingReps.size(), groupOpSizeInfo_.size());
    for (uint32_t i = 0; i < lgProfInfo.lgProfilingReps.size(); i += 2) { // 2: 一个goSize对应一个CcuProfilingInfo，对应1个loopGroup Rep
        if (taskArgs.empty() || varId2ArgIndexMap.empty()) {
            continue;
        }
        uint64_t loopParam {0};
        CHK_RET(GetArgIndex(varId2VarIdMap, varId2ArgIndexMap, taskArgs, groupOpSizeInfo_[i].loopParamId, loopParam));
        uint64_t parallelParam {0};
        CHK_RET(GetArgIndex(varId2VarIdMap, varId2ArgIndexMap, taskArgs, groupOpSizeInfo_[i].parallelParamId, parallelParam));
        HCCL_INFO("Collect loopgroup profiling info: repSize[%u], index[%u], loopParam[%llu], parallelParam[%llu].",
                lgProfInfo.lgProfilingReps.size(), i, loopParam, parallelParam);

        if (loopParam != 0) {
            lgProfInfo.ccuProfilingInfos[i].dataSize = loopParam * moConfig_.loopCount * moConfig_.memSlice;
            lgProfInfo.ccuProfilingInfos[i].instrId = dynamic_cast<CcuRep::CcuRepLoopGroup*>(lgProfInfo.lgProfilingReps[i].get())->StartInstrId();
            allCcuProfilingInfos_.push_back(lgProfInfo.ccuProfilingInfos[i]);
        }

        if (parallelParam != 0) {
            HCCL_INFO("[GetCcuProfilingInfo] collect lg, residual start i=%lu", i);
            uint64_t residual {0};
            CHK_RET(GetArgIndex(varId2VarIdMap, varId2ArgIndexMap, taskArgs, groupOpSizeInfo_[i].residualId, residual));
            uint64_t repeatNum = Hccl::CcuRep::ParseRepeatNumFromParallelParam(parallelParam);
            lgProfInfo.ccuProfilingInfos[i].dataSize = repeatNum * moConfig_.memSlice + residual;
            lgProfInfo.ccuProfilingInfos[i].instrId = dynamic_cast<CcuRep::CcuRepLoopGroup*>(lgProfInfo.lgProfilingReps[i + 1].get())->StartInstrId();
            allCcuProfilingInfos_.push_back(lgProfInfo.ccuProfilingInfos[i]);
        }
    }
    DumpCcuProfilingInfo(allCcuProfilingInfos_);
    allCcuProfilingInfo = allCcuProfilingInfos_;
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::AddProfilingInfo(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType,
                                HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName)
{
    CHK_PTR_NULL(channels);
    ccuProfilingInfoCache.type           = (uint8_t)CcuProfilinType::CCU_LOOPGROUP_PROFILING;
    ccuProfilingInfoCache.name           = opName;
    ccuProfilingInfoCache.reduceOpType   = opType;
    ccuProfilingInfoCache.inputDataType  = dataType;
    ccuProfilingInfoCache.outputDataType = outputDataType;
    ccuProfilingInfoCache.missionId      = GetMissionId();
    
    CHK_SAFETY_FUNC_RET(memset_s(ccuProfilingInfoCache.channelId, sizeof(ccuProfilingInfoCache.channelId),
                                    INVALID_VALUE_CHANNELID, sizeof(ccuProfilingInfoCache.channelId)));
    for (uint32_t i = 0; i < channelNum; i++) {
        void *channelPtr{nullptr};
        CHK_RET(static_cast<HcclResult>(HcommChannelGet(channels[i], &channelPtr)));
        auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
        CHK_PTR_NULL(channelImpl);
        ccuProfilingInfoCache.channelId[i] = channelImpl->GetChannelId();
        ccuProfilingInfoCache.channelHandle[i] = channels[i];
        HCCL_INFO("[%s]type[%d], name[%s], opType[%d], dataType[%d], outputDataType[%d], missionId[%u], "
                "channelHandle[0x%llx], channelId[%u]", __func__, ccuProfilingInfoCache.type, 
                ccuProfilingInfoCache.name.c_str(), opType, dataType, outputDataType, ccuProfilingInfoCache.missionId,
                ccuProfilingInfoCache.channelHandle[i], ccuProfilingInfoCache.channelId[i]);
    }
    lgProfilingInfo.ccuProfilingInfos.push_back(ccuProfilingInfoCache);
    lgProfilingInfo.lgProfilingReps.push_back(allLgProfilingReps.back());
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::AddCcuProfiling(GroupInfo groupInfo, const std::vector<ChannelHandle> channelHandle, HcclDataType dataType,
                                 HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName)
{
    CHK_RET(AddCcuProfiling(channelHandle.data(), channelHandle.size(), dataType, outputDataType, opType, opName));
    groupOpSizeInfo_.push_back(groupInfo);
    return HCCL_SUCCESS;
}

HcclResult CcuKernel::AddCcuProfiling(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType,
                                HcclDataType outputDataType, HcclReduceOp opType, const std::string& opName)
{
    CHK_PTR_NULL(channels);
    CHK_RET(AddProfilingInfo(channels, channelNum, dataType, outputDataType, opType, opName));
    return HCCL_SUCCESS;
}

}; // namespace hcomm