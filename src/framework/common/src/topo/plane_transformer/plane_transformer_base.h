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
#include <vector>

#include "hccl/base.h"

namespace hccl {
/**
 * @brief 平面变换器抽象接口。
 *
 * @note factory 阶段通过该接口把不同算法的平面变换矩阵计算逻辑解耦。
 */
class IPlaneTransformer {
public:
    virtual ~IPlaneTransformer() = default;

    /**
     * @brief 克隆当前变换器实例。
     * @return std::unique_ptr<IPlaneTransformer>
     */
    virtual std::unique_ptr<IPlaneTransformer> Clone() const = 0;

    /**
     * @brief 根据平面规模与平面数量计算变换矩阵。
     * @param planeSize 当前平面规模。
     * @param planeNum 当前对称平面数量。
     * @return std::vector<std::vector<u32>>
     */
    virtual std::vector<std::vector<u32>> CalcPlaneTransform(u32 planeSize, u32 planeNum) = 0;

protected:
    /**
     * @brief 生成单位矩阵式的稳定映射。
     * @param planeSize 平面规模。
     * @return std::vector<std::vector<u32>>
     */
    static std::vector<std::vector<u32>> BuildIdentityTransform(u32 planeSize)
    {
        std::vector<std::vector<u32>> transform(1, std::vector<u32>(planeSize, 0));
        for (u32 i = 0; i < planeSize; ++i) {
            transform[0][i] = i;
        }
        return transform;
    }
};
}  // namespace hccl

#endif  // PLANE_TRANSFORMER_BASE_H
