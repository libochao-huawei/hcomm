/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef OPS_BUILT_IN_OP_TILING_AUTO_TILING_REGISTER_H
#define OPS_BUILT_IN_OP_TILING_AUTO_TILING_REGISTER_H

#include <vector>
#include "vector_tiling_rt2.h"
namespace TbeReduce {
using AutoTilingFunc = bool (*)(gert::TilingContext *, const TbeReduce::OpInfoImpl *);
using AutoTilingParseFunc = TbeReduce::AutoTilingCompileInfo *(*)(const char *opType,
    const nlohmann::json &jsonCompileInfo);

#define REGISTER_AUTO_TILING(pattern, tilingfunc, parsefunc) \
    static AutoTilingRegister g_auto_tiling_register_##__COUNTER__(pattern, tilingfunc, parsefunc)

constexpr size_t PATTERN_BASE = 0x10;
constexpr size_t PATTERN_SIZE = static_cast<size_t>(TbeReduce::SchPattern::DEFAULT) - PATTERN_BASE;

inline size_t PatternIndex(TbeReduce::SchPattern _pattern)
{
    return static_cast<size_t>(_pattern) - PATTERN_BASE;
}

class AutoTilingRegister {
public:
    AutoTilingRegister(TbeReduce::SchPattern _pattern, AutoTilingFunc _tiling_func, AutoTilingParseFunc _parser)
    {
        size_t index = PatternIndex(_pattern);
        auto &register_parser = RegisterParser();
        register_parser[index] = _parser;
        auto &register_tiling = RegisterTiling();
        register_tiling[index] = _tiling_func;
    };
    ~AutoTilingRegister() = default;
    static std::array<AutoTilingParseFunc, PATTERN_SIZE> &RegisterParser()
    {
        HCCL_DEBUG("unsorted_segment_sum: RegisterParser");
        static std::array<AutoTilingParseFunc, PATTERN_SIZE> g_auto_tiling_parsers;
        return g_auto_tiling_parsers;
    }
    static std::array<AutoTilingFunc, PATTERN_SIZE> &RegisterTiling()
    {
        static std::array<AutoTilingFunc, PATTERN_SIZE> g_auto_tiling_funcs;
        return g_auto_tiling_funcs;
    }
};
}
#endif // OPS_BUILT_IN_OP_TILING_AUTO_TILING_REGISTER_H