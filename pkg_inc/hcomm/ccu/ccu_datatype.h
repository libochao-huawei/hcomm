/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_CCU_DATATYPE_H
#define HCOMM_CCU_DATATYPE_H

#include <memory>
#include <vector>
#include "ccu_types.h"

namespace hcomm {
namespace CcuRep {

class CcuRepContext;
class Variable;
class Address;

class CcuPhyRes {
public:
    CcuPhyRes() = default;
    ~CcuPhyRes() = default;
    void Reset(uint16_t id);
    void SetDieId(uint16_t dieId);
    uint16_t Id() const;
    uint16_t DieId() const;

private:
    uint16_t dieId{0};
    uint16_t id{0};
};

class CcuVirRes {
public:
    explicit CcuVirRes(CcuRepContext *context);
    virtual ~CcuVirRes() = default;
    void Reset(uint16_t id);
    void Reset(uint16_t id, uint16_t dieId);
    void SetDieId(uint16_t dieId);
    virtual uint16_t Id() const;
    uint16_t DieId() const;

protected:
    std::shared_ptr<CcuPhyRes> phyRes{nullptr};
    CcuRepContext *context{nullptr};
};

class Variable : public CcuVirRes {
public:
    explicit Variable(CcuRepContext *context = nullptr);
    Variable(const Variable &other);
    void operator=(Variable &&other);

    void operator=(uint64_t immediate);
    void operator=(const Variable &other);
    void operator=(CcuArithmeticOperator<Variable, Variable> op);

    CcuArithmeticOperator<Variable, Variable> operator+(const Variable &varB) const;
    CcuArithmeticOperator<Variable, Address> operator+(const Address &addrB) const;

    void operator+=(const Variable &other);

    CcuRelationalOperator<Variable, uint64_t> operator!=(uint64_t immediate) const;
    CcuRelationalOperator<Variable, uint64_t> operator==(uint64_t immediate) const;
};

class Address : public CcuVirRes {
public:
    explicit Address(CcuRepContext *context = nullptr);
    Address(const Address &other);
    void operator=(Address &&other);

    void operator=(uint64_t immediate);
    void operator=(const Address &other);
    void operator=(const Variable &other);

    void operator=(CcuArithmeticOperator<Variable, Address> op);
    void operator=(CcuArithmeticOperator<Address, Variable> op);
    void operator=(CcuArithmeticOperator<Address, Address> op);

    CcuArithmeticOperator<Variable, Address> operator+(const Variable &varB) const;
    CcuArithmeticOperator<Address, Address> operator+(const Address &addrB) const;

    void operator+=(const Variable &other);
};

class LocalAddr {
public:
    LocalAddr() = default;
    LocalAddr(Address addr, Variable token) : addr(addr), token(token) {}
    Address addr;
    Variable token;
};

class RemoteAddr {
public:
    RemoteAddr() = default;
    RemoteAddr(Address addr, Variable token) : addr(addr), token(token) {}
    Address addr;
    Variable token;
};

class LocalNotify : public CcuVirRes {
public:
    explicit LocalNotify(CcuRepContext *context = nullptr);
};

class CompletedEvent : public CcuVirRes {
public:
    explicit CompletedEvent(CcuRepContext *context = nullptr);
    void SetMask(uint32_t compeletedMask);
    uint32_t mask{1};
};

class CcuBuf : public CcuVirRes {
public:
    explicit CcuBuf(CcuRepContext *context = nullptr);
    uint16_t Id() const override;
    static constexpr uint16_t CCUBUFFER_DIE_ID_BIT = 0x8000;
};

class Executor : public CcuVirRes {
public:
    explicit Executor(CcuRepContext *context = nullptr);
};

struct CcuRepArg {
    explicit CcuRepArg(const Variable &var) : type(CcuArgType::VARIABLE), var(var) {}
    explicit CcuRepArg(const LocalAddr &addr) : type(CcuArgType::LOCAL_ADDR), localAddr(addr) {}
    explicit CcuRepArg(const RemoteAddr &addr) : type(CcuArgType::REMOTE_ADDR), remoteAddr(addr) {}
    explicit CcuRepArg(const std::vector<Variable> &varList) : type(CcuArgType::VARIABLE_LIST), varList(varList) {}
    explicit CcuRepArg(const std::vector<LocalAddr> &addrList) : type(CcuArgType::LOCAL_ADDR_LIST), localAddrList(addrList) {}
    explicit CcuRepArg(const std::vector<RemoteAddr> &addrList) : type(CcuArgType::REMOTE_ADDR_LIST), remoteAddrList(addrList) {}

    CcuArgType type;
    Variable var;
    std::vector<Variable> varList;
    LocalAddr localAddr;
    std::vector<LocalAddr> localAddrList;
    RemoteAddr remoteAddr;
    std::vector<RemoteAddr> remoteAddrList;
};

} // namespace CcuRep

uint64_t GetTokenInfo(uint64_t va, uint64_t size);

} // namespace hcomm

#endif // HCOMM_CCU_DATATYPE_H
