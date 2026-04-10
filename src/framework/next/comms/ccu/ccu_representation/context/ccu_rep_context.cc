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

void CcuRepContext::Append(std::shared_ptr<CcuRep::CcuRepBase> rep)
{
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

}; // namespace CcuRep
}; // namespace hcomm