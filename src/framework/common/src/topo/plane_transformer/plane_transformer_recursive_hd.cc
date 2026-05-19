/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "plane_transformer_recursive_hd.h"

namespace hccl {
namespace {
bool IsPowerOfTwo(const u32 value)
{
    return value > 0U && ((value & (value - 1U)) == 0U);
}

u32 CountBits(const u32 value)
{
    u32 count = 0;
    u32 current = value;
    while (current != 0U) {
        count += (current & 1U);
        current >>= 1U;
    }
    return count;
}
}

std::unique_ptr<IPlaneTransformer> PlaneTransformerRecursiveHD::Clone() const
{
    return ClonePlaneTransformer(*this);
}

HcclResult PlaneTransformerRecursiveHD::CalcPlaneTransform(const u32 planeSize, const u32 planeNum,
    TransformMatrix &transformMatrix) const
{
    transformMatrix.clear();
    if (!IsPowerOfTwo(planeSize) || planeSize <= 1U) {
        BuildIdentityMatrix(planeSize, transformMatrix);
        return HCCL_SUCCESS;
    }
    if (planeNum <= 1U) {
        BuildIdentityMatrix(planeSize, transformMatrix);
        return HCCL_SUCCESS;
    }

    u32 prevOdd = planeSize - 1U;
    const u32 stepNum = static_cast<u32>(__builtin_ctz(planeSize));
    if ((stepNum & 1U) == 0U && prevOdd > 0U) {
        --prevOdd;
    }

    std::vector<u32> prevMgr(planeSize, 0U);
    u32 numOddInBinary = 0U;
    for (u32 i = 0; i < planeSize; ++i) {
        const bool isOddInBinary = (CountBits(i) & 1U) == 1U;
        if (isOddInBinary) {
            prevMgr[i] = prevOdd;
            prevOdd = i;
            ++numOddInBinary;
        } else {
            prevMgr[i] = i;
        }
    }

    const u32 metaPlaneNum = std::max(1U, numOddInBinary);
    transformMatrix.assign(metaPlaneNum, std::vector<u32>(planeSize, 0U));
    for (u32 i = 0; i < planeSize; ++i) {
        transformMatrix[0][i] = i;
    }
    for (u32 metaPlaneId = 1; metaPlaneId < metaPlaneNum; ++metaPlaneId) {
        for (u32 i = 0; i < planeSize; ++i) {
            transformMatrix[metaPlaneId][i] = transformMatrix[metaPlaneId - 1U][prevMgr[i]];
        }
    }
    return HCCL_SUCCESS;
}
}  // namespace hccl
