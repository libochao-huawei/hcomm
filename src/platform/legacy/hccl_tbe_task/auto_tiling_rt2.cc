/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "auto_tiling_rt2.h"
#include <mutex>
#include "exe_graph/runtime/kernel_run_context.h"
#include "register/op_impl_registry.h"
#include "auto_tiling_register.h"
#include "vector_tiling_rt2.h"
#include "legacy_log.h"
namespace TbeReduce {
std::string GetStrPattern(const SchPattern pattern)
{
    if (pattern == SchPattern::ELETWISE) {
        return "ElemWise";
    } else if (pattern == SchPattern::SEGMENT) {
        return "Segment";
    } else {
        return "default";
    }
}

HcclResult SalParseInformation(nlohmann::json &parseInformation, const std::string &information)
{
    try {
        parseInformation = nlohmann::json::parse(information);
    } catch (...) {
        HCCL_ERROR("[Parse][Information] errNo[0x%016llx] load allocated resource to json fail. "\
            "please check json input!", HCOM_ERROR_CODE(HCCL_E_PARA));
        return HCCL_E_PARA;
    }
    return HCCL_SUCCESS;
}

SchPattern GetSchPattern(const std::string &pattern)
{
    if (pattern == "ElemWise") {
        return SchPattern::ELETWISE;
    } else if (pattern == "Segment") {
        return SchPattern::SEGMENT;
    } else {
        return SchPattern::DEFAULT;
    }
}

bool AutoTilingRun(SchPattern pattern, gert::TilingContext *context, const OpInfoImpl *opInfo)
{
    HCCL_DEBUG("unsorted_segment_sum: AutoTilingRun");
    auto finder = AutoTilingRegister::RegisterTiling();
    size_t index = PatternIndex(pattern);
    if (index >= PATTERN_SIZE || finder[index] == nullptr) {
        std::string nodeName = context->GetNodeName();
        HCCL_ERROR("%s Pattern %s is not supported by AutoTiling", nodeName.c_str(), GetStrPattern(pattern).c_str());
        return false;
    }
    if (!finder[index](context, opInfo)) {
        HCCL_ERROR("Autotiling func failed");
        return false;
    }
    HCCL_DEBUG("unsorted_segment_sum: AutoTilingRun end");
    return true;
}

AutoTilingCompileInfo *ParseAutoTilingRun(const char *opType, const nlohmann::json &jsonCompileInfo)
{
    HCCL_DEBUG("unsorted_segment_sum: ParseAutoTilingRun2");
    // Default parsing sequence, parse compile info directly to nlohmann::json object
    CHK_PRT_RET((jsonCompileInfo.find("_pattern") == jsonCompileInfo.end()),
        HCCL_ERROR("opType:%s, compile info not contain [_pattern]", opType), nullptr);
    CHK_PRT_RET(!jsonCompileInfo["_pattern"].is_string(),
        HCCL_ERROR("opType:%s, compile info[_pattern] not is string", opType), nullptr);
    const string &s_pattern = jsonCompileInfo["_pattern"];
    SchPattern pattern = GetSchPattern(s_pattern);
    auto finder = AutoTilingRegister::RegisterParser();
    size_t index = PatternIndex(pattern);
    if (index >= PATTERN_SIZE || finder[index] == nullptr) {
        HCCL_ERROR("Pattern %s is not supported by AutoTiling Compile Info Parser", s_pattern.c_str());
        return nullptr;
    }
    AutoTilingCompileInfo *compileInfo = finder[index](opType, jsonCompileInfo);
    if (compileInfo == nullptr) {
        HCCL_ERROR("Failed to parse %s pattern compilation information", s_pattern.c_str());
        return nullptr;
    }
    compileInfo->pattern = pattern;

    // set int64_mode
    compileInfo->int64_mode = false;
    if (jsonCompileInfo.find("_int64_mode") != jsonCompileInfo.end()) {
        HCCL_DEBUG("get _int64_mode from compile info. will set int64_mode");
        compileInfo->int64_mode = jsonCompileInfo["_int64_mode"].get<bool>();
    }
    HCCL_DEBUG("unsorted_segment_sum: ParseAutoTilingRun2 end");

    return compileInfo;
}

/*
 * @brief: tiling function of custom ops
 * @param [in] context: inputs/outputs/attrs of ops
 * @param [in] opInfo: inputs/in_type/attrs/compileInfo of ops
 * @return bool: success or not
 */
bool DoAutoTiling(gert::TilingContext *context, const OpInfo *opInfo)
{
    HCCL_INFO("Entering customized autoTiling.");
    auto opInfoImpl = OpInfoImplGetter::GetOpInfoImpl(opInfo);
    if (opInfo == nullptr || opInfoImpl == nullptr || opInfoImpl->GetCompileInfo() == nullptr) {
        HCCL_ERROR("Op info cannot be empty");
        return false;
    }
    auto pattern = opInfoImpl->GetCompileInfo()->pattern;
    return AutoTilingRun(pattern, context, opInfoImpl.get());
}

AutoTilingCompileInfo *ParseAutoTilingRun(const gert::TilingParseContext *context,
    const nlohmann::json &jsonCompileInfo)
{
    HCCL_DEBUG("unsorted_segment_sum: ParseAutoTilingRun1");
    const char *opType = context->GetNodeName();
    AutoTilingCompileInfo *compileInfo = ParseAutoTilingRun(opType, jsonCompileInfo);
    if (compileInfo == nullptr) {
        return nullptr;
    }
    compileInfo->soc_info.core_num = context->GetPlatformInfo()->GetCoreNum();
    if (compileInfo->soc_info.core_num <= 0) {
        HCCL_INFO("Could not obtain core num from GE, core num get is %d.", compileInfo->soc_info.core_num);
        return nullptr;
    }

    HCCL_INFO("Core num is %d.", compileInfo->soc_info.core_num);
    HCCL_DEBUG("unsorted_segment_sum: ParseAutoTilingRun1 end");

    return compileInfo;
}

/*
 * @brief: tiling parser function of custom ops
 * @param [in] opInfo: opInfo of ops
 * @param [in] jsonCompileInfo: compileInfo of ops
 * @return std::shared_ptr<AutoTilingCompileInfo>: AutoTilingCompileInfo pointer
 */
std::shared_ptr<AutoTilingCompileInfo> ParseAutoTiling(const gert::TilingParseContext *context,
    const nlohmann::json &jsonCompileInfo)
{
    HCCL_DEBUG("unsorted_segment_sum: ParseAutoTiling");
    return std::shared_ptr<AutoTilingCompileInfo>(ParseAutoTilingRun(context, jsonCompileInfo));
}

/*
 * @brief: tiling parser function of custom ops
 * @param [in] opInfo: opInfo of ops
 * @param [in] jsonCompileInfo: compileInfo of ops
 * @return std::shared_ptr<AutoTilingCompileInfo>: AutoTilingCompileInfo pointer
 */
std::shared_ptr<AutoTilingCompileInfo> ParseAutoTiling(const char *opType, const nlohmann::json &jsonCompileInfo)
{
    return std::shared_ptr<AutoTilingCompileInfo>(ParseAutoTilingRun(opType, jsonCompileInfo));
}

/*
 * @brief: tiling function of ops
 * @param [in] context: inputs/outputs/attrs of ops
 * @return bool: success or not
 */
ge::graphStatus AutoTiling(gert::TilingContext *context)
{
    HCCL_INFO("Entering AutoTiling.");
    auto *compileInfo = reinterpret_cast<const AutoTilingCompileInfo *>(context->GetCompileInfo());
    if (compileInfo == nullptr) {
        HCCL_ERROR("Autotiling compile info is null");
        return ge::GRAPH_FAILED;
    }
    if (!AutoTilingRun(compileInfo->pattern, context, nullptr)) {
        return ge::GRAPH_FAILED;
    }
    return ge::GRAPH_SUCCESS;
}

/*
 * @brief: tiling parser function of ops
 * @param [in] context: compile info of ops
 * @return bool: success or not
 */
ge::graphStatus AutoTilingParser(gert::TilingParseContext *context)
{
    HCCL_INFO("Entering AutoTiling parser.");
    auto json_str = context->GetCompiledJson();
    if (json_str == nullptr) {
        HCCL_ERROR("No json_str gotten from context.");
        return ge::GRAPH_FAILED;
    }
    nlohmann::json jsonCompileInfo;
    CHK_RET(SalParseInformation(jsonCompileInfo, json_str));
    AutoTilingCompileInfo *compileInfo = ParseAutoTilingRun(context, jsonCompileInfo);
    if (compileInfo == nullptr) {
        HCCL_ERROR("compileInfo is nullptr.");
        return ge::GRAPH_FAILED;
    }

    auto out = reinterpret_cast<gert::KernelContext *>(context)->GetOutput(0);
    auto deleter = [](void *obj) { delete static_cast<AutoTilingCompileInfo *>(obj); };
    out->Set(compileInfo, deleter);
    return ge::GRAPH_SUCCESS;
}

gert::AttrDataType GetGeAttrDataType(const std::string &attr_data_type)
{
    static const std::unordered_map<std::string, gert::AttrDataType> attr_data_type_map{ { "bool",
        gert::AttrDataType::kBool },
        { "str", gert::AttrDataType::kString },
        { "string", gert::AttrDataType::kString },
        { "int", gert::AttrDataType::kInt32 },
        { "int32", gert::AttrDataType::kInt32 },
        { "int64", gert::AttrDataType::kInt64 },
        { "uint", gert::AttrDataType::kUint32 },
        { "uint32", gert::AttrDataType::kUint32 },
        { "float", gert::AttrDataType::kFloat32 },
        { "float32", gert::AttrDataType::kFloat32 },
        { "float16", gert::AttrDataType::kFloat16 },
        { "list_bool", gert::AttrDataType::kListBool },
        { "list_str", gert::AttrDataType::kListString },
        { "list_string", gert::AttrDataType::kListString },
        { "list_int", gert::AttrDataType::kListInt32 },
        { "list_int32", gert::AttrDataType::kListInt32 },
        { "list_int64", gert::AttrDataType::kListInt64 },
        { "list_uint", gert::AttrDataType::kListUint32 },
        { "list_uint32", gert::AttrDataType::kListUint32 },
        { "list_float", gert::AttrDataType::kListFloat32 },
        { "list_float32", gert::AttrDataType::kListFloat32 },
        { "list_float16", gert::AttrDataType::kListFloat16 },
        { "list_list_int", gert::AttrDataType::kListListInt32 },
        { "list_list_int32", gert::AttrDataType::kListListInt32 },
        { "list_list_int64", gert::AttrDataType::kListListInt64 } };
    auto findit = attr_data_type_map.find(attr_data_type);
    if (findit == attr_data_type_map.end()) {
        return gert::AttrDataType::kTypeEnd;
    }
    return findit->second;
}

IMPL_OP(Allreduce).Tiling(AutoTiling).TilingParse<AutoTilingCompileInfo>(AutoTilingParser);
IMPL_OP(Reduce).Tiling(AutoTiling).TilingParse<AutoTilingCompileInfo>(AutoTilingParser);
IMPL_OP(ReduceScatter).Tiling(AutoTiling).TilingParse<AutoTilingCompileInfo>(AutoTilingParser);
} // namespace TbeReduce
