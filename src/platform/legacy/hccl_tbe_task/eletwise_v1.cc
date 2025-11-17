/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "eletwise_v1.h"
#include <algorithm>
#include <unordered_map>
#include "vector_tiling.h"
#include "op_tiling.h"

namespace TbeReduce {
namespace {
// a block size in D
const int32_t BLOCK_SIZE_32 = 32;
const int32_t N_LAST_BROADCAST_THRESHOLD = 512;
constexpr int32_t TYPE_SIZE_32 = 4;
constexpr int32_t TYPE_SIZE_64 = 8;
std::unordered_map<int32_t, int32_t> split_factors {
    {1, 32767},
    {2, 32767},
    {4, 16383},
    {8, 8191},
};
std::unordered_map<int32_t, Pattern> special_pattern {
    {100, Pattern::COMMON},    {120, Pattern::COMMON_BROADCAST}, {121, Pattern::COMMON_BROADCAST_COMMON},
    {200, Pattern::BROADCAST}, {210, Pattern::BROADCAST_COMMON},
};
}

int32_t GetTypeSize(const std::string& dtype)
{
    int32_t typeSize = 2;
    if (dtype == "int8" || dtype == "uint8") {
        typeSize = 1;
    } else if (dtype == "float32" || dtype == "int32" || dtype == "uint32") {
        typeSize = TYPE_SIZE_32;
    } else if (dtype == "int64" || dtype == "uint64") {
        typeSize = TYPE_SIZE_64;
    } else if (dtype == "bool") {
        typeSize = 1;
    }
    return typeSize;
}

bool EletwiseV1::Init()
{
    CHECK((!opParas_.inputs.empty() && !opParas_.inputs[0].tensor.empty()), "op [%s] : input shape cannot be empty",
        opType_.c_str());
    inType_ = opParas_.inputs[0].tensor[0].dtype;
    CHECK((!opParas_.outputs.empty() && !opParas_.outputs[0].tensor.empty()), "op [%s] : output shape cannot be empty",
        opType_.c_str());
    outType_ = opParas_.outputs[0].tensor[0].dtype;
    CHECK((opInfo_.count("_is_support_broadcast") > 0), "op [%s] : compile info not contain [_is_support_broadcast]",
        opType_.c_str());
    compileInfo.isSupportBroadcast = opInfo_["_is_support_broadcast"].get<bool>();
    compileInfo.isSupportAbsorbableBroadcast = opInfo_["_is_support_absorbable_broadcast"].get<bool>();
    CHECK((opInfo_.count("_use_special_pattern") > 0), "op [%s] : compile info not contain [_use_special_pattern]",
        opType_.c_str());
    compileInfo.useSpecialPattern = opInfo_["_use_special_pattern"].get<bool>();
    CHECK((opInfo_.count("_is_const_shapes") > 0), "op [%s] : compile info not contain [_is_const_shapes]",
        opType_.c_str());
    isConst_ = opInfo_["_is_const_shapes"].get<bool>();
    if (!isConst_) {
        CHECK((opInfo_.count("_ub_size") > 0), "op [%s] : compile info not contain [_ub_size]", opType_.c_str());
        compileInfo.ubSize = opInfo_["_ub_size"].get<std::int32_t>();
        CHECK((opInfo_.count("_max_dtype_bytes") > 0), "op [%s] : compile info not contain [_max_dtype_bytes]",
            opType_.c_str());
        compileInfo.maxDtype = opInfo_["_max_dtype_bytes"].get<std::int32_t>();
        CHECK((opInfo_.count("_coexisting_quantity") > 0), "op [%s] : compile info not contain [_coexisting_quantity]",
            opType_.c_str());
        compileInfo.coexistingQuantity = opInfo_["_coexisting_quantity"].get<std::int32_t>();
        CHECK((opInfo_.count("_core_num") > 0), "op [%s] : compile info not contain [_core_num]", opType_.c_str());
        compileInfo.coreNum = opInfo_["_core_num"].get<std::int32_t>();
        CHECK((opInfo_.count("_fusion") > 0), "op [%s] : compile info not contain [_fusion]", opType_.c_str());
        compileInfo.fusionFlag = opInfo_["_fusion"].get<std::int32_t>();
    }
    needMultiCore_ = true;
    blockAxis_ = -1;
    ubaxis_ = -1;
    blockDims_ = 1;
    sPattern_ = Pattern::ORIGINAL;
    return true;
}

bool EletwiseV1::AddShapeInfo(const std::vector<int64_t>& inputShapeX, const std::vector<int64_t>& inputShapeY)
{
    CHECK_EQ(inputShapeX.size(), inputShapeY.size(),
             "op [%s] : the length of the input shape is different from that of the output shape",
             opType_.c_str());
    int32_t baseX = 100;
    int32_t baseY = 101;
    int32_t base = 10;
    for (size_t i = 0; i < outputShape_.size(); i++) {
        varNames_[baseX + i * base] = inputShapeX[i];
        varNames_[baseY + i * base] = inputShapeY[i];
    }
    return true;
}

bool EletwiseV1::TrySwitchToPerfPattern(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY)
{
    if (compileInfo.fusionFlag == 0 || !compileInfo.useSpecialPattern) {
        return true;
    }
    CHECK_EQ(inputShapeX.size(), inputShapeY.size(), "op [%s] : input shapes not match, cannot fuse axis",
             opType_.c_str());
    CHECK((!inputShapeX.empty() && !inputShapeY.empty()), "op [%s] : input shapes cannot be empty",
          opType_.c_str());
    std::vector<int64_t> fusedShapeX{inputShapeX[0]};
    std::vector<int64_t> fusedShapeY{inputShapeY[0]};
    bool state = (inputShapeX[0] == inputShapeY[0]);
    GetFusedShape(fusedShapeX, fusedShapeY, inputShapeX, inputShapeY, state);
    uint32_t fusedShapeXSize = 3;
    if (fusedShapeX.size() > fusedShapeXSize) {
        return true;
    }
    int32_t patternKey = 0;
    int32_t base = 100;
    int32_t baseKey = 10;
    int32_t keyNum = 2;
    for (size_t i = 0; i < fusedShapeX.size(); i++) {
        if (fusedShapeX[i] == fusedShapeY[i]) {
            patternKey += base;
        } else {
            patternKey += (base * keyNum);
        }
        base /= baseKey;
    }
    if (special_pattern.find(patternKey) != special_pattern.end()) {
        sPattern_ = special_pattern[patternKey];
        if (sPattern_ == Pattern::BROADCAST && compileInfo.isSupportAbsorbableBroadcast) {
            specialScalarKey_ = fusedShapeX[0] == 1 ? static_cast<int32_t>(Pattern::COMMON_BROADCAST) :
                static_cast<int32_t>(Pattern::BROADCAST_COMMON);
        }
        inputShapeX = std::move(fusedShapeX);
        inputShapeY = std::move(fusedShapeY);
        BroadcastShapes(inputShapeX, inputShapeY);
    }
    return true;
}

void EletwiseV1::GetFusedShape(std::vector<int64_t>& fusedShapeX, std::vector<int64_t>& fusedShapeY,
    std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY, bool& state)
{
    size_t last = 0;
    for (size_t i = 1; i < inputShapeX.size(); i++) {
        if (inputShapeX[i] == 1 && inputShapeY[i] == 1) {
            continue;
        }
        if (state && (inputShapeX[i] == inputShapeY[i])) {
            fusedShapeX[fusedShapeX.size() - 1] *= inputShapeX[i];
            fusedShapeY[fusedShapeY.size() - 1] *= inputShapeY[i];
        } else if (((inputShapeX[i] == inputShapeX[last]) && inputShapeX[i] == 1) ||
                   ((inputShapeY[i] == inputShapeY[last]) && inputShapeY[i] == 1)) {
            fusedShapeX[fusedShapeX.size() - 1] *= inputShapeX[i];
            fusedShapeY[fusedShapeY.size() - 1] *= inputShapeY[i];
            state = (inputShapeX[i] == inputShapeY[i]);
        } else {
            fusedShapeX.push_back(inputShapeX[i]);
            fusedShapeY.push_back(inputShapeY[i]);
            state = (inputShapeX[i] == inputShapeY[i]);
        }
        last = i;
    }
}

bool EletwiseV1::GenerateOutputShape()
{
    bool ret = true;
    int32_t outputShapeNnm = 100;
    if (compileInfo.isSupportBroadcast) {
        ret = ret && GenerateBroadcastShape();
    } else {
        const std::vector<int64_t>& shapes = opParas_.outputs[0].tensor[0].shape;
        if (compileInfo.fusionFlag > 0 || shapes.size() == 1) {
            int64_t fusedOutput = std::accumulate(shapes.begin(), shapes.end(), 1, std::multiplies<int64_t>());
            outputShape_.push_back(fusedOutput);
            CHECK(!outputShape_.empty(), "op [%s] : output shape cannot be empty", opType_.c_str());
            varNames_[outputShapeNnm] = outputShape_[0];
            if (compileInfo.useSpecialPattern) {
                sPattern_ = Pattern::COMMON;
            }
        } else {
            int32_t base = 100;
            int32_t baseNum = 10;
            for (size_t j = 0; j < shapes.size(); j++) {
                varNames_[base + baseNum * j] = shapes[j];
            }
            outputShape_ = shapes;
        }
    }
    return ret;
}

bool EletwiseV1::GenerateBroadcastShape()
{
    uint32_t udoubleInputs = 2;
    bool verifyInput = opParas_.inputs.size() == udoubleInputs &&
                       !opParas_.inputs[0].tensor.empty() && !opParas_.inputs[1].tensor.empty();
    CHECK(verifyInput, "op [%s] : input shape cannot be empty", opType_.c_str());
    std::vector<int64_t> inputShapeX = opParas_.inputs[0].tensor[0].shape;
    std::vector<int64_t> inputShapeY = opParas_.inputs[1].tensor[0].shape;
    bool ret = BroadcastShapes(inputShapeX, inputShapeY);
    ret = ret && TrySwitchToPerfPattern(inputShapeX, inputShapeY);
    ret = ret && AddShapeInfo(inputShapeX, inputShapeY);
    int32_t doubleInputs = 2;
    if (compileInfo.fusionFlag == doubleInputs && sPattern_ == Pattern::ORIGINAL) {
        ret = ret && RefineShapesForBroadcast(inputShapeX, inputShapeY);
    }
    if (ret) {
        CHECK_EQ(inputShapeX.size(), inputShapeY.size(), "op [%s] : input shapes not match, cannot fuse axis",
                 opType_.c_str());
        int32_t inputLength = inputShapeX.size();
        for (int32_t i = inputLength - 1; i >= 0; i--) {
            if ((inputShapeX[i] == 1 && inputShapeY[i] != 1) || (inputShapeY[i] == 1 && inputShapeX[i] != 1)) {
                broadcastAixs_ = i;
                break;
            }
        }
    }
    int32_t baseZ = 102;
    int32_t base = 10;
    for (size_t i = 0; i < outputShape_.size(); i++) {
        varNames_[baseZ + base * i] = outputShape_[i];
    }
    return ret;
}

bool EletwiseV1::BroadcastShapes(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY)
{
    size_t shapeLenX = inputShapeX.size();
    size_t shapeLenY = inputShapeY.size();
    size_t maxLen = shapeLenX > shapeLenY ? shapeLenX : shapeLenY;

    std::vector<int64_t> output(maxLen, 1);
    if (shapeLenX < maxLen) {
        inputShapeX.insert(inputShapeX.begin(), output.begin(), output.begin() + (maxLen - shapeLenX));
    } else {
        inputShapeY.insert(inputShapeY.begin(), output.begin(), output.begin() + (maxLen - shapeLenY));
    }

    for (size_t i = 0; i < maxLen; i++) {
        bool verifyBroadcast = inputShapeX[i] != inputShapeY[i] && inputShapeX[i] != 1 && inputShapeY[i] != 1;
        CHECK(!verifyBroadcast, "op [%s] : input shapes [%s] cannot BroadCast to shape [%s]", opType_.c_str(),
            std::to_string(inputShapeX[i]).c_str(), std::to_string(inputShapeY[i]).c_str());
        output[i] = inputShapeX[i] == 1 ? inputShapeY[i] : inputShapeX[i];
    }
    outputShape_ = std::move(output);
    return true;
}

bool EletwiseV1::RefineShapesForBroadcast(std::vector<int64_t>& inputShapeX, std::vector<int64_t>& inputShapeY)
{
    CHECK_EQ(inputShapeX.size(), inputShapeY.size(), "op [%s] : input shapes not match, cannot broadcast",
             opType_.c_str());
    CHECK((opInfo_.count("_fusion_index") > 0), "op [%s] : compile info not contain [_fusion_index]", opType_.c_str());
    compileInfo.fusionIndex = opInfo_["_fusion_index"].get<std::vector<std::vector<size_t>>>();
    size_t fusionLen = compileInfo.fusionIndex.size();
    std::vector<int64_t> fusedShapeX(fusionLen);
    std::vector<int64_t> fusedShapeY(fusionLen);
    for (size_t i = 0; i < fusionLen; i++) {
        int64_t fusedX = 1;
        int64_t fusedY = 1;
        for (size_t j = 0; j < compileInfo.fusionIndex[i].size(); j++) {
            fusedX *= inputShapeX[compileInfo.fusionIndex[i][j]];
            fusedY *= inputShapeY[compileInfo.fusionIndex[i][j]];
        }
        fusedShapeX[i] = fusedX;
        fusedShapeY[i] = fusedY;
    }
    inputShapeX = std::move(fusedShapeX);
    inputShapeY = std::move(fusedShapeY);
    bool ret = BroadcastShapes(inputShapeX, inputShapeY);
    return ret;
}

bool EletwiseV1::CalcTiling()
{
    maxAvailableUb_ =
        (((compileInfo.ubSize / compileInfo.coexistingQuantity) / BLOCK_SIZE_32) * BLOCK_SIZE_32) /
        compileInfo.maxDtype;
    int64_t outputSize = std::accumulate(outputShape_.begin(), outputShape_.end(), 1, std::multiplies<int64_t>());
    CHECK_LE(outputSize, INT32_MAX, "op [%s] : The output shape is too large", opType_.c_str());
    const int64_t multiCoreThreshold = 1024;
    if (outputSize < multiCoreThreshold) {
        needMultiCore_ = false;
    }
    return true;
}

bool EletwiseV1::DoBlockTiling()
{
    int32_t curCore = compileInfo.coreNum;
    size_t len = outputShape_.size();
    int32_t blockFactorKey = 200;
    int32_t eleInBlock = BLOCK_SIZE_32 / GetTypeSize(outType_);
    if (len == 1) {
        blockAxis_ = 0;
        CHECK((outputShape_.size() > 0), "op [%s] : output shape cannot be empty", opType_.c_str())
        int32_t blockFactor = static_cast<int32_t>(std::ceil(outputShape_[0] * 1.0 / curCore));
        blockFactor = static_cast<int32_t>(std::ceil(blockFactor * 1.0 / eleInBlock) * eleInBlock);
        blockDims_ = static_cast<int32_t>(std::ceil(outputShape_[0] * 1.0 / blockFactor));
        varNames_[blockFactorKey] = blockFactor;
        outputShape_[0] = blockFactor;
    } else {
        for (size_t i = 0; i < outputShape_.size(); i++) {
            if (outputShape_[i] > curCore) {
                blockAxis_ = i;
                int32_t blockFactor = static_cast<int32_t>(std::ceil(outputShape_[i] * 1.0 / curCore));
                blockDims_ *= static_cast<int32_t>(std::ceil(outputShape_[i] * 1.0 / blockFactor));
                varNames_[blockFactorKey + i] = blockFactor;
                outputShape_[i] = blockFactor;
                break;
            } else {
                curCore /= outputShape_[i];
                blockDims_ *= outputShape_[i];
                outputShape_[i] = 1;
            }
        }
    }
    return true;
}

bool EletwiseV1::DoUbTiling()
{
    int32_t limit = maxAvailableUb_;
    ubaxis_ = 0;
    ubFactor_ = static_cast<int32_t>(outputShape_[0]);
    limit = std::min(limit, split_factors[compileInfo.maxDtype]);
    if (limit < ubFactor_) {
        int32_t eleInBlock = BLOCK_SIZE_32 / GetTypeSize(outType_);
        int32_t ubForNum = static_cast<int32_t>(std::ceil(ubFactor_ * 1.0 / limit));
        int32_t adjustFacto = static_cast<int32_t>(std::ceil(ubFactor_ * 1.0 / ubForNum));
        int32_t alignFactor = static_cast<int32_t>(std::ceil(adjustFacto * 1.0 / eleInBlock));
        ubFactor_ = alignFactor * eleInBlock;
        if (ubFactor_ > limit) {
            ubFactor_ = static_cast<int32_t>(std::floor(adjustFacto * 1.0 / eleInBlock) * eleInBlock);
        }
    }
    int32_t ubFactorKey = 300;
    varNames_[ubFactorKey] = ubFactor_;
    return true;
}

void EletwiseV1::CalcKey()
{
    int32_t baseKey = 0;
    const int32_t patternOriginNum = 300000000;
    const int32_t baseNum = 100000;
    const int32_t patternNUM = 200000000;
    if (sPattern_ != Pattern::ORIGINAL) {
        if (sPattern_ == Pattern::BROADCAST && compileInfo.isSupportAbsorbableBroadcast) {
            baseKey = patternOriginNum + specialScalarKey_ * baseNum;
        } else {
            baseKey = patternNUM + static_cast<int32_t>(sPattern_) * baseNum;
        }
    }
    if (outputShape_.size() == 1) {
        key_ = baseKey;
    } else {
        if (ubaxis_ == -1 && blockAxis_ == -1) {
            key_ = baseKey;
        } else {
            key_ = baseKey + blockAxis_ * outputShape_.size() + ubaxis_ + 1;
        }
    }
}

bool EletwiseV1::WriteTilingData(OpRunInfo& runInfo)
{
    runInfo.blockDim = blockDims_;
    ByteBufferPut(runInfo.tilingData, key_);
    if (!isConst_) {
        CHECK((opInfo_.count("_vars") > 0), "op [%s] : compile info not contain [_vars]", opType_.c_str());
        const int32_t keyZeroLen = 7;
        const int32_t keyNotZeroLen = 8;
        int32_t keyLen = key_ == 0 ? keyZeroLen : keyNotZeroLen;
        const int32_t keysSize = 10;
        char keys[keysSize] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '\0'};
        while (key_ && keyLen >= 0) {
            keys[keyLen] = static_cast<char>('0') + key_ % keysSize;
            key_ /= keysSize;
            keyLen--;
        }
        std::string strKey = keys + keyLen + 1;
        const auto& allVars = opInfo_["_elewise_vars"][strKey];
        for (const auto& var : allVars) {
            CHECK((varNames_.count(var) > 0), "op [%s] : Compile info error", opType_.c_str())
            ByteBufferPut(runInfo.tilingData, varNames_[var]);
        }
    }
    return true;
}

bool EletwiseV1::DoTiling()
{
    bool ret = true;
    ret = ret && Init();
    ret = ret && GenerateOutputShape();
    ret = ret && CalcTiling();
    if (needMultiCore_) {
    // cut block
        ret = ret && DoBlockTiling();
        ret = ret && DoUbTiling();
    } else {
        if (outputShape_.size() == 1) {
            int32_t outputShapeKey = 200;
            varNames_[outputShapeKey] = outputShape_[0];
            ubFactor_ = outputShape_[0];
            int32_t ubFactorKey = 300;
            varNames_[ubFactorKey] = ubFactor_;
            blockAxis_ = 0;
            ubaxis_ = 0;
        }
    }
    if (ret) {
        CalcKey();
    }
    return ret;
}

bool EletwiseTilingV1(const std::string& opType_, const TeOpParas& opParas_, const nlohmann::json& opInfo_,
    OpRunInfo& runInfo)
{
    EletwiseV1 eletwise_v1(opType_, opParas_, opInfo_);
    bool ret = eletwise_v1.DoTiling();
    ret = ret && eletwise_v1.WriteTilingData(runInfo);
    return ret;
}
}  // namespace TbeReduce
