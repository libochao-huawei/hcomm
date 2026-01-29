/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_ELETWISEV2_H
#define OPS_BUILT_IN_OP_TILING_ELETWISEV2_H

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "op_tiling.h"
namespace TbeReduce {
/* int8类型的算子与其他算子开发时间不同，与老的tiling逻辑不匹配，该类提供了新的tiling逻辑 */
class EletwiseV2 {
public:
static const int64_t ELEWISE_REPEATE_NUMS = 128;
static const int64_t ELEWISE_UINT1_REPEATE_NUMS = 256;
static const int64_t DOUBLE_BUFFER_SIZE = 2;

public:
    explicit EletwiseV2(const std::string& opType, const TbeReduce::TeOpParas& opParas,
                          const nlohmann::json& opInfo, const std::vector<bool>& flagInfo)
        : opType_(opType), opParas_(opParas), opInfo_(opInfo), flagInfo_(flagInfo)
    {
    }
        ~EletwiseV2()
    {
    }
    bool WriteTilingData(TbeReduce::OpRunInfo& runInfo) const;
    bool DoTiling();

private:
    bool Init();
    bool GenerateOutputShape();
    bool CalcTiling();
    bool DoBlockTiling();
    bool DoUbTiling();
    void CalcKey();

private:
    const std::string& opType_;
    const TbeReduce::TeOpParas& opParas_;
    const nlohmann::json& opInfo_;
    const std::vector<bool>& flagInfo_;
    std::vector<int64_t> outputShape_{};
    int64_t key_{-1};
    int64_t maxAvailableUb_{0};
    int64_t maxAvailableUbDb_{0};
    int64_t blockAxis_{-1};
    int64_t ubAxis_{-1};
    int64_t numBlocks_{1};
    int64_t ubFactor_{1};
    int64_t blockFactor_{1};
    int64_t maxDtype_{0};
    int64_t coreNum_{0};
    std::string inType_;
    std::string outType_;
    bool onlyConstTiling_{false};
    bool useSpecialPattern_{false};
    bool needMultiCore_{true};
    bool needDoubleBuffer_{false};
};
}  // namespace TbeReduce
#endif  // OPS_BUILT_IN_OP_TILING_ELETWISE_H