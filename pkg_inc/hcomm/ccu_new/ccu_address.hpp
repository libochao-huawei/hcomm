#ifndef CCU_ADDRESS_HPP
#define CCU_ADDRESS_HPP

#include <cstdint>
#include <type_traits>

#include "ccu_types.h"
#include "ccu_data_utils.hpp"

class CcuVariable;

class CcuAddress final {
public:
    explicit CcuAddress() {}

    CcuAddress(const CcuAddress& other) {
        this->handle = other.handle;
    }

    void operator=(const CcuAddress& other) const;

    void operator=(CcuAddress&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const;
    void operator=(const CcuVariable &var) const;
    void operator=(CcuArithmeticOperator<CcuAddress, CcuAddress> op) const;
    void operator=(CcuArithmeticOperator<CcuAddress, CcuVariable> op) const;
    void operator=(CcuArithmeticOperator<CcuVariable, CcuAddress> op) const;

    // addr + addr
    CcuArithmeticOperator<CcuAddress, CcuAddress> operator+(const CcuAddress &that) const {
        return CcuArithmeticOperator<CcuAddress, CcuAddress>(
            *this, that, CcuArithmeticOperatorType::ADDITION);
    }

    // addr + variable
    CcuArithmeticOperator<CcuAddress, CcuVariable> operator+(const CcuVariable &var) const;

    void operator+=(const CcuVariable &var) const;

    // // addr += addr
    void operator+=(const CcuAddress &other) const;

    CcuAddressHandle handle{0};
};

// variable + addr
inline CcuArithmeticOperator<CcuVariable, CcuAddress>
operator+(const CcuVariable &var, const CcuAddress &addr) {
    return CcuArithmeticOperator<CcuVariable, CcuAddress>(
        var, addr, CcuArithmeticOperatorType::ADDITION);
}

// addr + variable
inline CcuArithmeticOperator<CcuAddress, CcuVariable>
CcuAddress::operator+(const CcuVariable &var) const {
    return CcuArithmeticOperator<CcuAddress, CcuVariable>(
        *this, var, CcuArithmeticOperatorType::ADDITION);
}

template <> inline void CcuArithmeticOperator<CcuAddress, CcuAddress>::Check() const {}
template <> inline void CcuArithmeticOperator<CcuAddress, CcuVariable>::Check() const {}
template <> inline void CcuArithmeticOperator<CcuVariable, CcuAddress>::Check() const {}

static_assert(std::is_standard_layout<CcuAddress>::value,
    "CcuAddress must be standard layout for .so ABI stability");
static_assert(sizeof(CcuAddress) == sizeof(CcuAddressHandle),
    "CcuAddress layout changed - will break .so ABI!");

extern "C" CcuResult CcuAddressAssignImm(CcuAddress addr, uint64_t immediate);
extern "C" CcuResult CcuAddressAssignVar(CcuAddress addr, CcuVariable var);
extern "C" CcuResult CcuAddressAssignAddr(CcuAddress dst, CcuAddress src);
extern "C" CcuResult CcuAddressAddAddrToAddr(CcuAddress result, CcuAddress a, CcuAddress b);
extern "C" CcuResult CcuAddressAddVarToAddr(CcuAddress result, CcuAddress addr, CcuVariable var);
extern "C" CcuResult CcuAddressAddAssignVar(CcuAddress addr, CcuVariable var);

inline void CcuAddress::operator=(const CcuAddress& other) const {
    auto ret = CcuAddressAssignAddr(*this, other);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator=(uint64_t immediate) const {
    auto ret = CcuAddressAssignImm(*this, immediate);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator=(const CcuVariable &var) const {
    auto ret = CcuAddressAssignVar(*this, var);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator=(CcuArithmeticOperator<CcuAddress, CcuAddress> op) const {
    auto ret = CcuAddressAddAddrToAddr(*this, op.lhs, op.rhs);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator=(CcuArithmeticOperator<CcuAddress, CcuVariable> op) const {
    auto ret = CcuAddressAddVarToAddr(*this, op.lhs, op.rhs);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator=(CcuArithmeticOperator<CcuVariable, CcuAddress> op) const {
    auto ret = CcuAddressAddVarToAddr(*this, op.rhs, op.lhs);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

inline void CcuAddress::operator+=(const CcuVariable &var) const {
    auto ret = CcuAddressAddAssignVar(*this, var);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}
inline void CcuAddress::operator+=(const CcuAddress &other) const {
    auto ret = CcuAddressAddAddrToAddr(*this, *this, other);
    if (ret != CcuResult::CCU_SUCCESS) {
        throw "todo: failed";
    }
}

#endif // CCU_ADDRESS_HPP
