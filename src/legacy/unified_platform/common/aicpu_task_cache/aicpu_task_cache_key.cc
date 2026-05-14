/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sstream>

#include "aicpu_task_cache_key.h"

#include "log.h"

namespace Hccl {
AicpuTaskCacheKey::AicpuTaskCacheKey()
    : opType(HcclCMDType::HCCL_CMD_INVALID), dataType(HcclDataType::HCCL_DATA_TYPE_RESERVED),
      reduceType(HcclReduceOp::HCCL_REDUCE_RESERVED), isZeroCopy(false), inputSize(0), opMode(OpMode::OPBASE)
{
}

AicpuTaskCacheKey::AicpuTaskCacheKey(const AicpuTaskCacheKey& other)
    : opType(other.opType), dataType(other.dataType), reduceType(other.reduceType), isZeroCopy(other.isZeroCopy),
      inputSize(other.inputSize), opMode(other.opMode)
{
    CHK_PRT_CONT(opType == HcclCMDType::HCCL_CMD_INVALID,
        HCCL_ERROR("[AicpuTaskCacheKey][AicpuTaskCacheKey] opType is invalid"));
    
    // 暂时不考虑v类算子, dataType一定不是reserved
    CHK_PRT_CONT(dataType == HcclDataType::HCCL_DATA_TYPE_RESERVED,
        HCCL_ERROR("[AicpuTaskCacheKey][AicpuTaskCacheKey] dataType is reserved"));
}

HcclResult AicpuTaskCacheKey::Init(const HcclCMDType curOpType, const HcclDataType curDataType,
    const HcclReduceOp curReduceType, const bool curIsZeroCopy, const uint64_t curInputSize, const OpMode curOpMode)
{
    CHK_PRT_RET(curOpType == HcclCMDType::HCCL_CMD_INVALID,
        HCCL_ERROR("[AicpuTaskCacheKey][Init] opType is invalid"), HCCL_E_INTERNAL);
    
    // 暂时不考虑v类算子, dataType一定不是reserved
    CHK_PRT_RET(curDataType == HcclDataType::HCCL_DATA_TYPE_RESERVED,
        HCCL_ERROR("[AicpuTaskCacheKey][Init] dataType is reserved"), HCCL_E_INTERNAL);

    opType = curOpType;
    dataType = curDataType;
    reduceType = curReduceType;
    isZeroCopy = curIsZeroCopy;
    inputSize = curInputSize;
    opMode = curOpMode;

    return HCCL_SUCCESS;
}

std::string AicpuTaskCacheKey::GetKeyString() const
{
    std::ostringstream oss;
    oss << "opType" << static_cast<uint32_t>(opType) << "-dataType" << static_cast<uint32_t>(dataType)
        << "-reduceType" << static_cast<uint32_t>(reduceType) << "-isZeroCopy" << isZeroCopy << "-inputSize"
        << inputSize << "-opMode" << static_cast<uint32_t>(opMode);
    return oss.str();
}

bool AicpuTaskCacheKey::operator==(const AicpuTaskCacheKey& other) const
{
    return opType == other.opType && dataType == other.dataType && reduceType == other.reduceType &&
           isZeroCopy == other.isZeroCopy && inputSize == other.inputSize && opMode == other.opMode;
}

const AicpuTaskCacheKey& AicpuTaskCacheKey::operator=(const AicpuTaskCacheKey& other)
{
    if (this != &other) {
        this->opType = other.opType;
        this->dataType = other.dataType;
        this->reduceType = other.reduceType;
        this->isZeroCopy = other.isZeroCopy;
        this->inputSize = other.inputSize;
        this->opMode = other.opMode;
    }
    return *this;
}

} // namespace Hccl
