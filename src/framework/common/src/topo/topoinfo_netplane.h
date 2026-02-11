/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_TOPOINFO_NETPLANE_H
#define HCCL_TOPOINFO_NETPLANE_H

#include "hccl_common.h"
#include "topo_info.h"
#include "topoinfo_struct.h"

namespace hccl {
/**
 * @brief TopoinfoNetplane 类提供并行平面信息的计算功能
 *
 * 该类根据 globalRankTable 和 subRankTable 计算当前 rank 的并行平面信息（netPlaneId 和 netPlaneNum）。
 * 并行平面信息用于标识当前 rank 所属的并行平面以及总的并行平面数量。
 */
class TopoinfoNetplane {
public:
    /**
     * @brief 根据 globalRankTable 和 subRankTable 计算并行平面信息
     *
     * @param globalRankTable 全局 RankTable（包含所有 rank 的拓扑信息）
     * @param subRankTable 子 RankTable（从 globalRankTable 中提取的部分 rank 的拓扑信息）
     * @param netPlaneId 输出参数：当前 rank 的并行平面 ID
     * @param netPlaneNum 输出参数：总的并行平面数量
     * @return HcclResult
     * @retval HCCL_SUCCESS 计算成功
     * @retval HCCL_E_PARA 参数错误
     *
     * @note 当前实现为假的计算逻辑，返回固定值（netPlaneId=0, netPlaneNum=1）。
     *       实际使用时需要根据具体业务逻辑替换计算方法。
     */
    static HcclResult CalculateNetPlaneInfo(const RankTable_t &globalRankTable,
                                             const RankTable_t &subRankTable,
                                             u32 &netPlaneId,
                                             u32 &netPlaneNum);
};
} // namespace hccl

#endif // HCCL_TOPOINFO_NETPLANE_H
