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

#include <vector>
#include <memory>
#include <mutex>
#include "hccl_common.h"
#include "common.h"  // HcclAlgoType 定义

namespace hccl {

/**
 * @brief 平面变换器接口
 *
 * 定义算法特定的平面变换计算接口
 */
class IPlaneTransformer {
public:
    virtual ~IPlaneTransformer() = default;

    /**
     * @brief 计算平面变换矩阵
     *
     * @param planeSize 平面大小（节点数量）
     * @param planeNum 平面数量
     * @return 变换矩阵，每个元素是一个 rank 索引列表
     */
    virtual std::vector<std::vector<u32>> CalcPlaneTransform(u32 planeSize, u32 planeNum) = 0;
};

/**
 * @brief Ring 算法平面变换器
 *
 * 实现 Ring 算法的平面变换，返回单位矩阵
 */
class RingPlaneTransformer : public IPlaneTransformer {
public:
    std::vector<std::vector<u32>> CalcPlaneTransform(u32 planeSize, u32 planeNum) override;
};

/**
 * @brief 平面变换器工厂（单例模式）
 *
 * 根据算法类型创建对应的平面变换器
 */
class PlaneTransformerFactory {
public:
    /**
     * @brief 获取工厂单例
     */
    static PlaneTransformerFactory& Instance();

    /**
     * @brief 根据算法类型获取平面变换器
     *
     * @param algType 算法类型
     * @return 平面变换器智能指针，如果算法不支持则返回 nullptr
     */
    std::unique_ptr<IPlaneTransformer> Get(HcclAlgoType algType);

private:
    PlaneTransformerFactory() = default;
    ~PlaneTransformerFactory() = default;
    PlaneTransformerFactory(const PlaneTransformerFactory&) = delete;
    PlaneTransformerFactory& operator=(const PlaneTransformerFactory&) = delete;

    std::mutex mutex_;
};

} // namespace hccl

#endif // PLANE_TRANSFORMER_FACTORY_H
