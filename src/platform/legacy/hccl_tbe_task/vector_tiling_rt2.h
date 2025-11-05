/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef OPS_BUILT_IN_OP_TILING_VECTOR_TILING_RT2_H
#define OPS_BUILT_IN_OP_TILING_VECTOR_TILING_RT2_H


#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "vector_op_info.h"
#include "op_tiling.h"
#include "exe_graph/runtime/tiling_context.h"
#include "exe_graph/runtime/tiling_parse_context.h"

namespace TbeReduce {

inline std::unique_ptr<nlohmann::json> GetCompileInfoJson(gert::TilingParseContext *context)
{
    auto json_str = context->GetCompiledJson();
    if (json_str == nullptr) {
        HCCL_ERROR("json_str is null");
        return nullptr;
    }
    std::unique_ptr<nlohmann::json> parsed_object_cinfo = nullptr;
    EXECEPTION_CATCH((parsed_object_cinfo = std::make_unique<nlohmann::json>(nlohmann::json::parse(json_str))),
        return nullptr);

    return parsed_object_cinfo;
}

template <typename T> inline T *GetCompileInfoPtr(gert::TilingParseContext *context)
{
    return context->GetCompiledInfo<T>();
}
/*
 * transfer attr datatype from string to enum
 * param[in] attr_data_type: attr datatype
 * return AttrDataType:
 * enum AttrDataType
 */
gert::AttrDataType GetGeAttrDataType(const std::string &attr_data_type);

constexpr int32_t VAR_ATTR_MODE_CONSISTENT = 0;
constexpr int32_t VAR_ATTR_MODE_INDEPENDENT = 1;
struct VarAttr_rt2 {
    VarAttr_rt2(const std::string &_name, size_t _index, const std::string &_type, const std::string &_src_type,
        const int32_t &_length)
        : name(_name), index(_index), type(_type), src_type(_src_type), length(_length)
    {
        ge_var_src_type = GetGeAttrDataType(src_type);
        ge_var_type = GetGeAttrDataType(type);
    }
    std::string name;
    size_t index;
    std::string type;
    std::string src_type;
    int32_t length { 0 };
    gert::AttrDataType ge_var_src_type;
    gert::AttrDataType ge_var_type;
};

class VarAttrWrap_rt2 {
public:
    bool ParseVarAttr(const nlohmann::json &json_info)
    {
        if (json_info.count("_var_attr_mode") <= 0) {
            return true;
        }

        is_empty = false;
        try {
            mode = json_info.at("_var_attr_mode").get<std::int32_t>();
            if (mode == VAR_ATTR_MODE_CONSISTENT) {
                const auto &json_var_attrs = json_info.at("_var_attrs");
                var_attrs.reserve(json_var_attrs.size());
                for (const auto &var : json_var_attrs) {
                    var_attrs.emplace_back(var.at("name"), var.at("index"), var.at("type"), var.at("src_type"),
                        var.at("length"));
                }
            } else {
                const auto &json_var_attr_map = json_info.at("_var_attrs");
                for (const auto &item : json_var_attr_map.items()) {
                    const std::uint64_t &tiling_key = std::stoull(item.key());
                    const auto &json_var_attrs = item.value();

                    const auto &ret = var_attr_map.emplace(std::piecewise_construct, std::forward_as_tuple(tiling_key),
                        std::forward_as_tuple());
                    auto &var_attrs_of_map = ret.first->second;
                    var_attrs_of_map.reserve(json_var_attrs.size());
                    for (const auto &var : json_var_attrs) {
                        var_attrs_of_map.emplace_back(var.at("name"), var.at("index"), var.at("type"), var.at("src_type"),
                            var.at("length"));
                    }
                }
            }
        } catch (const std::exception &e) {
            HCCL_ERROR("ParseVarAttr error. Error message: %s", e.what());
            return false;
        }
        return true;
    }
    bool WriteVarAttrs(const uint64_t tiling_key, gert::TilingContext &context) const
    {
        if (mode == VAR_ATTR_MODE_CONSISTENT) {
            return SetVarAttrs(var_attrs, context);
        }

        if (mode == VAR_ATTR_MODE_INDEPENDENT) {
            auto findit = var_attr_map.find(tiling_key);
            if (findit == var_attr_map.end()) {
                HCCL_DEBUG("op_type[%s], TilingKey of %d do not has var attrs.", context.GetNodeType(), tiling_key);
                return true;
            } else {
                return SetVarAttrs(findit->second, context);
            }
        }

        return true;
    }
    bool IsEmpty() const
    {
        return is_empty;
    }

private:
    bool SetVarAttrs(const std::vector<VarAttr_rt2> &var_attrs, gert::TilingContext &context) const
    {
        try {
            gert::TilingData *tiling_data = context.GetRawTilingData();
            for (VarAttr_rt2 var_attr : var_attrs) {
                ge::graphStatus graphStatus = tiling_data->AppendConvertedAttrVal(context.GetAttrs(), var_attr.index,
                    var_attr.ge_var_src_type, var_attr.ge_var_type);
                if (graphStatus != ge::GRAPH_SUCCESS) {
                    HCCL_ERROR(
                        "op_type:%s, getAttrVars from GE error. Error message: var index is %zu , var type is %s, ",
                        "var_src_type is %s", context.GetNodeType(), var_attr.index, var_attr.type.c_str(),
                        var_attr.src_type.c_str());
                    return false;
                }
            }
        } catch (const std::exception &e) {
            HCCL_ERROR("op_type:%s, SetAttrVars error. Error message: %s", context.GetNodeType(), e.what());
            return false;
        }
        return true;
    }

private:
    int32_t mode { -1 };
    std::vector<VarAttr_rt2> var_attrs;
    std::unordered_map<std::uint64_t, std::vector<VarAttr_rt2>> var_attr_map;
    bool is_empty { true };
};

struct SocInfo {
    int32_t core_num { -1 };
};

struct AutoTilingCompileInfo {
    SchPattern pattern;
    VarAttrWrap_rt2 var_attr_wrap;
    SocInfo soc_info;
    bool int64_mode { false };

public:
    AutoTilingCompileInfo() {}
    virtual ~AutoTilingCompileInfo() {}
};

class OpInfoImplGetter {
public:
    static std::shared_ptr<OpInfoImpl> GetOpInfoImpl(const OpInfo *obj)
    {
        if (obj == nullptr) {
            return nullptr;
        }
        return obj->op_info_impl;
    }
};
} // namespace TbeReduce

#endif // OPS_BUILT_IN_OP_TILING_VECTOR_TILING_RT2_H
