/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * Description: ccu executor -- jmp
 * Author: caiyifan
 */

#include "jump_executor.h"

#include <cstdint>

#include "ccu_executor_manager.h"
#include "ccu_string_util.h"

using namespace std;
using namespace hcomm::CcuRep;
constexpr uint32_t JUMP_INSTR_MAX = 0x10000;
// 注册JumpExecutor create Func
REG_CCU_EXECUTOR_CREATE_FUNC(SimCcuV1::CTRL_TYPE, SimCcuV1::JMP_CODE, JumpExecutor);

void JumpExecutor::Parser()
{
    if (version_ == RunnerCcuVersion::CCU_V1) {
        dstInstrXnId_   = instr_.v1.jmp.dstInstrXnId;
        conditionXnId_  = instr_.v1.jmp.conditionXnId;
        expectData_     = instr_.v1.jmp.expectData;
    } else {
        HCCL_VM_ERROR("Invalid ccu version:{}", RunnerCcuVersionToString(version_));
        ccuSimulator_->SetExecState(CcuExecState::EXEC_FAIL);
        return;
    }
}

void JumpExecutor::RunV1() {
    auto &ccuResMgr = CcuResourceManager::GetInstance();
    uint64_t instrId = ccuResMgr.GetXnValue(rankId_, dieId_, dstInstrXnId_);
    uint64_t value = ccuResMgr.GetXnValue(rankId_, dieId_, conditionXnId_);

    if (value != expectData_) {
        ccuSimulator_->InitJumpStatus(instrId);
    }
    return;
}

void JumpExecutor::Run()
{
    if (version_ == RunnerCcuVersion::CCU_V1) {
        RunV1();
    } else {
        HCCL_VM_ERROR("Invalid ccu version:{}", RunnerCcuVersionToString(version_));
        ccuSimulator_->SetExecState(CcuExecState::EXEC_FAIL);
        return;
    }
}

std::string JumpExecutor::Describe()
{
    return HcclSim::StringFormat("[Simulation Execute] When conditionXn[%u] not equal to expectData[%lu], Jump To InstrIdXn[%u]\n",
        conditionXnId_, expectData_, dstInstrXnId_);
}
