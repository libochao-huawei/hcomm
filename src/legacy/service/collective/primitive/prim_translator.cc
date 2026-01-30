/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: primitive translator
 * Author: zhangqingwei
 * Create: 2023-09-28
 */

#include <iostream>
#include "prim_translator.h"
#include "prim_rules.h"

namespace Hccl {
PrimTranslator::PrimTranslator()
    : primTranslateRuleMap({{PrimType::POST_TO, GetRule<PrimPostTo>()},
                                                         {PrimType::WAIT_FROM, GetRule<PrimWaitFrom>()},
                                                         {PrimType::WAIT_GROUP, GetRule<PrimWaitGroup>()},
                                                         {PrimType::LOCAL_COPY, GetRule<PrimLocalCopy>()},
                                                         {PrimType::LOCAL_REDUCE, GetRule<PrimLocalReduce>()},
                                                         {PrimType::SEND, GetRule<PrimSend>()},
                                                         {PrimType::RECV, GetRule<PrimRecv>()},
                                                         {PrimType::GROUP, GetRule<PrimGroup>()},
                                                         {PrimType::SEND_REDUCE, GetRule<PrimSendReduce>()},
                                                         {PrimType::RECV_REDUCE, GetRule<PrimRecvReduce>()}})
{
}

void PrimTranslator::TranslateOnePrimQue(const PrimQueue &primQueue, shared_ptr<InsQueue> insQueue)
{
    for (auto iter = primQueue.Iter(); iter.HasNext(); ++iter) {
        HCCL_INFO("primitive being translated is %s", iter->Describe().c_str());
        vector<unique_ptr<Instruction>> instructions = primTranslateRuleMap.at(iter->GetType())(*iter);
        for (auto &instruction : instructions) {
            HCCL_INFO("instruction is %s", instruction->Describe().c_str());
            insQueue->Append(std::move(instruction));
        }
    }
}

shared_ptr<InsQueue> PrimTranslator::Translate(const PrimQueue &primQueue)
{
    auto masterInsQue = make_shared<InsQueue>();
    for (auto slaveIter = primQueue.IterSlaves(); slaveIter.HasNext(); ++slaveIter) {
        auto slaveInsQue = masterInsQue->Fork();
        TranslateOnePrimQue(*slaveIter, slaveInsQue);
    }
    TranslateOnePrimQue(primQueue, masterInsQue);
    return masterInsQue;
}
} // namespace Hccl