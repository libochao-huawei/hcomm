/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCL_RES_EXP_H
#define HCCL_RES_EXP_H

#include <stdint.h>
#include "hccl/hccl_types.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @brief 算子包向通信域注入建链时待交换的一致性校验信息
 * @param[in] comm 通信域句柄
 * @param[in] data 待交换数据指针
 * @param[in] length 数据长度
 * @return HcclResult 执行结果状态码
 * @note 每次建链前只能调用一次，交换信息在HcclChannelAcquire执行后被自动清空
 */
extern HcclResult HcclCommAddExchangeInfo(HcclComm comm, void* data, uint32_t length);

/**
 * @brief 算子包获取通信域建链时从对端rank交换到的一致性校验信息
 * @param[in] comm 通信域句柄
 * @param[in] remoteRank 对端rank ID
 * @param[out] data 接收数据的缓冲区
 * @param[out] length 缓冲区长度
 * @return HcclResult 执行结果状态码
 * @note 读取后自动清空该remoteRank的交换信息，不可重复读取，在HcclChannelAcquire成功返回后调用
 */
extern HcclResult HcclCommGetExchangeInfo(HcclComm comm, uint32_t remoteRank, void* data, uint32_t &length);

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // HCCL_RES_EXP_H
