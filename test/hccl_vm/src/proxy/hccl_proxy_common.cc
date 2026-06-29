/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <iostream>
#include <map>
#include "dtype_common.h"
#include "hccl_types.h"
#include "sim_log.h"
#include "db_sim_runner_db.h"

#define PROXY_STUB_ERROR(format, ...) HCCL_VM_ERROR("[PROXY_STUB]" format, ##__VA_ARGS__)
#define PROXY_STUB_DEBUG(format, ...) HCCL_VM_DEBUG("[PROXY_STUB]" format, ##__VA_ARGS__)
#define PROXY_STUB_INFO(format, ...)  HCCL_VM_INFO("[PROXY_STUB]" format, ##__VA_ARGS__)
#define PROXY_STUB_WARN(format, ...)  HCCL_VM_WARN("[PROXY_STUB]" format, ##__VA_ARGS__)
#define PROXY_STUB_TRACE(format, ...) HCCL_VM_TRACE("[PROXY_STUB]" format, ##__VA_ARGS__)

namespace sim {
const std::map<HcclDataType, u32> DATA_TYPE_SIZE_MAP = {
    {HcclDataType::HCCL_DATA_TYPE_INT8, sizeof(s8)},
    {HcclDataType::HCCL_DATA_TYPE_INT16, sizeof(s16)},
    {HcclDataType::HCCL_DATA_TYPE_INT32, sizeof(s32)},
    {HcclDataType::HCCL_DATA_TYPE_FP16, 2},
    {HcclDataType::HCCL_DATA_TYPE_FP32, sizeof(float)},
    {HcclDataType::HCCL_DATA_TYPE_INT64, sizeof(s64)},
    {HcclDataType::HCCL_DATA_TYPE_UINT64, sizeof(u64)},
    {HcclDataType::HCCL_DATA_TYPE_UINT8, sizeof(u8)},
    {HcclDataType::HCCL_DATA_TYPE_UINT16, sizeof(u16)},
    {HcclDataType::HCCL_DATA_TYPE_UINT32, sizeof(u32)},
    {HcclDataType::HCCL_DATA_TYPE_FP64, sizeof(double)}, 
    {HcclDataType::HCCL_DATA_TYPE_BFP16, 2},
    {HcclDataType::HCCL_DATA_TYPE_INT128, 16},
    {HcclDataType::HCCL_DATA_TYPE_HIF8, 1},
    {HcclDataType::HCCL_DATA_TYPE_FP8E4M3, 1},
    {HcclDataType::HCCL_DATA_TYPE_FP8E5M2, 1},
    {HcclDataType::HCCL_DATA_TYPE_FP8E8M0, 1}
};

int GetDataTypeSize(HcclDataType dataType, uint32_t &size)
{
    auto iter = DATA_TYPE_SIZE_MAP.find(dataType);
    if (iter == DATA_TYPE_SIZE_MAP.end()) {
        PROXY_STUB_ERROR("not support data type: {}", static_cast<uint32_t>(dataType));
        return 1;
    }
    size = iter->second;
    return 0;
}

bool IsDeviceAddress(void *addr)
{
    uint64_t devPtr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(addr));
    auto virMemRes = RunnerDB::GetOneByPred<sim::VirtualMemBlock>(
        [devPtr](const sim::VirtualMemBlock &virMem)
        { return ((virMem.start_ptr <= devPtr) &&
                    (devPtr < (virMem.start_ptr + virMem.size)) &&
                    (virMem.src_type == (uint8_t)sim::VIR_MEM_TYPE_DEV)); });
    if (!virMemRes.second) {
        PROXY_STUB_INFO("this buff offset ptr not found:{:p} in VirtualMemBlock.", addr);
        return false;
    }

    return true;
}
}
