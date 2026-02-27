/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_CCU_TYPES_H
#define HCOMM_CCU_TYPES_H

#include <cstdint>

namespace hcomm {

constexpr uint8_t CCU_MAX_IODIE_NUM = 2;

namespace CcuRep {

enum class CcuRepType {
    BASE,
    BLOCK,
    NOP,
    LOAD,
    STORE,
    LOAD_ARG,
    LOAD_VAR,
    ASSIGN,
    ADD,
    SET_LOOP,
    JUMP,
    JUMP_NE,
    JUMP_EQ,
    JUMP_LABEL,
    FUNC_CALL,
    FUNC_BLOCK,
    LOOP_CALL,
    LOOP,
    LOOPGROUP,
    LOOP_BLOCK,
    LOOPGROUP_BLOCK,
    LOC_RECORD_EVENT,
    LOC_WAIT_EVENT,
    LOC_WAIT_NOTIFY,
    REM_POST_SEM,
    REM_WAIT_SEM,
    REM_POST_VAR,
    REM_WAIT_GROUP,
    READ,
    WRITE,
    LOCAL_CPY,
    LOCAL_REDUCE,
    REM_MEM,
    BUF_READ,
    BUF_WRITE,
    BUF_LOC_READ,
    BUF_LOC_WRITE,
    BUF_REDUCE,
    RECORD_SHARED_NOTIFY,
};

enum class AssignSubType { INVALID, IMD_TO_VARIABLE, IMD_TO_ADDR, VAR_TO_ADDR, ADDR_TO_ADDR, VAR_TO_VAR };

enum class AddSubType {
    INVALID,
    ADDR_PLUS_VAR_TO_ADDR,
    ADDR_PLUS_ADDR_TO_ADDR,
    VAR_PLUS_VAR_TO_VAR,
    SELF_ADD_ADDRESS,
    SELF_ADD_VARIABLE
};

template <typename lhsT, typename rhsT>
class CcuOperator {
public:
    CcuOperator(lhsT lhs, rhsT rhs) : lhs(lhs), rhs(rhs) {}
    lhsT lhs;
    rhsT rhs;
};

enum class CcuArithmeticOperatorType { ADDITION, INVALID };

template <typename lhsT, typename rhsT>
class CcuArithmeticOperator : public CcuOperator<lhsT, rhsT> {
public:
    CcuArithmeticOperator(lhsT lhs, rhsT rhs, CcuArithmeticOperatorType type)
        : CcuOperator<lhsT, rhsT>(lhs, rhs), type(type) {}
    CcuArithmeticOperatorType type{CcuArithmeticOperatorType::INVALID};
};

enum class CcuRelationalOperatorType { EQUAL, NOT_EQUAL, INVALID };

template <typename lhsT, typename rhsT>
class CcuRelationalOperator : public CcuOperator<lhsT, rhsT> {
public:
    CcuRelationalOperator(lhsT lhs, rhsT rhs, CcuRelationalOperatorType type)
        : CcuOperator<lhsT, rhsT>(lhs, rhs), type(type) {}
    CcuRelationalOperatorType type{CcuRelationalOperatorType::INVALID};
};

enum class CcuArgType {
    VARIABLE,
    MEMORY,
    VARIABLE_LIST,
    MEMORY_LIST,
    LOCAL_ADDR,
    LOCAL_ADDR_LIST,
    REMOTE_ADDR,
    REMOTE_ADDR_LIST,
};

} // namespace CcuRep

constexpr uint32_t CCU_SQE_ARGS_LEN = 13;

struct CcuTaskParam {
    uint8_t dieId;
    uint8_t missionId;
    uint16_t timeout;
    uint32_t instStartId;
    uint32_t instCnt;
    uint32_t key;
    uint32_t argSize;
    uint64_t args[CCU_SQE_ARGS_LEN];
};

class CcuTaskArg {
public:
    explicit CcuTaskArg() = default;
    virtual ~CcuTaskArg() = default;
};

} // namespace hcomm

#endif // HCOMM_CCU_TYPES_H
