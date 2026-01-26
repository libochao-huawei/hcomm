/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "eletwise_v4.h"
#include <algorithm>
#include <unordered_map>
#include <hccl/hccl_types.h>
#include "auto_tiling_register.h"
#include "legacy_log.h"

namespace TbeReduce {
namespace v4 {
namespace {
constexpr size_t PATTERN_INDEX_CONST = 0;
constexpr size_t PATTERN_INDEX_PURE_ELEWISE = 1;
constexpr size_t PATTERN_INDEX_ONE_RANK = 2;
constexpr size_t PATTERN_INDEX_BROADCAST_SCALAR = 3;
constexpr size_t PATTERN_INDEX_SCALAR_BROADCAST = 4;
constexpr size_t PATTERN_INDEX_FRACTAL_FORMAT = 5;
constexpr size_t PATTERN_INDEX_DISABLE_FUSE = 6;
constexpr int64_t BLOCK_NUM = 8;
constexpr int64_t COMMON_BLOCK_SIZE_BYTES = 32;
constexpr uint32_t BROADCAST_SCALAR_INPUT_NUM = 2;
constexpr uint64_t CONST_TILING_KEY = 100000000;
constexpr uint64_t DB_TILING_KEY = 10000;
constexpr int64_t DOUBLE_BUFFER_SIZE = 2;
constexpr uint32_t ELEWISE_FLAG_SIZE = 6;
constexpr int64_t MAX_REPEAT_TIMES = 8;
constexpr int64_t MIN_DIM_CUT_INDEX = 10000;
constexpr int64_t MIN_BLOCK_CUT_INDEX = 20000;
constexpr int64_t MIN_UB_CUT_INDEX = 30000;
constexpr int64_t ORI_DIM_INDEX = 40000;
constexpr int32_t PATTERN_AXIS_DIV_VALUE = 10;
constexpr int64_t VAR_INDEX_NUM = 100;
constexpr size_t FHD_SHAPE_LEN = 5;
constexpr int64_t SMALL_SHAPE_BLOCK_THRESHOLD = 128;
}

static const int64_t GetElementByType(const ge::DataType &dtype, int64_t blockSizeBytes)
{
    // element nums in one block, default, fp16, int16, uin16
    constexpr int64_t oneBitDtypeValue = 100;
    int64_t ELEMENT_IN_BLOCK_DOUBLE = blockSizeBytes / 8;
    int64_t ELEMENT_IN_BLOCK_FLOAT = blockSizeBytes / 4;
    int64_t ELEMENT_IN_BLOCK_HALF = blockSizeBytes / 2;
    int64_t ELEMENT_IN_BLOCK_BOOL = blockSizeBytes / 1;
    int64_t ELEMENT_IN_BLOCK_BIT = blockSizeBytes * 8;
    if (dtype == ge::DataType::DT_INT64 || dtype == ge::DataType::DT_UINT64) {
        // element nums in one block by b64
        return ELEMENT_IN_BLOCK_DOUBLE;
    } else if (dtype == ge::DataType::DT_FLOAT || dtype == ge::DataType::DT_INT32 || dtype == ge::DataType::DT_UINT32) {
        // element nums in one block by b32
        return ELEMENT_IN_BLOCK_FLOAT;
    } else if (dtype == ge::DataType::DT_FLOAT16 || dtype == ge::DataType::DT_INT16 ||
        dtype == ge::DataType::DT_UINT16) {
        // element nums in one block by b16
        return ELEMENT_IN_BLOCK_HALF;
    } else if (dtype == ge::DataType::DT_INT8 || dtype == ge::DataType::DT_UINT8 || dtype == ge::DataType::DT_BOOL) {
        // element nums in one block by b8
        return ELEMENT_IN_BLOCK_BOOL;
    } else if (dtype == oneBitDtypeValue) {
        // element nums in one block by uint1
        return ELEMENT_IN_BLOCK_BIT;
    } else {
        HCCL_ERROR("The elewise pattern not support dtype!");
        return -1;
    }
}

ElewisePattern GetDispatchPattern(const Fusion &fusion, size_t input_num, size_t dim_len, bool disable_optimization)
{
    if (disable_optimization) {
        return ElewisePattern::UNKNOWN;
    }
    if (input_num == 1) {
        return ElewisePattern::PURE_ELEWISE;
    }
    // calc shape multiplication, record input index when input shape multiplication !=1
    BitArray<B_MAX_INPUT_NUMS> mask;
    for (size_t j = 0; j < dim_len; j++) {
        const BitArray<B_MAX_INPUT_NUMS> &mask_j = fusion.GetBitArray(j);
        if (mask_j.IsAllOff(input_num)) {
            continue;
        }
        if (mask.IsAllOff(input_num)) {
            mask = mask_j;
            continue;
        }
        if (mask != mask_j) {
            return ElewisePattern::UNKNOWN;
        }
    }

    if (mask.IsAllOff(input_num)) {
        return ElewisePattern::PURE_ELEWISE;
    }

    if (mask.IsAllOn(input_num)) {
        return ElewisePattern::PURE_ELEWISE;
    } else if (input_num > BROADCAST_SCALAR_INPUT_NUM) {
        return ElewisePattern::ONE_RANK;
    } else if (mask.IsOn(0)) {
        return ElewisePattern::BROADCAST_SCALAR;
    } else {
        return ElewisePattern::SCALAR_BROADCAST;
    }
}

int64_t ElewiseCalcAlignCore(const int64_t &shape, const int64_t &core, const int64_t &num_blocks,
    const int64_t &half_core)
{
    int64_t align_core = core;
    for (; align_core > 0; align_core--) {
        if (shape % align_core == 0) {
            break;
        }
    }
    return (num_blocks * align_core) > half_core ? align_core : core;
}

template <typename T> void Elewise<T>::SetElewisePattern(const ElewisePattern &pattern)
{
    if (pattern != ElewisePattern::UNKNOWN) {
        broadcast_dispatch = true;
        classify_pattern = pattern;
    }
}

template <typename T> void Elewise<T>::SetRuntimeCore(const int64_t &broadcast_runtime_core)
{
    if (broadcast_runtime_core != -1) {
        runtime_core = broadcast_runtime_core;
    }
}

template <typename T> bool Elewise<T>::CheckCompileInfo()
{
    CHK_PRT_RET((compile_info->ub_factor_align <= 0),
        HCCL_ERROR("op_type:%s, ub_factor_align must be greater than zero!", op_type), false);
    CHK_PRT_RET((compile_info->max_out_dtype_num <= 0),
        HCCL_ERROR("op_type:%s, max_out_dtype_num byte must be greater than zero!", op_type), false);
    return true;
}

template <typename T> void Elewise<T>::GetMaxDimLen()
{
    // some custom tiling op will selfly extend input len without extend output len at same time
    max_dim_len = is_custom_tiling ? op_info->GetMaxDimNum() : ele_out_shape.GetDimNum();
}

template <typename T> bool Elewise<T>::DisableAllFuse()
{
    // check if disable all fuse, including disable_optimization and fractal_format tiling:
    // 1. elewise_disable_fuse = true
    // 2. dynamic fractal_format and contains_need_pad_compute is true
    // 3. static and const only check if satisfy fractal format
    if (compile_info->elewise_disable_fuse) {
        return true;
    }
    if (!compile_info->enable_fractal_format) {
        return false;
    }
    // const 5hd will not all fuse
    if (compile_info->only_const_tiling || compile_info->is_const_shapes) {
        return true;
    }
    HCCL_RUN_WARNING("DisableAllFuse return false");
    return false;
}

template <typename T> bool Elewise<T>::GetInputsShapes()
{
    // align all shapes to same dim, and check dim value of those inputs which shape size is not one
    int64_t first_shape_size = 0;
    all_shape_same = true;
    if (is_custom_tiling) {
        for (size_t in_index = 0; in_index < input_num; in_index++) {
            const std::vector<int64_t> &cur_input_shape = op_info->GetInputShape()->at(in_index);
            const int64_t cur_shape_size =
                std::accumulate(cur_input_shape.begin(), cur_input_shape.end(), 1LL, std::multiplies<int64_t>());
            if (cur_shape_size != 1LL) {
                input_fuse_shapes.SetOn(in_index);
            }
            if (in_index == 0) {
                first_shape_size = cur_shape_size;
            } else if (first_shape_size != cur_shape_size) {
                all_shape_same = false;
            }
            // custom input no needs to check each shape dim, only check shape_size
            if (cur_shape_size != 1) {
                CHK_PRT_RET((cur_shape_size != out_shape_size),
                    HCCL_ERROR("op_type:%s, custom inputs shape error!", op_type), false);
            }
        }
        return true;
    }
    for (size_t in_index = 0; in_index < input_num; in_index++) {
        const OpShape &cur_input_shape = context->GetInputShape(in_index);
        const size_t &cur_dim_len = cur_input_shape.GetDimNum();
        // GE think scalar shape size is empty, but we think it is one
        const int64_t cur_shape_size = cur_dim_len != 0 ? cur_input_shape.GetShapeSize() : 1;
        if (cur_shape_size != 1LL) {
            input_fuse_shapes.SetOn(in_index);
        }
        if (in_index == 0) {
            first_shape_size = cur_shape_size;
        } else if (first_shape_size != cur_shape_size) {
            all_shape_same = false;
        }
        if (cur_shape_size != 1) {
            for (size_t dim_index = 0; dim_index < cur_dim_len; dim_index++) {
                // check if cur dim value equals to output same dim value
                CHK_PRT_RET(
                    (ele_out_shape.GetDim(max_dim_len - cur_dim_len + dim_index) != cur_input_shape.GetDim(dim_index)),
                    HCCL_ERROR("op_type:%s, non_custom inputs shape error!", op_type), false);
            }
        }
    }
    return true;
}

template <typename T> bool Elewise<T>::WriteKnownData()
{
    HCCL_DEBUG("elewise known tiling key is:%llu and num_blocks is:%lld", tiling_key, num_blocks);
    context->SetNumBlocks(static_cast<uint32_t>(num_blocks));
    context->SetTilingKey(tiling_key);
    if (compile_info->has_var_attrs) {
        return context->WriteVarAttrs(tiling_key);
    }
    return true;
}

template <typename T> bool Elewise<T>::CalcConstKey()
{
    // elewise const dims only will be 1 or 2
    const size_t &const_dim_size = compile_info->elewise_const_dims.second.size();
    constexpr size_t pure_elewise_size = 1;
    constexpr size_t elewise_broadcast_size = 2;
    if (const_dim_size == pure_elewise_size) {
        num_blocks = compile_info->elewise_const_dims.second[0];
        tiling_key = CONST_TILING_KEY;
    } else if (const_dim_size == elewise_broadcast_size) {
        if (!all_shape_same) {
            // inputs with diff shapes such as [1] and [4] will secondly add info during compiler time
            num_blocks = compile_info->elewise_const_dims.second[1];
            tiling_key = CONST_TILING_KEY + 1;
        } else {
            // inputs with diff shapes such as [4] and [4] will first add info during compiler time
            num_blocks = compile_info->elewise_const_dims.second[0];
            tiling_key = CONST_TILING_KEY;
        }
    } else {
        HCCL_ERROR("op_type:%s, The const key calc fail due to error const shapes size!", op_type);
        return false;
    }
    return true;
}

template <typename T> bool Elewise<T>::CalcPatternKey()
{
    // pattern key calculation depends on classify mode and compute patterns
    // 1. compute pattern contains elemwise and broadcast: dispatch from broadcast tiling without 5HD known pattern
    // 2. compute only contains elemwise:
    // 2-1. classify func called by operations, only contains all fuse and partial fuse decided by is_fractal_format
    // 2-2. classify from dispatch, contains all shape same and only vector_dup broadcast scenes: bs, sb and ub
    if (broadcast_dispatch && !disable_all_fuse) {
        // broadcast will calc and return the true classify_pattern
        return true;
    }
    if (compile_info->only_const_tiling) {
        // static and const compiler-tiling will be here.
        classify_pattern = ElewisePattern::CONST;
    } else if (!disable_all_fuse) {
        if (compile_info->is_pure_elewise || all_shape_same) {
            // pure elewise pattern, all shape same
            classify_pattern = ElewisePattern::PURE_ELEWISE;
        } else {
            // only vector_dup broadcast scene
            if (input_num == BROADCAST_SCALAR_INPUT_NUM) {
                // scalar broadcast or broadcast scalar pattern
                classify_pattern =
                    input_fuse_shapes.IsOn(0) ? ElewisePattern::BROADCAST_SCALAR : ElewisePattern::SCALAR_BROADCAST;
            } else {
                // unknown broadcast pattern
                classify_pattern = ElewisePattern::ONE_RANK;
            }
        }
    } else {
        if (!compile_info->elewise_disable_fuse) {
            classify_pattern = ElewisePattern::FRACTAL_FORMAT;
        } else {
            classify_pattern = ElewisePattern::DISABLE_FUSE;
        }
    }
    return true;
}

std::array<size_t, ELEWISE_PATTERN_INDEX_NUM> AllElewisePattern{
    static_cast<size_t>(ElewisePattern::CONST),
    static_cast<size_t>(ElewisePattern::PURE_ELEWISE),
    static_cast<size_t>(ElewisePattern::ONE_RANK),
    static_cast<size_t>(ElewisePattern::BROADCAST_SCALAR),
    static_cast<size_t>(ElewisePattern::SCALAR_BROADCAST),
    static_cast<size_t>(ElewisePattern::FRACTAL_FORMAT),
    static_cast<size_t>(ElewisePattern::DISABLE_FUSE),
};

template <typename T> bool Elewise<T>::ParseBaseInfo()
{
    try {
        auto it = std::find(AllElewisePattern.begin(), AllElewisePattern.end(), static_cast<size_t>(classify_pattern));
        // elewise base info size may be 4
        CHK_PRT_RET((it == AllElewisePattern.cend()),
            HCCL_ERROR("op_type:%s, error classify_pattern %zu!", op_type, static_cast<size_t>(classify_pattern)),
            false);
        int64_t find_index = std::distance(AllElewisePattern.begin(), it);
        CHK_PRT_RET((compile_info->base_info_indexes[find_index] == -1),
            HCCL_ERROR("op_type:%s, classify_pattern %zu is not exist!", op_type,
            static_cast<size_t>(classify_pattern)),
            false);
        const auto &current_base_info = compile_info->base_info.second[compile_info->base_info_indexes[find_index]];
        constexpr uint32_t base_info_size = 4;
        // elewise base info size may be 4
        CHK_PRT_RET((current_base_info.size() != base_info_size),
            HCCL_ERROR("op_type:%s, elewise base info size must be four!", op_type), false);
        constexpr uint32_t core_index = 0;
        constexpr uint32_t max_dtype_index = 1;
        constexpr uint32_t ub_available_index = 2;
        constexpr uint32_t ub_available_db_index = 3;
        core_num = compile_info->soc_info.core_num == -1 ?
            (runtime_core == -1 ? current_base_info[core_index] : runtime_core) :
            static_cast<int64_t>(compile_info->soc_info.core_num);
        max_dtype = current_base_info[max_dtype_index];
        max_available_ub = current_base_info[ub_available_index];
        max_available_ub_db = current_base_info[ub_available_db_index];
    } catch (const std::exception &e) {
        HCCL_ERROR("op_type:%s, get base info error. Error message: %s", op_type, e.what());
        return false;
    }
    return true;
}

template <typename T> void Elewise<T>::CalcMultiCore()
{
    const int64_t multi_core_threshold = compile_info->max_out_dtype_num * core_num * DOUBLE_BUFFER_SIZE;
    if (out_shape_size < multi_core_threshold && multi_core_threshold < max_available_ub) {
        need_multi_core = false;
    }
}

template <typename T> void Elewise<T>::DoBlockTiling()
{
    int64_t cur_core = core_num;
    int64_t block_size_bytes = compile_info->ub_block_size;
    int64_t aligned_block_factor = compile_info->ub_factor_align;
    block_factor = (out_shape_size + cur_core - 1) / cur_core;
    block_factor = ((block_factor + aligned_block_factor - 1) / aligned_block_factor) * aligned_block_factor;
    block_factor =
        std::min(std::max(block_factor, SMALL_SHAPE_BLOCK_THRESHOLD * block_size_bytes / max_dtype), out_shape_size);
    num_blocks = (out_shape_size + block_factor - 1) / block_factor;
}

template <typename T> bool Elewise<T>::DoUbTiling(const int64_t factor)
{
    ub_factor = block_factor;
    int64_t limit = std::min(max_available_ub, factor);
    if (need_double_buffer) {
        limit = std::min(max_available_ub_db, factor);
    }
    if (limit < ub_factor) {
        int64_t aligned_ub_factor = compile_info->ub_factor_align;
        CHK_PRT_RET((limit <= 0),
            HCCL_ERROR("op_type:%s, ub limit must be greater than zero, but it is [%ld]", op_type, limit), false);
        int64_t ub_for_num = (ub_factor + limit - 1) / limit;
        int64_t adjust_factor = (ub_factor + ub_for_num - 1) / ub_for_num;
        int64_t align_factor = (adjust_factor + aligned_ub_factor - 1) / aligned_ub_factor;
        ub_factor = align_factor * aligned_ub_factor;
        if (ub_factor > limit) {
            ub_factor = (adjust_factor / aligned_ub_factor) * aligned_ub_factor;
        }
    }
    return true;
}

const int TILING_KEY_CONST = 100000000;
const int TILING_KEY_PURE_ELEWISE = 210000000;
const int TILING_KEY_ONE_RANK = 220000000;
const int TILING_KEY_BROADCAST_SCALAR = 223000000;
const int TILING_KEY_SCALAR_BROADCAST = 232000000;
const int TILING_KEY_FRACTAL_FORMAT = 211100000;
const int TILING_KEY_DISABLE_FUSE = 244400000;

template <typename T> void Elewise<T>::CalcTilingKey()
{
    switch (classify_pattern) {
        case ElewisePattern::CONST:
            tiling_key = TILING_KEY_CONST;
            break;
        case ElewisePattern::PURE_ELEWISE:
            tiling_key = TILING_KEY_PURE_ELEWISE;
            break;
        case ElewisePattern::ONE_RANK:
            tiling_key = TILING_KEY_ONE_RANK;
            break;
        case ElewisePattern::BROADCAST_SCALAR:
            tiling_key = TILING_KEY_BROADCAST_SCALAR;
            break;
        case ElewisePattern::SCALAR_BROADCAST:
            tiling_key = TILING_KEY_SCALAR_BROADCAST;
            break;
        case ElewisePattern::FRACTAL_FORMAT:
            tiling_key = TILING_KEY_FRACTAL_FORMAT;
            break;
        case ElewisePattern::DISABLE_FUSE:
            tiling_key = TILING_KEY_DISABLE_FUSE;
            break;
        case ElewisePattern::UNKNOWN:
        default:
            break;
    }
    if (need_double_buffer) {
        tiling_key += DB_TILING_KEY;
    }
    tiling_key += ub_axis;
}

template <typename T> bool Elewise<T>::WriteTilingDataInt32()
{
    HCCL_DEBUG(
        "op_type[%s], ElemWise tiling key:%llu, num_blocks:%lld, block_axis:%lld, block_factor:%lld, ub_axis:%lld,"
        "ub_factor:%lld",
        op_type, tiling_key, num_blocks, block_axis, block_factor, ub_axis, ub_factor);

    context->SetNumBlocks(static_cast<uint32_t>(num_blocks));
    if (compile_info->only_const_tiling) {
        int32_t double_buffer_num = need_double_buffer ? 1 : 0;
        context->Append(static_cast<int32_t>(need_multi_core));
        context->Append(static_cast<int32_t>(block_axis));
        context->Append(static_cast<int32_t>(block_factor));
        context->Append(static_cast<int32_t>(ub_axis));
        context->Append(static_cast<int32_t>(ub_factor));
        context->Append(double_buffer_num);
        return true;
    }
    context->SetTilingKey(tiling_key);
    try {
        // Add elewise vars params
        const std::vector<int64_t> &var_list = compile_info->elewise_vars.second.at(tiling_key);
        for (const auto &var : var_list) {
            if (var < MIN_BLOCK_CUT_INDEX) {
                // dim value
                if (!disable_all_fuse) {
                    // all fuse
                    context->Append(
                        static_cast<int32_t>(input_fuse_shapes.IsOn(var % MIN_DIM_CUT_INDEX) ? out_shape_size : 1LL));
                    continue;
                } else if (!compile_info->elewise_disable_fuse) {
                    // fractal format h, w fuse
                    context->Append(static_cast<int32_t>(disable_all_fuse_shape[var / VAR_INDEX_NUM % VAR_INDEX_NUM]));
                    continue;
                }
            } else if (var < MIN_UB_CUT_INDEX) {
                // block factor value
                context->Append(static_cast<int32_t>(block_factor));
            } else if (var < ORI_DIM_INDEX) {
                // ub factor value
                context->Append(static_cast<int32_t>(ub_factor));
            } else {
                // ori c dim value, elewise only support all c value same
                context->Append(static_cast<int32_t>(ele_ori_c));
            }
        }
    } catch (const std::exception &e) {
        HCCL_ERROR("op_type:%s, get elewise_vars error. Error message: %s", op_type, e.what());
        return false;
    }
    if (compile_info->has_var_attrs) {
        return context->WriteVarAttrs(tiling_key);
    }
    return true;
}

template <typename T> bool Elewise<T>::WriteTilingDataInt64()
{
    HCCL_DEBUG(
        "op_type[%s], ElemWise tiling key:%llu, num_blocks:%lld, block_axis:%lld, block_factor:%lld, ub_axis:%lld,"
        "ub_factor:%lld",
        op_type, tiling_key, num_blocks, block_axis, block_factor, ub_axis, ub_factor);

    context->SetNumBlocks(static_cast<uint32_t>(num_blocks));
    if (compile_info->only_const_tiling) {
        int64_t double_buffer_num = need_double_buffer ? 1 : 0;
        context->Append(static_cast<int64_t>(need_multi_core));
        context->Append(static_cast<int64_t>(block_axis));
        context->Append(static_cast<int64_t>(block_factor));
        context->Append(static_cast<int64_t>(ub_axis));
        context->Append(static_cast<int64_t>(ub_factor));
        context->Append(double_buffer_num);
        return true;
    }
    context->SetTilingKey(tiling_key);
    try {
        // Add elewise vars params
        const std::vector<int64_t> &var_list = compile_info->elewise_vars.second.at(tiling_key);
        for (const auto &var : var_list) {
            if (var < MIN_BLOCK_CUT_INDEX) {
                // dim value
                if (!disable_all_fuse) {
                    // all fuse
                    context->Append(
                        static_cast<int64_t>(input_fuse_shapes.IsOn(var % MIN_DIM_CUT_INDEX) ? out_shape_size : 1LL));
                    continue;
                } else if (!compile_info->elewise_disable_fuse) {
                    // fractal format h, w fuse
                    context->Append(static_cast<int64_t>(disable_all_fuse_shape[var / VAR_INDEX_NUM % VAR_INDEX_NUM]));
                    continue;
                }
            } else if (var < MIN_UB_CUT_INDEX) {
                // block factor value
                context->Append(static_cast<int64_t>(block_factor));
            } else if (var < ORI_DIM_INDEX) {
                // ub factor value
                context->Append(static_cast<int64_t>(ub_factor));
            } else {
                // ori c dim value, elewise only support all c value same
                context->Append(static_cast<int64_t>(ele_ori_c));
            }
        }
    } catch (const std::exception &e) {
        HCCL_ERROR("op_type:%s, get elewise_vars error. Error message: %s", op_type, e.what());
        return false;
    }
    if (compile_info->has_var_attrs) {
        return context->WriteVarAttrs(tiling_key);
    }
    return true;
}

template <typename T> bool Elewise<T>::WriteTilingData()
{
    if (compile_info->int64_mode) {
        return WriteTilingDataInt64();
    }
    return WriteTilingDataInt32();
}

template <typename T> void Elewise<T>::DisableAllFuseBlockTiling()
{
    int64_t cur_core = core_num;
    // multi core need more than half of cores
    int64_t half_core = core_num / 2;
    bool is_one_dim = disable_all_fuse_shape.size() == 1;
    // calc if need do block align
    const int64_t &block_align_threshold = compile_info->max_out_dtype_num * BLOCK_NUM * MAX_REPEAT_TIMES * core_num;
    bool need_block_align = out_shape_size <= block_align_threshold;

    for (size_t i = 0; i < disable_all_fuse_shape.size(); i++) {
        if (disable_all_fuse_shape[i] > cur_core) {
            int64_t block_align_core = need_block_align ?
                ElewiseCalcAlignCore(disable_all_fuse_shape[i], cur_core, num_blocks, half_core) :
                cur_core;
            multi_core_output = disable_all_fuse_shape[i];
            block_axis = i;
            block_factor = (disable_all_fuse_shape[i] + block_align_core - 1) / block_align_core;
            CHK_PRT_RET((block_factor <= 0),
                HCCL_ERROR("op_type:%s, block_factor must be greater than zero.", op_type),);
            num_blocks *= ((disable_all_fuse_shape[i] + block_factor - 1) / block_factor);
            disable_all_fuse_shape[i] = block_factor;
            break;
        }
        if (need_block_align && cur_core % disable_all_fuse_shape[i] != 0 &&
            num_blocks * disable_all_fuse_shape[i] > half_core) {
            multi_core_output = disable_all_fuse_shape[i];
            block_axis = i;
            block_factor = 1;
            num_blocks *= disable_all_fuse_shape[i];
            disable_all_fuse_shape[i] = block_factor;
            if (!is_one_dim) {
                block_axis = i + 1;
                block_factor = disable_all_fuse_shape[i + 1];
                disable_all_fuse_shape[i] = multi_core_output;
                multi_core_output = disable_all_fuse_shape[i + 1];
            }
            break;
        }
        cur_core /= disable_all_fuse_shape[i];
        num_blocks *= disable_all_fuse_shape[i];
    }
}

template <typename T> void Elewise<T>::AdjustUbTiling(const int64_t &under_ub_shape, const int64_t &limit)
{
    if (block_axis == ub_axis) {
        int64_t ub_for_num = (disable_all_fuse_shape[ub_axis] + ub_factor - 1) / ub_factor;
        CHK_PRT_RET((ub_for_num > 0), HCCL_ERROR("op_type:%s, ub_for_num must be greater than zero.", op_type),);
        ub_factor = (disable_all_fuse_shape[ub_axis] + ub_for_num - 1) / ub_for_num;
    }
    int64_t shape_len = static_cast<int64_t>(disable_all_fuse_shape.size()) - 1;
    int64_t ele_in_block = compile_info->max_out_dtype_num;
    CHK_PRT_RET((ele_in_block == 0), HCCL_ERROR("op_type:%s, ele_in_block can not be zero.", op_type),);
    if (ub_axis == shape_len && ub_factor != disable_all_fuse_shape[shape_len]) {
        if (disable_all_fuse_shape.size() == 1) {
            ele_in_block = compile_info->ub_factor_align;
        }
        int64_t last_factor = ub_factor;
        int64_t align_factor = (ub_factor + ele_in_block - 1) / ele_in_block;
        ub_factor = align_factor * ele_in_block;
        if (ub_factor > limit) {
            ub_factor = (last_factor / ele_in_block) * ele_in_block;
        }
    }
    // adjust the ub factor to avoid tail block less than 32B
    CHK_PRT_RET((ub_factor == 0), HCCL_ERROR("op_type:%s, ub_factor can not be zero.", op_type),);
    int64_t ub_tail = disable_all_fuse_shape[ub_axis] % ub_factor;
    if (ub_tail != 0 && (under_ub_shape * ub_tail < ele_in_block)) {
        CHK_PRT_RET((under_ub_shape == 0), HCCL_ERROR("op_type:%s, under_ub_shape can not be zero.", op_type),);
        int64_t need_tail = (ele_in_block + under_ub_shape - 1) / under_ub_shape;
        int64_t cur_tail = need_tail - ub_tail;
        int64_t ub_factor_num = disable_all_fuse_shape[ub_axis] / ub_factor;
        int64_t ub_gap = (cur_tail + ub_factor_num - 1) / ub_factor_num;
        ub_factor -= ub_gap;
    }
}

template <typename T> void Elewise<T>::CheckUpdateUbTiling()
{
    bool need_single_core = false;
    for (size_t i = 0; i < context->GetOutputNums(); i++) {
        ge::DataType eachOutDtype = ge::DataType::DT_INT8;
        CHK_PRT_RET((!(context->GetOutputDataType(i, eachOutDtype))),
            HCCL_ERROR("op_type:%s, get out dtype error.", op_type),);
        int64_t block_size_bytes = compile_info->ub_block_size;
        int64_t ele_in_block = GetElementByType(eachOutDtype, block_size_bytes);
        int64_t cut_output = disable_all_fuse_shape[ub_axis];
        int64_t under_ub_shape = std::accumulate(disable_all_fuse_shape.begin() + ub_axis + 1,
            disable_all_fuse_shape.end(), 1LL, std::multiplies<int64_t>());
        need_single_core = (cut_output % ub_factor != 0 && (cut_output % ub_factor) * under_ub_shape < ele_in_block) ||
            (cut_output % ub_factor == 0 && ub_factor * under_ub_shape < ele_in_block);
        if (block_axis == ub_axis) {
            int64_t tail = multi_core_output % block_factor % ub_factor;
            need_single_core = need_single_core || (tail != 0 && tail * under_ub_shape < ele_in_block);
        }
    }
    if (need_single_core) {
        disable_all_fuse_shape[block_axis] = multi_core_output;
        block_axis = 0;
        block_factor = disable_all_fuse_shape[block_axis];
        num_blocks = 1;
    }
    int64_t max_tiling_core_num = core_num;
    if (need_single_core) {
        max_tiling_core_num = 1;
    }
    disable_all_fuse_shape[block_axis] = multi_core_output;
    int64_t shape_before_ub = std::accumulate(disable_all_fuse_shape.begin(), disable_all_fuse_shape.begin() + ub_axis,
        1LL, std::multiplies<int64_t>());
    int64_t ub_split_out = (disable_all_fuse_shape[ub_axis] + ub_factor - 1) / ub_factor;
    CHK_PRT_RET((max_tiling_core_num == 0), HCCL_ERROR("op_type:%s, max_tiling_core_num can not be zero.", op_type),);
    block_factor = (shape_before_ub * ub_split_out + max_tiling_core_num - 1) / max_tiling_core_num;
    CHK_PRT_RET((block_factor == 0), HCCL_ERROR("op_type:%s, block_factor can not be zero.", op_type),);
    num_blocks = (shape_before_ub * ub_split_out + block_factor - 1) / block_factor;
    block_axis = 0;
}

template <typename T> void Elewise<T>::DisableAllFuseUbTiling()
{
    int64_t block_size_factor = COMMON_BLOCK_SIZE_BYTES / compile_info->ub_block_size;
    const std::unordered_map<int64_t, int64_t> SPLIT_FACTORS{
        { 1, 32767 / block_size_factor },
        { 2, 32767 / block_size_factor },
        { 4, 16383 / block_size_factor },
        { 8, 8191 / block_size_factor },
    };
    int64_t limit = std::min(max_available_ub, SPLIT_FACTORS.at(max_dtype));
    if (need_double_buffer) {
        limit = std::min(max_available_ub_db, SPLIT_FACTORS.at(max_dtype));
    }
    int64_t max_ub_shape = 1;
    int64_t shape_len = static_cast<int64_t>(disable_all_fuse_shape.size()) - 1;
    for (int64_t i = shape_len; i >= block_axis; i--) {
        if (disable_all_fuse_shape[i] >= limit) {
            ub_axis = i;
            ub_factor = limit;
            max_ub_shape *= ub_factor;
            break;
        }
        limit /= disable_all_fuse_shape[i];
        max_ub_shape *= disable_all_fuse_shape[i];
        ub_axis = i;
        ub_factor = disable_all_fuse_shape[i];
    }
    int64_t under_ub_shape = max_ub_shape / ub_factor;
    if (ub_axis == shape_len) {
        ub_factor = std::min(ub_factor, disable_all_fuse_shape[ub_axis]);
    }
    AdjustUbTiling(under_ub_shape, limit);
    if (disable_all_fuse_shape.size() != 1) {
        CheckUpdateUbTiling();
    }
}

template <typename T> bool Elewise<T>::RefineOutShape()
{
    if (disable_all_fuse && !compile_info->elewise_disable_fuse) {
        if (compile_info->only_const_tiling) {
            for (size_t index = 0; index < max_dim_len; index++) {
                disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(index));
            }
            return true;
        }
        // fractal format scene which fuse h and w axis
        CHK_PRT_RET((max_dim_len != FHD_SHAPE_LEN),
            HCCL_ERROR("op_type:%s, fractal format shape is not five! please check it!", op_type), false);
        constexpr size_t n_index = 0;
        constexpr size_t c1_index = 1;
        constexpr size_t h_index = 2;
        constexpr size_t w_index = 3;
        constexpr size_t c0_index = 4;
        disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(n_index));
        disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(c1_index));
        disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(h_index) * ele_out_shape.GetDim(w_index));
        disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(c0_index));
        return true;
    }
    for (size_t dim_index = 0; dim_index < max_dim_len; dim_index++) {
        disable_all_fuse_shape.emplace_back(ele_out_shape.GetDim(dim_index));
    }
    return true;
}

template <typename T> bool Elewise<T>::DisableAllFuseTiling()
{
    int64_t block_size_factor = COMMON_BLOCK_SIZE_BYTES / compile_info->ub_block_size;
    const std::unordered_map<int64_t, int64_t> SPLIT_FACTORS{
        { 1, 32767 / block_size_factor },
        { 2, 32767 / block_size_factor },
        { 4, 16383 / block_size_factor },
        { 8, 8191 / block_size_factor },
    };
    if (need_multi_core) {
        DisableAllFuseBlockTiling();
        if (block_factor > std::min(max_available_ub, SPLIT_FACTORS.at(max_dtype))) {
            need_double_buffer = true;
        }
        DisableAllFuseUbTiling();
    } else {
        num_blocks = 1;
        block_axis = 0;
        ub_axis = 0;
        block_factor = disable_all_fuse_shape[0];
        ub_factor = disable_all_fuse_shape[0];
    }
    return true;
}

template <typename T> bool Elewise<T>::AllFuseTiling()
{
    int64_t block_size_factor = COMMON_BLOCK_SIZE_BYTES / compile_info->ub_block_size;
    const std::unordered_map<int64_t, int64_t> SPLIT_FACTORS{
        { 1, 32767 / block_size_factor },
        { 2, 32767 / block_size_factor },
        { 4, 16383 / block_size_factor },
        { 8, 8191 / block_size_factor },
    };
    if (need_multi_core) {
        DoBlockTiling();
        int64_t factor = SPLIT_FACTORS.at(max_dtype);
        if (block_factor > std::min(max_available_ub, factor)) {
            need_double_buffer = true;
        }
        return DoUbTiling(factor);
    }
    num_blocks = 1;
    block_axis = 0;
    ub_axis = 0;
    block_factor = out_shape_size;
    ub_factor = out_shape_size;
    return true;
}

template <typename T> bool Elewise<T>::DoTiling()
{
    // tiling dispatch to different classify mode:
    // 1. static: all known compiler-shape without shape_infer, only do compiler-tiling, no need do runtime-tiling.
    // 2. const: exists unknown shape but can be infer to enumerable shape, shares same compiler-tiling with static,
    //    needs runtime-tiling to match real before_infer shape and get right tiling_key.
    // 3. empty: same with const.
    // 4. all_fuse: compiler-tiling only get tiling vars, which need to be calculate by runtime known shapes, block
    // tiling
    //    first, ub tiling second.
    // 5. disable_fuse: same process with all_fuse, but ub tiling first, and block tiling second.
    op_type = context->GetOpType();
    compile_info = static_cast<const ElewiseCompileInfo *>(context->GetCompileInfo());
    bool ret = CheckCompileInfo();
    is_custom_tiling = op_info != nullptr;
    input_num = context->GetInputNums(op_info);
    ele_out_shape = context->GetOutputShape(0);
    out_shape_size = ele_out_shape.GetShapeSize();
    GetMaxDimLen();
    disable_all_fuse = DisableAllFuse();
    // get output shape from context no depends on is_custom_tiling
    CHK_PRT_RET((ele_out_shape.Empty()), HCCL_ERROR("op_type:%s, Error for get empty output shape!", op_type), false);
    is_empty_tensor = out_shape_size == 0;

    // get input shape depends on is_custom_tiling
    ret = ret && GetInputsShapes();
    CHK_PRT_RET(!ret, HCCL_ERROR("op_type:%s, elewise tiling inputs get failed.", op_type), false);

    // tiling dispatch
    if (compile_info->is_const_shapes) {
        // only const runtime-tiling will be here
        ret = CalcConstKey();
        ret = ret && WriteKnownData();
    } else if (is_empty_tensor) {
        num_blocks = 1;
        tiling_key = INT32_MAX;
        ret = WriteKnownData();
    } else {
        ret = CalcPatternKey();
        ret = ret && ParseBaseInfo();
        CalcMultiCore();
        if (!disable_all_fuse) {
            // block tiling first, ub tiling second
            ret = ret && AllFuseTiling();
        } else {
            ret = ret && RefineOutShape();
            ret = ret && DisableAllFuseTiling();
        }
        if (ret && !compile_info->only_const_tiling) {
            CalcTilingKey();
        }
        ret = ret && WriteTilingData();
    }
    return ret;
}

void ElewiseCompileInfo::ParseIsPureElewise(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_is_pure_elewise")) {
        is_pure_elewise = outer_compile_info.at("_is_pure_elewise").get<bool>();
    }
}

void ElewiseCompileInfo::ParseEnableFractalFormat(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_enable_fractal_format")) {
        enable_fractal_format = outer_compile_info.at("_enable_fractal_format").get<bool>();
    }
}

void ElewiseCompileInfo::ParseElewiseDisableFuse(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_elewise_disable_fuse")) {
        elewise_disable_fuse = outer_compile_info.at("_elewise_disable_fuse").get<bool>();
    }
}

void ElewiseCompileInfo::ParseInputDtypeBytes(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_inputs_ele_in_block")) {
        inputs_ele_in_block = outer_compile_info.at("_inputs_ele_in_block").get<std::vector<int64_t>>();
    }
}

void ElewiseCompileInfo::ParseIsConstShapes(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_is_const_shapes")) {
        is_const_shapes = outer_compile_info.at("_is_const_shapes").get<bool>();
    }
}

void ElewiseCompileInfo::ParseUbblockSize(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_ub_block_size")) {
        ub_block_size = outer_compile_info.at("_ub_block_size").get<int64_t>();
    }
}

void ElewiseCompileInfo::ParseMaxOutDtypeNum(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_max_out_dtype_num")) {
        max_out_dtype_num = outer_compile_info.at("_max_out_dtype_num").get<int64_t>();
    }
}

void ElewiseCompileInfo::ParseUbFactorAlign(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_ub_factor_align")) {
        ub_factor_align = outer_compile_info.at("_ub_factor_align").get<int64_t>();
    }
}

void ElewiseCompileInfo::ParseInt64Mode(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_int64_mode")) {
        int64_mode = outer_compile_info.at("_int64_mode").get<bool>();
    }
}

void ElewiseCompileInfo::ParseRequiredCompileInfo(const nlohmann::json &outer_compile_info)
{
    ParseIsPureElewise(outer_compile_info);
    ParseEnableFractalFormat(outer_compile_info);
    ParseElewiseDisableFuse(outer_compile_info);
    ParseInputDtypeBytes(outer_compile_info);
    ParseIsConstShapes(outer_compile_info);
    ParseMaxOutDtypeNum(outer_compile_info);
    ParseUbFactorAlign(outer_compile_info);
    ParseInt64Mode(outer_compile_info);
}

HcclResult ElewiseCompileInfo::SalStrToLonglong(const std::string str, int base, s64 &val)
{
    try {
        val = std::stoll(str, nullptr, base);
    }
    catch (std::invalid_argument&) {
        HCCL_ERROR("[Transform][SalStrToLonglong]stoll invalid argument, str[%s] base[%d] val[%lld]",
            str.c_str(), base, val);
        return HCCL_E_PARA;
    }
    catch (std::out_of_range&) {
        HCCL_ERROR("[Transform][SalStrToLonglong]stoll out of range, str[%s] base[%d] val[%lld]",
            str.c_str(), base, val);
        return HCCL_E_PARA;
    }
    catch (...) {
        HCCL_ERROR("[Transform][SalStrToLonglong]stoll catch error, str[%s] base[%d] val[%lld]",
            str.c_str(), base, val);
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

void ElewiseCompileInfo::ParseBaseInfo(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_base_info")) {
        base_info.first = true;
        const auto &local_base_info =
            outer_compile_info.at("_base_info").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        int64_t cur_index = 0;
        s64 first = 0;
        base_info_indexes.fill(-1);
        for (const auto &each_pair : local_base_info) {
            CHK_PRT(SalStrToLonglong(each_pair.first, HCCL_BASE_DECIMAL, first));
            auto it = std::find(AllElewisePattern.begin(), AllElewisePattern.end(), first);
            if (it == AllElewisePattern.cend()) {
                continue;
            }
            int64_t find_index = std::distance(AllElewisePattern.begin(), it);
            base_info_indexes[find_index] = cur_index++;
            base_info.second.push_back(each_pair.second);
        }
    }
}

void ElewiseCompileInfo::ParseConstCompileInfo(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_elewise_const_dims") && is_const_shapes) {
        elewise_const_dims.first = true;
        elewise_const_dims.second = outer_compile_info.at("_elewise_const_dims").get<std::vector<int64_t>>();
    }
}

void ElewiseCompileInfo::ParseContainsPadCompute(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_contains_need_pad_compute")) {
        contains_need_pad_compute = outer_compile_info.at("_contains_need_pad_compute").get<bool>();
    }
}

void ElewiseCompileInfo::ParseElewiseVar(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_elewise_vars")) {
        elewise_vars.first = true;
        const auto &local_elewise_vars =
            outer_compile_info.at("_elewise_vars").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        s64 first = 0;
        for (const auto &each_pair : local_elewise_vars) {
            CHK_PRT(SalStrToLonglong(each_pair.first, HCCL_BASE_DECIMAL, first));
            (elewise_vars.second)[first] = each_pair.second;
        }
    }
}

void ElewiseCompileInfo::ParseFusedIndex(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_elewise_fused_index")) {
        elewise_fused_index.first = true;
        elewise_fused_index.second =
            outer_compile_info.at("_elewise_fused_index").get<std::vector<std::vector<size_t>>>();
    }
}

void ElewiseCompileInfo::ParseOnlyConstTiling(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_only_const_tiling")) {
        only_const_tiling = outer_compile_info.at("_only_const_tiling").get<bool>();
    }
}

bool ElewiseCompileInfo::ParseVarsAttr(const nlohmann::json &outer_compile_info)
{
    bool ret = var_attr_wrap.ParseVarAttr(outer_compile_info);
    has_var_attrs = !var_attr_wrap.IsEmpty();
    return ret;
}

bool ElewiseCompileInfo::ParseOptionalCompileInfo(const nlohmann::json &outer_compile_info)
{
    if (ParseVarsAttr(outer_compile_info)) {
        ParseBaseInfo(outer_compile_info);
        ParseConstCompileInfo(outer_compile_info);
        ParseContainsPadCompute(outer_compile_info);
        ParseElewiseVar(outer_compile_info);
        ParseFusedIndex(outer_compile_info);
        ParseOnlyConstTiling(outer_compile_info);
        return true;
    } else {
        return false;
    }
}

bool ElewiseCompileInfo::Parse(const char *op_type, const nlohmann::json &json_compile_info)
{
    HCCL_DEBUG("op_type[%s], Elewise compile info parse is running.", op_type);
    try {
        ParseRequiredCompileInfo(json_compile_info);
        ParseOptionalCompileInfo(json_compile_info);
    } catch (const std::exception &e) {
        HCCL_ERROR("Elemwise compile_info parses failed. Error message: %s", e.what());
        return false;
    }
    return true;
}

ElewiseCompileInfo::ElewiseCompileInfo(const std::string &op_type, const nlohmann::json &outer_compile_info)
{
    HCCL_DEBUG("op_type[%s], Elewise compile info parse is running.", op_type.c_str());
    try {
        ParseRequiredCompileInfo(outer_compile_info);
        ParseOptionalCompileInfo(outer_compile_info);
    } catch (const std::exception &e) {
        HCCL_ERROR("Elemwise compile_info parses failed. Error message: %s", e.what());
    }
}
} // namespace v4

bool CreateElewiseDslTiling(gert::TilingContext *context, const OpInfoImpl *op_info)
{
    HCCL_DEBUG("Enter into Elemwise runtime2 process.");
    AutoTilingContext auto_tiling_context(context);
    if (op_info != nullptr) {
        HCCL_DEBUG("Elewise runtime2 tiling runs with op_info.");
        auto_tiling_context.SetCompileInfo(op_info->GetCompileInfo());
    }
    v4::Elewise<AutoTilingContext> elewise(&auto_tiling_context, op_info);
    return elewise.DoTiling();
}

AutoTilingCompileInfo *CreateElewiseDslParser(const char *op_type, const nlohmann::json &json_compile_info)
{
    auto compile_info = new (std::nothrow) v4::ElewiseCompileInfo();
    if (compile_info == nullptr) {
        HCCL_ERROR("compile_info is nullptr!");
        return nullptr;
    }

    if (!compile_info->Parse(op_type, json_compile_info)) {
        HCCL_ERROR("json compile info parse failed!");
        return nullptr;
    }
    return compile_info;
}

REGISTER_AUTO_TILING(SchPattern::ELETWISE, CreateElewiseDslTiling, CreateElewiseDslParser);
} // namespace TbeReduce
