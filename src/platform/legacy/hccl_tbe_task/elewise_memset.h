/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_RUNTIME_V2_OP_IMPL_DYNAMIC_ATOMIC_ADDR_CLEAN_IMPL_H_
#define AIR_CXX_RUNTIME_V2_OP_IMPL_DYNAMIC_ATOMIC_ADDR_CLEAN_IMPL_H_
#include <cstdint>
#include <vector>
#include "op_tiling.h"

namespace TbeReduce {
const int64_t BYTE_FP32 = 4;
struct InputScalar {
    int32_t init_times_full_mask_full_repeat_time;
    int32_t ele_num_front_part;
    int32_t burst_len_last_part;
    int32_t repeat_time_last_part;
};

struct MemSetTilingData {
  // common input scalar
    int32_t select_key_input_scalar;
    int32_t need_core_num_input_scalar;
    int32_t ele_num_full_mask_full_repeat_time_input_scalar;
    int32_t burst_len_full_mask_full_repeat_time_input_scalar;

    // init input scalar
    // front core
    int32_t ele_num_front_core_input_scalar;
    InputScalar front_core_input_scalar;

    // last core
    int32_t ele_num_last_core_input_scalar;
    InputScalar last_core_input_scalar;
    int32_t core_num_input_scalar;
    int32_t gap_last_core_last_time;
};

struct MemSetCompileInfo {
    int workspace_num = 0;
    uint32_t core_num = 0;
    uint32_t ub_size = 0;
    int max_repeat_time = 0;
    bool is_dynamic = false;
    std::vector<int64_t> mask_nums;
    std::vector<int64_t> byte_list;
    std::vector<int64_t> _workspace_index_list;
};

bool MemSetTiling(const std::string& op_type, const std::vector<int64_t>& sizes, const MemSetCompileInfo& compile_info,
    OpRunInfo& run_info);

}  // namespace optiling
#endif  // AIR_CXX_RUNTIME_V2_OP_IMPL_DYNAMIC_ATOMIC_ADDR_CLEAN_IMPL_H_
