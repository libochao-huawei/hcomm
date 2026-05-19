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

#include "common.h"
#include "plane_transformer/plane_transformer_base.h"
#include "topoinfo_struct.h"

namespace hccl {
/**
 * @brief `TopoinfoPlaneTransformer` 负责 OXC 并行平面解析与 A2 layered helper 闭环。
 *
 * @note `ParsePlane` 仍负责 netplane 主线；Layered helper 保留 A2 调用面：
 *       `RegroupAndSelectAlgo/ReparseGroupedPlane/TransformPlaneByAlgo`。
 */
class TopoinfoPlaneTransformer {
public:
    /**
     * @brief 兼容旧 subcomm 调用面的并行平面解析接口。
     * @param globalRankTable 全局通信域 ranktable。
     * @param subRankTable 子通信域 ranktable。
     * @param subCommRankId 当前 rank 在子通信域中的局部编号。
     * @param netPlaneId 输出：当前 rank 所属并行平面 ID。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParsePlane(const RankTable_t &globalRankTable, const RankTable_t &subRankTable,
        u32 subCommRankId, u32 &netPlaneId, u32 &netPlaneNum);

    /**
     * @brief 以 C 风格 rankId 数组为输入解析通信域的并行平面信息。
     * @param rankNum `rankIds` 中元素数量。
     * @param rankIds 子通信域成员在全局通信域中的 rankId 数组。
     * @param globalRankList 全局通信域 rank 列表。
     * @param subRankList 子通信域 rank 列表，函数会把解析结果写回每个 rank。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParsePlane(u32 rankNum, const u32 *rankIds, const std::vector<RankInfo_t> &globalRankList,
        std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    /**
     * @brief 以 `std::vector<u32>` rankId 列表为输入解析通信域的并行平面信息。
     * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
     * @param globalRankList 全局通信域 rank 列表。
     * @param subRankList 子通信域 rank 列表，函数会把解析结果写回每个 rank。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParsePlane(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList,
        std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    static HcclResult RegroupAndSelectAlgo(u32 netPlaneNum, bool enabledBroke, std::vector<std::vector<u32>> &groups,
        HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType);

    static HcclResult ReparseGroupedPlane(u32 localIndex, const std::vector<std::vector<u32>> &groups,
        u32 &netPlaneId, u32 &netPlaneNum);

    static HcclResult TransformPlaneByAlgo(HcclAlgoType algType, u32 netPlaneId, u32 localIndex,
        const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList);

private:
    static HcclResult ApplyTransform(u32 netPlaneId, u32 localIndex,
        const std::vector<std::vector<u32>> &groups, const TransformMatrix &transformMatrix,
        std::vector<u32> &indexList);
    static u32 CalcSymGroupSize(const std::vector<std::vector<u32>> &groups);
};
}  // namespace hccl

#endif  // TOPOINFO_PLANE_TRANSFORMER_H
