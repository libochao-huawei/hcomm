/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "elewise_memset.h"
#include <vector>
#include <inttypes.h>
#include <nlohmann/json.hpp>
#include "op_tiling.h"

namespace TbeReduce {
    constexpr uint32_t BYTE_BLOCK = 32;
    constexpr uint32_t MAX_MASK_BYTE = 256;
    constexpr uint32_t MAX_CORE_NUM = 512;
    constexpr uint32_t INT32_BYTE = 4;
    constexpr uint32_t INT64_BYTE = 8;
    constexpr int MAX_REPEAT_TIME = 255;
    constexpr uint32_t FP32_MASK_NUM = 64;
    constexpr uint32_t FP32_TYPE_BYTE = 4;

template<typename T>
T CeilDiv(const T n1, const T n2)
{
    if (n1 == 0) {
        return 0;
    }
    return (n2 != 0) ? (((n1 - 1) / n2) + 1) : n1;
}

void WriteTilingParams(const MemSetTilingData& params, OpRunInfo& run_info)
{
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.select_key_input_scalar));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.need_core_num_input_scalar));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.ele_num_full_mask_full_repeat_time_input_scalar));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.burst_len_full_mask_full_repeat_time_input_scalar));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.ele_num_front_core_input_scalar));
    ByteBufferPut(run_info.tilingData,\
        static_cast<int32_t>(params.front_core_input_scalar.init_times_full_mask_full_repeat_time));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.front_core_input_scalar.ele_num_front_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.front_core_input_scalar.burst_len_last_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.front_core_input_scalar.repeat_time_last_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.ele_num_last_core_input_scalar));
    ByteBufferPut(run_info.tilingData,\
        static_cast<int32_t>(params.last_core_input_scalar.init_times_full_mask_full_repeat_time));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.last_core_input_scalar.ele_num_front_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.last_core_input_scalar.burst_len_last_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.last_core_input_scalar.repeat_time_last_part));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.core_num_input_scalar));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(params.gap_last_core_last_time));
}

void PrintMemSetParams(const MemSetTilingData& params)
{
    HCCL_DEBUG("memset params.select_key_input_scalar=%d", params.select_key_input_scalar);
    HCCL_DEBUG("memset params.need_core_num_input_scalar=%d", params.need_core_num_input_scalar);
    HCCL_DEBUG("memset params.ele_num_full_mask_full_repeat_time_input_scalar=%d",
        params.ele_num_full_mask_full_repeat_time_input_scalar);
    HCCL_DEBUG("memset params.burst_len_full_mask_full_repeat_time_input_scalar=%d",
        params.burst_len_full_mask_full_repeat_time_input_scalar);
    HCCL_DEBUG("memset params.ele_num_front_core_input_scalar=%d", params.ele_num_front_core_input_scalar);
    HCCL_DEBUG("memset params.front_core_input_scalar.init_times_full_mask_full_repeat_time=%d",
        params.front_core_input_scalar.init_times_full_mask_full_repeat_time);
    HCCL_DEBUG("memset params.front_core_input_scalar.ele_num_front_part=%d",
        params.front_core_input_scalar.ele_num_front_part);
    HCCL_DEBUG("memset params.front_core_input_scalar.burst_len_last_part=%d",
        params.front_core_input_scalar.burst_len_last_part);
    HCCL_DEBUG("memset params.front_core_input_scalar.repeat_time_last_part=%d",
        params.front_core_input_scalar.repeat_time_last_part);
    HCCL_DEBUG("memset params.ele_num_last_core_input_scalar=%d", params.ele_num_last_core_input_scalar);
    HCCL_DEBUG("memset params.last_core_input_scalar.init_times_full_mask_full_repeat_time=%d",
        params.last_core_input_scalar.init_times_full_mask_full_repeat_time);
    HCCL_DEBUG("memset params.last_core_input_scalar.ele_num_front_part=%d",
        params.last_core_input_scalar.ele_num_front_part);
    HCCL_DEBUG("memset params.last_core_input_scalar.burst_len_last_part=%d",
        params.last_core_input_scalar.burst_len_last_part);
    HCCL_DEBUG("memset params.last_core_input_scalar.repeat_time_last_part=%d",
        params.last_core_input_scalar.repeat_time_last_part);
    HCCL_DEBUG("memset params.core_num_input_scalar=%d", params.core_num_input_scalar);
    HCCL_DEBUG("memset params.gap_last_core_last_time=%d", params.gap_last_core_last_time);
}

static void ComputeParamsOneCore(const int32_t& ele_num_one_core, const int32_t& tensor_type_mask_num,
    const int32_t& type_byte, const int32_t& ele_num_full_mask_full_repeat_time_input_scalar,
    InputScalar& input_scalar)
{
    int32_t scaler = ele_num_full_mask_full_repeat_time_input_scalar;
    input_scalar.init_times_full_mask_full_repeat_time = ele_num_one_core / scaler;
    input_scalar.ele_num_front_part = input_scalar.init_times_full_mask_full_repeat_time * scaler;
    uint32_t ele_num_last_part = ele_num_one_core - input_scalar.ele_num_front_part;
    input_scalar.burst_len_last_part = CeilDiv(ele_num_last_part * type_byte, BYTE_BLOCK);
    if (ele_num_last_part % tensor_type_mask_num == 0) {
        input_scalar.repeat_time_last_part = ele_num_last_part / tensor_type_mask_num;
    } else {
        input_scalar.repeat_time_last_part = ele_num_last_part / tensor_type_mask_num + 1;
    }
}

void WriteAtomicTilingData(MemSetTilingData* params,
    int64_t tensor_size, uint32_t core_num, int64_t tensor_type_mask_num,
    int64_t type_byte, int64_t idx, int64_t max_repeat_time, int64_t size_num)
{
    uint32_t ele_num = tensor_size / type_byte;
    params->select_key_input_scalar = size_num;
    // is using all core
    if (tensor_size > core_num * MAX_MASK_BYTE) {
        params->need_core_num_input_scalar = core_num;
    } else {
        params->need_core_num_input_scalar = 1;
    }
    // compute tiling params
    params->ele_num_full_mask_full_repeat_time_input_scalar = tensor_type_mask_num * max_repeat_time;
    params->burst_len_full_mask_full_repeat_time_input_scalar =
        params->ele_num_full_mask_full_repeat_time_input_scalar * type_byte / BYTE_BLOCK;
    if (params->need_core_num_input_scalar == 1) {
      // use one core
        params->ele_num_front_core_input_scalar = ele_num;
        ComputeParamsOneCore(params->ele_num_front_core_input_scalar, tensor_type_mask_num, type_byte,
            params->ele_num_full_mask_full_repeat_time_input_scalar, params->front_core_input_scalar);

        params->ele_num_last_core_input_scalar = params->ele_num_front_core_input_scalar;
        ComputeParamsOneCore(params->ele_num_last_core_input_scalar, tensor_type_mask_num, type_byte,
            params->ele_num_full_mask_full_repeat_time_input_scalar, params->last_core_input_scalar);
    } else if (params->need_core_num_input_scalar > 1) {
        // use all core
        // front core
        params->ele_num_front_core_input_scalar = tensor_size / params->need_core_num_input_scalar;
        params->ele_num_front_core_input_scalar =
            CeilDiv(params->ele_num_front_core_input_scalar, static_cast<int32_t>(BYTE_BLOCK)) * BYTE_BLOCK;
        // the bytes per core will be BYTE_BLOCK aligned
        if (type_byte != 0) {
            params->ele_num_front_core_input_scalar = params->ele_num_front_core_input_scalar / type_byte;
        }
        params->need_core_num_input_scalar =
            (static_cast<int64_t>(ele_num) + params->ele_num_front_core_input_scalar - 1) /
            params->ele_num_front_core_input_scalar;
        ComputeParamsOneCore(params->ele_num_front_core_input_scalar, tensor_type_mask_num, type_byte,
            params->ele_num_full_mask_full_repeat_time_input_scalar, params->front_core_input_scalar);
        // last core
        params->ele_num_last_core_input_scalar =
            ele_num - params->ele_num_front_core_input_scalar * (params->need_core_num_input_scalar - 1);
        ComputeParamsOneCore(params->ele_num_last_core_input_scalar, tensor_type_mask_num, type_byte,
            params->ele_num_full_mask_full_repeat_time_input_scalar, params->last_core_input_scalar);
    }
    params->core_num_input_scalar = core_num;
}

void InitTilingParams(MemSetTilingData& params)
{
    params.select_key_input_scalar = 0;
    params.need_core_num_input_scalar = 0;
    params.ele_num_full_mask_full_repeat_time_input_scalar = 0;
    params.burst_len_full_mask_full_repeat_time_input_scalar = 0;

    // init input scalar
    // front core
    params.ele_num_front_core_input_scalar = 0;
    params.front_core_input_scalar.init_times_full_mask_full_repeat_time = 0;
    params.front_core_input_scalar.ele_num_front_part = 0;
    params.front_core_input_scalar.burst_len_last_part = 0;
    params.front_core_input_scalar.repeat_time_last_part = 0;
    // last core
    params.ele_num_last_core_input_scalar = 0;
    params.last_core_input_scalar.init_times_full_mask_full_repeat_time = 0;
    params.last_core_input_scalar.ele_num_front_part = 0;
    params.last_core_input_scalar.burst_len_last_part = 0;
    params.last_core_input_scalar.repeat_time_last_part = 0;
    params.core_num_input_scalar = 0;
    params.gap_last_core_last_time = 0;
}

bool CheckSize(const int64_t& size)
{
    if (size < 0) {
        HCCL_ERROR("crack size < 0");
        return false;
    }
    return true;
}

bool MemSetTiling(const std::string& op_type, const std::vector<int64_t>& sizes,
    const MemSetCompileInfo& compile_info, OpRunInfo& run_info)
{
    HCCL_INFO("op[%s] op tiling begin.", op_type.c_str());

    const std::vector<int64_t> workspace_size_list = sizes;
    uint32_t core_num = compile_info.core_num;

    auto size_count = workspace_size_list.size();
    auto max_repeat_time = compile_info.max_repeat_time;
    uint32_t aligned_tiling_data_size = (sizeof(MemSetTilingData) * INT32_BYTE + BYTE_BLOCK - 1) / BYTE_BLOCK\
      * BYTE_BLOCK;
    uint32_t aligned_addr_ub_list_size = (size_count * INT64_BYTE + BYTE_BLOCK - 1) / BYTE_BLOCK * BYTE_BLOCK;
    uint32_t ub_size_remain = compile_info.ub_size - aligned_tiling_data_size * 2 - aligned_addr_ub_list_size;
    max_repeat_time = ub_size_remain / MAX_MASK_BYTE;
    if (max_repeat_time > MAX_REPEAT_TIME) {
        max_repeat_time = MAX_REPEAT_TIME;
    }

    for (size_t i = 0; i < size_count; i++) {
        int64_t addr_tensor_size = workspace_size_list[i];
        int64_t aligned_addr_tensor_size = (addr_tensor_size + BYTE_BLOCK - 1) / BYTE_BLOCK * BYTE_BLOCK;
        bool flag = CheckSize(addr_tensor_size);
        if (!flag) {
            return false;
        }
        HCCL_INFO("op_type:%s, op: addr_tensor_size=%d", op_type.c_str(), addr_tensor_size);

        MemSetTilingData params;
        // init tiling params
        InitTilingParams(params);
        auto tensor_type_mask_num = 64;
        auto type_byte = 4;
        params.gap_last_core_last_time = aligned_addr_tensor_size - addr_tensor_size;
        WriteAtomicTilingData(&params, aligned_addr_tensor_size, core_num, tensor_type_mask_num, type_byte, i,
            max_repeat_time, size_count);
        // write tiling params to run_info
        WriteTilingParams(params, run_info);

        // block_dim, core num used in tik op
        run_info.blockDim = static_cast<uint32_t>(params.need_core_num_input_scalar);

        PrintMemSetParams(params);
    }
    HCCL_INFO("op[%s] op tiling success", op_type.c_str());
    return true;
}
}