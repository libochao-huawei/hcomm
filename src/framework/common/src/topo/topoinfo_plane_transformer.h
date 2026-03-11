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

#include <vector>
#include <cmath>
#include <unordered_map>
#include <map>
#include <numeric>
#include "topoinfo_struct.h"
#include "hccl_common.h"
#include "common.h"  // HcclAlgoType 定义
#include "plane_transformer/plane_transformer_factory.h"

namespace hccl {

constexpr u32 HCCL_ADD_FLAG = 0;
constexpr u32 HCCL_SUBTRACT_FLAG = 1;
constexpr double STATIC_DELAY = 10.0;
constexpr double TRANSMIT_DELAY = 5.0;

/**
 * @brief TopoinfoPlaneTransformer 类提供并行平面解析和变换功能
 *
 * 该类根据 rankIds 和 globalRankList 计算并行平面信息，
 * 并支持根据不同算法类型进行平面变换。
 */
class TopoinfoPlaneTransformer {
public:
    /**
     * @brief 解析并行平面信息（数组版本）
     *
     * @param rankNum rank 数量
     * @param rankIds rank ID 数组
     * @param globalRankList 全局 rank 列表
     * @param subRankList 子 rank 列表（输出参数）
     * @param netPlaneNum 并行平面数量（输出参数）
     * @return HcclResult
     */
    static HcclResult ParsePlane(u32 rankNum, const u32 *rankIds, const std::vector<RankInfo_t> &globalRankList,
        std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    /**
     * @brief 解析并行平面信息（vector 版本）
     *
     * @param rankIds rank ID 向量
     * @param globalRankList 全局 rank 列表
     * @param subRankList 子 rank 列表（输出参数）
     * @param netPlaneNum 并行平面数量（输出参数）
     * @return HcclResult
     */
    static HcclResult ParsePlane(const std::vector<u32> &rankIds, const std::vector<RankInfo_t> &globalRankList,
        std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    /**
     * @brief 分组重整与算法选择
     *
     * @param netPlaneNum 并行平面数量
     * @param enabledBroke 是否启用 broke 模式
     * @param groups 分组信息（输入/输出参数）
     * @param intraAlgType 内部算法类型（输出参数）
     * @param interAlgType 外部算法类型（输出参数）
     * @return HcclResult
     */
    static HcclResult RegroupAndSelectAlgo(const u32 netPlaneNum, const bool enabledBroke,
        std::vector<std::vector<u32>> &groups, HcclAlgoType &intraAlgType, HcclAlgoType &interAlgType);

    /**
     * @brief 重新解析分组平面信息
     *
     * @param localIndex 本地索引
     * @param groups 分组信息
     * @param netPlaneId 并行平面 ID（输入/输出参数）
     * @param netPlaneNum 并行平面数量（输入/输出参数）
     * @return HcclResult
     */
    static HcclResult ReparseGroupedPlane(u32 localIndex, const std::vector<std::vector<u32>> &groups,
        u32 &netPlaneId, u32 &netPlaneNum);

    /**
     * @brief 根据算法类型变换平面
     *
     * @param algType 算法类型
     * @param netPlaneId 并行平面 ID
     * @param localIndex 本地索引
     * @param groups 分组信息
     * @param indexList 索引列表（输出参数）
     * @return HcclResult
     */
    static HcclResult TransformPlaneByAlgo(HcclAlgoType algType, u32 netPlaneId, u32 localIndex,
        const std::vector<std::vector<u32>> &groups, std::vector<u32> &indexList);

private:
    /**
     * @brief 计算对称组大小
     */
    static u32 CalcSymGroupSize(const std::vector<std::vector<u32>> &groups);

    /**
     * @brief 解析均匀平面
     */
    static HcclResult ParseUniformPlane(const std::vector<u32> &rankIds,
        const std::vector<RankInfo_t> &globalRankList, std::vector<RankInfo_t> &subRankList, u32 &netPlaneNum);

    /**
     * @brief 应用变换矩阵
     */
    static HcclResult ApplyTransform(u32 netPlaneId, u32 localIndex, const std::vector<std::vector<u32>> &groups,
        const std::vector<std::vector<u32>> &transformMatrix, std::vector<u32> &indexList);
};

} // namespace hccl

#endif // TOPOINFO_PLANE_TRANSFORMER_H
