/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: 算法库CcuContext公共头文件
 * Author: hulinkang
 * Create: 2025-02-20
 */

#ifndef HCCLV2_CCU_CONTEXT_UTILS_H_
#define HCCLV2_CCU_CONTEXT_UTILS_H_

#include <vector>
#include <queue>
#include "instruction.h"
#include "ins_queue.h"
#include "virtual_topo.h"
#include "ccu_ctx_arg.h"
#include "ccu_ins.h"
#include "ccu_ctx_signature.h"


namespace Hccl {
// namespace Ccu {

constexpr uint16_t LOC_CPY_LOOP_NUM = 8;
constexpr uint64_t UB_MAX_TRANS_SIZE = 256 * 1024 * 1024;  // UB单次最大传输量256*1024*1024 Byte
constexpr uint64_t MAX_LOOP_GROUP_TRANS_SIZE = 256 * 1024 * 1024;  // 暂时为 256M
constexpr uint64_t TAIL_MI0_LOOP_NUM = 128;
constexpr uint64_t TAIL_MI1_LOOP_NUM = 64;
constexpr uint64_t MESH_2D_NUM = 2;

uint64_t CalcLGMaxTransSize();

HcclResult GenerateCcuCtxSignature(CcuCtxSignature &sig, CcuInstType instType, const CollAlgOperator &op,
    const std::vector<std::vector<RankId>> &tempVTopo);
// }
}
#endif // HCCLV2_CCU_CONTEXT_UTILS_H_