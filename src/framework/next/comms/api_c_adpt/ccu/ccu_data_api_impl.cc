/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_data_api_impl.h"

#include "ccu_log.h"
#include "hcom_common.h"

#include "ccu_kernel_mgr.h"

CcuResult CcuVariableCreateImpl(CcuVariableHandle *resVar)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->VariableCreate(resVar));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableCreateFromChannelImpl(ChannelHandle channel, uint32_t varIndex, CcuVariableHandle *varHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->VariableCreate(channel, varIndex, varHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAssignImpl(CcuVariableHandle resVar, uint64_t immediate)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->VariableAssign(resVar, immediate));
    
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAssignVarImpl(CcuVariableHandle dstVarHandle, CcuVariableHandle srcVarHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->VariableAssignVar(dstVarHandle, srcVarHandle));
    
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuVariableAddVarToVarImpl(CcuVariableHandle resVar, CcuVariableHandle varA, CcuVariableHandle varB)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->VariableAddVarToVar(resVar, varA, varB));
    
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfBeginImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->IfBegin(var, immediate, condType, label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfElseImpl(const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->IfElse(label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuIfEndImpl(const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->IfEnd(label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileBeginImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WhileBegin(var, immediate, condType, label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWhileEndImpl(const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WhileEnd(label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileBeginImpl(const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->DoWhileBegin(label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuDoWhileEndImpl(CcuVariableHandle var, uint64_t immediate,
    CcuConditionType condType, const char *label)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->DoWhileEnd(var, immediate, condType, label));

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopCreateImpl(CcuLoopHandle *loop)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopCreate(loop));
    return CcuResult::CCU_SUCCESS;
}

CcuResult _CcuLoopBodyEnterImpl(CcuLoopHandle loop)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopBodyEnter(loop));
    return CcuResult::CCU_SUCCESS;
}

CcuResult _CcuLoopBodyExitImpl(CcuLoopHandle loop)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopBodyExit(loop));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopSetParamImpl(CcuLoopHandle loop,
    CcuVariableHandle formalParam, CcuVariableHandle actualParam)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopSetParam(loop, formalParam, actualParam));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopGroupCreateImpl(CcuLoopGroupHandle *group,
    const CcuLoopGroupConfig *config)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopGroupCreate(group, config));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopGroupCreateFromVarImpl(CcuLoopGroupHandle *group,
    CcuVariableHandle parallelVar, CcuVariableHandle offsetVar)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopGroupCreateFromVar(group, parallelVar, offsetVar));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopGroupAddLoopImpl(CcuLoopGroupHandle group,
    CcuLoopHandle loop, const CcuLoopConfig *config, bool isUnroll)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopGroupAddLoop(group, loop, config, isUnroll));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoopGroupAddLoopFromVarImpl(CcuLoopGroupHandle group,
    CcuLoopHandle loop, CcuVariableHandle loopParamVar, bool isUnroll)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoopGroupAddLoopFromVar(group, loop, loopParamVar, isUnroll));
    return CcuResult::CCU_SUCCESS;
}

/*
Address 相关接口
*/
CcuResult CcuAddressCreateImpl(CcuAddressHandle *addrHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressCreate(addrHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAssignImmImpl(CcuAddressHandle addr, uint64_t immediate)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAssignImm(addr, immediate));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAssignAddrImpl(CcuAddressHandle dstAddrHandle, CcuAddressHandle srcAddrHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAssignAddr(dstAddrHandle, srcAddrHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAssignVarImpl(CcuAddressHandle addr, CcuVariableHandle var)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAssignVar(addr, var));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddVarToAddrImpl(CcuAddressHandle resAddr, CcuAddressHandle lhsAddr, CcuVariableHandle rhsVar)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAddVarToAddr(resAddr, lhsAddr, rhsVar));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddAddrToAddrImpl(CcuAddressHandle resAddr, CcuAddressHandle addrA, CcuAddressHandle addrB)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAddAddrToAddr(resAddr, addrA, addrB));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuAddressAddAssignVarImpl(CcuAddressHandle addr, CcuVariableHandle var)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->AddressAddAssignVar(addr, var));
    return CcuResult::CCU_SUCCESS;
}

// CcuResult CcuAddressAddAssignAddrImpl(CcuAddressHandle addr, CcuAddressHandle otherAddr)
// {
//     const uint32_t devLogicId = HcclGetThreadDeviceId();
//     auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
//     CCU_CHK_PTR_NULL(kernel);
//     CCU_CHK_RET(kernel->AddressAddAssignAddr(addr, otherAddr));
//     return CcuResult::CCU_SUCCESS;
// }

CcuResult CcuLoadArgImpl(CcuVariableHandle varHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LoadArg(varHandle));
    
    return CcuResult::CCU_SUCCESS;
}    

CcuResult CcuContinuousVariableCreateImpl(CcuVariableHandle* varHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->ContinuousVariableCreate(varHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLoadImpl(uint64_t addr, CcuVariableHandle varHandle, uint32_t num ){
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    // if (num == 1){
    //     CCU_CHK_RET(kernel->LoadVariable(addr, varHandle));
    // }
    // else{
    //     CCU_CHK_RET(kernel->LoadVariable(addr, varHandle,num));
    // }
    CCU_CHK_RET(kernel->LoadVariable(addr, varHandle,num));
    
    return CcuResult::CCU_SUCCESS;
}

/*
CompletedEvent 相关接口
*/
CcuResult CcuCompletedEventCreateImpl(CcuEventHandle *eventHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->CompletedEventCreate(eventHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuBlockCompletedEventCreateImpl(CcuEventHandle *eventHandle, uint32_t num)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->ContinuousEventCreate(eventHandle, num));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuSetEventMaskImpl(CcuEventHandle eventHandle, uint32_t mask)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->SetEventMask(eventHandle, mask));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuRecordEventImpl(CcuEventHandle eventHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->CompletedEventRecord(eventHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWaitEventImpl(CcuEventHandle eventHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->CompletedEventWait(eventHandle));
    return CcuResult::CCU_SUCCESS;
}

/*
LocalAddr / RemoteAddr 相关接口
*/
CcuResult CcuLocalAddrCreateImpl(
    CcuLocalAddrHandle *handle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalAddrCreate(handle, addrHandle, tokenHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuRemoteAddrCreateImpl(
    CcuRemoteAddrHandle *handle, CcuAddressHandle *addrHandle, CcuVariableHandle *tokenHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->RemoteAddrCreate(handle, addrHandle, tokenHandle));
    return CcuResult::CCU_SUCCESS;
}

/*
Buffer 相关接口
*/
CcuResult CcuBufferCreateImpl(CcuBufferHandle *bufHandle)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->BufferCreate(bufHandle));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuBlockBufferCreateImpl(CcuBufferHandle *bufferHandles, uint32_t count)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->BlockBufferCreate(bufferHandles, count));
    return CcuResult::CCU_SUCCESS;
}  

CcuResult CcuLocalCopyHBMToBufferImpl(
    CcuBufferHandle dstBuffer, CcuLocalAddrHandle src,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalCopyToBuffer(dstBuffer, src, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLocalCopyBufferToHBMImpl(
    CcuLocalAddrHandle dst, CcuBufferHandle srcBuffer,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalCopyFromBuffer(dst, srcBuffer, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLocalCopyHBMToHBMImpl(
    CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalCopy(dst, src, len, event));
    return CcuResult::CCU_SUCCESS;
}

/*========== 本地 Reduce ==========*/
CcuResult CcuLocalHBMReduceImpl(
    CcuLocalAddrHandle dst, CcuLocalAddrHandle src,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalAddrReduce(dst, src, len, dataType, opType, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuLocalBufferReduceImpl(
    CcuBufferHandle* buffers, uint32_t count,
    HcclDataType dataType, HcclDataType outputDataType,
    HcclReduceOp opType,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->LocalBufferReduce(
        buffers, count, dataType, outputDataType, opType, len, event));
    return CcuResult::CCU_SUCCESS;
}

/*========== 远端数据传输操作 ==========*/
CcuResult CcuReadHBMToHBMImpl(
    ChannelHandle channel, CcuLocalAddrHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->Read(channel, local, remote, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuReadHBMToBufferImpl(
    ChannelHandle channel, CcuBufferHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->ReadBuffer(channel, local, remote, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteHBMToHBMImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuLocalAddrHandle local, 
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->Write(channel, local, remote, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteBufferToHBMImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuBufferHandle local, 
    CcuVariableHandle len, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WriteBuffer(channel, local, remote, len, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuReadHBMToHBMReduceImpl(
    ChannelHandle channel, CcuLocalAddrHandle local, CcuRemoteAddrHandle remote,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->ReadReduce(channel, local, remote, len, dataType, opType, event));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteHBMToHBMReduceImpl(
    ChannelHandle channel, CcuRemoteAddrHandle remote, CcuLocalAddrHandle local,
    CcuVariableHandle len, HcclDataType dataType,
    HcclReduceOp opType, CcuEventHandle event)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WriteReduce(channel, remote, local, len, dataType, opType, event));
    return CcuResult::CCU_SUCCESS;
}

/*========== 远端同步操作 ==========*/
CcuResult CcuWriteNotifyImpl(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WriteNotify(channel, remoteNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuWriteVariableWithNotifyImpl(ChannelHandle channel, CcuVariableHandle varHandle,
    uint32_t remoteVarIdx, uint32_t remoteNotifyIdx, uint32_t mask)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->WriteVariableWithNotify(channel, varHandle, remoteVarIdx, remoteNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuNotifyWaitImpl(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask)
{
    const uint32_t devLogicId = HcclGetThreadDeviceId();
    auto kernel = hcomm::CcuKernelMgr::GetInstance(devLogicId).GetCurrentKernel();
    CCU_CHK_PTR_NULL(kernel);
    CCU_CHK_RET(kernel->NotifyWait(channel, localNotifyIdx, mask));
    return CcuResult::CCU_SUCCESS;
}