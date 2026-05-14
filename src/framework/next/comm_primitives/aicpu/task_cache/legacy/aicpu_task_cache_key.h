/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_AICPU_TASK_CACHE_KEY_H
#define HCCLV2_AICPU_TASK_CACHE_KEY_H

#include <cstdint>
#include <functional>
#include <string>

#include "hccl_types.h"
#include "coll_alg_param.h"

namespace Hccl {

// 注意: 目前A5架构下暂不支持重执行和aclgraph, 因此不需要inplacePreSync以及isCapture字段
// inplacePreSync: 前同步下, 同一个算子会切分成两个aicpu kernel
// isCapture: A3下aclgraph销毁时会重新建链导致CCL buffer变化; A5如果aclgraph实现合理, 理论上不需要重新分配CCL buffer VA
struct AicpuTaskCacheKey {
    explicit AicpuTaskCacheKey();
    explicit AicpuTaskCacheKey(const AicpuTaskCacheKey& other);

    HcclResult Init(const HcclCMDType curOpType, const HcclDataType curDataType, const HcclReduceOp curReduceType,
        const bool curIsZeroCopy, const uint64_t curInputSize, const OpMode curOpMode);

    std::string GetKeyString() const;

    bool operator==(const AicpuTaskCacheKey& other) const;

    const AicpuTaskCacheKey& operator=(const AicpuTaskCacheKey& other);

    HcclCMDType opType;
    HcclDataType dataType;
    HcclReduceOp reduceType;
    bool isZeroCopy;
    uint64_t inputSize;
    OpMode opMode;
};

} // namespace Hccl

namespace std {

template <>
struct hash<Hccl::AicpuTaskCacheKey> {
    size_t operator()(const Hccl::AicpuTaskCacheKey& key) const
    {
        std::hash<bool> hashBool;
        std::hash<uint8_t> hashUint8;
        std::hash<uint64_t> hashUint64;

        const size_t opTypeHashValue = hashUint8(static_cast<uint8_t>(key.opType));
        const size_t dataTypeHashValue = hashUint8(static_cast<uint8_t>(key.dataType));
        const size_t reduceTypeHashValue = hashUint8(static_cast<uint8_t>(key.reduceType));

        const size_t isZeroCopyHashValue = hashBool(key.isZeroCopy);
        const size_t inputSizeHashValue = hashUint64(key.inputSize);
        const size_t opModeHashValue = hashUint8(static_cast<uint8_t>(key.opMode));

        size_t hashValue = opTypeHashValue;
        hashValue ^= dataTypeHashValue;
        hashValue ^= reduceTypeHashValue;
        hashValue ^= isZeroCopyHashValue;
        hashValue ^= inputSizeHashValue;
        hashValue ^= opModeHashValue;

        return hashValue;
    }
};

} // namespace std

#endif // HCCLV2_AICPU_TASK_CACHE_KEY_H
