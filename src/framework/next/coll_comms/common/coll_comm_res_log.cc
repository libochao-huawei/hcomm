/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_comm_res_log.h"
#include "hccl_comm_pub.h"
#include "hcomm_res_defs.h"  // ChannelHandle
#include "../endpoint_pairs/channels/channel.h"  // ChannelStatus 枚举定义
#include <arpa/inet.h>  // inet_ntop
#include <malloc.h>     // malloc, free

/**
 * @brief 将 CommAddr 转换为 IP 地址字符串
 * @param commAddr 通信地址
 * @return IP 地址字符串（需要调用者释放）
 *
 * @note 对于 IPv4 地址，返回点分十进制格式（如 "192.168.1.1"）
 * @note 对于 IPv6 地址，返回冒号分隔格式（如 "fe80::204:61ff:fe9d:f156"）
 * @note 对于非 IP 类型，返回十六进制字符串（如 "id:0x12345678"）
 * @note 调用者需要使用 free() 释放返回的字符串
 */
char* CommAddrToString(const CommAddr& commAddr)
{
    const char* funcName = __func__;
    char* buffer = nullptr;

    switch (commAddr.type) {
        case COMM_ADDR_TYPE_IP_V4: {
            buffer = (char*)malloc(INET_ADDRSTRLEN);
            if (buffer == nullptr) {
                HCCL_ERROR("[%s] malloc failed for IPv4 address", funcName);
                return nullptr;
            }
            const char* result = inet_ntop(AF_INET, &commAddr.addr, buffer, INET_ADDRSTRLEN);
            if (result == nullptr) {
                HCCL_ERROR("[%s] inet_ntop failed for IPv4 address", funcName);
                free(buffer);
                return nullptr;
            }
            break;
        }
        case COMM_ADDR_TYPE_IP_V6: {
            buffer = (char*)malloc(INET6_ADDRSTRLEN);
            if (buffer == nullptr) {
                HCCL_ERROR("[%s] malloc failed for IPv6 address", funcName);
                return nullptr;
            }
            const char* result = inet_ntop(AF_INET6, &commAddr.addr6, buffer, INET6_ADDRSTRLEN);
            if (result == nullptr) {
                HCCL_ERROR("[%s] inet_ntop failed for IPv6 address", funcName);
                free(buffer);
                return nullptr;
            }
            break;
        }
        case COMM_ADDR_TYPE_ID: {
            // ID 类型：返回格式化字符串
            const int idStrLen = 32;
            buffer = (char*)malloc(idStrLen);
            if (buffer == nullptr) {
                HCCL_ERROR("[%s] malloc failed for ID address", funcName);
                return nullptr;
            }
            (void)snprintf_s(buffer, idStrLen, idStrLen - 1, "id:0x%x", commAddr.id);
            break;
        }
        case COMM_ADDR_TYPE_EID: {
            // EID 类型：返回十六进制字符串
            const int eidStrLen = 64;
            buffer = (char*)malloc(eidStrLen);
            if (buffer == nullptr) {
                HCCL_ERROR("[%s] malloc failed for EID address", funcName);
                return nullptr;
            }
            (void)snprintf_s(buffer, eidStrLen, eidStrLen - 1,
                "eid:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                commAddr.eid[0], commAddr.eid[1], commAddr.eid[2], commAddr.eid[3],
                commAddr.eid[4], commAddr.eid[5], commAddr.eid[6], commAddr.eid[7],
                commAddr.eid[8], commAddr.eid[9], commAddr.eid[10], commAddr.eid[11],
                commAddr.eid[12], commAddr.eid[13], commAddr.eid[14], commAddr.eid[15]);
            break;
        }
        default: {
            // 保留类型或其他：返回通用格式
            const int rawStrLen = 128;
            buffer = (char*)malloc(rawStrLen);
            if (buffer == nullptr) {
                HCCL_ERROR("[%s] malloc failed for unknown address type", funcName);
                return nullptr;
            }
            (void)snprintf_s(buffer, rawStrLen, rawStrLen - 1, "unknown(type:%d)", commAddr.type);
            break;
        }
    }

    return buffer;
}

/**
 * @brief 打印 commAddr 详情（本地或远端端点的通信地址）
 * @param idx channel 索引
 * @param endpointName 端点名称（"localEndpoint" 或 "remoteEndpoint"）
 * @param commAddr 通信地址
 */
void PrintCommAddr(uint32_t idx, const char* endpointName, const CommAddr& commAddr)
{
    const char* funcName = __func__;
    char* addrStr = CommAddrToString(commAddr);
    if (addrStr != nullptr) {
        HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], addr[%s]",
            funcName, idx, endpointName, commAddr.type, addrStr);
        free(addrStr);
    } else {
        HCCL_ERROR("[%s] channelDescs[%u] %s commAddr: failed to convert to string",
            funcName, idx, endpointName);
    }
}

/**
 * @brief 打印端点位置信息（本地或远端端点的位置）
 * @param idx channel 索引
 * @param endpointName 端点名称（"localEndpoint" 或 "remoteEndpoint"）
 * @param loc 端点位置
 */
void PrintEndpointLoc(uint32_t idx, const char* endpointName, const EndpointLoc& loc)
{
    const char* funcName = __func__;
    if (loc.locType == ENDPOINT_LOC_TYPE_DEVICE) {
        HCCL_INFO("[%s] channelDescs[%u] %s loc: locType[%d], devPhyId[%u], superDevId[%u], serverIdx[%u], superPodIdx[%u]",
            funcName, idx, endpointName, loc.locType,
            loc.device.devPhyId, loc.device.superDevId,
            loc.device.serverIdx, loc.device.superPodIdx);
    } else if (loc.locType == ENDPOINT_LOC_TYPE_HOST) {
        HCCL_INFO("[%s] channelDescs[%u] %s loc: locType[%d], host.id[%u]",
            funcName, idx, endpointName, loc.locType, loc.host.id);
    } else {
        HCCL_INFO("[%s] channelDescs[%u] %s loc: locType[%d]",
            funcName, idx, endpointName, loc.locType);
    }
}

/**
 * @brief 打印 HcclChannelDesc 详细信息（调测辅助函数）
 * @param idx channel 索引
 * @param channelDesc channel 描述符
 */
void PrintChannelDescInfo(uint32_t idx, const HcclChannelDesc& channelDesc)
{
    const char* funcName = __func__;
    // 基本字段
    HCCL_INFO("[%s] channelDescs[%u]: remoteRank[%u], channelProtocol[%d], notifyNum[%u], memHandleNum[%u]",
        funcName, idx, channelDesc.remoteRank, channelDesc.channelProtocol,
        channelDesc.notifyNum, channelDesc.memHandleNum);

    // 本地端点
    PrintCommAddr(idx, "localEndpoint", channelDesc.localEndpoint.commAddr);
    PrintEndpointLoc(idx, "localEndpoint", channelDesc.localEndpoint.loc);

    // 远端端点
    PrintCommAddr(idx, "remoteEndpoint", channelDesc.remoteEndpoint.commAddr);
    PrintEndpointLoc(idx, "remoteEndpoint", channelDesc.remoteEndpoint.loc);

    // ROCE 协议特有属性
    if (channelDesc.channelProtocol == COMM_PROTOCOL_ROCE) {
        HCCL_INFO("[%s] channelDescs[%u] roceAttr: queueNum[%u], retryCnt[%u], retryInterval[%u], tc[%u], sl[%u]",
            funcName, idx, channelDesc.roceAttr.queueNum, channelDesc.roceAttr.retryCnt,
            channelDesc.roceAttr.retryInterval, channelDesc.roceAttr.tc, channelDesc.roceAttr.sl);
    }
}

/**
 * @brief 打印 Channel 连接错误信息表格头部
 */
void PrintChannelErrorTableHeader(uint32_t localRank)
{
    HCCL_ERROR("   _________________________CHANNEL_CONNECT_ERROR_INFO___________________________");
    HCCL_ERROR("   |  comm error, localRank[%u]", localRank);
    HCCL_ERROR("   |  idx  | localRank | remoteRank | channelHandle |            Status            | Protocol |  elapsed  |");
    HCCL_ERROR("   |-------|-----------|------------|---------------|----------------------------|----------|-----------|");
}

/**
 * @brief 将 ChannelStatus 状态值转换为可读字符串
 */
const char* ChannelStatusToString(int32_t status)
{
    hcomm::ChannelStatus::Value statusValue = static_cast<hcomm::ChannelStatus::Value>(status);
    hcomm::ChannelStatus statusEnum(statusValue);
    // 返回静态字符串以避免生命周期问题
    static thread_local std::string statusStr;
    statusStr = statusEnum.Describe();  // 返回 "ChannelStatus::SOCKET_TIMEOUT" 等
    return statusStr.c_str();
}

/**
 * @brief 打印单个 Channel 的错误状态
 */
void PrintChannelErrorInfo(
    uint32_t idx,
    uint32_t localRank,
    const HcclChannelDesc& channelDesc,
    ChannelHandle channelHandle,
    int32_t status,
    uint64_t elapsedMs)
{
    const char* statusStr = ChannelStatusToString(status);

    HCCL_ERROR("   | %5u | %9u | %10u | 0x%013llx | %26s | %8d | %9llu |",
        idx,
        localRank,
        channelDesc.remoteRank,
        static_cast<unsigned long long>(channelHandle),
        statusStr,
        channelDesc.channelProtocol,
        static_cast<unsigned long long>(elapsedMs));
}
