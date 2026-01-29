/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: error message report
 * Create： 2026-01-15
 */
#ifndef ERROR_MESSAGE_H
#define ERROR_MESSAGE_H
#include "hccl_common_v2.h"
#include "task_param.h"
 
namespace Hccl {
 
struct ErrorMessageReport {
    char tag[TAG_MAX_LENGTH] = {0};
    char group[GROUP_NAME_MAX_LEN + 1] = {0};
    u32 remoteUserRank = 0;
    s32 streamId = 0;
    u32 taskId = 0;
    u32 notifyId = 0;
    s32 stage = 0;
    u32 rankId = 0;
    u32 rankSize = 0;
    AlgType algType;
    TaskParamType taskType = TaskParamType::TASK_SDMA;
    uint64_t count = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint32_t opIndex = 0;                // 记录index
    uint32_t reduceType = 255; // 255 为 HcclReduceOp::HCCL_REDUCE_RESERVED
    uint8_t dataType = 0;
 
    Eid locEid{};
    Eid rmtEid{};
};
 
} // namespace Hccl
 
#endif