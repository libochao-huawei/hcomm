/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CCU_DATA_API_H
#define CCU_DATA_API_H

#include <stdint.h>

#include "hccl_types.h"
#include "hcomm_primitives.h"

#include "ccu_types.h"
#include "ccu_data_resource.h"

#ifdef __cplusplus
#include "ccu_loop_macro.h"
class CcuVariable;
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @defgroup ccu数据面编程接口
 * @{
 */



// LocalAddr / RemoteAddr 创建
extern CcuResult CcuLocalAddrCreate(CcuLocalAddr* localAddr);
extern CcuResult CcuRemoteAddrCreate(CcuRemoteAddr* remoteAddr);

// LocalAddr → Buffer（读入 Buffer）
extern CcuResult CcuLocalCopyHBMToBuffer(
    CcuBuffer dstBuffer, CcuLocalAddr src,
    CcuVariable len, CcuEvent event);
// Buffer → LocalAddr
extern CcuResult CcuLocalCopyBufferToHBM(
    CcuLocalAddr dst, CcuBuffer srcBuffer,
    CcuVariable len, CcuEvent event);
// LocalAddr → LocalAddr
extern CcuResult CcuLocalCopyHBMToHBM(
    CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, CcuEvent event);
/*========== 本地 Reduce ==========*/

// LocalAddr → LocalAddr Reduce
extern CcuResult CcuLocalHBMReduce(
    CcuLocalAddr dst, CcuLocalAddr src,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event);

// 多 Buffer Reduce
extern CcuResult CcuLocalBufferReduce(
    CcuBuffer* buffers, uint32_t count,
    HcclDataType dataType, HcclDataType outputDataType,
    HcclReduceOp opType,
    CcuVariable len, CcuEvent event);

/*========== 远端数据传输操作 ==========*/
extern CcuResult CcuReadHBMToHBM(
    ChannelHandle channel, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event);
extern CcuResult CcuReadHBMToBuffer(
    ChannelHandle channel, CcuBuffer local, CcuRemoteAddr remote,
    CcuVariable len, CcuEvent event);
extern CcuResult CcuReadHBMToHBMReduce(
    ChannelHandle channel, CcuLocalAddr local, CcuRemoteAddr remote,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event);
extern CcuResult CcuWriteHBMToHBM(
    ChannelHandle channel, CcuRemoteAddr remote,CcuLocalAddr local, 
    CcuVariable len, CcuEvent event);
extern CcuResult CcuWriteBufferToHBM(
    ChannelHandle channel, CcuRemoteAddr remote, CcuBuffer local,
    CcuVariable len, CcuEvent event);
extern CcuResult CcuWriteHBMToHBMReduce(
    ChannelHandle channel, CcuRemoteAddr remote, CcuLocalAddr local,
    CcuVariable len, HcclDataType dataType,
    HcclReduceOp opType, CcuEvent event);


/**
 * @brief 远端同步操作
 * @param[in] channel 链路句柄
 * 
 * @return HcclResult 执行结果状态码
 * @note x
 * @warning
 */
// extern CcuResult CcuNotifyRecord(ChannelHandle channel, uint32_t remoteNotifyIdx, uint32_t mask);

/**
 * @brief 远端同步操作
 * @param[in] channel 链路句柄
 * 
 * @return HcclResult 执行结果状态码
 * @note x
 * @warning
 */
// extern CcuResult CcuNotifyWait(ChannelHandle channel, uint32_t localNotifyIdx, uint32_t mask);


// LoopEngine
extern CcuResult CcuCreateBlockExecutor(CcuLoopExecutors *pool, uint32_t count);

// Loop
extern CcuResult CcuLoopCreate(CcuLoop *loop);
extern CcuResult _CcuLoopBodyEnter(CcuLoop loop);
extern CcuResult _CcuLoopBodyExit(CcuLoop loop);
extern CcuResult CcuLoopSetParam(CcuLoop loop,
    CcuVariable *formalParam, CcuVariable *actualParam);
extern CcuResult CcuLoopGroupCreate(CcuLoopGroup *group,
    const CcuLoopGroupConfig *config, CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupCreateFromVar(CcuLoopGroup *group,
    CcuVariable *parallelVar, CcuVariable *offsetVar,
    CcuLoopExecutors enginePool);
extern CcuResult CcuLoopGroupAddLoop(CcuLoopGroup group,
    CcuLoop loop, const CcuLoopConfig *config);
extern CcuResult CcuLoopGroupAddLoopFromVar(CcuLoopGroup group,
    CcuLoop loop, CcuVariable *loopParamVar);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CCU_DATA_API_H