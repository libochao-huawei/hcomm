/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_rep.h"
#include "ccu_interface_assist.h"

#include "string_util.h"
#include "exception_util.h"
#include "ccu_api_exception.h"

namespace Hccl {
namespace CcuRep {

void LoopGroupCall::Run(const std::vector<LoopCall> &loopVec, const std::vector<Variable> &loopCfg,
                        const std::vector<Executor> &executors, Variable paraCfgIn, Variable offsetCfgIn) const
{
    auto loopGroup = std::make_shared<CcuRepLoopGroup>(CreateVariable(context), CreateVariable(context));

    std::vector<std::shared_ptr<CcuRepLoop>> loops;
    for (uint32_t index = 0; index < loopVec.size(); index++) {
        auto repLoop = std::make_shared<CcuRepLoop>(loopVec[index].GetLabel(), CreateVariable(context));
        AppendToContext(context, repLoop->SetLoopParam(executors[index], loopCfg[index]));
        loops.push_back(repLoop);
    }

    auto hideLoop      = std::make_shared<CcuRepJump>("hideLoop", CreateVariable(context));
    auto hideLoopLabel = std::make_shared<CcuRepJumpLabel>("hideLoop");
    hideLoop->Reference(hideLoopLabel);

    AppendToContext(context, loopGroup->SetParallelParam(paraCfgIn));
    AppendToContext(context, loopGroup->SetOffsetParam(offsetCfgIn));
    AppendToContext(context, loopGroup);

    AppendToContext(context, hideLoop);
    for (auto loop : loops) {
        AppendToContext(context, loop);
    }
    AppendToContext(context, hideLoopLabel);
}

}; // namespace CcuRep
}; // namespace Hccl