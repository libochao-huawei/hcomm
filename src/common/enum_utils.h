/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ENUM_UTILS_H
#define ENUM_UTILS_H

#include <iostream>
#include <string>
#include <map>
#include <type_traits>
#include "hcomm_res_defs.h"

namespace hccl {

const std::map<CommEngine, std::string> COMMENGINE_STATUS_STR_MAP {
    {CommEngine::COMM_ENGINE_RESERVED, "COMM_ENGINE_RESERVED"},
    {CommEngine::COMM_ENGINE_CPU, "COMM_ENGINE_CPU"},
    {CommEngine::COMM_ENGINE_CPU_TS, "COMM_ENGINE_CPU_TS"},
    {CommEngine::COMM_ENGINE_AICPU, "COMM_ENGINE_AICPU"},
    {CommEngine::COMM_ENGINE_AICPU_TS, "COMM_ENGINE_AICPU_TS"},
    {CommEngine::COMM_ENGINE_AIV, "COMM_ENGINE_AIV"},
    {CommEngine::COMM_ENGINE_CCU, "COMM_ENGINE_CCU"}
};

template <typename MapType>
std::string GetEnumToString(const MapType& mappingMap, typename MapType::key_type enumValue) {
    auto iter = mappingMap.find(enumValue);
    if (iter != mappingMap.end()) {
        return iter->second;
    } else {
        return  "Unknown"
    }
}
}

#endif