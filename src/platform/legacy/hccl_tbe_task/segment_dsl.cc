/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "segment_dsl.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include "auto_tiling_register.h"
#include "vector_tiling_handle.h"
#include "auto_tiling_rt2.h"
#include "auto_tiling_register.h"
#include "auto_tiling_context.h"

namespace TbeReduce {
namespace {
constexpr int64_t BLOCK_SIZE_BYTES = 32;
constexpr int64_t HIGH_PERFORMANCE = 1;
constexpr int64_t ONE = 1;
constexpr int64_t TWO = 2;
constexpr int64_t THREE = 3;
constexpr int64_t FOUR = 4;
constexpr size_t BASE_INFO_SIZE = 5;
constexpr size_t CUSTOM_INFO_SIZE = 4;
constexpr int64_t ZERO = 0;

// SCH ENUM TYPE
const std::string ATOMIC_NO_PAD_NO_CACHE_SCH = "10";
const std::string ATOMIC_NO_PAD_CACHE_SCH = "11";
const std::string ATOMIC_PAD_NO_CACHE_SCH = "20";
const std::string ATOMIC_PAD_CACHE_SCH = "21";
const std::string NO_ATOMIC_NO_PAD_NO_CACHE_SCH = "30";
const std::string NO_ATOMIC_NO_PAD_CACHE_SCH = "31";
const std::string NO_ATOMIC_PAD_NO_CACHE_SCH = "40";
const std::string NO_ATOMIC_PAD_CACHE_SCH = "41";
const std::string NO_ATOMIC_CORE_X_SCH = "50";
const std::string ATOMIC_SPECIAL_SCH = "61";
const std::string NO_PAD_ALL_OUT_SCH = "71";
const std::string PAD_ALL_OUT_SCH = "81";

// KEY && TILING_KEY
constexpr int64_t ATOMIC_NO_PAD_NO_CACHE_KEY = 1000;
constexpr int64_t ATOMIC_NO_PAD_CACHE_KEY = 1100;
constexpr int64_t ATOMIC_PAD_NO_CACHE_KEY = 2000;
constexpr int64_t ATOMIC_PAD_CACHE_KEY = 2100;
constexpr int64_t NO_ATOMIC_NO_PAD_NO_CACHE_KEY = 3000;
constexpr int64_t NO_ATOMIC_NO_PAD_CACHE_KEY = 3100;
constexpr int64_t NO_ATOMIC_PAD_NO_CACHE_KEY = 4000;
constexpr int64_t NO_ATOMIC_PAD_CACHE_KEY = 4100;
constexpr int64_t NO_ATOMIC_CORE_X_KEY = 5000;
constexpr int64_t ATOMIC_SPECIAL_KEY = 6100;
constexpr int64_t ATOMIC_NO_PAD_ALL_OUT_KEY = 7110;
constexpr int64_t ATOMIC_PAD_ALL_OUT_KEY = 8110;
constexpr int64_t NO_ATOMIC_NO_PAD_ALL_OUT_KEY = 7100;
constexpr int64_t NO_ATOMIC_PAD_ALL_OUT_KEY = 8100;
constexpr int64_t SET_MASK_KEY = 10000;

constexpr int64_t BASE_KEY = 770000000;

constexpr size_t KEY_SIZE = 10;
constexpr int64_t DECIMAL_TEN = 10;
constexpr int64_t DUMMY_DIM = -10;
constexpr int64_t NUM_SEGMENT_IDX = 20000;
constexpr int64_t BLOCK_IDX = 30000;
constexpr int64_t UB_NORM_IDX = 40000;
constexpr int64_t UB_REDUCE_IDX = 50000;
constexpr int64_t CACHE_NUM_IDX = 60000;
constexpr int64_t CACHE_START_IDX = 70000;
constexpr int64_t OUTPUT_VAR_IDX = 80000;
constexpr int64_t ALL_PATCH_LARGE = 1000;

const std::map<ge::DataType, int64_t> REMOVE_PAD_DTYPE{ { ge::DataType::DT_FLOAT16, 160 },
    { ge::DataType::DT_FLOAT, 168 },
    { ge::DataType::DT_INT32, 168 } };
const std::map<ge::DataType, int64_t> PAD_DTYPE{ { ge::DataType::DT_FLOAT16, 256 },
    { ge::DataType::DT_FLOAT, 128 },
    { ge::DataType::DT_INT32, 128 } };
const std::map<ge::DataType, int64_t> ONE_REPEAT_DTYPE{ { ge::DataType::DT_FLOAT16, 128 },
    { ge::DataType::DT_FLOAT, 64 },
    { ge::DataType::DT_INT32, 64 } };
} // namespace

template <typename T> static T CeilDiv(const T &dividend, const T &divisor)
{
    if (divisor == 0) {
        HCCL_ERROR("divisor can not be zero!");
        return false;
    }
    T result = (dividend + divisor - 1) / divisor;
    return result;
}

SegmentDslCompileInfo::SegmentDslCompileInfo(const std::string &op_type, const nlohmann::json &ori_compile_info)
{
    Parse(op_type.c_str(), ori_compile_info);
}

bool SegmentDslCompileInfo::Parse(const char *op_type, const nlohmann::json &ori_compile_info)
{
    HCCL_DEBUG("unsorted_segment_sum: begin parser");
    is_valid = true;
    try {
        // parse base info
        const auto &base_info = ori_compile_info.at("_base_info");
        constexpr size_t base_info_size = BASE_INFO_SIZE;
        constexpr int64_t block_size = BLOCK_SIZE_BYTES;

        if (base_info.size() != base_info_size) {
            HCCL_ERROR("base info must be 5 element");
            return false;
        }

        core_num = base_info[0];
        ub_size = base_info[1];

        segment_type = base_info[TWO];
        var_dtype_size = base_info[THREE];
        id_dtype_size = base_info[FOUR];
        num_per_block = block_size / var_dtype_size;

        // parse custom info
        const auto &custom_info = ori_compile_info.at("_custom_info");
        constexpr size_t custom_info_size = CUSTOM_INFO_SIZE;

        if (custom_info.size() != custom_info_size) {
            HCCL_ERROR("custom info must be 4 element");
            return false;
        }

        is_support_atomic = custom_info[0];
        impl_mode = custom_info[1];
        num_segments = custom_info[TWO];
        is_spe_soc = custom_info[THREE];
        HCCL_DEBUG("SegmentDslCompileInfo:%lld, %lld, %lld, %lld, %lld, %lld", segment_type, is_support_atomic,
            impl_mode, num_segments, core_num, ub_size);

        tensor_sizes =
            ori_compile_info.at("_tensor_sizes").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        is_static = ori_compile_info.count("_const_axis") > 0;
        if (is_static) {
            HCCL_DEBUG("is static");
            const_axis = ori_compile_info.at("_const_axis").get<int64_t>();
        } else {
            HCCL_DEBUG("is dynamic");
            segment_vars =
                ori_compile_info.at("_segment_vars").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        }
    } catch (const std::exception &e) {
        is_valid = false;
        HCCL_ERROR("construct compile_info error. Error message: %s", e.what());
        return false;
    }
    HCCL_DEBUG("unsorted_segment_sum: begin parser end");
    return true;
}

template <typename T> bool SegmentDsl<T>::Init()
{
    segment_compile_info = dynamic_cast<const SegmentDslCompileInfo *>(context->GetCompileInfo());
    if (segment_compile_info == nullptr) {
        HCCL_ERROR("segment_compile_info is nullptr.");
    }
    size_t input_idx = 0;
    const OpShape &ori_var_ge_shape = context->GetInputShape(0);
    context->GetInputDataType(input_idx, dtype);
    size_t cur_var_dim_num = ori_var_ge_shape.GetDimNum();
    if (cur_var_dim_num == 0) {
        HCCL_DEBUG("cur_var_dim_num is 0");
        is_zero = true;
        return true;
    }

    var_size_total = 1;
    for (size_t j = 0; j < cur_var_dim_num; j++) {
        HCCL_DEBUG("i is %d,input_x is %d", j, ori_var_ge_shape.GetDim(j));
        if (j < ori_var_shape.size()) {
            ori_var_shape[j] = ori_var_ge_shape.GetDim(j);
        }
        var_size_total = ori_var_ge_shape.GetDim(j) * var_size_total;
    }

    const OpShape &ori_id_ge_shape = context->GetInputShape(1);
    size_t id_dim_num = ori_id_ge_shape.GetDimNum();
    if (id_dim_num == 0) {
        is_zero = true;
        HCCL_DEBUG("cur_var_dim_num is 0");
        return true;
    }

    id_size_total = 1;
    for (size_t j = 0; j < id_dim_num; j++) {
        HCCL_DEBUG("j is %zu, ori_id_ge_shape is %ld", j, ori_id_ge_shape.GetDim(j));
        ori_id_shape[j] = ori_id_ge_shape.GetDim(j);
        id_size_total = id_size_total * ori_id_ge_shape.GetDim(j);
    }
    HCCL_DEBUG("var_size_total is %d,id_size_total is %d", var_size_total, id_size_total);

    if (id_size_total > var_size_total) {
        HCCL_ERROR("id shape is more than var shape");
        return false;
    }

    for (size_t i = 0; i < id_dim_num; i++) {
        if (ori_var_ge_shape.GetDim(i) != ori_id_ge_shape.GetDim(i)) {
            HCCL_ERROR(" shape is error, i is %zu, ori_id_ge_shape is %ld, ori_var_ge_shape is %ld", i,
                ori_id_ge_shape.GetDim(i), ori_var_ge_shape.GetDim(i));
            return false;
        }
    }
    if (segment_compile_info->soc_info.core_num > 0) {
        core_num = segment_compile_info->soc_info.core_num;
    } else {
        core_num = segment_compile_info->core_num;
    }
    if (core_num < 1) {
        HCCL_ERROR("core num is invalid.");
    }
    if (id_size_total == 0 || var_size_total == 0) {
        is_zero = true;
        segment_rows = 1;
        HCCL_DEBUG("ids or x shape is empty");
    } else {
        segment_rows = var_size_total / id_size_total;
    }
    HCCL_DEBUG("segment_rows is %d,", segment_rows);
    var_shape[0] = id_size_total;
    var_shape[1] = segment_rows;
    id_shape[0] = id_size_total;

    const OpShape &ori_y_ge_shape = context->GetOutputShape(0);
    num_segment = ori_y_ge_shape.GetDim(0);

    HCCL_DEBUG("num_segments=%d", num_segment);
    output_shape[0] = num_segment;
    output_shape[1] = segment_rows;
    out_size_total = num_segment * segment_rows;
    var_align_factor = BLOCK_SIZE_BYTES / segment_compile_info->var_dtype_size;
    segment_rows_align = (segment_rows + var_align_factor - 1) / var_align_factor * var_align_factor;
    out_size_total_align = num_segment * segment_rows_align;
    is_align = segment_rows % var_align_factor == 0;
    if (ONE_REPEAT_DTYPE.find(dtype) != ONE_REPEAT_DTYPE.end()) {
        one_repeat = ONE_REPEAT_DTYPE.at(dtype);
    }
    is_set_mask = (!segment_compile_info->is_spe_soc) && (segment_rows <= one_repeat || segment_rows % one_repeat == 0);
    HCCL_DEBUG("is_align is:%d, is_set_mask %d ", is_align, is_set_mask);
    return true;
}

template <typename T> void SegmentDsl<T>::CalCacheNum(int64_t &factor)
{
    segment_cache_num = cache_num_ub / segment_rows_align;
    if (segment_rows < segment_compile_info->num_per_block) {
        HCCL_DEBUG("segment_compile_info->num_per_block IS %d,pad_threshold is %d", segment_compile_info->num_per_block,
            pad_threshold);
        segment_cache_num = ((cache_num_ub / segment_rows_align) / pad_threshold) * pad_threshold;
    }
    if (segment_cache_num > factor) {
        HCCL_DEBUG("segment_cache_num >num_segment");
        segment_cache_num = factor;
    }
    HCCL_DEBUG("segment_cache_num %d", segment_cache_num);
}

template <typename T> bool SegmentDsl<T>::DoAtomicTiling()
{
    HCCL_DEBUG("unsorted_segment_sum: DoAtomicTiling");
    id_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[1];
    var_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[0];
    cache_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[TWO];
    HCCL_DEBUG("atomic id_num_ub is %d,var_num_ub is %d, cache_num_ub is %d", id_num_ub, var_num_ub, cache_num_ub);
    if (segment_rows_align > var_num_ub) {
        ub_norm_axis = 1;
        ub_reduce_axis = 0;
        ub_norm_factor = var_num_ub;
        ub_reduce_factor = 1;
    } else {
        ub_norm_axis = DUMMY_DIM;
        ub_reduce_axis = 0;
        ub_norm_factor = DUMMY_DIM;
        ub_reduce_factor =
            block_factor * segment_rows_align > var_num_ub ? var_num_ub / segment_rows_align : block_factor;
    }
    segment_cache_num = DUMMY_DIM;
    if (cur_sch == ATOMIC_NO_PAD_NO_CACHE_SCH) {
        HCCL_DEBUG("key is ATOMIC_NO_PAD_NO_CACHE_KEY");
        key_special_pattern = ATOMIC_NO_PAD_NO_CACHE_KEY;
    } else if (cur_sch == ATOMIC_PAD_NO_CACHE_SCH) {
        key_special_pattern = ATOMIC_PAD_NO_CACHE_KEY;
        HCCL_DEBUG("&&ATOMIC_PAD_NO_CACHE_KEY");
    } else {
        CalCacheNum(num_segment);
        if (cur_sch == ATOMIC_NO_PAD_CACHE_SCH) {
            HCCL_DEBUG("key is ATOMIC_NO_PAD_CACHE_KEY");
            key_special_pattern = ATOMIC_NO_PAD_CACHE_KEY + is_set_mask * SET_MASK_KEY;
        } else if (cur_sch == ATOMIC_PAD_CACHE_SCH) {
            HCCL_DEBUG("ATOMIC_PAD_CACHE_KEY");
            key_special_pattern = ATOMIC_PAD_CACHE_KEY + is_set_mask * SET_MASK_KEY;
        } else if (cur_sch == ATOMIC_SPECIAL_SCH) {
            HCCL_DEBUG("Special key");
            key_special_pattern = ATOMIC_SPECIAL_KEY + is_set_mask * SET_MASK_KEY;
        } else if (cur_sch == PAD_ALL_OUT_SCH) {
            HCCL_DEBUG("ATOMIC_PAD_ALL_OUT_KEY");
            key_special_pattern = ATOMIC_PAD_ALL_OUT_KEY + is_set_mask * SET_MASK_KEY;
        } else {
            HCCL_DEBUG("ATOMIC_NO_PAD_ALL_OUT_KEY");
            key_special_pattern = ATOMIC_NO_PAD_ALL_OUT_KEY + is_set_mask * SET_MASK_KEY;
        }
    }
    HCCL_DEBUG("atomic success");
    HCCL_DEBUG("unsorted_segment_sum: DoAtomicTiling end");
    return true;
}

template <typename T> void SegmentDsl<T>::CalUbReduceFactor()
{
    if (cur_sch != NO_ATOMIC_CORE_X_SCH) {
        ub_reduce_factor =
            var_num_ub / segment_rows_align > id_shape[0] ? id_shape[0] : var_num_ub / segment_rows_align;
        HCCL_DEBUG("cur_sch not NO_ATOMIC_CORE_X_SCH, ub_reducefactor is %d", ub_reduce_factor);
    } else {
        ub_reduce_factor =
            var_num_ub / segment_rows_align_core_x > id_shape[0] ? id_shape[0] : var_num_ub / segment_rows_align_core_x;
        HCCL_DEBUG("cur_sch is NO_ATOMIC_CORE_X_SCH, ub_reduce_factor is %d", ub_reduce_factor);
    }
}

template <typename T> bool SegmentDsl<T>::DoNoAtomicTiling()
{
    HCCL_DEBUG("unsorted_segment_sum: DoNoAtomicTiling");
    HCCL_DEBUG("DoNoAtomicTiling is running");
    block_axis = 1;
    block_factor = CeilDiv(num_segment, segment_compile_info->core_num);

    block_factor = block_factor > 0 ? block_factor : 1;
    block_dims = (num_segment + block_factor - 1) / block_factor;

    if (!is_support_no_atomic_sch()) {
        HCCL_ERROR("no atomic is_support_no_atomic_sch is false");
        return false;
    }
    if (cur_sch == NO_ATOMIC_CORE_X_SCH) {
        HCCL_DEBUG("begin NO_ATOMIC_CORE_X_SCH");
        block_factor = CeilDiv(var_shape[1], segment_compile_info->core_num);
        block_factor = block_factor > 0 ? block_factor : 1;
        block_dims = (var_shape[1] + block_factor - 1) / block_factor;
    }
    segment_rows_align_core_x = (block_factor + var_align_factor - 1) / var_align_factor * var_align_factor;
    HCCL_DEBUG("DoNoAtomicTiling,, block_factor:%lld, "
        "block_dims:%lld",
        block_factor, block_dims);
    id_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[1];
    var_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[0];
    cache_num_ub = segment_compile_info->tensor_sizes.at(cur_sch)[TWO];
    HCCL_DEBUG("id_num_ub is %d,var_num_ub is %d, cache_num_ub is %d", id_num_ub, var_num_ub, cache_num_ub);
    if (segment_rows_align > var_num_ub && cur_sch != NO_ATOMIC_CORE_X_SCH) {
        ub_norm_axis = 1;
        ub_norm_factor = var_num_ub;
    } else if ((segment_rows_align_core_x > var_num_ub) && (cur_sch == NO_ATOMIC_CORE_X_SCH)) {
        ub_norm_axis = 1;
        ub_norm_factor = var_num_ub;
    } else {
        ub_norm_axis = DUMMY_DIM;
        ub_norm_factor = DUMMY_DIM;
    }

    ub_reduce_axis = 0;
    if (ub_norm_axis == 1) {
        ub_reduce_factor = 1;
    } else {
        CalUbReduceFactor();
    }
    segment_cache_num = DUMMY_DIM;
    segment_cache_start = DUMMY_DIM;
    if (cur_sch == NO_ATOMIC_NO_PAD_NO_CACHE_SCH) {
        key_special_pattern = NO_ATOMIC_NO_PAD_NO_CACHE_KEY;
        HCCL_DEBUG("NO_ATOMIC_NO_PAD_NO_CACHE_KEY");
    } else if (cur_sch == NO_ATOMIC_PAD_NO_CACHE_SCH) {
        key_special_pattern = NO_ATOMIC_PAD_NO_CACHE_KEY;
        HCCL_DEBUG("NO_ATOMIC_PAD_NO_CACHE_KEY");
    } else if (cur_sch == NO_ATOMIC_CORE_X_SCH) {
        key_special_pattern = NO_ATOMIC_CORE_X_KEY;
        HCCL_DEBUG("NO_ATOMIC_CORE_X_KEY");
    } else {
        CalCacheNum(block_factor);
        segment_cache_start = block_factor;
        if (cur_sch == NO_ATOMIC_NO_PAD_CACHE_SCH) {
            key_special_pattern = NO_ATOMIC_NO_PAD_CACHE_KEY + is_set_mask * SET_MASK_KEY;
            HCCL_DEBUG("NO_ATOMIC_NO_PAD_CACHE_KEY");
        } else if (cur_sch == PAD_ALL_OUT_SCH) {
            key_special_pattern = NO_ATOMIC_PAD_ALL_OUT_KEY + is_set_mask * SET_MASK_KEY;
            HCCL_DEBUG("NO_ATOMIC_PAD_ALL_OUT_KEY");
        } else if (cur_sch == NO_PAD_ALL_OUT_SCH) {
            key_special_pattern = NO_ATOMIC_NO_PAD_ALL_OUT_KEY + is_set_mask * SET_MASK_KEY;
            HCCL_DEBUG("NO_ATOMIC_NO_PAD_ALL_OUT_KEY");
        } else {
            key_special_pattern = NO_ATOMIC_PAD_CACHE_KEY + is_set_mask * SET_MASK_KEY;
            HCCL_DEBUG("NO_ATOMIC_PAD_CACHE_KEY");
        }
    }
    segment_last_dim = segment_rows;
    HCCL_DEBUG("DoNoAtomicTiling, ub_norm_axis:%lld, ub_norm_factor:%lld, "
        "ub_reduce_axis:%lld, ub_reduce_factor:%lld,segment_cache_num "
        "%lld,segment_cache_start %lld,segment_last_dim is%lld",
        ub_norm_axis, ub_norm_factor, ub_reduce_axis, ub_reduce_factor, segment_cache_num, segment_cache_start,
        segment_last_dim);
    HCCL_DEBUG("unsorted_segment_sum: DoNoAtomicTiling end");
    return true;
}

template <typename T> void SegmentDsl<T>::EnsureBlockUBTiling()
{
    if (is_align || segment_compile_info->is_support_atomic == 1) {
        return;
    }
    if (ub_norm_axis != DUMMY_DIM || cur_sch == NO_ATOMIC_CORE_X_SCH) {
        return;
    }
    // check if ub factor less than 32B
    int64_t total_norm_ub_size = block_factor * segment_rows;
    HCCL_DEBUG("============ total_norm_ub_size:%lld, var_align_factor:%lld", total_norm_ub_size, var_align_factor);
    if (total_norm_ub_size < var_align_factor) {
        SafeTiling();
    }
    return;
}

template <typename T> void SegmentDsl<T>::SafeTiling()
{
    block_dims = 1;
    ub_norm_axis = DUMMY_DIM;
    ub_norm_factor = DUMMY_DIM;
    block_factor = num_segment;
    if (segment_cache_num != DUMMY_DIM) {
        HCCL_DEBUG("segment_cache_num not DUMMY_DIM");
        CalCacheNum(block_factor);
        segment_cache_start = block_factor;
    }
    HCCL_DEBUG("SafeTiling is selected ");
}

template <typename T> void SegmentDsl<T>::refine_axis(int64_t &refine_axis, const int64_t ori_axis)
{
    refine_axis = ori_axis == DUMMY_DIM ? 0 : ori_axis;
}

template <typename T> bool SegmentDsl<T>::check_pad(const std::string sch)
{
    if (segment_compile_info->tensor_sizes.count(sch) != 0) {
        HCCL_DEBUG("sch is %s", sch.c_str());
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(sch)[0];
        HCCL_DEBUG("atomic pad cache var_ub_num %d", var_ub_num);
        if (segment_rows_align <= remove_pad_threshold && pad_threshold * segment_rows_align <= var_ub_num) {
            HCCL_DEBUG("atomic pad cache support");
            return true;
        }
    }
    HCCL_DEBUG("not support pad");
    return false;
}

template <typename T> bool SegmentDsl<T>::is_support_pad(const std::string sch)
{
    if (is_align) {
        HCCL_DEBUG("is align not pad");
        return false;
    }
    if (segment_compile_info->tensor_sizes.count(ATOMIC_SPECIAL_SCH) != 0 && segment_rows == 1) {
        HCCL_DEBUG("sch is sepcial");
        return true;
    }
    if (check_pad(sch)) {
        return true;
    }
    HCCL_DEBUG("atomic doesn't support align_pad");
    return false;
}

template <typename T> bool SegmentDsl<T>::is_support_all_out()
{
    if (segment_compile_info->tensor_sizes.count(PAD_ALL_OUT_SCH) != 0 && is_support_pad(PAD_ALL_OUT_SCH)) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(PAD_ALL_OUT_SCH)[0];
        int64_t cache_ub_num = segment_compile_info->tensor_sizes.at(PAD_ALL_OUT_SCH)[2];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE && out_size_total_align <= cache_ub_num &&
            id_shape[0] > ALL_PATCH_LARGE) {
            cur_sch = PAD_ALL_OUT_SCH;
            HCCL_DEBUG("cur_sch is PAD_ALL_OUT_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(NO_PAD_ALL_OUT_SCH) != 0) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(NO_PAD_ALL_OUT_SCH)[0];
        int64_t cache_ub_num = segment_compile_info->tensor_sizes.at(NO_PAD_ALL_OUT_SCH)[2];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE && out_size_total_align <= cache_ub_num &&
            id_shape[0] > ALL_PATCH_LARGE) {
            cur_sch = NO_PAD_ALL_OUT_SCH;
            HCCL_DEBUG("sch is NO_PAD_ALL_OUT_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(ATOMIC_SPECIAL_SCH) != 0 && segment_rows == 1) {
        cur_sch = ATOMIC_SPECIAL_SCH;
        HCCL_DEBUG("cur_sch is ATOMIC_SPECIAL_SCH");
        return true;
    }
    return false;
}

template <typename T> bool SegmentDsl<T>::is_support_sch()
{
    int64_t impl_mode = segment_compile_info->impl_mode;
    HCCL_DEBUG("atomic impl_mode is %ld", impl_mode);
    if (is_support_all_out()) {
        return true;
    }
    if (impl_mode == HIGH_PERFORMANCE && segment_compile_info->tensor_sizes.count(ATOMIC_PAD_CACHE_SCH) != 0 &&
        is_support_pad(ATOMIC_PAD_CACHE_SCH)) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(ATOMIC_PAD_CACHE_SCH)[0];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE) {
            cur_sch = ATOMIC_PAD_CACHE_SCH;
            HCCL_DEBUG("cur_sch is ATOMIC_PAD_CACHE_SCH");
            return true;
        }
    }
    if (impl_mode == HIGH_PERFORMANCE && segment_compile_info->tensor_sizes.count(ATOMIC_NO_PAD_CACHE_SCH) != 0) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(ATOMIC_NO_PAD_CACHE_SCH)[0];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE) {
            cur_sch = ATOMIC_NO_PAD_CACHE_SCH;
            HCCL_DEBUG("sch is ATOMIC_NO_PAD_CACHE_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(ATOMIC_PAD_NO_CACHE_SCH) != 0 &&
        is_support_pad(ATOMIC_PAD_NO_CACHE_SCH)) {
        cur_sch = ATOMIC_PAD_NO_CACHE_SCH;
        HCCL_DEBUG("ATOMIC_PAD_NO_CACHE_SCH");
        return true;
    }
    if (segment_compile_info->tensor_sizes.count(ATOMIC_NO_PAD_NO_CACHE_SCH) != 0) {
        cur_sch = ATOMIC_NO_PAD_NO_CACHE_SCH;
        HCCL_DEBUG("ATOMIC_NO_PAD_NO_CACHE_SCH");
        return true;
    }
    HCCL_ERROR("not support ");
    return false;
}

template <typename T> bool SegmentDsl<T>::is_support_no_atomic_sch()
{
    int64_t impl_mode = segment_compile_info->impl_mode;
    if (segment_compile_info->tensor_sizes.count(PAD_ALL_OUT_SCH) != 0 && is_support_pad(PAD_ALL_OUT_SCH)) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(PAD_ALL_OUT_SCH)[0];
        int64_t cache_ub_num = segment_compile_info->tensor_sizes.at(PAD_ALL_OUT_SCH)[2];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE && out_size_total_align <= cache_ub_num) {
            cur_sch = PAD_ALL_OUT_SCH;
            HCCL_DEBUG("cur_sch is PAD_ALL_OUT_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(NO_PAD_ALL_OUT_SCH) != 0) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(NO_PAD_ALL_OUT_SCH)[0];
        int64_t cache_ub_num = segment_compile_info->tensor_sizes.at(NO_PAD_ALL_OUT_SCH)[2];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE && out_size_total_align <= cache_ub_num) {
            cur_sch = NO_PAD_ALL_OUT_SCH;
            HCCL_DEBUG("no atomic cur_sch is NO_PAD_ALL_OUT_SCH");
            return true;
        }
    }
    if (impl_mode == HIGH_PERFORMANCE && segment_compile_info->tensor_sizes.count(NO_ATOMIC_PAD_CACHE_SCH) != 0 &&
        is_support_pad(NO_ATOMIC_PAD_CACHE_SCH)) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(NO_ATOMIC_PAD_CACHE_SCH)[0];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE) {
            cur_sch = NO_ATOMIC_PAD_CACHE_SCH;
            HCCL_DEBUG("cur_sch is NO_ATOMIC_PAD_CACHE_SCH");
            return true;
        }
    }
    if (impl_mode == HIGH_PERFORMANCE && segment_compile_info->tensor_sizes.count(NO_ATOMIC_NO_PAD_CACHE_SCH) != 0) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(NO_ATOMIC_NO_PAD_CACHE_SCH)[0];
        HCCL_DEBUG("var_ub_num is %d", var_ub_num);
        if (var_ub_num / segment_rows_align >= ONE) {
            cur_sch = NO_ATOMIC_NO_PAD_CACHE_SCH;
            HCCL_DEBUG("cur_sch is NO_ATOMIC_NO_PAD_CACHE_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(NO_ATOMIC_PAD_NO_CACHE_SCH) != 0 &&
        is_support_pad(NO_ATOMIC_PAD_NO_CACHE_SCH)) {
        cur_sch = NO_ATOMIC_PAD_NO_CACHE_SCH;
        HCCL_DEBUG("NO_ATOMIC_PAD_NO_CACHE_SCH");
        return true;
    }
    if (segment_compile_info->tensor_sizes.count(NO_ATOMIC_CORE_X_SCH) != 0) {
        int64_t var_ub_num = segment_compile_info->tensor_sizes.at(NO_ATOMIC_CORE_X_SCH)[0];
        HCCL_DEBUG("segment_rows_align_core_x,var_ub_num is %d", var_ub_num);
        if (segment_rows_align > var_ub_num) {
            cur_sch = NO_ATOMIC_CORE_X_SCH;
            HCCL_DEBUG("NO_ATOMIC_CORE_X_SCH");
            return true;
        }
    }
    if (segment_compile_info->tensor_sizes.count(NO_ATOMIC_NO_PAD_NO_CACHE_SCH) != 0) {
        cur_sch = NO_ATOMIC_NO_PAD_NO_CACHE_SCH;
        HCCL_DEBUG("NO_ATOMIC_NO_PAD_NO_CACHE_SCH");
        return true;
    }
    HCCL_ERROR("no pad not support");
    return false;
}

template <typename T> bool SegmentDsl<T>::CalcKey()
{
    HCCL_DEBUG("CalcKey running");
    // base key
    key = BASE_KEY;

    // segment info
    key += key_special_pattern;

    // split info
    int64_t refine_ub_norm_axis = 0;
    refine_axis(refine_ub_norm_axis, ub_norm_axis);
    key += refine_ub_norm_axis;
    return true;
}

template <typename T> bool SegmentDsl<T>::WriteTilingData()
{
    HCCL_DEBUG("tiling key:%lld block_dims:%lld block_axis:%lld block_factor:%lld ub_norm_axis:%lld"
        "ub_norm_factor:%lld ub_reduce_axis:%lld ub_reduce_factor:%lld segment_cache_num:%lld,"
        "segment_cache_start:%lld, segment_last_dim is %lld",
        key, block_dims, block_axis, block_factor, ub_norm_axis, ub_norm_factor, ub_reduce_axis, ub_reduce_factor,
        segment_cache_num, segment_cache_start, segment_last_dim);
    if (segment_compile_info->is_static) {
        context->Append(static_cast<uint64_t>(key));
        context->Append(static_cast<int64_t>(block_axis));
        context->Append(static_cast<int64_t>(block_factor));
        context->Append(static_cast<int64_t>(ub_norm_axis));
        context->Append(static_cast<int64_t>(ub_norm_factor));
        context->Append(static_cast<int64_t>(ub_reduce_axis));
        context->Append(static_cast<int64_t>(ub_reduce_factor));
        context->Append(static_cast<int64_t>(segment_cache_num));
        context->Append(static_cast<int64_t>(segment_cache_start));
        context->Append(static_cast<int64_t>(segment_last_dim));
        HCCL_DEBUG("write success static");
        return true;
    }

    context->SetBlockDim(static_cast<uint64_t>(block_dims));
    context->SetTilingKey(static_cast<uint64_t>(key));

    int64_t cur_key = key;
    int64_t key_len = 8;

    char keys[KEY_SIZE] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '\0'};
    while (cur_key && key_len >= 0) {
        keys[key_len] = '0' + cur_key % DECIMAL_TEN;
        cur_key /= DECIMAL_TEN;
        key_len--;
    }
    std::string str_key = keys;
    HCCL_DEBUG("str_key is %s", str_key.c_str());
    try {
        const auto &all_vars = segment_compile_info->segment_vars.at(str_key);
        for (const auto &var : all_vars) {
            HCCL_DEBUG("var is %lld", var);
            if (var >= OUTPUT_VAR_IDX) {
                context->Append(static_cast<int64_t>(segment_last_dim));
                HCCL_DEBUG("[Segment]segment_last_dim is %lld", static_cast<int64_t>(segment_last_dim));
            } else if (var >= CACHE_START_IDX) {
                context->Append(static_cast<int64_t>(segment_cache_start));
                HCCL_DEBUG("[Segment]segment_cache_start is %lld",
                    static_cast<int64_t>(segment_cache_start));
            } else if (var >= CACHE_NUM_IDX) {
                context->Append(static_cast<int64_t>(segment_cache_num));
                HCCL_DEBUG("[Segment]segment_cache_num is %lld",
                    static_cast<int64_t>(segment_cache_num));
            } else if (var >= UB_REDUCE_IDX) {
                context->Append(static_cast<int64_t>(ub_reduce_factor));
                HCCL_DEBUG("[Segment]ub_reduce_factor is %lld", static_cast<int64_t>(ub_reduce_factor));
            } else if (var >= UB_NORM_IDX) {
                context->Append(static_cast<int64_t>(ub_norm_factor));
                HCCL_DEBUG("[Segment]ub_norm_factor is %lld", static_cast<int64_t>(ub_norm_factor));
            } else if (var >= BLOCK_IDX) {
                context->Append(static_cast<int64_t>(block_factor));
                HCCL_DEBUG("[Segment]block_factor is %lld", static_cast<int64_t>(block_factor));
            } else if (var >= NUM_SEGMENT_IDX) {
                context->Append(static_cast<int64_t>(num_segment));
                HCCL_DEBUG("[Segment]num_segment is %lld", static_cast<int64_t>(num_segment));
            } else {
                int64_t var_value = var;
                size_t dim_index = var_value % DECIMAL_TEN;
                context->Append(static_cast<int64_t>(var_shape[dim_index]));
                HCCL_DEBUG("[Segment]var_shape[dim_index] is %lld",
                    static_cast<int64_t>(var_shape[dim_index]));
            }
        }
    } catch (const std::exception &e) {
        HCCL_ERROR("get compile info[_segment_vars] error, error message: %s", e.what());
        return false;
    }
    HCCL_DEBUG("write success dynamic");
    return true;
}

template <typename T> bool SegmentDsl<T>::IsAtomicTiling()
{
    HCCL_DEBUG("IsAtomicTiling running");
    if (segment_compile_info->is_support_atomic != 1) {
        HCCL_DEBUG("not atomic");
        return false;
    }
    block_axis = 0;
    block_factor = CeilDiv(id_shape[block_axis], segment_compile_info->core_num);
    block_factor = block_factor > 0 ? block_factor : 1;

    block_dims = (id_shape[block_axis] + block_factor - 1) / block_factor;
    if (!is_support_sch()) {
        HCCL_ERROR("is_support_sch is false");
        return false;
    }
    return segment_compile_info->is_support_atomic == 1;
}

template <typename T> bool SegmentDsl<T>::DoTiling()
{
    HCCL_DEBUG("unsorted_segment_sum: DoTiling1");
    op_type = context->GetOpType();
    HCCL_DEBUG("SegmentDsl tiling running");
    bool init_ret = Init();
    if (!init_ret) {
        HCCL_ERROR("SegmentDsl init_ret is false");
        return false;
    }
    if (REMOVE_PAD_DTYPE.find(dtype) != REMOVE_PAD_DTYPE.end()) {
        remove_pad_threshold = REMOVE_PAD_DTYPE.at(dtype);
    }

    if (PAD_DTYPE.find(dtype) != PAD_DTYPE.end()) {
        pad_threshold = PAD_DTYPE.at(dtype);
    }
    bool tiling_ret = false;
    if (is_zero) {
        HCCL_DEBUG(" tiling key is base key");
        key = BASE_KEY;
        block_dims = 1;
        context->SetBlockDim(static_cast<uint64_t>(block_dims));
        context->SetTilingKey(static_cast<uint64_t>(key));
        return true;
    }
    if (IsAtomicTiling()) {
        HCCL_DEBUG("Atomic Tiling is selected ");
        tiling_ret = DoAtomicTiling();
    } else {
        HCCL_DEBUG("DoNoAtomicTiling is selected ");
        tiling_ret = DoNoAtomicTiling();
    }

    if (block_dims != 1) {
        EnsureBlockUBTiling();
    }

    tiling_ret = tiling_ret && CalcKey();
    tiling_ret = tiling_ret && WriteTilingData();
    HCCL_DEBUG("unsorted_segment_sum: DoTiling end");
    return tiling_ret;
}

bool CreateSegmentDslTiling(gert::TilingContext *context, const OpInfoImpl *op_info)
{
    HCCL_DEBUG("unsorted_segment_sum: CreateSegmentDslTiling");
    HCCL_DEBUG("CreateSegmentDslTiling rt2.0 DoTiling running");
    AutoTilingContext auto_tiling_context(context);
    if (op_info) {
        HCCL_DEBUG("segment auto_tiling_context op_info");
        auto_tiling_context.SetCompileInfo(op_info->GetCompileInfo());
    }
    SegmentDsl<AutoTilingContext> SegmentDsl(&auto_tiling_context, nullptr);
    HCCL_DEBUG("segment auto_tiling_context return");
    HCCL_DEBUG("unsorted_segment_sum: CreateSegmentDslTiling end");
    return SegmentDsl.DoTiling();
}

AutoTilingCompileInfo *CreateSegmentDslParser(const char *op_type, const nlohmann::json &compile_info)
{
    HCCL_DEBUG("unsorted_segment_sum: CreateSegmentDslParser");
    HCCL_DEBUG("CreateSegmentDslParser DoTiling running");
    SegmentDslCompileInfo *segment_compile_info = new SegmentDslCompileInfo(op_type, compile_info);
    if (!segment_compile_info->is_valid) {
        HCCL_ERROR("Segment parse failed");
        return nullptr;
    }
    HCCL_DEBUG("unsorted_segment_sum: CreateSegmentDslParser end");
    return segment_compile_info;
}

REGISTER_AUTO_TILING(SchPattern::SEGMENT, CreateSegmentDslTiling, CreateSegmentDslParser);
} // namespace TbeReduce
