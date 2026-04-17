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
#include "ccu_if_label_stack.hpp"

class CcuVariable;

struct CcuCondExpr {
    CcuVariable *var;
    uint64_t imm;
    CcuConditionType cond;
};

class CcuVariable final {
public:
    explicit CcuVariable() {}

    CcuVariable(const CcuVariable& other) {
        this->handle = other.handle;
    }

    void operator=(const CcuVariable& other) const{
        auto ret = CcuVariableAssignVar(this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
    

    void operator=(CcuVariable&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const{
        auto ret = CcuVariableAssignImm(this->handle, immediate);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<CcuVariable, CcuVariable> op) const{
        auto ret = CcuVariableAddVarToVar(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator+=(const CcuVariable &other) const{
        auto ret = CcuVariableAddVarToVar(this->handle, this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    CcuArithmeticOperator<CcuVariable, CcuVariable> operator+(const CcuVariable &that) const {
        return CcuArithmeticOperator<CcuVariable, CcuVariable>(*this, that, CcuArithmeticOperatorType::ADDITION);
    }

    CcuCondExpr operator==(uint64_t immediate) {
        return CcuCondExpr{this, immediate, CCU_CONDITION_EQ};
    }

    CcuCondExpr operator!=(uint64_t immediate) {
        return CcuCondExpr{this, immediate, CCU_CONDITION_NE};
    }

    CcuVariableHandle handle{0};
};

template <> inline void CcuArithmeticOperator<CcuVariable, CcuVariable>::Check() const
{
}

#endif // CCU_VARIABLE_HPP
