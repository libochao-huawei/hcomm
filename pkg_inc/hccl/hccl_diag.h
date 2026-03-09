/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_DIAG_H
#define HCCL_DIAG_H

#include <cstddef>
#include <hccl/hccl_types.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
/**
 * @brief 注册算子信息的DFX接口
 * @param[in] comm 通信域句柄，标识当前通信上下文
 * @param[in] HcclDfxOpInfo 算子信息结构体，包含算子对象，通信操作标签名等
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclDfxRegOpInfo(HcclComm comm, void* dfxOpInfo);
/**
 * @brief 算子上报性能数据（开始时间戳）
 * @param[in] beginTime 算子开始执行的时间戳
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclProfilingReportOp(HcclComm comm, uint64_t beginTime);

/**
 * @brief kernel上报
 * @param[in] comm 通信域句柄，标识当前通信上下文
 * @param[in] beginTime 算子开始执行的时间戳
 * @param[in] thread 线程上下文
 * @return HcclResult 执行结果状态码
 * @note host侧
 */
extern HcclResult HcclReportAicpuKernel(HcclComm comm, uint64_t beginTime, uint64_t thread);

extern uint64_t HcommGetProfilingSysCycleTime();

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif