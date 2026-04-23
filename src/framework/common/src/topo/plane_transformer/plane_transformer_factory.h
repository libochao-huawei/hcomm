/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef PLANE_TRANSFORMER_FACTORY_H
#define PLANE_TRANSFORMER_FACTORY_H

#include <functional>
#include <memory>
#include <unordered_map>

#include "common.h"
#include "plane_transformer_base.h"

namespace hccl {
/**
 * @brief 平面变换器工厂。
 */
class PlaneTransformerFactory {
public:
    using Creator = std::function<std::unique_ptr<IPlaneTransformer>()>;

    static PlaneTransformerFactory &Instance();

    /**
     * @brief 注册算法与变换器构造器。
     * @param algType 算法类型。
     * @param creator 构造器。
     * @return bool
     */
    bool Register(HcclAlgoType algType, Creator creator);

    /**
     * @brief 获取指定算法对应的变换器实例。
     * @param algType 算法类型。
     * @return std::unique_ptr<IPlaneTransformer>
     */
    std::unique_ptr<IPlaneTransformer> Get(HcclAlgoType algType) const;

private:
    PlaneTransformerFactory() = default;
    std::unordered_map<HcclAlgoType, Creator> registry_;
};

template <HcclAlgoType AlgType, typename TransformerT>
bool RegisterAlgo()
{
    return PlaneTransformerFactory::Instance().Register(AlgType,
        []() { return std::unique_ptr<IPlaneTransformer>(new (std::nothrow) TransformerT()); });
}
}  // namespace hccl

#endif  // PLANE_TRANSFORMER_FACTORY_H
