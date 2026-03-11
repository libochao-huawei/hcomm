/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "plane_transformer_factory.h"
#include "log.h"

namespace hccl {

// RingPlaneTransformer 实现
std::vector<std::vector<u32>> RingPlaneTransformer::CalcPlaneTransform(u32 planeSize, u32 planeNum)
{
    // Ring 算法返回单位矩阵：每个 rank 映射到自身
    // transformMatrix[i][j] = j
    std::vector<std::vector<u32>> transformMatrix(planeNum);
    for (u32 i = 0; i < planeNum; ++i) {
        transformMatrix[i].resize(planeSize);
        for (u32 j = 0; j < planeSize; ++j) {
            transformMatrix[i][j] = j;
        }
    }
    return transformMatrix;
}

// PlaneTransformerFactory 实现
PlaneTransformerFactory& PlaneTransformerFactory::Instance()
{
    static PlaneTransformerFactory instance;
    return instance;
}

std::unique_ptr<IPlaneTransformer> PlaneTransformerFactory::Get(HcclAlgoType algType)
{
    std::lock_guard<std::mutex> lock(mutex_);

    switch (algType) {
        case HcclAlgoType::HCCL_ALGO_TYPE_RING:
            return std::unique_ptr<IPlaneTransformer>(new RingPlaneTransformer());
        default:
            // 对于其他算法类型（NB、HD、NHR等），暂时返回 nullptr
            // 后续可以扩展实现
            HCCL_WARNING("[OXC][PlaneTransformerFactory] Algorithm type[%d] not supported, using identity mapping.",
                         static_cast<s32>(algType));
            return nullptr;
    }
}

} // namespace hccl
