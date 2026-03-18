/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define private public
#define protected public

#include "ccu_res_batch_allocator.h"
#include "ccu_pfe_cfg_mgr.h"
#include "ccu_res_specs.h"
#include "ccu_comp.h"

#undef protected
#undef private

HcclResult MockCcuResources(int32_t devLogicId, hcomm::CcuVersion ccuVersion)
{
    auto &ccuResSpecs = hcomm::CcuResSpecifications::GetInstance(devLogicId);
    ccuResSpecs.initFlag_ = true;
    ccuResSpecs.ccuVersion_ = ccuVersion;
    ccuResSpecs.armX86Flag_ = false;
    for (uint8_t dieId = 0; dieId < hcomm::CCU_MAX_IODIE_NUM; dieId++) {
        ccuResSpecs.dieEnableFlags_[dieId] = true;

        ccuResSpecs.resSpecs_[dieId].loopEngineNum = 200;
        
        ccuResSpecs.resSpecs_[dieId].msNum = 1536;
        ccuResSpecs.resSpecs_[dieId].ckeNum = 1024;

        ccuResSpecs.resSpecs_[dieId].xnNum = 3072;

        ccuResSpecs.resSpecs_[dieId].gsaNum = 3072;

        ccuResSpecs.resSpecs_[dieId].instructionNum = 32768;
        ccuResSpecs.resSpecs_[dieId].missionNum = 16;

        ccuResSpecs.resSpecs_[dieId].channelNum = 128;

        ccuResSpecs.resSpecs_[dieId].jettyNum = 128;
        ccuResSpecs.resSpecs_[dieId].wqeBBNum = 4096;

        ccuResSpecs.resSpecs_[dieId].pfeNum = 10;

        ccuResSpecs.resSpecs_[dieId].resourceAddr = 0xE7FFBF800000;
    }

    CHK_RET(hcomm::CcuPfeCfgMgr::GetInstance(devLogicId).Init());
    CHK_RET(hcomm::CcuComponent::GetInstance(devLogicId).Init());
    CHK_RET(hcomm::CcuResBatchAllocator::GetInstance(devLogicId).Init());

    return HcclResult::HCCL_SUCCESS;
}