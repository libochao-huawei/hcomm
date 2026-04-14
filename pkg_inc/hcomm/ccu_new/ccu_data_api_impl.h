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

//Alloc 相关接口
extern CcuResult CcuVariableAlloc(CcuVariableHandle *varHandle);
extern CcuResult CcuAddressAlloc(CcuAddressHandle *addrHandle);
extern CcuResult CcuEventAlloc(CcuEventHandle *eventHandle);
extern CcuResult CcuBufferAlloc(CcuBufferHandle *bufHandle);
extern CcuResult CcuLocalAddrAlloc(CcuLocalAddrHandle *localAddrHandle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);
extern CcuResult CcuRemoteAddrAlloc(CcuRemoteAddrHandle *remoteAddrHandle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle);

//BlockAlloc 相关接口
extern CcuResult CcuBlockVariableAlloc(CcuVariableHandle *varHandles, uint32_t count);
// extern CcuResult CcuBlockAddressAlloc(CcuAddressHandle *addrHandles, uint32_t count);
extern CcuResult CcuBlockEventAlloc(CcuEventHandle *eventHandles, uint32_t count);
extern CcuResult CcuBlockBufferAlloc(CcuBufferHandle *bufHandles, uint32_t count);

extern CcuResult CcuVariableCreateByChannel(ChannelHandle channel,
    uint32_t varIndex, CcuVariableHandle *varHandle);

    
//Variable操作类 相关接口
extern CcuResult CcuVariableAssign(CcuVariableHandle resVar, uint64_t immediate);
extern CcuResult CcuVariableAssignVar(CcuVariableHandle dstVarHandle, CcuVariableHandle srcVarHandle);
extern CcuResult CcuVariableAddVarToVar(CcuVariableHandle resVar, CcuVariableHandle varA, CcuVariableHandle varB);

//Address操作类 相关接口
extern CcuResult CcuAddressAssignImm(CcuAddressHandle addr, uint64_t immediate);
extern CcuResult CcuAddressAssignAddr(CcuAddressHandle dstAddrHandle, CcuAddressHandle srcAddrHandle);
extern CcuResult CcuAddressAssignVar(CcuAddressHandle addr, CcuVariableHandle var);
extern CcuResult CcuAddressAddVarToAddr(CcuAddressHandle resAddr, CcuAddressHandle lhsAddr, CcuVariableHandle rhsVar);
extern CcuResult CcuAddressAddAddrToAddr(CcuAddressHandle resAddr, CcuAddressHandle addrA, CcuAddressHandle addrB);
extern CcuResult CcuAddressAddAssignVar(CcuAddressHandle addr, CcuVariableHandle var);
// extern CcuResult CcuAddressAddAssignAddr(CcuAddressHandle addr, CcuAddressHandle otherAddr);


//参数加载类 相关接口
extern CcuResult CcuLoadArg(CcuVariableHandle varHandle);
extern CcuResult CcuLoadVar(uint64_t addr, CcuVariableHandle varHandle, uint32_t num);

//Event信号同步类 相关接口
extern CcuResult CcuRecordEvent(CcuEventHandle eventHandle);
extern CcuResult CcuWaitEvent(CcuEventHandle eventHandle);
extern CcuResult CcuSetMask(CcuEventHandle eventHandle, uint32_t mask);
extern CcuResult CcuNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask);
extern CcuResult CcuNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask);
extern CcuResult CcuWriteVariableWithNotify(ChannelHandle channel, CcuVariableHandle varHandle,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask);


extern CcuResult CcuIfBegin(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

extern CcuResult CcuIfElse(const char *label);

extern CcuResult CcuIfEnd(const char *label);

extern CcuResult CcuWhileBegin(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

extern CcuResult CcuWhileEnd(const char *label);

extern CcuResult CcuDoWhileBegin(const char *label);

extern CcuResult CcuDoWhileEnd(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label);

    extern CcuResult CcuLoopCreateImpl(CcuLoopHandle *loop);
extern CcuResult _CcuLoopBodyEnterImpl(CcuLoopHandle loop);
extern CcuResult _CcuLoopBodyExitImpl(CcuLoopHandle loop);
extern CcuResult CcuLoopSetParamImpl(CcuLoopHandle loop,
    CcuVariableHandle formalParam, CcuVariableHandle actualParam);
extern CcuResult CcuLoopGroupCreateImpl(CcuLoopGroupHandle *group,
    const CcuLoopGroupConfig *config);
extern CcuResult CcuLoopGroupCreateFromVarImpl(CcuLoopGroupHandle *group,
    CcuVariableHandle parallelVar, CcuVariableHandle offsetVar);
extern CcuResult CcuLoopGroupAddLoopImpl(CcuLoopGroupHandle group,
    CcuLoopHandle loop, const CcuLoopConfig *config, bool isUnroll);
extern CcuResult CcuLoopGroupAddLoopFromVarImpl(CcuLoopGroupHandle group,
    CcuLoopHandle loop, CcuVariableHandle loopParamVar, bool isUnroll);





/*
Buffer 相关接口
*/
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


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_DATA_API_IMPL_H