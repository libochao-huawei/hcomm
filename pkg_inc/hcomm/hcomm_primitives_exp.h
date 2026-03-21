/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCOMM_PRIMITIVES_EXP_H
#define HCOMM_PRIMITIVES_EXP_H

#include <stdint.h>
#include <securec.h>
#include <arpa/inet.h>
#include "acl/acl_rt.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief Get symmetric memory pointer.
 *
 * @param winHandle A pointer identifying the registered memory window handle.
 * @param offset A size_t identifying the offset of symmetric memory heap.
 * @param peerRank A u_integer identifying the identify for the peer rank.
 * @param ptr A pointer identifying the symmetric memory heap address.
 * @return HcclResult
 */
extern HcclResult HcommSymWinGetPeerPointer(CommSymWindow winHandle, size_t offset, uint32_t peerRank, void** ptr);


/**
 * @brief NPU上查询 rtsq任务执行完成的接口（阻塞）
 * @param[in] thread NPU上执行的线程句柄
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommThreadSynchronize(ThreadHandle thread);

using MsgHandle = uint64_t;

/**
 * @brief NPU 通过 HBM 共享内存向 DPU 发送同步消息（非阻塞）
 * @param[in] handle 目的地址，位于 HBM 共享内存
 * @param[in] msgTag 消息（算子任务）标签（char[256])
 * @param[in] src 附加消息源地址
 * @param[in] sizeByte 消息大小（字节）
 * @param[out] msgId 消息 Id 指针
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommSendRequest(MsgHandle handle, const char* msgTag, const void *src, size_t sizeByte, uint32_t *msgId);

/**
 * @brief NPU 通过 HBM 共享内存接收 DPU 同步消息（非阻塞）
 * @param[in] handle 源地址，位于 HBM 共享内存
 * @param[in] dst 读出数据的地址
 * @param[in] sizeByte 数据大小（字节）
 * @param[out] msgId 消息 Id 指针
 * @return int32_t 执行结果状态码
 * 
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommWaitResponse(MsgHandle handle, void *dst, size_t sizeByte, uint32_t *msgId);

/**
 * @brief DPU数据面flush接口
 * @param[in] void
 * @return int32_t 执行结果状态码
 *
 * WARNING: experimental API, No compatibility is currently guaranteed for this API
 */
extern int32_t HcommFlush();

/** @} */  // 算子编程接口
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif