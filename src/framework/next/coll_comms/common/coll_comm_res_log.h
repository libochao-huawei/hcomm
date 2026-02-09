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

/**
 * @brief 打印 Channel 连接错误信息表格头部
 * @param localRank 本地 Rank ID
 */
void PrintChannelErrorTableHeader(uint32_t localRank);

/**
 * @brief 打印单个 Channel 的错误状态（表格行）
 * @param idx Channel 索引
 * @param localRank 本地 Rank ID
 * @param channelDesc Channel 描述符
 * @param channelHandle Channel 句柄
 * @param status Channel 状态值（int32_t，对应 ChannelStatus 枚举）
 * @param elapsedMs 已耗时（毫秒）
 */
void PrintChannelErrorInfo(
    uint32_t idx,
    uint32_t localRank,
    const HcclChannelDesc& channelDesc,
    ChannelHandle channelHandle,
    int32_t status,
    uint64_t elapsedMs);

/**
 * @brief 将 ChannelStatus 状态值转换为可读字符串（使用 Describe() 方法）
 * @param status ChannelStatus 状态值
 * @return 状态字符串（如 "ChannelStatus::SOCKET_TIMEOUT"）
 */
const char* ChannelStatusToString(int32_t status);

#ifdef __cplusplus
}
#endif

#endif // COLL_COMM_RES_LOG_H
