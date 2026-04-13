/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_data_api.h"
#include "ccu_variable.hpp"

#include "ccu_log.h"
#include "ccu_data_api_impl.h"

CcuResult CcuVariableCreate(CcuVariable* variable)
{
    CcuVariableHandle varHandle{0};
    CCU_CHK_RET(CcuVariableCreateImpl(&varHandle));

    variable->handle = varHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableCreateFromChannel(ChannelHandle channel, uint32_t varIndex, CcuVariable *var)
{
    CcuVariableHandle varHandle{0};
    CCU_CHK_RET(CcuVariableCreateFromChannelImpl(channel, varIndex, &varHandle));
    var->handle = varHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfBegin(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuIfBeginImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfElse(const char *label)
{
    CCU_CHK_RET(CcuIfElseImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfEnd(const char *label)
{
    CCU_CHK_RET(CcuIfEndImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileBegin(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuWhileBeginImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileEnd(const char *label)
{
    CCU_CHK_RET(CcuWhileEndImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileBegin(const char *label)
{
    CCU_CHK_RET(CcuDoWhileBeginImpl(label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileEnd(CcuVariable *var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    CCU_CHK_RET(CcuDoWhileEndImpl(var->handle, immediate, condType, label));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopCreate(CcuLoopHandle *loop)
{
    return CcuLoopCreateImpl(loop);
}

CcuResult CcuLoopSetParam(CcuLoopHandle loop,
    CcuVariable *formalParam, CcuVariable *actualParam)
{
    if (formalParam == nullptr || actualParam == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopSetParamImpl(loop, formalParam->handle, actualParam->handle);
}

CcuResult CcuLoopGroupCreate(CcuLoopGroupHandle *group,
    const CcuLoopGroupConfig *config)
{
    return CcuLoopGroupCreateImpl(group, config);
}

CcuResult CcuLoopGroupCreateFromVar(CcuLoopGroupHandle *group,
    CcuVariable *parallelVar, CcuVariable *offsetVar)
{
    if (parallelVar == nullptr || offsetVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupCreateFromVarImpl(group, parallelVar->handle, offsetVar->handle);
}

CcuResult CcuLoopGroupAddLoop(CcuLoopGroupHandle group,
    CcuLoopHandle loop, const CcuLoopConfig *config, bool isUnroll)
{
    return CcuLoopGroupAddLoopImpl(group, loop, config, isUnroll);
}

CcuResult CcuLoopGroupAddLoopFromVar(CcuLoopGroupHandle group,
    CcuLoopHandle loop, CcuVariable *loopParamVar, bool isUnroll)
{
    if (loopParamVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupAddLoopFromVarImpl(group, loop, loopParamVar->handle, isUnroll);
}

CcuResult CcuContinuousVariableCreate(CcuVariable* variables, uint32_t num)
{
    if (variables == nullptr || num == 0) {
        return CcuResult::CCU_E_PARA;
    }
    for (uint32_t i = 0; i < num; i++) {
        CcuVariableHandle varHandle{0};
        CCU_CHK_RET(CcuContinuousVariableCreateImpl(&varHandle));
        variables[i].handle = varHandle;
    }
    return CcuResult::CCU_SUCCESS;
}
/*
Address 相关接口
*/
CcuResult CcuAddressCreate(CcuAddress* address)
{
    CcuAddressHandle addrHandle{0};
    CCU_CHK_RET(CcuAddressCreateImpl(&addrHandle));
    address->handle = addrHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoadArg(CcuVariable variable)
{
    CCU_CHK_RET(CcuLoadArgImpl(variable.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLoad(uint64_t addr, CcuVariable variable, uint32_t num)
{
    CCU_CHK_RET(CcuLoadImpl(addr,variable.handle,num));
    return CcuResult::CCU_SUCCESS;
}

/*
Event 相关接口
*/
CcuResult CcuCompletedEventCreate(CcuEvent* event)
{
    CcuEventHandle eventHandle{0};
    CCU_CHK_RET(CcuCompletedEventCreateImpl(&eventHandle));
    event->handle = eventHandle;
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuBlockCompletedEventCreate(CcuEvent* events, uint32_t count)
{
    if (events == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuEventHandle eventHandles[count];
    CCU_CHK_RET(CcuBlockCompletedEventCreateImpl(eventHandles, count));
    for (uint32_t i = 0; i < count; i++) {
        events[i].handle = eventHandles[i];
    }
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuRecordEvent(CcuEvent event)
{
    CCU_CHK_RET(CcuRecordEventImpl(event.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWaitEvent(CcuEvent event)
{
    CCU_CHK_RET(CcuWaitEventImpl(event.handle));
    return CcuResult::CCU_SUCCESS;
}

/*
Buffer 相关接口
*/
CcuResult CcuBufferCreate(CcuBuffer* buffer)
{
    CcuBufferHandle bufferHandle{0};
    CCU_CHK_RET(CcuBufferCreateImpl(&bufferHandle));
    buffer->handle = bufferHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuBlockBufferCreate(CcuBuffer* buffers, uint32_t count)
{
    if (buffers == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuBufferHandle bufferHandles[count];
    CCU_CHK_RET(CcuBlockBufferCreateImpl(bufferHandles, count));
    for (uint32_t i = 0; i < count; i++) {
        buffers[i].handle = bufferHandles[i];
    }
    return CcuResult::CCU_SUCCESS;
}
/*
LocalAddr / RemoteAddr 相关接口
*/
CcuResult CcuLocalAddrCreate(CcuLocalAddr* localAddr)
{
    CcuLocalAddrHandle laHandle{0};
    CcuAddressHandle addrHandle{0};
    CcuVariableHandle tokenHandle{0};
    CCU_CHK_RET(CcuLocalAddrCreateImpl(&laHandle, &addrHandle, &tokenHandle));
    localAddr->handle = laHandle;
    localAddr->addr.handle = addrHandle;
    localAddr->token.handle = tokenHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuRemoteAddrCreate(CcuRemoteAddr* remoteAddr)
{
    CcuRemoteAddrHandle raHandle{0};
    CcuAddressHandle addrHandle{0};
    CcuVariableHandle tokenHandle{0};
    CCU_CHK_RET(CcuRemoteAddrCreateImpl(&raHandle, &addrHandle, &tokenHandle));
    remoteAddr->handle = raHandle;
    remoteAddr->addr.handle = addrHandle;
    remoteAddr->token.handle = tokenHandle;
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLocalCopyHBMToBuffer(
    CcuBuffer dstBuffer, CcuLocalAddr src,
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuLocalCopyHBMToBufferImpl(
        dstBuffer.handle, src.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalCopyBufferToHBM(
    CcuLocalAddr dst, CcuBuffer srcBuffer,
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuLocalCopyBufferToHBMImpl(
        dst.handle, srcBuffer.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalCopyHBMToHBM(
    CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuLocalCopyHBMToHBMImpl(
        dst.handle, src.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
/*========== 本地 Reduce ==========*/

CcuResult CcuLocalHBMReduce(
    CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    CCU_CHK_RET(CcuLocalHBMReduceImpl(
        dst.handle, src.handle, len.handle, dataType, opType, event.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLocalBufferReduce(
    CcuBuffer* buffers, uint32_t count,
    HcclDataType dataType, HcclDataType outputDataType,
    HcclReduceOp opType,
    CcuVariable len, CcuEvent event)
{
    if (buffers == nullptr || count == 0) {
        return CcuResult::CCU_E_PARA;
    }
    CcuBufferHandle bufHandles[count];
    for (uint32_t i = 0; i < count; i++) {
        bufHandles[i] = buffers[i].handle;
    }
    CCU_CHK_RET(CcuLocalBufferReduceImpl(
        bufHandles, count, dataType, outputDataType, opType,
        len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
/*========== 远端数据传输操作 ==========*/
CcuResult CcuReadHBMToHBM(
    ChannelHandle channel, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuReadHBMToHBMImpl(
        channel, local.handle, remote.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuReadHBMToBuffer(
    ChannelHandle channel, CcuBuffer local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuReadHBMToBufferImpl(
        channel, local.handle, remote.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteHBMToHBM(
    ChannelHandle channel, CcuRemoteAddr remote, CcuLocalAddr local, 
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuWriteHBMToHBMImpl(
        channel, remote.handle, local.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuWriteBufferToHBM(
    ChannelHandle channel, CcuRemoteAddr remote, CcuBuffer local, 
    CcuVariable len, CcuEvent event)
{
    CCU_CHK_RET(CcuWriteBufferToHBMImpl(
        channel, remote.handle, local.handle, len.handle, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuReadHBMToHBMReduce(
    ChannelHandle channel, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    CCU_CHK_RET(CcuReadHBMToHBMReduceImpl(
        channel, local.handle, remote.handle, len.handle,
        dataType, opType, event.handle));
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuWriteHBMToHBMReduce(
    ChannelHandle channel, CcuRemoteAddr remote, CcuLocalAddr local,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event)
{
    CCU_CHK_RET(CcuWriteHBMToHBMReduceImpl(
        channel, remote.handle, local.handle, len.handle,
        dataType, opType, event.handle));
    return CcuResult::CCU_SUCCESS;
}
/*========== 远端同步操作 ==========*/
CcuResult CcuWriteVariableWithNotify(ChannelHandle channel, CcuVariable var,uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask)
{
    CCU_CHK_RET(CcuWriteVariableWithNotifyImpl(channel, var.handle, remoteVarIdx, remoteNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteNotify(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask)
{
    CCU_CHK_RET(CcuWriteNotifyImpl(channel, remoteNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask)
{
    CCU_CHK_RET(CcuNotifyWaitImpl(channel, localNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}

/*
Variable 赋值与运算
*/
CcuResult CcuVariableAssign(CcuVariable var, uint64_t immediate)
{
    CCU_CHK_RET(CcuVariableAssignImpl(var.handle, immediate));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAssignVar(CcuVariable dst, CcuVariable src)
{
    CCU_CHK_RET(CcuVariableAssignVarImpl(dst.handle, src.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAddVarToVar(CcuVariable result, CcuVariable a, CcuVariable b)
{
    CCU_CHK_RET(CcuVariableAddVarToVarImpl(result.handle, a.handle, b.handle));
    return CcuResult::CCU_SUCCESS;
}

/*
Event mask
*/
CcuResult CcuSetEventMask(CcuEvent event, uint32_t mask)
{
    CCU_CHK_RET(CcuSetEventMaskImpl(event.handle, mask));
    return CcuResult::CCU_SUCCESS;
}

/*
Address 赋值与运算
*/
CcuResult CcuAddressAssignImm(CcuAddress addr, uint64_t immediate)
{
    CCU_CHK_RET(CcuAddressAssignImmImpl(addr.handle, immediate));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAssignVar(CcuAddress addr, CcuVariable var)
{
    CCU_CHK_RET(CcuAddressAssignVarImpl(addr.handle, var.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAssignAddr(CcuAddress dst, CcuAddress src)
{
    CCU_CHK_RET(CcuAddressAssignAddrImpl(dst.handle, src.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddAddrToAddr(CcuAddress result, CcuAddress a, CcuAddress b)
{
    CCU_CHK_RET(CcuAddressAddAddrToAddrImpl(result.handle, a.handle, b.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddVarToAddr(CcuAddress result, CcuAddress addr, CcuVariable var)
{
    CCU_CHK_RET(CcuAddressAddVarToAddrImpl(result.handle, addr.handle, var.handle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddAssignVar(CcuAddress addr, CcuVariable var)
{
    CCU_CHK_RET(CcuAddressAddAssignVarImpl(addr.handle, var.handle));
    return CcuResult::CCU_SUCCESS;
}

/*
Loop body scope
*/
CcuResult _CcuLoopBodyEnter(CcuLoopHandle loop)
{
    CCU_CHK_RET(_CcuLoopBodyEnterImpl(loop));
    return CcuResult::CCU_SUCCESS;
}

CcuResult _CcuLoopBodyExit(CcuLoopHandle loop)
{
    CCU_CHK_RET(_CcuLoopBodyExitImpl(loop));
    return CcuResult::CCU_SUCCESS;
}
