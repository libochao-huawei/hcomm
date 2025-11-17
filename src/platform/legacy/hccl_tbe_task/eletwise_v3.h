/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_ELEWISE_V3_H
#define OPS_BUILT_IN_OP_TILING_ELEWISE_V3_H

#include <string>
#include <nlohmann/json.hpp>
#include "op_tiling.h"

namespace TbeReduce {
namespace v3 {

struct ElewiseCompileInfo {
    ElewiseCompileInfo() = default;
    ElewiseCompileInfo(const std::string &op_type, const nlohmann::json &outer_compile_info);

    // required compile_info
    uint32_t flag_info_size { 0 };
    bool only_const_tiling { false };
    bool is_const_shapes { false };
    bool use_special_pattern { true };
    int64_t ub_factor_align { -1 };
    // optional base_info
    int64_t pattern_key { -1 };
    int64_t core_num { -1 };
    int64_t max_dtype { -1 };
    int64_t max_available_ub { -1 };
    int64_t max_available_ub_db { -1 };
    // const_core_dims
    int64_t const_block_dims { -1 };
    // elewise_vars_size
    uint32_t elewise_vars_size { 0 };
    // tiling info from broadcast
    bool broadcast_pattern { false };

private:
    void ParseFlagInfoSize(const nlohmann::json &outer_compile_info);
    void ParseBaseInfo(const nlohmann::json &outer_compile_info);
    void ParseElewiseVarSize(const nlohmann::json &outer_compile_info);
    void ParseUbFactorAlign(const nlohmann::json &outer_compile_info);
};

class EletwiseV3 {
public:
    explicit EletwiseV3(const std::string &op_type, const TbeReduce::TeOpParas& op_paras,
        const ElewiseCompileInfo &compile_info, TbeReduce::OpRunInfo &run_info)
        : op_type(op_type), op_paras(op_paras), compile_info(compile_info), run_info(run_info)
    {}
    ~EletwiseV3() = default;
    bool DoTiling();

private:
    const int64_t GetElementByType(const std::string& dtype) const;
    bool CheckCompileInfo();
    bool GetOutShape();
    void GetOutputDtype();
    void CalcMultiCore();
    void DoBlockTiling();
    bool DoUbTiling();
    void CalcCommonKey();
    bool DoCommonTiling();
    bool WriteCommonData() const;

private:
    const std::string &op_type;
    const TbeReduce::TeOpParas &op_paras;
    const ElewiseCompileInfo &compile_info;
    TbeReduce::OpRunInfo &run_info;
    // tiling info
    bool need_multi_core { true };
    bool need_double_buffer { false };
    uint64_t tiling_key { 1 };
    int64_t block_dims { 1 };
    int64_t ub_factor { 1 };
    int64_t block_factor { 1 };
    int64_t out_shape { 1 };
    std::string out_dtype;
};
} // namespace v3
} // namespace TbeReduce
#endif // OPS_BUILT_IN_OP_TILING_ELEWISE_V3_H
