/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_ELETWISE_H
#define OPS_BUILT_IN_OP_TILING_ELETWISE_H

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "op_tiling.h"

namespace TbeReduce {
struct CompileInfo {
    int32_t ubSize = 0;
    int32_t maxDtype = 0;
    int32_t coexistingQuantity = 0;
    int32_t coreNum = 0;
    int32_t fusionFlag = 0;
    bool isSupportBroadcast = false;
    bool isSupportAbsorbableBroadcast = false;
    bool useSpecialPattern = false;
    std::vector<std::vector<size_t>> fusionIndex;
    CompileInfo() : ubSize(0), maxDtype(0), coexistingQuantity(0), coreNum(0), fusionFlag(0),
        isSupportBroadcast(true), isSupportAbsorbableBroadcast(true), useSpecialPattern(true)
    {
    }
};

enum class Pattern {
    ORIGINAL = 0,
    COMMON = 100,
    COMMON_BROADCAST = 120,
    COMMON_BROADCAST_COMMON = 121,
    BROADCAST = 200,
    BROADCAST_COMMON = 210
};

class EletwiseV1 {
public:
    explicit EletwiseV1(const std::string& opType, const TeOpParas& opParas, const nlohmann::json& opInfo)
        : opType_(opType), opParas_(opParas), opInfo_(opInfo) {
    }
    ~EletwiseV1()
    {
    }
    bool Init();
    bool GenerateOutputShape();
    bool GenerateBroadcastShape();
    bool TrySwitchToPerfPattern(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY);
    void GetFusedShape(std::vector<int64_t>& fusedShapeX, std::vector<int64_t>& fusedShapeY,
                       std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY, bool& state);
    bool BroadcastShapes(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY);
    bool RefineShapesForBroadcast(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY);
    bool CalcTiling();
    bool DoBlockTiling();
    bool DoUbTiling();
    void CalcKey();
    bool CalcConstKey();
    bool WriteTilingData(OpRunInfo& runInfo);
    bool DoTiling();

private:
    bool AddShapeInfo(const std::vector<int64_t>& inputShapeX, const std::vector<int64_t>& inputShapeY);

private:
    const std::string& opType_;
    const TeOpParas& opParas_;
    const nlohmann::json& opInfo_;
    bool isConst_{false};
    CompileInfo compileInfo;
    int32_t maxAvailableUb_{0};
    std::string inType_;
    std::string outType_;
    bool needMultiCore_{true};
    int32_t key_{0};
    int32_t blockAxis_{-1};
    int32_t ubaxis_{-1};
    int32_t numBlocks_{1};
    int32_t ubFactor_{1};
    int32_t specialScalarKey_{0};
    int32_t broadcastAixs_{-1};
    Pattern sPattern_{Pattern::ORIGINAL};
    std::unordered_map<int32_t, int32_t> varNames_;
    std::vector<int64_t> outputShape_;
};
}  // namespace TbeReduce
#endif  // OPS_BUILT_IN_OP_TILING_ELETWISE_H
