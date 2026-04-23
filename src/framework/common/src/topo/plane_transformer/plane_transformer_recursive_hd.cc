/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "plane_transformer_recursive_hd.h"

#include <vector>

#include "common.h"
#include "plane_transformer_factory.h"

namespace hccl {
namespace {
/**
 * @brief 判断 plane size 是否满足 HD/HDR 递归变换的基础约束。
 * @param planeSize 当前 plane 大小。
 * @return true 表示 `planeSize` 为大于 1 的 2 次幂。
 * @return false 表示需要退化为单位矩阵。
 */
bool IsPowerOfTwo(const u32 planeSize)
{
    return planeSize > 1 && (planeSize & (planeSize - 1)) == 0;
}

/**
 * @brief 判断 rank 的二进制表示中 `1` 的个数是否为奇数。
 * @param rank 当前 rank 编号。
 * @return true 表示 popcount 为奇数。
 * @return false 表示 popcount 为偶数。
 */
bool HasOddBitCount(const u32 rank)
{
    return (__builtin_popcount(rank) & 1) == 1;
}

/**
 * @brief 收集所有 popcount 为奇数的 rank。
 * @param planeSize 当前 plane 大小。
 * @return std::vector<u32>
 *
 * @note 这些 rank 会构成用户指定的“左移链”，同时决定 meta plane 的数量。
 */
std::vector<u32> CollectOddBitCountRanks(const u32 planeSize)
{
    std::vector<u32> oddRanks;
    oddRanks.reserve(planeSize);
    for (u32 rank = 0; rank < planeSize; ++rank) {
        if (HasOddBitCount(rank)) {
            oddRanks.push_back(rank);
        }
    }
    return oddRanks;
}

/**
 * @brief 按用户最新指定语义构造 `prevMgr`。
 * @param planeSize 当前 plane 大小。
 * @param oddRanks 所有 popcount 为奇数的 rank，按升序排列。
 * @return std::vector<u32>
 *
 * @note 语义对齐用户更正后的说明：
 *       1. 若 `i` 本身是奇数-popcount rank，则 `prevMgr[i]` 取“前一个奇数-popcount rank”；
 *       2. 若 `i` 本身不是奇数-popcount rank，则 `prevMgr[i] = i`；
 *       3. 左移链按首尾相接处理，因此首个奇数-popcount rank 的前驱回绕到最后一个奇数 rank。
 */
std::vector<u32> BuildPrevMgr(const u32 planeSize, const std::vector<u32> &oddRanks)
{
    std::vector<u32> prevMgr(planeSize, 0);
    for (u32 rank = 0; rank < planeSize; ++rank) {
        prevMgr[rank] = rank;
    }

    if (oddRanks.empty()) {
        return prevMgr;
    }

    for (u32 oddIndex = 0; oddIndex < oddRanks.size(); ++oddIndex) {
        const u32 rank = oddRanks[oddIndex];
        const u32 prevOddRank = (oddIndex == 0) ? oddRanks.back() : oddRanks[oddIndex - 1];
        prevMgr[rank] = prevOddRank;
    }
    return prevMgr;
}

bool g_regTriggerHDR = RegisterAlgo<HcclAlgoType::HCCL_ALGO_TYPE_HDR, PlaneTransformerRecursiveHD>();
}  // namespace

std::unique_ptr<IPlaneTransformer> PlaneTransformerRecursiveHD::Clone() const
{
    return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) PlaneTransformerRecursiveHD(*this));
}

std::vector<std::vector<u32>> PlaneTransformerRecursiveHD::CalcPlaneTransform(const u32 planeSize, const u32 planeNum)
{
    (void)planeNum;
    if (!IsPowerOfTwo(planeSize)) {
        return BuildIdentityTransform(planeSize);
    }

    const std::vector<u32> oddRanks = CollectOddBitCountRanks(planeSize);
    if (oddRanks.empty()) {
        return BuildIdentityTransform(planeSize);
    }

    const u32 metaPlaneNum = static_cast<u32>(oddRanks.size());
    const std::vector<u32> prevMgr = BuildPrevMgr(planeSize, oddRanks);

    std::vector<std::vector<u32>> rankMapping(metaPlaneNum, std::vector<u32>(planeSize, 0));
    for (u32 rank = 0; rank < planeSize; ++rank) {
        rankMapping[0][rank] = rank;
    }

    /**
     * @brief 递归生成变换矩阵。
     *
     * @note 严格对齐用户口述：
     *       `rankMapping[planeId][i] = rankMapping[planeId - 1][prevMgr[i]]`。
     */
    for (u32 planeId = 1; planeId < metaPlaneNum; ++planeId) {
        for (u32 rank = 0; rank < planeSize; ++rank) {
            rankMapping[planeId][rank] = rankMapping[planeId - 1][prevMgr[rank]];
        }
    }
    return rankMapping;
}
}  // namespace hccl
