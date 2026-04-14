/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TOPOINFO_PLANE_TRANSFORMER_H
#define TOPOINFO_PLANE_TRANSFORMER_H

#include "topoinfo_struct.h"

namespace hccl {
/**
 * @brief `TopoinfoPlaneTransformer` 在主仓中的受限子集落地。
 *
 * @note 当前阶段只落地并行平面解析/计算相关的最小能力子集：
 *       `ParsePlane`、`ParseUniformPlane` 以及必要 helper。
 *       `RegroupAndSelectAlgo`、`ReparseGroupedPlane`、`TransformPlaneByAlgo`
 *       等更深的平面变换能力不属于本阶段范围。
 */
class TopoinfoPlaneTransformer {
public:
    /**
     * @brief 为 subcomm 解析并计算 netPlane 信息。
     * @param globalRankTable 全局通信域 ranktable，保留全局 rank 身份语义。
     * @param subRankTable 子通信域 ranktable，包含被分区后的 rank 成员。
     * @param subCommRankId 当前 rank 在 subcomm 内的局部编号。
     * @param netPlaneId 输出：当前 rank 所属并行平面 ID。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParsePlane(const RankTable_t &globalRankTable, const RankTable_t &subRankTable,
        u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum);

private:
    /**
     * @brief 针对当前阶段支持的均匀平面场景计算 netPlane 信息。
     * @param globalRankTable 全局通信域 ranktable。
     * @param subRankTable 子通信域 ranktable。
     * @param subCommRankId 当前 rank 在 subcomm 内的局部编号。
     * @param netPlaneId 输出：当前 rank 所属并行平面 ID。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParseUniformPlane(const RankTable_t &globalRankTable, const RankTable_t &subRankTable,
        u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum);
};
}  // namespace hccl

#endif  // TOPOINFO_PLANE_TRANSFORMER_H
