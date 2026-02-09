/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_COMM_RES_LOG_H
#define COLL_COMM_RES_LOG_H

#include <cstdint>
#include "hccl/hccl_res.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 打印 HcclChannelDesc 详细信息（调测辅助函数）
 * @param idx channel 索引
 * @param channelDesc channel 描述符
 */
void PrintChannelDescInfo(uint32_t idx, const HcclChannelDesc& channelDesc);

/**
 * @brief 打印 commAddr 详情（本地或远端端点的通信地址）
 * @param idx channel 索引
 * @param endpointName 端点名称（"localEndpoint" 或 "remoteEndpoint"）
 * @param commAddr 通信地址
 */
void PrintCommAddr(uint32_t idx, const char* endpointName, const CommAddr& commAddr);

/**
 * @brief 打印端点位置信息（本地或远端端点的位置）
 * @param idx channel 索引
 * @param endpointName 端点名称（"localEndpoint" 或 "remoteEndpoint"）
 * @param loc 端点位置
 */
void PrintEndpointLoc(uint32_t idx, const char* endpointName, const EndpointLoc& loc);

#ifdef __cplusplus
}
#endif

#endif // COLL_COMM_RES_LOG_H
