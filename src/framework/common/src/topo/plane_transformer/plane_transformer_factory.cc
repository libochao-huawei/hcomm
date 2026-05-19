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

#include <algorithm>
#include <utility>

#include "plane_transformer_nb.h"
#include "plane_transformer_recursive_hd.h"

namespace hccl {
namespace {
class PlaneTransformerRing final : public IPlaneTransformer {
public:
    std::unique_ptr<IPlaneTransformer> Clone() const override
    {
        return ClonePlaneTransformer(*this);
    }

    HcclResult CalcPlaneTransform(const u32 planeSize, const u32 planeNum,
        TransformMatrix &transformMatrix) const override
    {
        (void)planeNum;
        BuildIdentityMatrix(planeSize, transformMatrix);
        return HCCL_SUCCESS;
    }
};

class PlaneTransformerReverse final : public IPlaneTransformer {
public:
    std::unique_ptr<IPlaneTransformer> Clone() const override
    {
        return ClonePlaneTransformer(*this);
    }

    HcclResult CalcPlaneTransform(const u32 planeSize, const u32 planeNum,
        TransformMatrix &transformMatrix) const override
    {
        (void)planeNum;
        BuildIdentityMatrix(planeSize, transformMatrix);
        std::reverse(transformMatrix.front().begin(), transformMatrix.front().end());
        return HCCL_SUCCESS;
    }
};

template<typename T>
std::unique_ptr<IPlaneTransformer> MakeTransformer()
{
    return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) T());
}
}  // namespace

PlaneTransformerFactory &PlaneTransformerFactory::Instance()
{
    static PlaneTransformerFactory instance;
    return instance;
}

PlaneTransformerFactory::PlaneTransformerFactory()
{
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_RING, MakeTransformer<PlaneTransformerRing>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_AHC, MakeTransformer<PlaneTransformerRing>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_AHC_BROKE, MakeTransformer<PlaneTransformerReverse>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_NB, MakeTransformer<PlaneTransformerNB>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_NHR, MakeTransformer<PlaneTransformerNB>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_NHR_V1, MakeTransformer<PlaneTransformerNB>());
    registry_.emplace(HcclAlgoType::HCCL_ALGO_TYPE_HDR, MakeTransformer<PlaneTransformerRecursiveHD>());
}

const IPlaneTransformer *PlaneTransformerFactory::Get(const HcclAlgoType algoType) const
{
    auto iter = registry_.find(algoType);
    return (iter == registry_.end()) ? nullptr : iter->second.get();
}
}  // namespace hccl
