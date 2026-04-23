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

#include "common.h"
#include "plane_transformer_factory.h"

namespace hccl {
namespace {
bool ValidatePlaneSize(u32 planeSize)
{
    return planeSize > 1 && (planeSize % 2) == 1;
}

bool g_regTriggerNB = RegisterAlgo<HcclAlgoType::HCCL_ALGO_TYPE_NB, PlaneTransformerNB>();
bool g_regTriggerNHR = RegisterAlgo<HcclAlgoType::HCCL_ALGO_TYPE_NHR, PlaneTransformerNB>();
}  // namespace

std::unique_ptr<IPlaneTransformer> PlaneTransformerNB::Clone() const
{
    return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) PlaneTransformerNB(*this));
}

std::vector<std::vector<u32>> PlaneTransformerNB::CalcPlaneTransform(const u32 planeSize, const u32 planeNum)
{
    (void)planeNum;
    if (!ValidatePlaneSize(planeSize)) {
        return BuildIdentityTransform(planeSize);
    }

    std::set<u32> offsetSet;
    u32 base = 1;
    while (offsetSet.count(base) == 0) {
        offsetSet.insert(base);
        base = (base << 1) % planeSize;
    }

    std::vector<u32> offsets(offsetSet.begin(), offsetSet.end());
    std::vector<std::vector<u32>> rankMapping(offsets.size(), std::vector<u32>(planeSize, 0));
    for (u32 planeId = 0; planeId < offsets.size(); ++planeId) {
        const u32 offset = offsets[planeId];
        for (u32 rank = 0; rank < planeSize; ++rank) {
            const u32 mappedIndex = (offset * rank) % planeSize;
            rankMapping[planeId][mappedIndex] = rank;
        }
    }
    return rankMapping;
}
}  // namespace hccl
