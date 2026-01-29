/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "eletwise_v3.h"
#include <algorithm>
#include <unordered_map>
#include "vector_tiling.h"

namespace TbeReduce {
namespace v3 {
namespace {
const std::unordered_map<int64_t, int64_t> SPLIT_FACTORS {
    { 1, 32767 },
    { 2, 32767 },
    { 4, 16383 },
    { 8, 8191 },
};
constexpr int64_t DOUBLE_BUFFER_SIZE = 2;
constexpr int64_t ELEWISE_MAX_INPUT_NUMS = 70;
constexpr int64_t KNOWN_PATTERN_KEY = 0;
constexpr int64_t COMMON_PATTERN_KEY = 1;
constexpr uint64_t CONST_TILING_KEY = 100000000;
constexpr uint32_t ELEWISE_FLAG_SIZE = 6;
constexpr int64_t ELEMENT_IN_BLOCK_DOUBLE = 4;
constexpr int64_t ELEMENT_IN_BLOCK_FLOAT = 8;
constexpr int64_t ELEMENT_IN_BLOCK_HALF = 16;
constexpr int64_t ELEMENT_IN_BLOCK_BOOL = 32;
constexpr int64_t ELEMENT_IN_BLOCK_BIT = 256;
constexpr int32_t VAR_ATTR_MODE_NOT_EXIST = -1;
constexpr int32_t VAR_ATTR_MODE_CONSISTENT = 0;
constexpr int32_t VAR_ATTR_MODE_INDEPENDENT = 1;
}

const int64_t EletwiseV3::GetElementByType(const std::string& dtype) const
{
    // element nums in float32 int32 uint32
    if (dtype == "float32" || dtype == "int32" || dtype == "uint32") {
        // element nums in one block by b32
        return ELEMENT_IN_BLOCK_FLOAT;
    } else if (dtype == "int8" || dtype == "uint8" || dtype == "bool") {
        // element nums in one block by b8
        return ELEMENT_IN_BLOCK_BOOL;
    } else if (dtype == "int64" || dtype == "uint64") {
        // element nums in one block by b64
        return ELEMENT_IN_BLOCK_DOUBLE;
    } else {
        // element nums in one block by b16
        return ELEMENT_IN_BLOCK_HALF;
    }
}

bool EletwiseV3::CheckCompileInfo()
{
    // required compile_info check
    CHECK_GT(compile_info.flag_info_size, 0, "opType[%s] elewise flag_info_size must be greater than zero!",
        op_type.c_str());
    CHECK_GT(compile_info.ub_factor_align, 0, "opType[%s] elewise ub_factor_align must be greater than zero!",
        op_type.c_str());
    if (compile_info.pattern_key == KNOWN_PATTERN_KEY || compile_info.pattern_key == COMMON_PATTERN_KEY) {
        CHECK_GT(compile_info.core_num, 0, "opType[%s] elewise base_info core_num can not be neg.", op_type.c_str());
        CHECK_GT(compile_info.max_dtype, 0, "opType[%s] elewise base_info max_dtype can not be neg.", op_type.c_str());
        CHK_PRT_RET((SPLIT_FACTORS.find(compile_info.max_dtype) == SPLIT_FACTORS.end()),
            HCCL_ERROR("opType[%s] elewise base_info max_dtype not in SPLIT_FACTORS.", op_type.c_str()), false);
        CHECK_GT(compile_info.max_available_ub, 0, "opType[%s] elewise base_info max_available_ub can not be neg.",
            op_type.c_str());
        CHECK_GT(compile_info.max_available_ub_db, 0,
            "opType[%s] elewise base_info max_available_ub_db can not be neg.", op_type.c_str());
    }
    return true;
}

bool EletwiseV3::GetOutShape()
{
    const std::vector<int64_t>& shapes = op_paras.outputs[0].tensor[0].shape;
    out_shape = std::accumulate(shapes.begin(), shapes.end(), 1LL, std::multiplies<int64_t>());
    CHECK_LE(out_shape, INT32_MAX,
             "opType[%s] The output shape is too large", op_type.c_str());
    CHECK_GT(out_shape, 0,
             "opType[%s] The output shape must be greater than 0", op_type.c_str());
    return true;
}

void EletwiseV3::GetOutputDtype()
{
    out_dtype = op_paras.outputs[0].tensor[0].dtype;
}

void EletwiseV3::CalcMultiCore()
{
    const int64_t multi_core_threshold = GetElementByType(out_dtype) * compile_info.core_num * DOUBLE_BUFFER_SIZE;
    if (out_shape < multi_core_threshold) {
        need_multi_core = false;
    }
}

void EletwiseV3::DoBlockTiling()
{
    int64_t cur_core = compile_info.core_num;
    int64_t block_factor_align_size = compile_info.ub_factor_align;
    block_factor = static_cast<int64_t>(std::ceil(out_shape * 1.0 / cur_core));
    block_factor = static_cast<int64_t>(std::ceil(block_factor * 1.0 / block_factor_align_size) *
        block_factor_align_size);
    num_blocks = static_cast<int64_t>(std::ceil(out_shape * 1.0 / block_factor));
}

bool EletwiseV3::DoUbTiling()
{
    ub_factor = block_factor;
    int64_t limit = std::min(compile_info.max_available_ub, SPLIT_FACTORS.at(compile_info.max_dtype));
    if (need_double_buffer) {
        limit = std::min(compile_info.max_available_ub_db, SPLIT_FACTORS.at(compile_info.max_dtype));
    }
    if (limit < ub_factor) {
        int64_t ub_factor_align_size = compile_info.ub_factor_align;
        CHECK_GT(limit, 0, "opType[%s] ub limit must be greater than zero, but it is [%ld]", op_type.c_str(), limit);
        int64_t ub_for_num = static_cast<int64_t>(std::ceil(ub_factor * 1.0 / limit));
        int64_t adjust_factor = static_cast<int64_t>(std::ceil(ub_factor * 1.0 / ub_for_num));
        int64_t align_factor = static_cast<int64_t>(std::ceil(adjust_factor * 1.0 / ub_factor_align_size));
        ub_factor = align_factor * ub_factor_align_size;
        if (ub_factor > limit) {
            ub_factor = static_cast<int64_t>(std::floor(adjust_factor * 1.0 / ub_factor_align_size) *
                ub_factor_align_size);
        }
    }
    return true;
}

void EletwiseV3::CalcCommonKey()
{
    constexpr uint64_t common_tiling_key = 210000000;
    constexpr uint64_t db_tiling_key = 10000;
    tiling_key = compile_info.use_special_pattern ? common_tiling_key : 0;
    if (need_double_buffer) {
        tiling_key += db_tiling_key;
    }
}

bool EletwiseV3::DoCommonTiling()
{
    HCCL_DEBUG("opType[%s] Enter into elewise common shape tiling.", op_type.c_str());
    bool ret = true;
    CalcMultiCore();
    if (need_multi_core) {
        DoBlockTiling();
        if (block_factor > std::min(compile_info.max_available_ub, SPLIT_FACTORS.at(compile_info.max_dtype))) {
            need_double_buffer = true;
        }
        ret = ret && DoUbTiling();
    } else {
        num_blocks = 1;
        block_factor = out_shape;
        ub_factor = out_shape;
    }
    if (ret && !compile_info.only_const_tiling) {
        CalcCommonKey();
    }
    ret = WriteCommonData();
    return ret;
}

bool EletwiseV3::WriteCommonData() const
{
    HCCL_DEBUG("opType[%s] elewise tiling key is:%lld, num_blocks is:%lld, block_factor is:%lld, ub_factor is:%lld,"\
        "outshape:%lld", op_type.c_str(), tiling_key, num_blocks, block_factor, ub_factor,\
        static_cast<int32_t>(out_shape));

    run_info.numBlocks = static_cast<uint32_t>(num_blocks);
    constexpr uint32_t pure_elewise_var_size = 3;
    run_info.tilingKey = static_cast<uint32_t>(tiling_key);
    if (compile_info.elewise_vars_size == pure_elewise_var_size) {
        ByteBufferPut(run_info.tilingData, static_cast<int32_t>(out_shape));
    }
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(block_factor));
    ByteBufferPut(run_info.tilingData, static_cast<int32_t>(ub_factor));
    return true;
}

bool EletwiseV3::DoTiling()
{
    bool ret = CheckCompileInfo();
    ret = ret && GetOutShape();
    if (!ret) {
        HCCL_ERROR("opType[%s] elewise custom tiling input infos check failed.", op_type.c_str());
        return ret;
    }
    GetOutputDtype();

    // tbe原逻辑会判断is_const_shapes、out_shape为0，hccl不涉及
    ret = DoCommonTiling();
    if (!ret) {
        HCCL_ERROR("opType[%s] DoCommonTiling fail", op_type.c_str());
        return ret;
    }
    return ret;
}

void ElewiseCompileInfo::ParseFlagInfoSize(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_flag_info")) {
        const std::vector<bool> &input_flag_info = outer_compile_info.at("_flag_info").get<std::vector<bool>>();
        flag_info_size = input_flag_info.size();
        if (input_flag_info.size() > 0) {
            only_const_tiling = input_flag_info[0];
            constexpr uint32_t elewise_flag_size = 6;
            constexpr uint32_t const_shapes_index = 1;
            constexpr uint32_t special_pattern_index = 3;
            // broadcast scene flag info size is seven.
            if (flag_info_size >= elewise_flag_size) {
                is_const_shapes = input_flag_info[const_shapes_index];
                use_special_pattern = input_flag_info[special_pattern_index];
            }
        }
    }
}

void ElewiseCompileInfo::ParseBaseInfo(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_base_info") && !outer_compile_info.at("_base_info").empty()) {
        constexpr uint32_t base_info_size = 4;
        constexpr uint32_t max_ub_index = 2;
        constexpr uint32_t max_ub_db_index = 3;
        constexpr int64_t elewise_common_key = 1;
        const std::string known_str_key = "000";
        const std::string common_str_key = "100";
        const std::unordered_map<std::string, std::vector<int64_t>> &input_base_info =
            outer_compile_info.at("_base_info").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        if (input_base_info.count(common_str_key) && input_base_info.at(common_str_key).size() >= base_info_size) {
            pattern_key = elewise_common_key;
            core_num = input_base_info.at(common_str_key)[0];
            max_dtype = input_base_info.at(common_str_key)[1];
            max_available_ub = input_base_info.at(common_str_key)[max_ub_index];
            max_available_ub_db = input_base_info.at(common_str_key)[max_ub_db_index];
        }
    }
}


void ElewiseCompileInfo::ParseElewiseVarSize(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_elewise_vars") && !outer_compile_info.at("_elewise_vars").empty()) {
        const std::string tiling_key_str = "210000000";
        const std::string tiling_key_db_str = "210010000";
        const std::unordered_map<std::string, std::vector<int64_t>> &elewise_vars =
            outer_compile_info.at("_elewise_vars").get<std::unordered_map<std::string, std::vector<int64_t>>>();
        if (elewise_vars.count(tiling_key_str) != 0) {
            elewise_vars_size = elewise_vars.at(tiling_key_str).size();
        } else if (elewise_vars.count(tiling_key_db_str) != 0) {
            elewise_vars_size = elewise_vars.at(tiling_key_db_str).size();
        }
    }
}

void ElewiseCompileInfo::ParseUbFactorAlign(const nlohmann::json &outer_compile_info)
{
    if (outer_compile_info.contains("_ub_factor_align")) {
        ub_factor_align = outer_compile_info.at("_ub_factor_align").get<int64_t>();
    }
}

ElewiseCompileInfo::ElewiseCompileInfo(const std::string &op_type, const nlohmann::json &outer_compile_info)
{
    HCCL_DEBUG("opType[%s] elewise compile info parse running", op_type.c_str());
    ParseFlagInfoSize(outer_compile_info);
    ParseBaseInfo(outer_compile_info);
    ParseElewiseVarSize(outer_compile_info);
    ParseUbFactorAlign(outer_compile_info);
}
} // namespace v3

bool EletwiseTilingV3(const std::string &opType, const TeOpParas& opParas, const nlohmann::json &opTilingInfo,
    OpRunInfo &runInfo)
{
    // 解析tiling json
    v3::ElewiseCompileInfo elewiseCompileInfo(opType, opTilingInfo);
    v3::EletwiseV3 elewise(opType, opParas, elewiseCompileInfo, runInfo);
    bool ret = elewise.DoTiling();
    if (!ret) {
        HCCL_ERROR("[Tiling][Op]DoTiling failed");
    }
    return ret;
}


} // namespace TbeReduce
