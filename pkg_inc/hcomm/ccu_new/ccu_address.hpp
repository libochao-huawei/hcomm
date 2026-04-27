#ifndef CCU_ADDRESS_HPP
#define CCU_ADDRESS_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"
#include "ccu_data_utils.hpp"
#include "ccu_data_api_impl.h"
#include "ccu_variable.hpp"

namespace ccu {

class Address final {
public:
    explicit Address() {}

    Address(const Address& other) {
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
