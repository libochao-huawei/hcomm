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

//本地数据拷贝 相关接口
extern CcuResult CcuLocalCopyMemToMem(CcuLocalAddrHandle dst, CcuLocalAddrHandle src, CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuLocalCopyMemToBuffer(CcuBufferHandle dst, CcuLocalAddrHandle src,CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuLocalCopyBufferToMem(CcuLocalAddrHandle dst, CcuBufferHandle src, CcuVariableHandle len, CcuEventHandle event);
//本地reduce 相关接口
extern CcuResult CcuLocalMemReduce(CcuLocalAddrHandle dst, CcuLocalAddrHandle src, CcuVariableHandle len, HcclDataType dataType, HcclReduceOp opType, CcuEventHandle event);
extern CcuResult CcuLocalBufferReduce(CcuBufferHandle* buffers, uint32_t count, HcclDataType dataType, HcclDataType outputDataType, HcclReduceOp opType, CcuVariableHandle len, CcuEventHandle event);

/*========== 远端数据传输操作 ==========*/
extern CcuResult CcuReadMemToMem(ChannelHandle channel, CcuLocalAddrHandle localHandle, CcuRemoteAddrHandle remoteHandle, CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuReadMemToBuffer(ChannelHandle channel, CcuBufferHandle localHandle, CcuRemoteAddrHandle remoteHandle, CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuReadMemToMemReduce(ChannelHandle channel, CcuLocalAddrHandle localHandle, CcuRemoteAddrHandle remoteHandle, CcuVariableHandle len, HcclDataType dataType, HcclReduceOp opType, CcuEventHandle event);
extern CcuResult CcuWriteMemToMem(ChannelHandle channel, CcuRemoteAddrHandle remoteHandle, CcuLocalAddrHandle localHandle, CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuWriteBufferToMem(ChannelHandle channel, CcuRemoteAddrHandle remoteHandle, CcuBufferHandle localHandle, CcuVariableHandle len, CcuEventHandle event);
extern CcuResult CcuWriteMemToMemReduce(ChannelHandle channel, CcuRemoteAddrHandle remoteHandle, CcuLocalAddrHandle localHandle, CcuVariableHandle len, HcclDataType dataType, HcclReduceOp opType, CcuEventHandle event);


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

extern CcuResult CcuCreateBlockExecutor(CcuLoopExecutors *pool, uint32_t count);

extern CcuResult CcuLoopCreate(CcuLoop *loop);
extern CcuResult _CcuLoopBodyEnter(CcuLoop loop);
extern CcuResult _CcuLoopBodyExit(CcuLoop loop);
extern CcuResult CcuLoopSetParam(CcuLoop loop,
    CcuVariableHandle formalParam, CcuVariableHandle actualParam);
extern CcuResult CcuLoopGroupCreate(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupCreateFromVar(CcuLoopGroup *group,
    CcuVariableHandle parallelVar, CcuVariableHandle offsetVar,
    CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupAddLoop(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config);
extern CcuResult CcuLoopGroupAddLoopFromVar(CcuLoopGroup group,
    CcuLoop loop, CcuVariableHandle loopParamVar);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_DATA_API_IMPL_H