//
// Created by w00422550 on 10/25/23.
// Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
//

#ifndef HCCLV2_PRIM_RULES_H
#define HCCLV2_PRIM_RULES_H

#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>

#include "instruction.h"
#include "primitive.h"
#include "prim_translator.h"

namespace Hccl {
using namespace std;

MAKE_ENUM(InsArraySize, ZERO, ONE, TWO, THREE, FOUR, FIVE);
MAKE_ENUM(InsArrayIndex, ZERO, ONE, TWO, THREE, FOUR);
constexpr u32 REDUCE_SIZE_DOUBLE = 2;

vector<unique_ptr<Instruction>> Translate(const PrimPostTo &postTo);
vector<unique_ptr<Instruction>> Translate(const PrimWaitFrom &waitFrom);
vector<unique_ptr<Instruction>> Translate(const PrimWaitGroup &waitGroup);
vector<unique_ptr<Instruction>> Translate(const PrimLocalReduce &localReduce);
vector<unique_ptr<Instruction>> Translate(const PrimLocalCopy &localCopy);
vector<unique_ptr<Instruction>> Translate(const PrimSend &send);
vector<unique_ptr<Instruction>> Translate(const PrimRecv &recv);
vector<unique_ptr<Instruction>> TranslateWithInlineReduce(const PrimSendReduce &sendReduce);
vector<unique_ptr<Instruction>> TranslateWithoutInlineReduce(const PrimSendReduce &sendReduce);
vector<unique_ptr<Instruction>> Translate(const PrimSendReduce &sendReduce);
vector<unique_ptr<Instruction>> TranslateWithInlineReduce(const PrimRecvReduce &recvReduce);
vector<unique_ptr<Instruction>> TranslateWithoutInlineReduce(const PrimRecvReduce &recvReduce);
vector<unique_ptr<Instruction>> Translate(const PrimRecvReduce &recvReduce);
vector<unique_ptr<Instruction>> GenerateTempInstruction(const PrimGroup &group);
vector<unique_ptr<Instruction>> Translate(const PrimGroup &group);

template <typename PRIM_TYPE> PrimTranslator::TranslateRule GetRule()
{
    return [](const Primitive &prim) -> vector<unique_ptr<Instruction>> {
        return Translate(static_cast<const PRIM_TYPE &>(prim));
    };
}

} // namespace Hccl

#endif // HCCLV2_PRIM_RULES_H
