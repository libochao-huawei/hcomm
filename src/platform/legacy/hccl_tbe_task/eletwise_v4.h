/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/* !
 * \file elewise_v4.h
 * \brief
 */
#ifndef OPS_BUILT_IN_OP_TILING_ELEWISE_V4_H
#define OPS_BUILT_IN_OP_TILING_ELEWISE_V4_H

#include <vector>
#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "vector_tiling_rt2.h"
#include "vector_tiling_handle.h"
#include "op_tiling.h"
#include "fusion.h"

namespace TbeReduce {
namespace v4 {
constexpr size_t ELEWISE_MAX_INPUT_NUM = 320;
constexpr size_t ELEWISE_PATTERN_INDEX_NUM = 7;
constexpr int HCCL_BASE_DECIMAL = 10; // 10进制字符串转换

struct ElewiseCompileInfo : AutoTilingCompileInfo {
    ElewiseCompileInfo() = default;
    ElewiseCompileInfo(const std::string &op_type, const nlohmann::json &outer_compile_info);
    ~ElewiseCompileInfo() override = default;

    bool Parse(const char *op_type, const nlohmann::json &json_compile_info);

    // required compile_info
    bool is_pure_elewise { true };
    bool enable_fractal_format { false };
    bool elewise_disable_fuse { false };
    std::vector<int64_t> inputs_ele_in_block {};
    int64_t max_out_dtype_num { 0 };
    int64_t ub_block_size { 32 };
    bool is_const_shapes { false };
    bool only_const_tiling { false };
    bool has_var_attrs { false };
    int64_t ub_factor_align { -1 };
    // optional compile_info
    std::array<int64_t, ELEWISE_PATTERN_INDEX_NUM> base_info_indexes;
    std::pair<bool, std::vector<std::vector<int64_t>>> base_info;
    std::pair<bool, std::vector<int64_t>> elewise_const_dims;
    std::pair<bool, std::unordered_map<size_t, std::vector<int64_t>>> elewise_vars;
    bool contains_need_pad_compute { false };
    std::pair<bool, std::vector<std::vector<size_t>>> elewise_fused_index;

private:
    // required compile info parser functions
    void ParseIsPureElewise(const nlohmann::json &outer_compile_info);
    void ParseEnableFractalFormat(const nlohmann::json &outer_compile_info);
    void ParseElewiseDisableFuse(const nlohmann::json &outer_compile_info);
    void ParseInputDtypeBytes(const nlohmann::json &outer_compile_info);
    void ParseIsConstShapes(const nlohmann::json &outer_compile_info);
    void ParseMaxOutDtypeNum(const nlohmann::json &outer_compile_info);
    void ParseUbblockSize(const nlohmann::json &outer_compile_info);
    void ParseOnlyConstTiling(const nlohmann::json &outer_compile_info);
    void ParseUbFactorAlign(const nlohmann::json &outer_compile_info);
    void ParseRequiredCompileInfo(const nlohmann::json &outer_compile_info);
    void ParseInt64Mode(const nlohmann::json &outer_compile_info);
    // optional compile info parser function
    bool ParseVarsAttr(const nlohmann::json &outer_compile_info);
    void ParseBaseInfo(const nlohmann::json &outer_compile_info);
    void ParseConstCompileInfo(const nlohmann::json &outer_compile_info);
    void ParseElewiseVar(const nlohmann::json &outer_compile_info);
    void ParseContainsPadCompute(const nlohmann::json &outer_compile_info);
    void ParseFusedIndex(const nlohmann::json &outer_compile_info);
    bool ParseOptionalCompileInfo(const nlohmann::json &outer_compile_info);
    HcclResult SalStrToLonglong(const std::string str, int base, s64 &val);
};

enum class ElewisePattern {
    CONST = 000,
    PURE_ELEWISE = 100,
    ONE_RANK = 200,
    BROADCAST_SCALAR = 230,
    SCALAR_BROADCAST = 320,
    FRACTAL_FORMAT = 111,
    DISABLE_FUSE = 444,
    UNKNOWN = 250
};

template <typename T> class Elewise {
public:
    explicit Elewise(T *_context, const OpInfoImpl *_op_info) : context(_context), op_info(_op_info) {}
    ~Elewise() = default;
    bool DoTiling();
    void SetElewisePattern(const ElewisePattern &pattern);
    void SetRuntimeCore(const int64_t &broadcast_runtime_core);

private:
    bool CheckCompileInfo();
    void GetMaxDimLen();
    bool CheckOriCValue();
    bool DisableAllFuse();
    bool GetInputsShapes();
    bool WriteKnownData();
    bool CalcConstKey();
    bool CalcPatternKey();
    bool ParseBaseInfo();
    void CalcMultiCore();
    void DoBlockTiling();
    bool DoUbTiling(const int64_t factor);
    bool AllFuseTiling();
    void CalcTilingKey();
    bool WriteTilingData();
    bool WriteTilingDataInt64();
    bool WriteTilingDataInt32();
    void DisableAllFuseBlockTiling();
    void AdjustUbTiling(const int64_t &under_ub_shape, const int64_t &limit);
    void CheckUpdateUbTiling();
    void DisableAllFuseUbTiling();
    bool RefineOutShape();
    bool DisableAllFuseTiling();

private:
    T *context;
    const OpInfoImpl *op_info { nullptr };
    const char *op_type { nullptr };
    const ElewiseCompileInfo *compile_info;
    // input infos
    size_t input_num { 0 };
    bool is_custom_tiling = false;
    bool all_shape_same = true;
    BitArray<ELEWISE_MAX_INPUT_NUM> input_fuse_shapes;
    // output infos
    OpShape ele_out_shape;
    int64_t out_shape_size { 1 };
    size_t max_dim_len = 0;
    std::vector<int64_t> disable_all_fuse_shape {};
    bool is_empty_tensor;
    // base infos
    int64_t core_num { -1 };
    int64_t max_dtype { -1 };
    int64_t max_available_ub { -1 };
    int64_t max_available_ub_db { -1 };
    // tiling infos
    bool need_multi_core { true };
    bool need_double_buffer { false };
    uint64_t tiling_key { 1 };
    int64_t num_blocks { 1 };
    int64_t multi_core_output { 1 };
    int64_t block_axis { 0 };
    int64_t ub_axis { 0 };
    int64_t block_factor { 1 };
    int64_t ub_factor { 1 };
    bool broadcast_dispatch { false };
    ElewisePattern classify_pattern { ElewisePattern::UNKNOWN };
    bool disable_all_fuse = false;
    int64_t ele_ori_c { -1 };
    int64_t runtime_core { -1 };
};

ElewisePattern GetDispatchPattern(const Fusion &fusion, size_t input_num, size_t dim_len, bool disable_optimization);

template class Elewise<AutoTilingContext>;
} // namespace v4
} // namespace TbeReduce
#endif // OPS_BUILT_IN_OP_TILING_ELEWISE_V4_H
