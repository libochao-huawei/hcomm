/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_ENUM_FACTORY_H
#define HCCLV2_ENUM_FACTORY_H

#include "string_util.h"
#include "hcomm_res_defs.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>

#define MAKE_ENUM(enumClass, ...)                                                                                      \
    class enumClass {                                                                                                  \
    public:                                                                                                            \
        enum Value : uint8_t { __VA_ARGS__, __COUNT__, INVALID };                                                      \
                                                                                                                       \
        enumClass()                                                                                                    \
        {                                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        constexpr enumClass(Value v) : value(v)                                                                        \
        {                                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        constexpr operator Value() const                                                                               \
        {                                                                                                              \
            return value;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator==(enumClass a) const                                                                   \
        {                                                                                                              \
            return value == a.value;                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator!=(enumClass a) const                                                                   \
        {                                                                                                              \
            return value != a.value;                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator<(enumClass a) const                                                                    \
        {                                                                                                              \
            return value < a.value;                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator==(Value v) const                                                                       \
        {                                                                                                              \
            return value == v;                                                                                         \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator!=(Value v) const                                                                       \
        {                                                                                                              \
            return value != v;                                                                                         \
        }                                                                                                              \
                                                                                                                       \
        constexpr bool operator<(Value v) const                                                                        \
        {                                                                                                              \
            return value < v;                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        std::string Describe() const                                                                                   \
        {                                                                                                              \
            static std::vector<std::string> m = InitStrs();                                                            \
            if (value >= m.size())                                                                                      \
                return std::string(#enumClass) + "::Invalid";                                                          \
            return m[value];                                                                                           \
        }                                                                                                              \
                                                                                                                       \
        friend std::ostream &operator<<(std::ostream &stream, const enumClass &v)                                      \
        {                                                                                                              \
            return stream << v.Describe();                                                                             \
        }                                                                                                              \
                                                                                                                       \
    private:                                                                                                           \
        Value value{INVALID};                                                                                          \
                                                                                                                       \
        static std::vector<std::string> InitStrs()                                                                     \
        {                                                                                                              \
            std::vector<std::string> strings;                                                                          \
            std::string              s = #__VA_ARGS__;                                                                 \
            std::string              token;                                                                            \
            for (char c : s) {                                                                                         \
                if (c == ' ' || c == ',') {                                                                            \
                    if (!token.empty()) {                                                                              \
                        strings.push_back({std::string(#enumClass) + "::" + token});                                   \
                        token.clear();                                                                                 \
                    }                                                                                                  \
                } else {                                                                                               \
                    token += c;                                                                                        \
                }                                                                                                      \
            }                                                                                                          \
            if (!token.empty())                                                                                        \
                strings.push_back({std::string(#enumClass) + "::" + token});                                           \
            return strings;                                                                                            \
        }                                                                                                              \
    };

namespace std {
struct EnumClassHash {
    template <typename T> std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};
} // namespace std

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

std::string GetCommProtocolEnumStr(CommProtocol protocol)
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

std::string GetCommEngineEnumStr(CommEngine engine)
{
    auto iter = HCOM_COMM_ENGINE_STR_MAP.find(engine);
    if (iter == HCOM_COMM_ENGINE_STR_MAP.end()) {
        return "CommEngine(" + std::to_string(engine) + ")";
    } else {
        return iter->second;
    }
}

#endif // HCCLV2_ENUM_FACTORY_H
