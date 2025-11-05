/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_VECTOR_TILING_H
#define OPS_BUILT_IN_OP_TILING_VECTOR_TILING_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "elewise_memset.h"
#include "eletwise_v1.h"
#include "op_tiling.h"
#include "eletwise_v2.h"
#include "eletwise_v3.h"
#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_parse_context.h"
#include "auto_tiling_rt2.h"

namespace TbeReduce {

/*
 * @brief: tiling function of elementwise operator
 * @param [in] opType_: opType_ of the elementwise operator
 * @param [in] opParas_: inputs/outputs/atts of the elementwise operator
 * @param [in] opInfo_: compile time generated info of the elementwise operator
 * @param [out] runInfo: result data
 * @return bool: success or not
 */
bool EletwiseTilingV1(const std::string& opType_, const TeOpParas& opParas_, const nlohmann::json& opInfo_,
    OpRunInfo& runInfo);

/*
 * @brief: tiling function of reduce operator
 * @param [in] opType_: opType_ of the reduce operator
 * @param [in] opParas_: inputs/outputs/atts of the reduce operator
 * @param [in] opInfo_: compile time generated info of the reduce operator
 * @param [out] runInfo: result data
 * @return bool: success or not
 */
/* int8类型的算子与其他算子开发时间不同，与老的tiling逻辑不匹配，该函数为新的tiling接口 */
bool EletwiseTilingV2(const std::string& opType, const TeOpParas& opParas, const nlohmann::json& opInfo,
    OpRunInfo& runInfo);

/* 910* tbe的算子与其他算子开发时间不同，与老的tiling逻辑不匹配，该函数为新的tiling接口 */
bool EletwiseTilingV3(const std::string& opType, const TeOpParas& opParas, const nlohmann::json& opTilingInfo,
    OpRunInfo& runInfo);

bool MemSetTiling(const std::string& op_type, std::vector<int64_t>& sizes, const MemSetCompileInfo& compile_info,
    OpRunInfo& run_info);
ge::graphStatus AutoTiling(gert::TilingContext* context);
ge::graphStatus AutoTilingParser(gert::TilingParseContext *context);
}  // namespace TbeReduce
#endif  // OPS_BUILT_IN_OP_TILING_VECTOR_TILING_H
