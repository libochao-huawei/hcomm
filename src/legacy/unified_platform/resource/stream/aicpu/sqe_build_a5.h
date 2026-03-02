/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_SQE_BUILD_A5_H
#define HCCLV2_SQE_BUILD_A5_H
#include "types.h"
#include "ub_jetty_lite.h"

namespace Hccl {

// 普通notify的record
void BuildA5SqeNotifyWait(u32 streamId, u32 taskId, u32 notifyId, uint8_t * const sqeIn);

// 普通notify的wait
void BuildA5SqeNotifyRecord(u32 streamId, u32 taskId, u32 notifyId, uint8_t * const sqeIn);

// cnt notify 1toN的record
void BuildA5SqeCnt1toNNotifyRecord(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn);

// cnt notify 1toN的wait
void BuildA5SqeCnt1toNNotifyWait(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn);

// cnt notify Nto1的record
void BuildA5SqeCntNto1NotifyRecord(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn);

// cnt notify Nto1的wait
void BuildA5SqeCntNto1NotifyWait(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn);

// sdma memcpy
void BuildA5SqeSdmaCopy(u32 streamId, u32 taskId, u64 dstAddr, u64 srcAddr, u32 size, u32 partId, u32 opcode,
                        uint8_t * const sqeIn);


void BuildA5SqeUbDbSend(u32 streamId, u32 taskId, const UbJettyLiteId &jettyLiteId, u16 piValue,
                        uint8_t * const sqeIn);

// CCore notify的wait
void BuildA5SqeCCoreNotifyWait(u32 streamId, u32 taskId, u64 waitAddr, u64 actAddr, bool last, uint8_t * const sqeIn);

// CCore notify的record
void BuildA5SqeCCoreNotifyRecord(u32 streamId, u32 taskId, u64 writeAddr, u64 valueAddr, uint8_t * const sqeIn);

u32 GetKernelExecTimeoutFromEnvConfig();

} // namespace Hccl

#endif