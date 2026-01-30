/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_ins_preprocessor.h"
#include "aiv_ins.h"

namespace Hccl {

void AivInsPreprocessor::Preprocess(std::shared_ptr<InsQueue> &insQueue) const
{
    HCCL_INFO("[AivInsPreprocessor::%s] insQueue Preprocess start.", __func__);

    // 对每主queue中每个ins进行预处理
    for (auto ins = insQueue->Iter(); ins.HasNext(); ++ins) {
        if (ins->GetType() != InstructionType::AIV_INS) {
            continue;
        }
        InsPreprocess(ins);
    }

    HCCL_INFO("[AivInsPreprocessor::%s] insQueue Preprocess end.", __func__);
}

void AivInsPreprocessor::InsPreprocess(InsIterator &insIter) const
{
    HCCL_INFO("[AivInsPreprocessor::%s] start.", __func__);

    const AivInstruction &aivIns = dynamic_cast<const AivInstruction &>(*insIter);

    auto links = aivIns.GetLinks();
    
    BatchBuildTransports(links);

    HCCL_INFO("[AivInsPreprocessor::%s] end.", __func__);
}

void AivInsPreprocessor::BatchBuildTransports(const vector<LinkData> &links) const
{
    HCCL_INFO("[AivInsPreprocessor::%s] start.", __func__);

    // 创建MemTransport并进行异步建链、交换
    comm->GetUbMemoryTransportMgr()->BatchCreateTransport(links);

    comm->GetUbMemoryTransportMgr()->TransportsConnect();

    HCCL_INFO("[AivInsPreprocessor::%s] end.", __func__);
}

AivInsPreprocessor::~AivInsPreprocessor()
{
}

} // namespace Hccl