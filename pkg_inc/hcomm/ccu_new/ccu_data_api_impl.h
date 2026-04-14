/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef CCU_DATA_API_IMPL_H
#define CCU_DATA_API_IMPL_H

#include "hccl_types.h"
#include "ccu_types.h"
#include "hcomm_primitives.h"
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern CcuResult CcuVariableCreateImpl(CcuVariableHandle *var);

extern CcuResult CcuVariableCreateFromChannelImpl(ChannelHandle channel,
    uint32_t varIndex, CcuVariableHandle *varHandle);

extern CcuResult CcuVariableAssignImpl(CcuVariableHandle resVar, uint64_t immediate);

extern CcuResult CcuVariableAssignVarImpl(CcuVariableHandle dstVarHandle, CcuVariableHandle srcVarHandle);

extern CcuResult CcuVariableAddVarToVarImpl(CcuVariableHandle resVar, CcuVariableHandle varA, CcuVariableHandle varB);


extern CcuResult CcuIfBeginImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

extern CcuResult CcuIfElseImpl(const char *label);

extern CcuResult CcuIfEndImpl(const char *label);

extern CcuResult CcuWhileBeginImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

extern CcuResult CcuWhileEndImpl(const char *label);

extern CcuResult CcuDoWhileBeginImpl(const char *label);

extern CcuResult CcuDoWhileEndImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

extern CcuResult CcuCreateBlockExecutorImpl(CcuLoopExecutors *pool, uint32_t count);

    extern CcuResult CcuLoopCreateImpl(CcuLoop *loop);
extern CcuResult _CcuLoopBodyEnterImpl(CcuLoop loop);
extern CcuResult _CcuLoopBodyExitImpl(CcuLoop loop);
extern CcuResult CcuLoopSetParamImpl(CcuLoop loop,
    CcuVariableHandle formalParam, CcuVariableHandle actualParam);
extern CcuResult CcuLoopGroupCreateImpl(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupCreateFromVarImpl(CcuLoopGroup *group,
    CcuVariableHandle parallelVar, CcuVariableHandle offsetVar,
    CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupAddLoopImpl(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config);
extern CcuResult CcuLoopGroupAddLoopFromVarImpl(CcuLoopGroup group,
    CcuLoop loop, CcuVariableHandle loopParamVar);

extern CcuResult CcuContinuousVariableCreateImpl(CcuVariableHandle* var);

extern CcuResult CcuAddressCreateImpl(CcuAddressHandle *addrHandle);

extern CcuResult CcuAddressAssignImmImpl(CcuAddressHandle addr, uint64_t immediate);

extern CcuResult CcuAddressAssignAddrImpl(CcuAddressHandle dstAddrHandle, CcuAddressHandle srcAddrHandle);

extern CcuResult CcuAddressAssignVarImpl(CcuAddressHandle addr, CcuVariableHandle var);

extern CcuResult CcuAddressAddVarToAddrImpl(CcuAddressHandle resAddr, CcuAddressHandle lhsAddr, CcuVariableHandle rhsVar);

extern CcuResult CcuAddressAddAddrToAddrImpl(CcuAddressHandle resAddr, CcuAddressHandle addrA, CcuAddressHandle addrB);

extern CcuResult CcuAddressAddAssignVarImpl(CcuAddressHandle addr, CcuVariableHandle var);

// extern CcuResult CcuAddressAddAssignAddrImpl(CcuAddressHandle addr, CcuAddressHandle otherAddr);


extern CcuResult CcuLoadArgImpl(CcuVariableHandle varHandle);

extern CcuResult CcuLoadImpl(uint64_t addr, CcuVariableHandle varHandle, uint32_t num);

extern CcuResult CcuSetEventMaskImpl(CcuEventHandle eventHandle, uint32_t mask);

/*
CompletedEvent 相关接口
*/
extern CcuResult CcuCompletedEventCreateImpl(CcuEventHandle *eventHandle);

extern CcuResult CcuBlockCompletedEventCreateImpl(CcuEventHandle *eventHandle, uint32_t num);

extern CcuResult CcuSetEventMaskImpl(CcuEventHandle eventHandle, uint32_t mask);

extern CcuResult CcuRecordEventImpl(CcuEventHandle eventHandle);

extern CcuResult CcuWaitEventImpl(CcuEventHandle eventHandle);

/*
LocalAddr / RemoteAddr 相关接口
*/
extern CcuResult CcuLocalAddrCreateImpl(
    CcuLocalAddrHandle *handle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);

extern CcuResult CcuRemoteAddrCreateImpl(
    CcuRemoteAddrHandle *handle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);

/*
Buffer 相关接口
*/
extern CcuResult CcuBufferCreateImpl(CcuBufferHandle *bufHandle);

extern CcuResult CcuBlockBufferCreateImpl(CcuBufferHandle *bufferHandles, uint32_t count);

extern CcuResult CcuLocalCopyHBMToBufferImpl(
    CcuBufferHandle dstBuffer, CcuLocalAddrHandle src,
    CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuLocalCopyBufferToHBMImpl(
        CcuLocalAddrHandle dst, CcuBufferHandle srcBuffer,
        CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuLocalCopyHBMToHBMImpl(
    CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
    CcuVariableHandle len, CcuEventHandle event);

/*========== 本地 Reduce ==========*/
extern CcuResult CcuLocalHBMReduceImpl(
    CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event);

extern CcuResult CcuLocalBufferReduceImpl(
    CcuBufferHandle* buffers, uint32_t count,
    HcclDataType dataType, HcclDataType outputDataType,
    HcclReduceOp opType,
    CcuVariableHandle len, CcuEventHandle event);

/*========== 远端数据传输操作 ==========*/
extern CcuResult CcuReadHBMToHBMImpl(
    ChannelHandle channel, CcuLocalAddrHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuReadHBMToBufferImpl(
    ChannelHandle channel, CcuBufferHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuWriteHBMToHBMImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuLocalAddrHandle local, 
    CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuWriteBufferToHBMImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuBufferHandle local, 
    CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuReadHBMToHBMReduceImpl(
    ChannelHandle channel, CcuLocalAddrHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event);
extern CcuResult CcuWriteHBMToHBMReduceImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuLocalAddrHandle local,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event);

/*========== 远端同步操作 ==========*/
extern CcuResult CcuWriteVariableWithNotifyImpl(ChannelHandle channel, CcuVariableHandle varHandle,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask);
extern CcuResult CcuWriteNotifyImpl(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask);
extern CcuResult CcuNotifyWaitImpl(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_DATA_API_IMPL_H