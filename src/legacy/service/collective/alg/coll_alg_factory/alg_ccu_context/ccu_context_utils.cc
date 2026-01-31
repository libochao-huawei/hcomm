/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库CcuContext公共函数实现
 * Author: hulinkang
 * Create: 2025-02-19
 */

#include <vector>
#include <string>
#include <sstream>
#include <ios>
#include <iostream>
#include "instruction.h"
#include "virtual_topo.h"
#include "ccu_ctx_arg.h"
#include "ccu_ins.h"
#include "ccu_ctx_signature.h"
#include "ccu_context_utils.h"

namespace Hccl {

constexpr int COMM_LEVEL_SIZE_1 = 1;
constexpr int COMM_LEVEL_SIZE_2 = 2;

uint64_t CalcLGMaxTransSize()
{
    return LOC_CPY_LOOP_NUM * 4096 * 8192;  // 单片MS搬4096B，每个loop循环最多8192次
}

HcclResult GenerateCcuCtxSignature(CcuCtxSignature &sig, CcuInstType instType, const CollAlgOperator &op,
    const std::vector<std::vector<RankId>> &tempVTopo)
{
    sig.Append<int>(int(instType));
    if (op.opType == OpType::REDUCESCATTER || op.opType == OpType::ALLREDUCE || op.opType == OpType::REDUCE
        || op.opType == OpType::REDUCESCATTERV) {
        sig.Append<int>(int(op.reduceOp));
        sig.Append<int>(int(op.dataType));
        sig.Append<int>(int(op.outputDataType));
    }
    if (op.opType == OpType::REDUCE || op.opType == OpType::BROADCAST || op.opType == OpType::GATHER
        || op.opType == OpType::SCATTER) {
        // 带有root属性的算子需要考虑自己与root的关系，暂定直接用root号做区分
        sig.Append<char>('R');
        sig.Append<int>(int(op.root));
        // sig.Append<std::string>("_");
    }
    if (tempVTopo.size() == COMM_LEVEL_SIZE_1) {
        sig.Append<uint32_t>(tempVTopo[0].size());
        sig.Append<char>('P');
    } else if (tempVTopo.size() == COMM_LEVEL_SIZE_2) {
        sig.Append<uint32_t>(tempVTopo[0].size());
        sig.Append<char>('X');
        sig.Append<uint32_t>(tempVTopo[1].size());
        sig.Append<char>('P');
    } else {
        THROW<InvalidParamsException>(
            StringFormat("GenerateCcuCtxSignature failed: unexpected tempVTopoSize[%u]", tempVTopo.size()));
    }
    return HcclResult::HCCL_SUCCESS;
}
}