/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_task_cache_key.h"

#include <sstream>

namespace Hccl {

HcclResult AicpuTaskCacheKey::GetAicpuTaskCacheTag(const char* commId, const HcclCMDType opType,
    const HcclDataType dataType, const HcclReduceOp reduceType, const bool isZeroCopy,
    const uint64_t inputSize, const uint64_t outputSize, const OpMode opMode, std::string& cacheTag)
{
    // 校验commId (一定不含delimiter)
    const char delimiter = '-';
    CHK_PTR_NULL(commId);
    CHK_RET(strchr(commId, delimiter) != nullptr,
        HCCL_ERROR("[AicpuTaskCacheKey][AicpuTaskCacheKey] commId[%s] contains delimiter[%c]", commId, delimiter),
        HCCL_E_PARA);

    // 校验opType
    CHK_RET(opType == HcclCMDType::HCCL_CMD_INVALID,
        HCCL_ERROR("[AicpuTaskCacheKey][AicpuTaskCacheKey] opType is invalid"),
        HCCL_E_PARA);

    // 暂时不考虑v类算子, dataType一定不是reserved
    CHK_RET(dataType == HcclDataType::HCCL_DATA_TYPE_RESERVED,
        HCCL_ERROR("[AicpuTaskCacheKey][AicpuTaskCacheKey] dataType is reserved"),
        HCCL_E_PARA);

    // 使用'-'作为间隔符, 拼接cacheTag
    // 注意: 把input/output size放在前面, 减少解析开销
    // 注意: commId放在最后, 如果需要解析commId则无需处理delimiter (否则需要使用getline(iss, commId, delimiter))
    std::ostringstream oss;
    oss << inputSize << delimiter
        << outputSize << delimiter
        << static_cast<uint8_t>(opType) << delimiter
        << static_cast<uint8_t>(dataType) << delimiter
        << static_cast<uint8_t>(reduceType) << delimiter
        << static_cast<uint8_t>(isZeroCopy) << delimiter
        << static_cast<uint8_t>(opMode) << delimiter
        << commId;
    cacheTag = oss.str();

    HCCL_INFO("[AicpuTaskCacheKey][GetAicpuTaskCacheTag] cacheTag[%s] from commId[%s] opType[%d] dataType[%d] "
        "reduceType[%d] isZeroCopy[%d] inputSize[%llu] outputSize[%llu] opMode[%d]",
        cacheTag.c_str(), commId, opType, dataType, reduceType, isZeroCopy, inputSize, outputSize, opMode);

    return HCCL_SUCCESS;
}

HcclResult AicpuTaskCacheKey::ParseAicpuTaskCacheTag(const std::string& cacheTag, uint64_t& inputSize, uint64_t& outputSize)
{
    std::istringstream iss(cacheTag);
    char delimiter = '-';
    iss >> inputSize >> delimiter >> outputSize;

    HCCL_INFO("[AicpuTaskCacheKey][ParseAicpuTaskCacheTag] cacheTag[%s] to inputSize[%llu] outputSize[%llu]",
        cacheTag.c_str(), inputSize, outputSize);
        
    return HCCL_SUCCESS;
}

} // namespace Hccl