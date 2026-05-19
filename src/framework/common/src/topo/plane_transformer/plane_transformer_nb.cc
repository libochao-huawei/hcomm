/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "plane_transformer_nb.h"

#include <set>

namespace hccl {
namespace {
bool ValidatePlaneSize(const u32 planeSize)
{
    return planeSize > 1U && (planeSize % 2U == 1U);
}
}

std::unique_ptr<IPlaneTransformer> PlaneTransformerNB::Clone() const
{
    return ClonePlaneTransformer(*this);
}

HcclResult PlaneTransformerNB::CalcPlaneTransform(const u32 planeSize, const u32 planeNum,
    TransformMatrix &transformMatrix) const
{
    transformMatrix.clear();
    if (!ValidatePlaneSize(planeSize)) {
        BuildIdentityMatrix(planeSize, transformMatrix);
        return HCCL_SUCCESS;
    }
    if (planeNum <= 1U) {
        BuildIdentityMatrix(planeSize, transformMatrix);
        return HCCL_SUCCESS;
    }

    std::set<u32> numSet;
    std::vector<u32> offsets;
    offsets.reserve(planeSize);
    u32 offset = 1U % planeSize;
    while (numSet.count(offset) == 0U) {
        numSet.insert(offset);
        offsets.push_back(offset);
        offset = (offset << 1U) % planeSize;
    }

    const u32 metaPlaneNum = static_cast<u32>(offsets.size());
    transformMatrix.assign(metaPlaneNum, std::vector<u32>(planeSize, 0U));
    for (u32 metaPlaneId = 0; metaPlaneId < metaPlaneNum; ++metaPlaneId) {
        const u32 rotateOffset = offsets[metaPlaneId];
        for (u32 rank = 0; rank < planeSize; ++rank) {
            const u32 mappedIndex = (rotateOffset * rank) % planeSize;
            transformMatrix[metaPlaneId][mappedIndex] = rank;
        }
    }
    return HCCL_SUCCESS;
}
}  // namespace hccl
