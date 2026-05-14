/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_VARIABLE_HPP
#define CCU_VARIABLE_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"
#include "ccu_data_utils.hpp"
#include "ccu_data_api_impl.h"

namespace ccu {

class Variable;
class LocalAddr;
class RemoteAddr;
template <typename U> class Array;
Variable GetResByChannel(ChannelHandle channel, uint32_t varIndex);

struct CondExpr {
    Variable *var;
    uint64_t imm;
    CcuConditionType cond;
};

class Variable final {
public:
    Variable() {
        auto ret = CcuVariableAlloc(&this->handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "CcuVariableAlloc: failed";
        }
    }

    Variable(const Variable& other) {
        this->handle = other.handle;
    }

    Variable(Variable&& other) noexcept {
        this->handle = other.handle;
    }

    void operator=(const Variable& other) const {
        auto ret = CcuVariableAssignVar(this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(Variable&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const {
        auto ret = CcuVariableAssignImm(this->handle, immediate);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<Variable, Variable> op) const {
        auto ret = CcuVariableAddVarToVar(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator+=(const Variable &other) const {
        auto ret = CcuVariableAddVarToVar(this->handle, this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    CcuArithmeticOperator<Variable, Variable> operator+(const Variable &that) const {
        return CcuArithmeticOperator<Variable, Variable>(*this, that, CcuArithmeticOperatorType::ADDITION);
    }

    CondExpr operator==(uint64_t immediate) {
        return CondExpr{this, immediate, CCU_CONDITION_EQ};
    }

    CondExpr operator!=(uint64_t immediate) {
        return CondExpr{this, immediate, CCU_CONDITION_NE};
    }

    CcuVariableHandle handle{0};

private:
    explicit Variable(NoAllocTag) {}
    template <typename U> friend class Array;
    friend class LocalAddr;
    friend class RemoteAddr;
    friend Variable GetResByChannel(ChannelHandle channel, uint32_t varIndex);
};

} // namespace ccu

template <> inline void CcuArithmeticOperator<ccu::Variable, ccu::Variable>::Check() const
{
}

#endif // CCU_VARIABLE_HPP
