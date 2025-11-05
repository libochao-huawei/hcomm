/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef OPS_BUILT_IN_OP_TILING_SEGMENT_SCHEDULE_H_
#define OPS_BUILT_IN_OP_TILING_SEGMENT_SCHEDULE_H_

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "exe_graph/runtime/kernel_run_context.h"
#include "exe_graph/runtime/tiling_context.h"
#include "vector_tiling_handle.h"
#include "vector_tiling_rt2.h"
#include "op_tiling.h"
#include "fusion.h"
#include "auto_tiling_context.h"
#include "vector_tiling.h"

namespace TbeReduce {
constexpr std::size_t VAR_INIT_DIM_LEN = 2;
constexpr std::size_t ID_INIT_DIM_LEN = 1;
constexpr std::size_t OUTPUT_INIT_DIM_LEN = 2;
constexpr std::size_t ORI_DIM_LEN = 11;
struct SegmentDslCompileInfo : AutoTilingCompileInfo {
    // construct func
    SegmentDslCompileInfo() = default;
    SegmentDslCompileInfo(const std::string &_op_type, const nlohmann::json &compile_info);
    ~SegmentDslCompileInfo() override = default;
    bool Parse(const char *op_type, const nlohmann::json &compile_info);

    // base info
    int64_t core_num{ 0 };
    int64_t ub_size{ 1 };
    int64_t segment_type{ 0 };
    int64_t var_dtype_size{ 4 };
    int64_t id_dtype_size{ 4 };
    int64_t num_per_block{ 32 };

    // custom info
    int64_t is_support_atomic{ 1 };
    int64_t impl_mode{ 0 };
    int64_t num_segments{ 0 };
    bool is_spe_soc{ false };
    // tensor size
    std::unordered_map<std::string, std::vector<int64_t>> tensor_sizes;
    // segment vars
    std::unordered_map<std::string, std::vector<int64_t>> segment_vars;

    bool is_static{ false };
    int64_t const_axis{ 0 };
    bool is_valid{ false };
};

template <typename T> class SegmentDsl {
public:
    explicit SegmentDsl(T *_context, const OpInfoImpl *_op_info) : context(_context), op_info(_op_info) {}
    ~SegmentDsl() = default;
    bool DoTiling();

private:
    bool Init();
    bool IsAtomicTiling();
    bool DoAtomicTiling();
    bool DoNoAtomicTiling();

    void EnsureBlockUBTiling();
    void SafeTiling();
    void CalCacheNum(int64_t &factor);
    void CalUbReduceFactor();

    bool CalcKey();
    bool WriteTilingData();
    void refine_axis(int64_t &refine_axis, const int64_t ori_axis);
    bool is_support_pad(const std::string sch);
    bool is_support_sch();
    bool is_support_all_out();
    bool check_pad(const std::string sch);
    bool is_support_no_atomic_sch();

    T *context;
    const OpInfoImpl *op_info{ nullptr };
    const char *op_type;
    const SegmentDslCompileInfo *segment_compile_info;

    std::array<int64_t, ORI_DIM_LEN> ori_var_shape{};
    std::array<int64_t, ORI_DIM_LEN> ori_id_shape{};

    std::vector<int64_t> var_shape{ std::vector<int64_t>(VAR_INIT_DIM_LEN, 0) };
    std::vector<int64_t> id_shape{ std::vector<int64_t>(ID_INIT_DIM_LEN, 0) };
    std::vector<int64_t> output_shape{ std::vector<int64_t>(VAR_INIT_DIM_LEN, 0) };

    int64_t segment_cache_num{ 0 };
    int64_t segment_cache_start{ 0 };
    int64_t segment_last_dim{ 0 };
    int64_t pad_threshold{ 1 };
    int64_t remove_pad_threshold{ 1 };
    int64_t input_nums{ 3 };
    int64_t core_num{ 0 };
    int64_t one_repeat{ 64 };
    bool is_zero{ false };
    bool is_set_mask{ false };

    int64_t segment_rows{ 1 };
    int64_t segment_rows_align{ 1 };
    int64_t var_align_factor{ 0 };
    int64_t segment_rows_align_core_x{ 1 };
    int64_t num_segment{ 0 };
    bool is_align{ true };
    std::string cur_sch{ "1" };
    ge::DataType dtype{ ge::DataType::DT_FLOAT16 };

    int64_t var_size_total{ 1 };
    int64_t id_size_total{ 1 };
    int64_t out_size_total{ 1 };
    int64_t out_size_total_align{ 1 };
    int64_t key{ -1 };
    int64_t key_special_pattern{ 0 };
    int64_t block_dims{ 1 };

    int64_t block_axis{ 0 };
    int64_t ub_norm_axis{ 0 };
    int64_t ub_reduce_axis{ 0 };
    int64_t block_factor{ 1 };
    int64_t ub_norm_factor{ 1 };
    int64_t ub_reduce_factor{ 1 };

    int64_t var_num_ub{ 0 };
    int64_t id_num_ub{ 0 };
    int64_t cache_num_ub{ 0 };
};
template class SegmentDsl<AutoTilingContext>;
} // namespace TbeReduce
#endif // OPS_BUILT_IN_OP_TILING_SEGMENT_SCHEDULE_H_
