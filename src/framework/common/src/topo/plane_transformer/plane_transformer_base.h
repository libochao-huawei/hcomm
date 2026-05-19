/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PLANE_TRANSFORMER_BASE_H
#define PLANE_TRANSFORMER_BASE_H

#include <memory>
#include <numeric>
#include <vector>

#include "common.h"

namespace hccl {
using TransformMatrix = std::vector<std::vector<u32>>;

class IPlaneTransformer {
public:
    virtual ~IPlaneTransformer() = default;
    virtual std::unique_ptr<IPlaneTransformer> Clone() const = 0;
    virtual HcclResult CalcPlaneTransform(u32 planeSize, u32 planeNum,
        TransformMatrix &transformMatrix) const = 0;
};

inline std::vector<u32> BuildIdentityOrder(u32 planeSize)
{
    std::vector<u32> indexList(planeSize);
    std::iota(indexList.begin(), indexList.end(), 0U);
    return indexList;
}

inline void BuildIdentityIndexList(u32 planeSize, std::vector<u32> &indexList)
{
    indexList = BuildIdentityOrder(planeSize);
}

inline void BuildIdentityMatrix(u32 planeSize, TransformMatrix &transformMatrix)
{
    transformMatrix.assign(1, BuildIdentityOrder(planeSize));
}

template<typename T>
inline std::unique_ptr<IPlaneTransformer> ClonePlaneTransformer(const T &transformer)
{
    return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) T(transformer));
}

template<typename T>
inline HcclResult ApplyTransform(const std::vector<u32> &indexList, std::vector<T> &values)
{
    CHK_PRT_RET(indexList.size() != values.size(),
        HCCL_ERROR("[OXC_HCOMM][PlaneTransformer][ApplyTransform] indexListSize[%zu] != valueSize[%zu].",
            indexList.size(), values.size()), HCCL_E_PARA);

    std::vector<T> reorderedValues;
    reorderedValues.reserve(values.size());
    for (u32 index : indexList) {
        CHK_PRT_RET(index >= values.size(),
            HCCL_ERROR("[OXC_HCOMM][PlaneTransformer][ApplyTransform] index[%u] out of range[%zu].",
                index, values.size()), HCCL_E_PARA);
        reorderedValues.push_back(values[index]);
    }
    values = std::move(reorderedValues);
    return HCCL_SUCCESS;
}
}  // namespace hccl

#endif
