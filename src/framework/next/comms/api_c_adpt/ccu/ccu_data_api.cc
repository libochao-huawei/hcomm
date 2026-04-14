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

