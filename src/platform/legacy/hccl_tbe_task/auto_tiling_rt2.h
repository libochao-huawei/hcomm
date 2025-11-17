/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_AUTO_TILING_RT2_H
#define OPS_BUILT_IN_OP_TILING_AUTO_TILING_RT2_H

#include <string>
#include <nlohmann/json.hpp>
#include "platform/platform_infos_def.h"
#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_parse_context.h"
#include "vector_op_info.h"
#include "op_tiling.h"

namespace TbeReduce {
/*
 * @brief: tiling parser function of custom ops
 * @param [in] opType: opType of ops
 * @param [in] jsonCompileInfo: compileInfo of ops
 * @return std::shared_ptr<AutoTilingCompileInfo>: AutoTilingCompileInfo pointer
 */
std::shared_ptr<TbeReduce::AutoTilingCompileInfo> ParseAutoTiling(const char *opType,
    const nlohmann::json &jsonCompileInfo);

/*
 * @brief: tiling parser function of custom ops
 * @param [in] opType: opType of ops
 * @param [in] jsonCompileInfo: compileInfo of ops
 * @return std::shared_ptr<AutoTilingCompileInfo>: AutoTilingCompileInfo pointer
 */
std::shared_ptr<TbeReduce::AutoTilingCompileInfo> ParseAutoTiling(const gert::TilingParseContext *context,
    const nlohmann::json &jsonCompileInfo);

/*
 * @brief: tiling function of custom ops
 * @param [in] context: inputs/outputs/attrs of ops
 * @param [in] opInfo: inputs/in_type/attrs/compileInfo of ops
 * @return bool: success or not
 */
bool DoAutoTiling(gert::TilingContext *context, const TbeReduce::OpInfo *opInfo);
}

#endif // OPS_BUILT_IN_OP_TILING_AUTO_TILING_RT2_H