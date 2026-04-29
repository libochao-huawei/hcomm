/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ccu representation loopgroup bundle implementation file
 * Create: 2026-03-22
 */

#include "ccu_rep_loopgroup_bundle_v1.h"
#include "ccu_assist_v1.h"
#include "ccu_microcode_v1.h"
#include "string_util.h"

namespace hcomm {
namespace CcuRep {

CcuRepLoopGroupBundle::CcuRepLoopGroupBundle(const CcuLoopGroupConfig &config,
                                             const Variable &parallelVar, const Variable &offsetVar)
    : config_(config), parallelVar_(parallelVar), offsetVar_(offsetVar)
{
    type = CcuRepType::LOOPGROUP;
}

CcuRepLoopGroupBundle::CcuRepLoopGroupBundle(const Variable &parallelVar, const Variable &offsetVar)
    : parallelVar_(parallelVar), offsetVar_(offsetVar), isGroupVarBased_(true)
{
    type = CcuRepType::LOOPGROUP;
}

void CcuRepLoopGroupBundle::AddLoop(const LoopEntry &entry)
{
    loops_.push_back(entry);
}

uint16_t CcuRepLoopGroupBundle::CalcParamBindingCount() const
{
    uint16_t count = 0;
    for (const auto &loop : loops_) {
        count += static_cast<uint16_t>(loop.paramBindings.size());
    }
    return count;
}

uint16_t CcuRepLoopGroupBundle::InstrCount()
{
    uint16_t paramBindingCount = CalcParamBindingCount();
    uint16_t loopCount = static_cast<uint16_t>(loops_.size());

    uint16_t varBasedLoopCount = 0;
    for (const auto &loop : loops_) {
        if (loop.isVarBased) {
            varBasedLoopCount++;
        }
    }

    instrCount = paramBindingCount
               + (loopCount - varBasedLoopCount)  // config-based: 1 LoadImd per loop
               + (varBasedLoopCount * 2)           // var-based: LoadImd + LoadXX per loop
               + (isGroupVarBased_ ? 0 : 2)
               + 1                   // LoopGroupInstr
               + 2                   // Jump (LoadImd + JumpInstr)
               + loopCount           // LoopInstr per loop
               + 1;                  // NOP (JumpLabel)
    return instrCount;
}

bool CcuRepLoopGroupBundle::Translate(CcuInstr *&instr, uint16_t &instrId, const TransDep &dep)
{
    for (const auto &loop : loops_) {
        if (!loop.repLoopBlock->Translated()) {
            return false;
        }
    }

    this->instrId = instrId;
    translated = true;

    // 1. LoopCall: param bindings (LoadXX)
    for (const auto &loop : loops_) {
        for (const auto &binding : loop.paramBindings) {
            LoadXXInstr(instr++, binding.formal.Id(), binding.actual.Id(), dep.reserveXnId);
            instrId++;
        }
    }

    // 2. Assign loopParam for each loop
    for (const auto &loop : loops_) {
        if (!loop.isVarBased) {
            uint64_t lpImm = GetLoopParam(loop.executorId, loop.config.addrOffset, loop.config.loopIterNum);
            LoadImdToXnInstr(instr++, loop.loopParamVar.Id(), lpImm);
            instrId++;
        } else {
            uint64_t ctxImm = static_cast<uint64_t>(loop.executorId) << 45;
            LoadImdToXnInstr(instr++, dep.reserveXnId, ctxImm);
            instrId++;
            LoadXXInstr(instr++, loop.loopParamVar.Id(), loop.loopParamVar.Id(), dep.reserveXnId);
            instrId++;
        }
    }

    // 3-4. Assign parallelParam & offsetParam (skip var-based group — registers already set)
    if (!isGroupVarBased_) {
        uint64_t parallelImm = GetParallelParam(config_.repeatNum, repeatLoopIdx_, totalLoopNum_);
        LoadImdToXnInstr(instr++, parallelVar_.Id(), parallelImm);
        instrId++;

        uint64_t offsetImm = GetOffsetParam(config_.addrOffset, config_.bufferOffset, config_.eventOffset);
        LoadImdToXnInstr(instr++, offsetVar_.Id(), offsetImm);
        instrId++;
    }

    // 5. LoopGroupInstr — startLoopInstrId = instrId + 3
    LoopGroupInstr(instr++, instrId + 3, parallelVar_.Id(), offsetVar_.Id(), 0);
    instrId++;

    // 6. Jump (skip over LoopInstr region)
    uint16_t loopCount = static_cast<uint16_t>(loops_.size());
    uint16_t jumpTargetInstrId = instrId + 2 + loopCount + 1;
    LoadImdToXnInstr(instr++, dep.reserveXnId, jumpTargetInstrId);
    instrId++;
    JumpInstr(instr++, dep.reserveXnId, dep.reserveXnId, 1);
    instrId++;

    // 7. LoopInstr for each loop
    for (const auto &loop : loops_) {
        const auto &block = loop.repLoopBlock;
        LoopInstr(instr++, block->StartInstrId(),
                  block->StartInstrId() + block->InstrCount() - 1,
                  loop.loopParamVar.Id());
        instrId++;
    }

    // 8. NOP (JumpLabel target)
    LoadImdToXnInstr(instr++, dep.reserveXnId, 0);
    instrId++;

    return translated;
}

std::string CcuRepLoopGroupBundle::Describe()
{
    return Hccl::StringFormat("LoopGroupBundle[loops=%zu, totalLoopNum=%lu]",
                              loops_.size(), totalLoopNum_);
}

}; // namespace CcuRep
}; // namespace hcomm
