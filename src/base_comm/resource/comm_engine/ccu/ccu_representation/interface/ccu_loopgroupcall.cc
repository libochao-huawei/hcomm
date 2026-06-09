/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: ccu representation implementation file
 * Author: sunzhepeng
 * Create: 2024-06-17
 */

#include "ccu_rep_v1.h"
#include "ccu_interface_assist_v1.h"

#include "string_util.h"
#include "exception_util.h"
#include "ccu_api_exception.h"

namespace hcomm {
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
}; // namespace hcomm