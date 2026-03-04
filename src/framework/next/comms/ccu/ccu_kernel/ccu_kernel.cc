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

#include "../../endpoint_pairs/channels/ccu/ccu_urma_channel.h"

namespace hcomm {

constexpr u32 TOKEN_VALUE_INDEX = 2;

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
    CHK_RET(HcommChannelGet(channel, &channelPtr));
    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
    if (channelImpl == nullptr) {
        HCCL_ERROR("[%s] failed to cast channel[0x%llx] to CcuUrmaChannel", __func__, channel);
        return HcclResult::HCCL_E_PTR;
    }
    dieId = channelImpl->GetDieId();
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
    CHK_RET(Algorithm());
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
    CHK_RET(HcommChannelGet(channel, &channelPtr));
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
    Append(std::make_shared<CcuRep::CcuRepLocWaitEvent>(event, isProfiling));
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

}; // namespace hcomm