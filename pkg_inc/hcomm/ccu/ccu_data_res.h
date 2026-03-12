/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ccu representation base header file
 * Create: 2025-02-18
 */

#ifndef CCU_DATA_RES_H
#define CCU_DATA_RES_H

#include <cstdint>

#include "ccu_data_api_utils.h"

/**
 * @brief CCU return value definition
 */
typedef enum {
    CCU_SUCCESS = 0,               /**< success */
    CCU_PARA = 1,                /**< parameter error */
    CCU_E_PTR = 2,                 /**< empty pointer */
    CCU_E_MEMORY = 3,              /**< memory error */
    CCU_E_INTERNAL = 4,            /**< internal error */
    CCU_E_NOT_SUPPORT = 5,         /**< not support feature */
    CCU_E_NOT_FOUND = 6,           /**< not found specific resource */
    CCU_E_UNAVAIL = 7,             /**< resource unavailable */
    CCU_E_SYSCALL = 8,             /**< call system interface error */
    CCU_E_TIMEOUT = 9,             /**< timeout */
    CCU_E_OPEN_FILE_FAILURE = 10,  /**< open file fail */
    CCU_E_TCP_CONNECT = 11,        /**< tcp connect fail */
    CCU_E_ROCE_CONNECT = 12,       /**< roce connect fail */
    CCU_E_TCP_TRANSFER = 13,       /**< tcp transfer fail */
    CCU_E_ROCE_TRANSFER = 14,      /**< roce transfer fail */
    CCU_E_RUNTIME = 15,            /**< call runtime api fail */
    CCU_E_DRV = 16,                /**< call driver api fail */
    CCU_E_PROFILING = 17,          /**< call profiling api fail */
    CCU_E_CCE = 18,                /**< call cce api fail */
    CCU_E_NETWORK = 19,            /**< call network api fail */
    CCU_E_AGAIN = 20,              /**< try again */
    CCU_E_REMOTE = 21,             /**< error cqe */
    CCU_E_SUSPENDING = 22,         /**< error communicator suspending */
    CCU_E_OPRETRY_FAIL = 23,       /**< retry constraint */
    CCU_E_OOM = 24,                /**< out of memory */
    CCU_E_IN_STATUS = 1041,        /**< The error information is in the status. */

    CCU_E_CHANNEL_CTX_UNAVAIL,
    CCU_E_JETTY_CTX_UNAVAIL,
    CCU_E_WQEBB_UNAVAIL,
    CCU_E_MS_UNAVAIL,
    CCU_E_LOOP_UNAVAIL,
    CCU_E_CKE_UNAVAIL,
    CCU_E_XN_UNAVAIL,
    CCU_E_GSA_UNAVAIL,

    CCU_E_TRANSLATE,

    CCU_E_RESERVED                 /**< reserved */
} CcuResult;

// 拆分文件
template <typename lhsT, typename rhsT> class CcuOperator {
public:
    CcuOperator(lhsT lhs, rhsT rhs) : lhs(lhs), rhs(rhs)
    {
    }
    lhsT lhs;
    rhsT rhs;
};

enum class CcuArithmeticOperatorType { ADDITION, INVALID };

template <typename lhsT, typename rhsT> class CcuArithmeticOperator : public CcuOperator<lhsT, rhsT> {
public:
    CcuArithmeticOperator(lhsT lhs, rhsT rhs, CcuArithmeticOperatorType type)
        : CcuOperator<lhsT, rhsT>(lhs, rhs), type(type)
    {
        Check();
    }
    void Check() const
    {
        // Hccl::THROW<Hccl::CcuApiException>("Invalid Arithmetic Operator");
        throw "Invalid Arithmetic Operator";
    }

    CcuArithmeticOperatorType type{CcuArithmeticOperatorType::INVALID};
};

// 拆分文件

class CcuVariable {
public:
    explicit CcuVariable() {}

    CcuVariable(const CcuVariable& other) {
        this->handle = other.handle;
    }

    void operator=(CcuVariable&& other) {
        this->handle = other.handle;
    }

    void operator=(uint64_t immediate) const {
        auto ret = CcuRepVarAssign(this->handle, immediate);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    void operator=(CcuArithmeticOperator<CcuVariable, CcuVariable> op) const {
        auto ret = CcuRepVarAddVarToVar(this->handle, op.lhs.handle, op.rhs.handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "todo: failed";
        }
    }

    CcuArithmeticOperator<CcuVariable, CcuVariable> operator+(const CcuVariable &that) const {
        return CcuArithmeticOperator<CcuVariable, CcuVariable>(*this, that, CcuArithmeticOperatorType::ADDITION);
    }
private:
    CcuVarHandle handle{0};
};

template <> void CcuArithmeticOperator<CcuVariable, CcuVariable>::Check() const
{
    // 放开CcuVariable与CcuVariable的运算拦截
}

#endif // CCU_DATA_RES_H