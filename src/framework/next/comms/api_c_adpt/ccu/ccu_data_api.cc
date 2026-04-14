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

CcuResult CcuLoopCreate(CcuLoop *loop)
{
    return CcuLoopCreateImpl(loop);
}

CcuResult CcuLoopSetParam(CcuLoop loop,
    CcuVariable *formalParam, CcuVariable *actualParam)
{
    if (formalParam == nullptr || actualParam == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopSetParamImpl(loop, formalParam->handle, actualParam->handle);
}

CcuResult CcuCreateBlockExecutor(CcuLoopExecutors *pool, uint32_t count)
{
    return CcuCreateBlockExecutorImpl(pool, count);
}

CcuResult CcuLoopGroupCreate(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool)
{
    return CcuLoopGroupCreateImpl(group, config, enginePool);
}

CcuResult CcuLoopGroupCreateFromVar(CcuLoopGroup *group,
    CcuVariable *parallelVar, CcuVariable *offsetVar,
    CcuLoopExecutors enginePool)
{
    if (parallelVar == nullptr || offsetVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupCreateFromVarImpl(group, parallelVar->handle, offsetVar->handle, enginePool);
}

CcuResult CcuLoopGroupAddLoop(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config)
{
    return CcuLoopGroupAddLoopImpl(group, loop, config);
}

CcuResult CcuLoopGroupAddLoopFromVar(CcuLoopGroup group,
    CcuLoop loop, CcuVariable *loopParamVar)
{
    if (loopParamVar == nullptr) {
        return CcuResult::CCU_E_PTR;
    }
    return CcuLoopGroupAddLoopFromVarImpl(group, loop, loopParamVar->handle);
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

/*
Loop body scope
*/
CcuResult _CcuLoopBodyEnter(CcuLoop loop)
{
    CCU_CHK_RET(_CcuLoopBodyEnterImpl(loop));
    return CcuResult::CCU_SUCCESS;
}

CcuResult _CcuLoopBodyExit(CcuLoop loop)
{
    CCU_CHK_RET(_CcuLoopBodyExitImpl(loop));
    return CcuResult::CCU_SUCCESS;
}
