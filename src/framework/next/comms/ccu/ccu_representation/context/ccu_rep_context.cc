/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ccu represnetation context implementation file
 * Author: sunzhepeng
 * Create: 2024-06-17
 */

#include "ccu_rep_context_v1.h"

#include "exception_util.h"
#include "ccu_api_exception.h"
#include "ccu_rep_assign_v1.h"
#include "const_val.h"

 #include "hcomm_c_adpt.h"  // 需优化
#include "../../../endpoint_pairs/channels/ccu/ccu_urma_channel.h"  // 需优化

namespace hcomm {
namespace CcuRep {

CcuRepContext::CcuRepContext()
{
    mainBlock   = std::make_shared<CcuRep::CcuRepBlock>();
    activeBlock = mainBlock;
}

CcuRepContext::~CcuRepContext()
{
    
}

std::shared_ptr<CcuRep::CcuRepBlock> CcuRepContext::CurrentBlock()
{
    if (activeBlock == nullptr) {
        Hccl::THROW<Hccl::CcuApiException>("Invalid ActiveBlock");
    }
    return activeBlock;
}

void CcuRepContext::SetCurrentBlock(std::shared_ptr<CcuRep::CcuRepBlock> repBlock)
{
    activeBlock = repBlock;
}

/**
 * @details:保存rep信息，后续有arg后配合补全profiling信息
 */
void CcuRepContext::CollectProfilingReps(std::shared_ptr<CcuRep::CcuRepBase> rep)
{
    if (rep->Type() == CcuRepType::ASSIGN) {
        auto assignRep = dynamic_cast<CcuRepAssign *>(rep.get());
        if (assignRep->subType == AssignSubType::VAR_TO_VAR) {
            lgProfilingInfo.assignProfilingReps.push_back(rep);
        }
    } else if (CurrentBlock()->Type() != CcuRep::CcuRepType::LOOP_BLOCK
            && (rep->Type() == CcuRepType::LOC_WAIT_EVENT || rep->Type() == CcuRepType::REM_WAIT_SEM
                || rep->Type() == CcuRepType::REM_WAIT_GROUP)) {
        waitCkeProfilingReps.push_back(rep);
    } else if (rep->Type() == CcuRepType::LOOPGROUP) {
        allLgProfilingReps.push_back(rep);
    }
}

void CcuRepContext::Append(std::shared_ptr<CcuRep::CcuRepBase> rep)
{
    CollectProfilingReps(rep);
    CurrentBlock()->Append(rep);
}

const std::vector<std::shared_ptr<CcuRep::CcuRepBase>> &CcuRepContext::GetRepSequence()
{
    return mainBlock->GetReps();
}

std::shared_ptr<CcuRep::CcuRepBase> CcuRepContext::GetRepByInstrId(uint16_t instrId)
{
    for (const auto& rep : GetRepSequence()) {
        const uint16_t startId = rep->StartInstrId();
        const uint16_t endId = startId + rep->InstrCount() - 1;
        if (instrId >= startId && instrId <= endId) {
            return rep;
        }
    }
    return nullptr;
}

void CcuRepContext::DumpReprestation()
{
    HCCL_INFO("Rep Count: %lu", GetRepSequence().size());
    for (uint32_t index = 0; index < GetRepSequence().size(); index++) {
        HCCL_INFO("index[%u]: %s", index, GetRepSequence()[index]->Describe().c_str());
    }
}

void CcuRepContext::SetDieId(uint32_t dieId)
{
    HCCL_INFO("set dieId[%u]", dieId);
    this->dieId = dieId;
}

uint32_t CcuRepContext::GetDieId() const
{
    return dieId;
}

void CcuRepContext::SetMissionId(uint32_t missionId)
{
    if (this->missionId == Hccl::INVALID_U32) {
        this->missionId = missionId;
    }
}

uint32_t CcuRepContext::GetMissionId() const
{
    return missionId;
}

void CcuRepContext::SetMissionKey(uint32_t missionKey)
{
    this->missionKey = missionKey;
}

uint32_t CcuRepContext::GetMissionKey() const
{
    return missionKey;
}

std::vector<CcuProfilingInfo> &CcuRepContext::GetProfilingInfo()
{
    return profilingInfo;
}

const std::vector<std::shared_ptr<CcuRepBase>> &CcuRepContext::GetWaiteCkeProfilingReps() const
{
    return waitCkeProfilingReps;
}

LoopGroupProfilingInfo &CcuRepContext::GetLGProfilingInfo()
{
    return lgProfilingInfo;
}

void CcuRepContext::AddSqeProfiling(const CcuKernelArg &arg)
{
    profilingInfo.clear();
    // 生成SQE粒度profiling信息
    ccuProfilingInfoCache.type      = (uint8_t)CcuProfilinType::CCU_TASK_PROFILING;
    ccuProfilingInfoCache.name      = arg.GetKernelSignature().Describe();
    ccuProfilingInfoCache.dieId     = GetDieId();

    profilingInfo.push_back(ccuProfilingInfoCache);
}
    
void CcuRepContext::AddProfiling(const std::string &name, uint32_t mask)
{
    ccuProfilingInfoCache.type  = (uint8_t)CcuProfilinType::CCU_WAITCKE_PROFILING;
    ccuProfilingInfoCache.name  = name;
    ccuProfilingInfoCache.ckeId = INVALID_CKE_ID;
    ccuProfilingInfoCache.mask  = mask;
    (void)memset_s(ccuProfilingInfoCache.channelId, sizeof(ccuProfilingInfoCache.channelId), INVALID_VALUE_CHANNELID, sizeof(ccuProfilingInfoCache.channelId));

    profilingInfo.push_back(ccuProfilingInfoCache);
}
    
void CcuRepContext::AddProfiling(const ChannelHandle channel, const std::string &name, uint32_t signalIndex, uint32_t mask)
{
    void *channelPtr{nullptr};
    HcommChannelGet(channel, &channelPtr);
    auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));

    ccuProfilingInfoCache.type     = (uint8_t)CcuProfilinType::CCU_WAITCKE_PROFILING;
    ccuProfilingInfoCache.name     = name;
    channelImpl->GetLocCkeByIndex(signalIndex, ccuProfilingInfoCache.ckeId);
    ccuProfilingInfoCache.mask     = mask;
    (void)memset_s(ccuProfilingInfoCache.channelId, sizeof(ccuProfilingInfoCache.channelId), INVALID_VALUE_CHANNELID, sizeof(ccuProfilingInfoCache.channelId));
    ccuProfilingInfoCache.channelId[0] = channelImpl->GetChannelId();

    profilingInfo.push_back(ccuProfilingInfoCache);
}

    
void CcuRepContext::AddProfiling(const ChannelHandle *channels, uint32_t channelNum)
{
    ccuProfilingInfoCache.type           = (uint8_t)CcuProfilinType::CCU_LOOPGROUP_PROFILING;
    ccuProfilingInfoCache.name           = "GroupBroadcast";
    ccuProfilingInfoCache.reduceOpType   = 0xFF; // 0xFF 无效值
    ccuProfilingInfoCache.inputDataType  = 0xFF; // 0xFF 无效值
    ccuProfilingInfoCache.outputDataType = 0xFF; // 0xFF 无效值
    ccuProfilingInfoCache.missionId      = GetMissionId();

    (void)memset_s(ccuProfilingInfoCache.channelId, sizeof(ccuProfilingInfoCache.channelId), INVALID_VALUE_CHANNELID, sizeof(ccuProfilingInfoCache.channelId));
    for (u32 i = 0; i < channelNum; i++) {
        void *channelPtr{nullptr};
        HcommChannelGet(channels[i], &channelPtr);
        auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
        ccuProfilingInfoCache.channelId[i] = channelImpl->GetChannelId();
    }

    lgProfilingInfo.ccuProfilingInfos.push_back(ccuProfilingInfoCache);
    lgProfilingInfo.lgProfilingReps.push_back(allLgProfilingReps.back());
}
    
void CcuRepContext::AddProfiling(const ChannelHandle *channels, uint32_t channelNum, HcclDataType dataType,
                                HcclDataType outputDataType, HcclReduceOp opType)
{
    ccuProfilingInfoCache.type           = (uint8_t)CcuProfilinType::CCU_LOOPGROUP_PROFILING;
    ccuProfilingInfoCache.name           = "GroupReduce";
    ccuProfilingInfoCache.reduceOpType   = opType;
    ccuProfilingInfoCache.inputDataType  = dataType;
    ccuProfilingInfoCache.outputDataType = outputDataType;
    ccuProfilingInfoCache.missionId      = GetMissionId();
    
    (void)memset_s(ccuProfilingInfoCache.channelId, sizeof(ccuProfilingInfoCache.channelId), INVALID_VALUE_CHANNELID, sizeof(ccuProfilingInfoCache.channelId));
    for (u32 i = 0; i < channelNum; i++) {
        void *channelPtr{nullptr};
        HcommChannelGet(channels[i], &channelPtr);
        auto *channelImpl = dynamic_cast<CcuUrmaChannel *>(static_cast<Channel *>(channelPtr));
        ccuProfilingInfoCache.channelId[i] = channelImpl->GetChannelId();
    }

    lgProfilingInfo.ccuProfilingInfos.push_back(ccuProfilingInfoCache);
    lgProfilingInfo.lgProfilingReps.push_back(allLgProfilingReps.back());
}

void CcuRepContext::AddCcuProfiling(GroupOpSize goSize, const std::vector<CcuTransport*> &transportsIn)
{
    AddProfiling(transportsIn);
    groupOpSizeInfo.push_back(goSize);
}

void CcuRepContext::AddCcuProfiling(GroupOpSize goSize, const std::vector<CcuTransport *> &transportsIn, DataType dataType,
                                 DataType outputDataType, ReduceOp opType)
{
    AddProfiling(transportsIn, dataType, outputDataType, opType);
    groupOpSizeInfo.push_back(goSize);
}

}; // namespace CcuRep
}; // namespace hcomm