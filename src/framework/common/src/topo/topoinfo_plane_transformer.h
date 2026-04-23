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
#include "topoinfo_struct.h"

namespace hccl {
/**
 * @brief `TopoinfoPlaneTransformer` 负责 OXC 并行平面解析与工厂化平面变换。
 *
 * @note 当前实现已经覆盖：
 *       1. `ParsePlane` / `ParseUniformPlane` 的 rank 级平面信息写回；
 *       2. `RegroupAndSelectAlgo` / `ReparseGroupedPlane` / `TransformPlaneByAlgo`
 *          的完整工厂化调用链；
 *       3. `plane_transformer_factory`、`plane_transformer_nb`、
 *          `plane_transformer_recursive_hd` 的接入。
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

    /**
     * @brief 对并行平面分组做对称化重整，并给出组内/组间算法选择结果。
     * @param netPlaneNum 当前通信域已有的并行平面总数。
     * @param enabledBroke 是否允许 broke 风格分组切分。
     * @param groups 输入输出：分组信息，函数可能原地重整为更对称的布局。
     * @param intraAlgType 输出：组内算法选择结果。
     * @param interAlgType 输出：组间算法选择结果。
     * @return HcclResult
     */
    static HcclResult RegroupAndSelectAlgo(u32 netPlaneNum, bool enabledBroke, std::vector<std::vector<u32>> &groups,
        HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType);

    /**
     * @brief 基于重整后的分组结果重新计算当前 rank 的并行平面编号。
     * @param localIndex 当前 rank 在被重排通信域中的本地索引。
     * @param groups 分组信息。
     * @param netPlaneId 输入输出：当前平面 ID，会被重算后的值覆盖。
     * @param netPlaneNum 输入输出：当前平面总数，会被重算后的值覆盖。
     * @return HcclResult
     */
    static HcclResult ReparseGroupedPlane(u32 localIndex, const std::vector<std::vector<u32>> &groups,
        u32 &netPlaneId, u32 &netPlaneNum);

    /**
     * @brief 根据算法类型执行平面重排。
     * @param algType 算法类型。
     * @param netPlaneId 当前并行平面 ID。
     * @param localIndex 当前 rank 在通信域中的本地索引。
     * @param groups 分组信息。
     * @param indexList 输出：重排后的索引映射。
     * @return HcclResult
     */
    static HcclResult TransformPlaneByAlgo(HcclAlgoType algType, u32 netPlaneId, u32 localIndex,
        const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList);

private:
    /**
     * @brief 针对均匀平面场景计算 netPlane 信息。
     * @param rankIds 子通信域成员在全局通信域中的 rankId 列表。
     * @param globalRankList 全局通信域 rank 列表。
     * @param subRankList 子通信域 rank 列表，函数会写回每个 rank 的 netPlane 信息。
     * @param netPlaneNum 输出：当前通信域总并行平面数量。
     * @return HcclResult
     */
    static HcclResult ParseUniformPlane(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList,
        std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    /**
     * @brief 计算分组中能够保持完全对称的最小组大小。
     * @param groups 分组信息。
     * @return u32
     */
    static u32 CalcSymGroupSize(const std::vector<std::vector<u32>> &groups);

    /**
     * @brief 按变换矩阵应用平面重排。
     * @param netPlaneId 当前并行平面 ID。
     * @param localIndex 当前 rank 的本地索引。
     * @param groups 分组信息。
     * @param transformMatrix 平面变换矩阵。
     * @param indexList 输出：变换后的索引表。
     * @return HcclResult
     */
    static HcclResult ApplyTransform(u32 netPlaneId, u32 localIndex, const std::vector<std::vector<u32>> &groups,
        const std::vector<std::vector<u32>> &transformMatrix, std::vector<u32> &indexList);
};
}  // namespace hccl

#endif  // TOPOINFO_PLANE_TRANSFORMER_H
