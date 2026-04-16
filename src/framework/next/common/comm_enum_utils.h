/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COMM_ENUM_UTILS_H
#define COMM_ENUM_UTILS_H

#include "hcomm_res_defs.h"
#include <vector>
#include <unordered_map>

/* 公共模块函数返回值定义，跟业务层同步 */
const std::unordered_map<CommProtocol, std::string> HCOM_COMM_PROTOCOL_STR_MAP = {
    {COMM_PROTOCOL_RESERVED, "RESERVED"},
    {COMM_PROTOCOL_HCCS, "HCCS"},
    {COMM_PROTOCOL_ROCE, "ROCE"},
    {COMM_PROTOCOL_PCIE, "PCIE"},
    {COMM_PROTOCOL_SIO, "SIO"},
    {COMM_PROTOCOL_UBC_CTP, "UBC_CTP"},
    {COMM_PROTOCOL_UBC_TP, "UBC_TP"},
    {COMM_PROTOCOL_UB_MEM, "UB_MEM"}
};

inline std::string GetCommProtocolEnumStr(CommProtocol protocol)
{
    auto iter = HCOM_COMM_PROTOCOL_STR_MAP.find(protocol);
    if (iter == HCOM_COMM_PROTOCOL_STR_MAP.end()) {
        return "CommProtocol(" + std::to_string(protocol) + ")";
    } else {
        return iter->second;
    }
}

const std::unordered_map<CommEngine, std::string> HCOM_COMM_ENGINE_STR_MAP = {
    {COMM_ENGINE_RESERVED, "RESERVED"},
    {COMM_ENGINE_CPU, "CPU"},
    {COMM_ENGINE_CPU_TS, "CPU_TS"},
    {COMM_ENGINE_AICPU, "AICPU"},
    {COMM_ENGINE_AICPU_TS, "AICPU_TS"},
    {COMM_ENGINE_AIV, "AIV"},
    {COMM_ENGINE_CCU, "CCU"}
};

inline std::string GetCommEngineEnumStr(CommEngine engine)
{
    auto iter = HCOM_COMM_ENGINE_STR_MAP.find(engine);
    if (iter == HCOM_COMM_ENGINE_STR_MAP.end()) {
        return "CommEngine(" + std::to_string(engine) + ")";
    } else {
        return iter->second;
    }
}

#endif