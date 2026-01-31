/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: primitive translator header file
 * Author: chenjie
 * Create: 2024-02-04
 */
#ifndef HCCLV2_PRIM_TRANSLATOR_H
#define HCCLV2_PRIM_TRANSLATOR_H

#include <functional>
#include "ins_queue.h"
#include "prim_queue.h"

namespace Hccl {
using namespace std;

class PrimTranslator {
public:
    using TranslateRule = std::function<vector<unique_ptr<Instruction>>(const Primitive &)>;

    explicit PrimTranslator();
    shared_ptr<InsQueue> Translate(const PrimQueue &primQueue);

private:
    std::map<PrimType, TranslateRule> primTranslateRuleMap;
    void                              TranslateOnePrimQue(const PrimQueue &primQueue, shared_ptr<InsQueue> insQueue);
};
} // namespace Hccl
#endif
