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
#include <map>
#include <chrono>
#include <memory>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "vector_op_info.h"
#include "op_tiling.h"

namespace TbeReduce {
class CompileInfoBase;
using CompileInfoPtr = std::shared_ptr<CompileInfoBase>;

class CompileInfoBase {
public:
    CompileInfoBase() {}
    virtual ~CompileInfoBase() {}
};

struct VarAttr {
    VarAttr(const std::string &_name, int32_t _index, const std::string &_type, const std::string &_src_type,
        const int32_t &_length)
        : name(_name), index(_index), type(_type), src_type(_src_type), length(_length)
    {}
    std::string name;
    int32_t index;
    std::string type;
    std::string src_type;
    int32_t length { 0 };
};


class VarAttrWrap {
public:
    bool ParseVarAttr(const nlohmann::json &json_info);

private:
    int32_t mode { -1 };
    std::vector<VarAttr> var_attrs;
    std::unordered_map<std::uint64_t, std::vector<VarAttr>> var_attr_map;
};
} // namespace optiling

#endif // OPS_BUILT_IN_OP_TILING_VECTOR_TILING_H_
