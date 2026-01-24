/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_pfe_cfg_generator.h"

#include "hccl_common_v2.h"

#include "ccu_eid_info.h"
#include "ccu_res_specs.h"
#include <unordered_set>

namespace Hccl {

CcuPfeCfgGenerator &CcuPfeCfgGenerator::GetInstance(const int32_t deviceLogicId)
{
    static CcuPfeCfgGenerator ccuPfeCfgGenerator[MAX_MODULE_DEVICE_NUM];

    if (deviceLogicId < 0 || static_cast<uint32_t>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM) {
        THROW<InvalidParamsException>("[CcuPfeCfgGenerator][%s] failed to get instance, devLogicId[%d] "
            "should be less than %u.", __func__, deviceLogicId, MAX_MODULE_DEVICE_NUM);
    }

    ccuPfeCfgGenerator[deviceLogicId].Init(deviceLogicId);

    return ccuPfeCfgGenerator[deviceLogicId];
}

void CcuPfeCfgGenerator::Init(const int32_t deviceLogicId)
{
    if (initFlag) {
        return;
    }

    devLogicId = deviceLogicId;

    std::vector<HrtDevEidInfo> eidInfoList;
    (void)CcuEidInfo::GetInstance(devLogicId).GetEidInfo(devLogicId, eidInfoList);
    if (eidInfoList.empty()) {
        HCCL_WARNING("[CcuPfeCfgGenerator][%s] failed to get eid infos, devLogicId[%d]",
            __func__, devLogicId);
        initFlag = true;
        return;
    }

    bool dieEnableFlags[MAX_MODULE_DEVICE_NUM] = {false, false};
    uint8_t interMaxJettyNums[MAX_CCU_IODIE_NUM]; // 优先计算出框可用jetty数量
    uint32_t pfeCnts[MAX_CCU_IODIE_NUM];
    for (uint8_t i = 0; i < MAX_CCU_IODIE_NUM; i++) {
        const auto &ccuResSpecs = CcuResSpecifications::GetInstance(devLogicId);
        (void)ccuResSpecs.GetDieEnableFlag(i, dieEnableFlags[i]);
        pfeCnts[i] = 0; // CcuPfeCfgGenerator 基于默认配置生成分配策略，jettyCtx分配时校验资源是否不足
        interMaxJettyNums[i] = CCU_PER_DIE_JETTY_RESERVED_NUM - INNER_MAX_TA_JETTY_ID_OFFSET;
    }

    // 不同die的feId独立分配，可能一致，需要die粒度去重
    std::array<std::unordered_set<uint32_t>, MAX_CCU_IODIE_NUM> dieFuncIdSet;
    for (auto& param : eidInfoList) {
        const uint32_t dieId = param.dieId;
        if (dieId >= MAX_CCU_IODIE_NUM) {
            continue; // 跳过HCCL不使用的dieId
        }

        if (!dieEnableFlags[dieId]) {
            continue; // die如果未使能认为无需分配
        }

        const uint32_t feId = param.funcId;
        if (dieFuncIdSet[dieId].find(feId) != dieFuncIdSet[dieId].end()) {
            continue; // 跳过已配置的feId
        }

        uint32_t startJettyCtxId = feId > MAX_CCU_INNER_FE_IDX ?
            INNER_MAX_TA_JETTY_ID_OFFSET : pfeCnts[dieId] * ONE_CCU_PFE_USE_JETTY_NUM;
        uint32_t startTaJettyId = CCU_START_TA_JETTY_ID + startJettyCtxId;
        uint8_t pfeJettyNum = feId > MAX_CCU_INNER_FE_IDX ? // 当前框内和出框jetty数量均为静态值
            interMaxJettyNums[dieId] : ONE_CCU_PFE_USE_JETTY_NUM;

        PfeJettyCtxCfg cfg{feId, startJettyCtxId, startTaJettyId, pfeJettyNum};
        pfeJettyCtxCfgs[dieId].emplace_back(std::move(cfg));
        dieFuncIdSet[dieId].insert(feId);
        pfeCnts[dieId] += 1;

        HCCL_RUN_INFO("[CcuPfeCfgGenerator] new pfe cfg set: feId[%u] startJettyCtxId[%u] "
            "startTaJettyId[%u] pfeJettyNum[%u].", feId, startJettyCtxId, startTaJettyId,
            pfeJettyNum);
    }

    initFlag = true;
}

std::vector<PfeJettyCtxCfg> CcuPfeCfgGenerator::GetPfeJettyCtxCfg(const uint8_t dieId)
{
    if (dieId >= MAX_CCU_IODIE_NUM) {
        HCCL_WARNING("[CcuPfeCfgGenerator][PfeJettyCtxCfg] invaild dieId[%u]", dieId);
        return std::vector<PfeJettyCtxCfg>();
    }

    if (pfeJettyCtxCfgs[dieId].empty()) {
        HCCL_WARNING("[CcuPfeCfgGenerator][PfeJettyCtxCfg] pfeJettyCtxCfgMap is empty, dieId[%u]", dieId);
        return std::vector<PfeJettyCtxCfg>();
    }

    return pfeJettyCtxCfgs[dieId];
}

}; // Hccl
