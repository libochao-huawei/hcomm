#ifndef CCU_ADDRESS_HPP
#define CCU_ADDRESS_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"
#include "ccu_data_utils.hpp"
 #include "ccu_data_api_impl.h"

class CcuVariable;

class CcuAddress final {
public:
    explicit CcuAddress() {}

    CcuAddress(const CcuAddress& other) {
        this->handle = other.handle;
    }

    void operator=(const CcuAddress& other) const{
        auto ret = CcuAddressAssignAddr(this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuAddress&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const{
        auto ret = CcuAddressAssignImm(this->handle, immediate);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
    void operator=(const CcuVariable &var) const{
        auto ret = CcuAddressAssignVar(this->handle, var.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
    void operator=(CcuArithmeticOperator<CcuAddress, CcuAddress> op) const {
        auto ret = CcuAddressAddAddrToAddr(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
    void operator=(CcuArithmeticOperator<CcuAddress, CcuVariable> op) const{
        auto ret = CcuAddressAddVarToAddr(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }
    void operator=(CcuArithmeticOperator<CcuVariable, CcuAddress> op) const{
        auto ret = CcuAddressAddVarToAddr(this->handle, op.rhs.handle, op.lhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    // addr + addr
    CcuArithmeticOperator<CcuAddress, CcuAddress> operator+(const CcuAddress &that) const {
        return CcuArithmeticOperator<CcuAddress, CcuAddress>(
            *this, that, CcuArithmeticOperatorType::ADDITION);
    }

    // addr + variable
    CcuArithmeticOperator<CcuAddress, CcuVariable> operator+(const CcuVariable &var) const{
        return CcuArithmeticOperator<CcuAddress, CcuVariable>(
            *this, var, CcuArithmeticOperatorType::ADDITION);
    }


    void operator+=(const CcuVariable &var) const{
        auto ret = CcuAddressAddAssignVar(this->handle, var.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    // // addr += addr
    void operator+=(const CcuAddress &other) const{
        auto ret = CcuAddressAddAddrToAddr(this->handle, this->handle, other.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    CcuAddressHandle handle{0};
};

// variable + addr（交换律）
inline CcuArithmeticOperator<CcuVariable, CcuAddress> operator+(const CcuVariable &var, const CcuAddress &addr) {
    return CcuArithmeticOperator<CcuVariable, CcuAddress>(var, addr, CcuArithmeticOperatorType::ADDITION);
}

template <> inline void CcuArithmeticOperator<CcuAddress, CcuAddress>::Check() const {}
template <> inline void CcuArithmeticOperator<CcuAddress, CcuVariable>::Check() const {}
template <> inline void CcuArithmeticOperator<CcuVariable, CcuAddress>::Check() const {}


#endif // CCU_ADDRESS_HPP
