/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "topoinfo_netplane.h"
#include "log.h"

namespace hccl {

HcclResult TopoinfoNetplane::CalculateNetPlaneInfo(const RankTable_t &globalRankTable,
                                                    const RankTable_t &subRankTable,
                                                    u32 &netPlaneId,
                                                    u32 &netPlaneNum)
{
    // TODO: 用户设计的计算逻辑
    // 当前实现：假的计算逻辑，返回固定值，待替换为实际计算逻辑

    // 输入参数校验
    if (globalRankTable.rankNum == 0 || globalRankTable.rankList.empty()) {
        HCCL_ERROR("[%s] globalRankTable is empty", __func__);
        return HCCL_E_PARA;
    }

    if (subRankTable.rankNum == 0 || subRankTable.rankList.empty()) {
        HCCL_ERROR("[%s] subRankTable is empty", __func__);
        return HCCL_E_PARA;
    }

    // 假的计算逻辑：所有 rank 在同一个并行平面
    netPlaneId = 0;
    netPlaneNum = 1;

    HCCL_INFO("[%s] CalculateNetPlaneInfo: netPlaneId[%u], netPlaneNum[%u] (placeholder implementation)",
              __func__, netPlaneId, netPlaneNum);

    return HCCL_SUCCESS;
}

} // namespace hccl
