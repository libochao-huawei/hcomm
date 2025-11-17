/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "eletwise_v2.h"
#include <algorithm>
#include <unordered_map>
#include "vector_tiling.h"
namespace TbeReduce {
namespace {
    const size_t B_MAX_DIM_LEN = 16;
    const size_t B_MAX_INPUT_NUMS = 70;
    const int TYPE_SIZE_8 = 8;
    const int TYPE_SIZE_16 = 16;
    const int TYPE_SIZE_32 = 32;
    const int TYPE_SIZE_4 = 4;
    const int TYPE_SIZE_256 = 256;
    const int INDEX_ONE = 1;
    const int INDEX_TWO = 2;
    const int INDEX_THREE = 3;
    const std::unordered_map<int64_t, int64_t> SPLIT_FACTORS {
        {1, 32767},
        {2, 32767},
        {4, 16383},
        {8, 8191},
    };
}

const int64_t GetElementByType(const std::string& dtype)
{
    // element nums in one block, default, fp16, int16, uin16
    int64_t elementInBlock = TYPE_SIZE_16;
    if (dtype == "float32" || dtype == "int32" || dtype == "uint32") {
        // element nums in one block by b32
        elementInBlock = TYPE_SIZE_8;
    } else if (dtype == "int8" || dtype == "uint8" || dtype == "bool") {
        // element nums in one block by b8
        elementInBlock = TYPE_SIZE_32;
    } else if (dtype == "int64" || dtype == "uint64") {
        // element nums in one block by b64
        elementInBlock = TYPE_SIZE_4;
    } else if (dtype == "uint1") {
        // element nums in one block by uint1
        elementInBlock = TYPE_SIZE_256;
    }
    return elementInBlock;
}

bool EletwiseV2::Init()
{
    CHECK((!opParas_.inputs.empty() && !opParas_.inputs[0].tensor.empty()),
          "opType[%s] input shape cannot be empty", opType_.c_str());
    inType_ = opParas_.inputs[0].tensor[0].dtype;
    CHECK((!opParas_.outputs.empty() && !opParas_.outputs[0].tensor.empty()),
          "opType[%s] output shape cannot be empty", opType_.c_str());
    outType_ = opParas_.outputs[0].tensor[0].dtype;
    int64_t typeSize = GetElementByType(outType_);
    for (size_t i = 1; i < opParas_.outputs.size(); i++) {
        CHECK(!opParas_.outputs[i].tensor.empty(),
              "opType[%s] output shape cannot be empty", opType_.c_str());
        int64_t cur_type_size = GetElementByType(opParas_.outputs[i].tensor[0].dtype);
        if (cur_type_size > typeSize) {
            outType_ = opParas_.outputs[i].tensor[0].dtype;
            typeSize = cur_type_size;
        }
    }
    CHECK_GE(flagInfo_.size(), 1, "opType[%s] flag info error", opType_.c_str());
    onlyConstTiling_ = flagInfo_[0];
    if (!onlyConstTiling_) {
        const size_t specialPatternIndex = 3;
        CHECK_GT(flagInfo_.size(), specialPatternIndex,
                 "opType[%s] flag info has no _use_special_pattern", opType_.c_str());
        useSpecialPattern_ = flagInfo_[INDEX_THREE];
    }
    return true;
}

bool EletwiseV2::GenerateOutputShape()
{
    const std::vector<int64_t>& shapes = opParas_.outputs[0].tensor[0].shape;
    int64_t fusedOutput = std::accumulate(shapes.begin(), shapes.end(), 1LL, std::multiplies<int64_t>());
    CHECK_LE(fusedOutput, INT32_MAX,
             "opType[%s] The output shape is too large", opType_.c_str());
    CHECK_GT(fusedOutput, 0,
             "opType[%s] The output shape must be greater than 0", opType_.c_str());
    outputShape_ = {fusedOutput};
    return true;
}

bool EletwiseV2::CalcTiling()
{
    // "_base_info": ["_core_num", "_max_dtype", "_max_available_ub", "_max_available_ub_db"]
    std::string patternKey = "100";
    if (onlyConstTiling_ || !useSpecialPattern_) {
        patternKey = "000";
    }
    try {
        const auto& baseInfo = opInfo_.at("_base_info").at(patternKey);
        const size_t baseInfoSize = 4;
        CHECK_EQ(baseInfo.size(), baseInfoSize,
                 "opType[%s] base info must be _ub_size, _max_dtype, _coexisting_quantity", opType_.c_str());
        coreNum_ = baseInfo[0];
        maxDtype_ = baseInfo[INDEX_ONE];
        maxAvailableUb_ = baseInfo[INDEX_TWO];
        maxAvailableUbDb_ = baseInfo[INDEX_THREE];
    } catch (const std::exception &e) {
        HCCL_ERROR("get compile_info[_base_info] error. Error message: %s", e.what());
        return false;
    }
    CHECK_GT(outputShape_.size(), 0, "opType[%s] outputShape_ index out of range", opType_.c_str());
    const int64_t multiCoreThreshold = GetElementByType(outType_) * coreNum_ * DOUBLE_BUFFER_SIZE;
    if (outputShape_[0] < multiCoreThreshold) {
        needMultiCore_ = false;
    }
    return true;
}

bool EletwiseV2::DoBlockTiling()
{
    int64_t curCore = coreNum_;
    bool outsUint1 = opInfo_.at("_outs_uint1");
    int64_t eleInBlock = outsUint1 ? ELEWISE_UINT1_REPEATE_NUMS : ELEWISE_REPEATE_NUMS;
    blockAxis_ = 0;
    CHECK((!outputShape_.empty()), "opType[%s] output shape cannot be empty", opType_.c_str());
    CHECK_GT(coreNum_, 0, "opType[%s] baseInfo coreNum_ error, it is [%d]", opType_.c_str(), coreNum_);
    blockFactor_ = static_cast<int64_t>(std::ceil(outputShape_[0] * 1.0 / curCore));
    blockFactor_ = static_cast<int64_t>(std::ceil(blockFactor_ * 1.0 / eleInBlock) * eleInBlock);
    blockDims_ = static_cast<int64_t>(std::ceil(outputShape_[0] * 1.0 / blockFactor_));
    return true;
}


bool EletwiseV2::DoUbTiling()
{
    ubAxis_ = 0;
    ubFactor_ = blockFactor_;
    int64_t limit = std::min(maxAvailableUb_, static_cast<int64_t>(SPLIT_FACTORS.at(maxDtype_)));
    if (limit < ubFactor_) {
        bool outsUint1 = opInfo_.at("_outs_uint1");
        int64_t eleInBlock = outsUint1 ? ELEWISE_UINT1_REPEATE_NUMS : ELEWISE_REPEATE_NUMS;
        CHECK_GT(limit, 0, "opType[%s] ub limit error, it is [%d]", opType_.c_str(), limit);
        int64_t ubForNum = static_cast<int64_t>(std::ceil(ubFactor_ * 1.0 / limit));
        int64_t adjustFactor = static_cast<int64_t>(std::ceil(ubFactor_ * 1.0 / ubForNum));
        int64_t alignFactor = static_cast<int64_t>(std::ceil(adjustFactor * 1.0 / eleInBlock));
        ubFactor_ = alignFactor * eleInBlock;
        if (ubFactor_ > limit) {
            ubFactor_ = static_cast<int64_t>(std::floor(adjustFactor * 1.0 / eleInBlock) * eleInBlock);
        }
    }
    return true;
}

void EletwiseV2::CalcKey()
{
    // special pattern: COMMON
    const int64_t baseNum = 210000000;
    key_ = baseNum;
    if (!useSpecialPattern_) {
        key_ = 0;
    }
    int64_t doubleBufferKey = 10000;
    if (needDoubleBuffer_) {
        key_ += doubleBufferKey;
    }
}

bool EletwiseV2::WriteTilingData(OpRunInfo& runInfo) const
{
    runInfo.blockDim = static_cast<uint32_t>(blockDims_);
    if (onlyConstTiling_) {
        ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(needMultiCore_));
        ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(blockAxis_));
        ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(blockFactor_));
        ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(ubAxis_));
        ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(ubFactor_));
        if (needDoubleBuffer_) {
            ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(1));
        } else {
            ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(0));
        }
        return true;
    }
    runInfo.tilingKey = static_cast<int32_t>(key_);
    std::string strKey = "210000000";
    if (!useSpecialPattern_) {
        strKey = "0";
    }
    if (needDoubleBuffer_) {
        strKey = "210010000";
        if (!useSpecialPattern_) {
            strKey = "10000";
        }
    }
    try {
        const int64_t cutUbNum = 30000;
        const int64_t cutBlockNum = 20000;
        const int64_t baseNum = 100;
        const auto& allVars = opInfo_.at("_elewise_vars").at(strKey);
        for (const auto& var : allVars) {
            if (var >= cutUbNum) {
                CHECK_GE(ubAxis_, 0, "opType[%s] Not cut ub", opType_.c_str());
                ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(ubFactor_));
            } else if (var >= cutBlockNum) {
                CHECK_GE(blockAxis_, 0, "opType[%s] Not cut block", opType_.c_str());
                ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(blockFactor_));
            } else {
                int64_t var_value = var;
                var_value /= baseNum;
                size_t dimIndex = var_value % baseNum;
                CHECK_LT(dimIndex, outputShape_.size(),
                         "opType[%s] dimIndex out of range outputShape_ index", opType_.c_str());
                ByteBufferPut(runInfo.tilingData, static_cast<int32_t>(outputShape_[dimIndex]));
            }
        }
    } catch (const std::exception &e) {
        HCCL_ERROR("get compile_info[_elewise_vars] error. Error message: %s", e.what());
        return false;
    }
    return true;
}

bool EletwiseV2::DoTiling()
{
    bool ret = Init();
    ret = ret && GenerateOutputShape();
    ret = ret && CalcTiling();
    if (needMultiCore_) {
        // cut block
        ret = ret && DoBlockTiling();
        CHECK((SPLIT_FACTORS.find(maxDtype_) != SPLIT_FACTORS.end()),
              "opType[%s] baseInfo maxDtype_ not in SPLIT_FACTORS", opType_.c_str());
        if (ret && blockFactor_ > std::min(maxAvailableUb_, static_cast<int64_t>(SPLIT_FACTORS.at(maxDtype_)))) {
            needDoubleBuffer_ = true;
            maxAvailableUb_ = maxAvailableUbDb_;
        }
        ret = ret && DoUbTiling();
    } else {
        ubAxis_ = 0;
        ubFactor_ = outputShape_[0];
        blockAxis_ = 0;
        blockFactor_ = outputShape_[0];
    }
    if (ret && !onlyConstTiling_) {
        CalcKey();
    }
    return ret;
}

bool CompletedShapes(std::array<std::array<int64_t, B_MAX_DIM_LEN>, B_MAX_INPUT_NUMS>& inputShapes,
                     size_t& dimLen, bool& isPureElementwise, const std::string& opType, const TeOpParas& opParas)
{
    size_t input_num = opParas.inputs.size();
    CHECK_LE(input_num, B_MAX_INPUT_NUMS, "opType[%s] more than 70 input are not supported", opType.c_str());
    for (size_t i = 0; i < input_num; i++) {
        CHECK(!opParas.inputs[i].tensor.empty(), "opType[%s] input tensor cannot be empty", opType.c_str());
        inputShapes[i].fill(1LL);
        if (opParas.inputs[i].tensor[0].shape.size() > dimLen) {
            dimLen = opParas.inputs[i].tensor[0].shape.size();
        }
    }
    CHECK_LE(dimLen, B_MAX_DIM_LEN, "opType[%s] more than 16 dims are not supported", opType.c_str());
    for (size_t i = 0; i < input_num; i++) {
        size_t cur_dim_len = opParas.inputs[i].tensor[0].shape.size();
        size_t start_index = dimLen - cur_dim_len;
        for (size_t j = 0; j < cur_dim_len; j++) {
            inputShapes[i][start_index++] = opParas.inputs[i].tensor[0].shape[j];
        }
    }
    for (size_t i = 0; i < dimLen; i++) {
        int64_t max_output = inputShapes[0][i];
        for (size_t j = 1; j < input_num; j++) {
            bool verify_broadcast = inputShapes[j][i] != 1 && (inputShapes[j][i] != max_output && max_output != 1);
            CHECK((!verify_broadcast), "opType[%s] input shapes [%s] cannot BroadCast to shape [%s]", opType.c_str(),
                  std::to_string(inputShapes[j][i]).c_str(), std::to_string(max_output).c_str());
            if (inputShapes[j][i] != max_output) {
                isPureElementwise = false;
            }
            if (inputShapes[j][i] > max_output) {
                max_output = inputShapes[j][i];
            }
        }
    }
    return true;
}

bool MatchConstShape(const std::string& opType,
                     const nlohmann::json& opInfo,
                     const std::vector<int64_t>& constShapes,
                     size_t& keyIndex)
{
    try {
        const auto& compileConstShapes = opInfo.at("_const_shapes");
        for (size_t i = 0; i < compileConstShapes.size(); i++) {
            bool shapeEqual = true;
            CHECK_EQ(constShapes.size(), compileConstShapes[i].size(),
                     "opType[%s] input shape and const shape not match", opType.c_str());
            for (size_t j = 0; j < compileConstShapes[i].size(); j++) {
                if (constShapes[j] != compileConstShapes[i][j].get<int64_t>()) {
                    shapeEqual = false;
                }
            }
            if (shapeEqual) {
                keyIndex = i;
                break;
            }
        }
    } catch (const std::exception &e) {
        HCCL_ERROR("get compile_info[_const_shapes] error. Error message: %s", e.what());
        return false;
    }
    return true;
}

bool CalcConstKey(const std::string& opType, const TeOpParas& opParas,
                  const nlohmann::json& opInfo, const bool isSupportBroadcast,
                  int64_t& key, int64_t& blockDims)
{
    size_t key_index = 0;
    bool ret = true;
    if (isSupportBroadcast) {
        bool verify_input = opParas.inputs.size() == INDEX_TWO && !opParas.inputs[0].tensor.empty() && \
                            !opParas.inputs[1].tensor.empty();
        CHECK(verify_input, "opType[%s] input shape cannot be empty", opType.c_str());
        std::vector<int64_t> input_shape_x = opParas.inputs[0].tensor[0].shape;
        std::vector<int64_t> input_shape_y = opParas.inputs[1].tensor[0].shape;
        size_t shape_len_x = input_shape_x.size();
        size_t shape_len_y = input_shape_y.size();
        size_t max_len = shape_len_x > shape_len_y ? shape_len_x : shape_len_y;
        if (shape_len_x < max_len) {
            input_shape_x.insert(input_shape_x.begin(), max_len - shape_len_x, 1);
        } else if (shape_len_y < max_len) {
            input_shape_y.insert(input_shape_y.begin(), max_len - shape_len_y, 1);
        }
        CHECK_EQ(input_shape_x.size(), input_shape_y.size(),
                 "opType[%s] input shape must be same", opType.c_str());
        std::vector<int64_t> const_shapes(input_shape_x.size(), 0);
        for (size_t i = 0; i < input_shape_x.size(); i++) {
            const_shapes[i] = static_cast<uint64_t>(input_shape_x[i]) & static_cast<uint64_t>(input_shape_y[i]);
        }
        ret = MatchConstShape(opType, opInfo, const_shapes, key_index);
    }
    if (ret) {
        try {
            const int64_t baseNum = 100000000;
            const auto& constBlockDims = opInfo["_const_block_dims"];
            CHECK_GT(constBlockDims.size(), key_index,
                     "opType[%s] const_block_dims index out of range", opType.c_str());
            blockDims = constBlockDims[key_index].get<int64_t>();
            key = baseNum + key_index;
        } catch (const std::exception &e) {
            HCCL_ERROR("get compile_info[_const_block_dims] error. Error message: %s", e.what());
            return false;
        }
    }
    return ret;
}

bool IsEmptyTensor(const std::string& opType, const TeOpParas& opParas)
{
    bool hasZero = false;
    for (size_t i = 0; i < opParas.outputs.size(); i++) {
        int64_t output_size = std::accumulate(opParas.outputs[i].tensor[0].shape.begin(),
            opParas.outputs[i].tensor[0].shape.end(), 1LL, std::multiplies<int64_t>());
        if (output_size == 0) {
            hasZero = true;
        } else {
            CHECK(!hasZero, "opType[%s] multi-output only supports all 0 output", opType.c_str());
        }
    }
    return hasZero;
}

bool WriteConstTiling(const std::string& opType, OpRunInfo& runInfo, const int64_t& key, const int64_t& blockDims)
{
    runInfo.blockDim = static_cast<uint32_t>(blockDims);
    runInfo.tilingKey = static_cast<int32_t>(key);
    return true;
}

bool EletwiseTilingV2(const std::string& opType, const TeOpParas& opParas, const nlohmann::json& opInfo,
    OpRunInfo& runInfo)
{
    std::array<std::array<int64_t, B_MAX_DIM_LEN>, B_MAX_INPUT_NUMS> inputShapes{};
    std::vector<bool> flagInfo;
    try {
        flagInfo = opInfo.at("_flag_info").get<std::vector<bool>>();
    } catch (const std::exception &e) {
        HCCL_ERROR("get compile_info[_flag_info] error. Error message: %s", e.what());
        return false;
    }
    bool is_const = false;
    bool isSupportBroadcast = true;
    bool useSpecialPattern_ = true;
    if (flagInfo.size() > INDEX_TWO) {
        is_const = flagInfo[1];
        isSupportBroadcast = flagInfo[INDEX_TWO];
        useSpecialPattern_ = flagInfo[INDEX_THREE];
    }
    size_t dimLen = 0;
    bool isPureElementwise = true;
    bool ret = CompletedShapes(inputShapes, dimLen, isPureElementwise, opType, opParas);
    if (is_const) {
        int64_t key_{0};
        int64_t blockDims_{1};
        ret = ret && CalcConstKey(opType, opParas, opInfo, isSupportBroadcast, key_, blockDims_);
        ret = ret && WriteConstTiling(opType, runInfo, key_, blockDims_);
    } else if (IsEmptyTensor(opType, opParas)) {
        ret = ret && WriteConstTiling(opType, runInfo, INT32_MIN, 1);
    } else if ((isPureElementwise && !(isSupportBroadcast && !useSpecialPattern_)) || !isSupportBroadcast) {
        EletwiseV2 eletwise_v2(opType, opParas, opInfo, flagInfo);
        ret = ret && eletwise_v2.DoTiling();
        ret = ret && eletwise_v2.WriteTilingData(runInfo);
    } else {
        HCCL_ERROR("[Tiling][Op]tiling is not supported");
        return false;
    }
    return ret;
}
}  // namespace TbeReduce
