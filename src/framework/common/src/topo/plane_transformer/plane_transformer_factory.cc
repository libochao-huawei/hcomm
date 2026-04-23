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
namespace {
class RingPlaneTransformer final : public IPlaneTransformer {
public:
    std::unique_ptr<IPlaneTransformer> Clone() const override
    {
        return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) RingPlaneTransformer(*this));
    }

    std::vector<std::vector<u32>> CalcPlaneTransform(u32 planeSize, u32 planeNum) override
    {
        (void)planeNum;
        return BuildIdentityTransform(planeSize);
    }
};

bool g_regTriggerRing = RegisterAlgo<HcclAlgoType::HCCL_ALGO_TYPE_RING, RingPlaneTransformer>();
bool g_regTriggerDefault = RegisterAlgo<HcclAlgoType::HCCL_ALGO_TYPE_DEFAULT, RingPlaneTransformer>();
}  // namespace

PlaneTransformerFactory &PlaneTransformerFactory::Instance()
{
    static PlaneTransformerFactory instance;
    return instance;
}

bool PlaneTransformerFactory::Register(HcclAlgoType algType, Creator creator)
{
    registry_[algType] = std::move(creator);
    return true;
}

std::unique_ptr<IPlaneTransformer> PlaneTransformerFactory::Get(HcclAlgoType algType) const
{
    auto iter = registry_.find(algType);
    if (iter == registry_.end() || !iter->second) {
        HCCL_WARNING("[OXC_HCOMM][PlaneTransformerFactory][Get] algType[%u] is not registered.",
            static_cast<u32>(algType));
        return nullptr;
    }
    return iter->second();
}
}  // namespace hccl
