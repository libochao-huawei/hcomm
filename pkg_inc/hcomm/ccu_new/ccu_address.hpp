/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_ADDRESS_HPP
#define CCU_ADDRESS_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"
#include "ccu_data_utils.hpp"
#include "ccu_data_api_impl.h"
#include "ccu_variable.hpp"

namespace ccu {

class LocalAddr;
class RemoteAddr;
template <typename U> class Array;

class Address final {
public:
    Address() {
        auto ret = CcuAddressAlloc(&this->handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "CcuAddressAlloc: failed";
        }
    }

    Address(const Address& other) {
        this->handle = other.handle;
    }

    Address(Address&& other) noexcept {
        this->handle = other.handle;
    }

    void operator=(const Address& other) const {
        auto ret = CcuAddressAssignAddr(this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(Address&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const {
        auto ret = CcuAddressAssignImm(this->handle, immediate);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(const Variable &var) const {
        auto ret = CcuAddressAssignVar(this->handle, var.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<Address, Address> op) const {
        auto ret = CcuAddressAddAddrToAddr(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<Address, Variable> op) const {
        auto ret = CcuAddressAddVarToAddr(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<Variable, Address> op) const {
        auto ret = CcuAddressAddVarToAddr(this->handle, op.rhs.handle, op.lhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    // addr + addr
    CcuArithmeticOperator<Address, Address> operator+(const Address &that) const {
        return CcuArithmeticOperator<Address, Address>(*this, that, CcuArithmeticOperatorType::ADDITION);
    }

    // addr + variable
    CcuArithmeticOperator<Address, Variable> operator+(const Variable &var) const {
        return CcuArithmeticOperator<Address, Variable>(*this, var, CcuArithmeticOperatorType::ADDITION);
    }

    void operator+=(const Variable &var) const {
        auto ret = CcuAddressAddAssignVar(this->handle, var.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    // addr += addr
    void operator+=(const Address &other) const {
        auto ret = CcuAddressAddAddrToAddr(this->handle, this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    CcuAddressHandle handle{0};

private:
    explicit Address(NoAllocTag) {}
    template <typename U> friend class Array;
    friend class LocalAddr;
    friend class RemoteAddr;
};

// variable + addr（交换律）
inline CcuArithmeticOperator<Variable, Address> operator+(const Variable &var, const Address &addr) {
    return CcuArithmeticOperator<Variable, Address>(var, addr, CcuArithmeticOperatorType::ADDITION);
}

} // namespace ccu

template <> inline void CcuArithmeticOperator<ccu::Address, ccu::Address>::Check() const {}
template <> inline void CcuArithmeticOperator<ccu::Address, ccu::Variable>::Check() const {}
template <> inline void CcuArithmeticOperator<ccu::Variable, ccu::Address>::Check() const {}

#endif // CCU_ADDRESS_HPP
