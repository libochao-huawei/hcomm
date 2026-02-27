/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sstream>

#include "async_unfold_key.h"

#include "log.h"

namespace hccl {
    AsyncUnfoldKey::AsyncUnfoldKey()
        : opBaseOpIdx(0), opType(HcclCMDType::HCCL_CMD_INVALID), dataType(HcclDataType::HCCL_DATA_TYPE_RESERVED) {}

    AsyncUnfoldKey::AsyncUnfoldKey(const AsyncUnfoldKey& other)
        : opBaseOpIdx(other.opBaseOpIdx), opType(other.opType), dataType(other.dataType) {}

    HcclResult AsyncUnfoldKey::Init(const uint64_t curOpBaseOpIdx, const HcclCMDType curOpType, const HcclDataType curDataType)
    {
        opBaseOpIdx = curOpBaseOpIdx;
        opType = curOpType;
        dataType = curDataType;
        return HCCL_SUCCESS;
    }

    HcclResult AsyncUnfoldKey::Verify(const AsyncUnfoldKey& other) const
    {
        if (opBaseOpIdx == other.opBaseOpIdx) {
            CHK_PRT_RET((opType != other.opType) || (dataType != other.dataType),
                HCCL_ERROR("[AsyncUnfoldKey][Verify] two keys with same opBaseOpIdx[%llu] have inconsistent fields: "
                    "opType[%u] other.opType[%u]; dataType[%u] other.dataType[%u]",
                    opBaseOpIdx, opType, other.opType, dataType, other.dataType),
                HCCL_E_INTERNAL);
        }
        return HCCL_SUCCESS;
    }

    std::string AsyncUnfoldKey::GetKeyString() const {
        std::ostringstream oss;
        oss << "opBaseOpIdx" << opBaseOpIdx
            << "-opType" << static_cast<uint32_t>(opType)
            << "-dataType" << static_cast<uint32_t>(dataType);
        return oss.str();
    }

    bool AsyncUnfoldKey::operator==(const AsyncUnfoldKey& other) const {
        return opBaseOpIdx == other.opBaseOpIdx;
    }

    const AsyncUnfoldKey& AsyncUnfoldKey::operator=(const AsyncUnfoldKey& other) {
        if (this != &other) {
            opBaseOpIdx = other.opBaseOpIdx;
            opType = other.opType;
            dataType = other.dataType;
        }
        return *this;
    }

}; // namespace hccl