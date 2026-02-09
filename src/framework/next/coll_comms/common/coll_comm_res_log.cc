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
#include <string>
#include <array>

/**
 * @brief 获取 CommAddr 类型的描述字符串
 * @param type 地址类型
 * @return 类型描述字符串（如 "IPv4", "IPv6", "ID", "EID"）
 */
static std::string GetCommAddrTypeString(CommAddrType type)
{
    switch (type) {
        case COMM_ADDR_TYPE_IP_V4:
            return "IPv4";
        case COMM_ADDR_TYPE_IP_V6:
            return "IPv6";
        case COMM_ADDR_TYPE_ID:
            return "ID";
        case COMM_ADDR_TYPE_EID:
            return "EID";
        default:
            return "Unknown(" + std::to_string(type) + ")";
    }
}

/**
 * @brief 将 CommAddr 转换为带类型的 IP 地址字符串
 * @param commAddr 通信地址
 * @return 地址字符串，格式为 "[type] addr"
 *
 * @note 对于 IPv4 地址，返回 "[IPv4] 192.168.1.1"
 * @note 对于 IPv6 地址，返回 "[IPv6] fe80::204:61ff:fe9d:f156"
 * @note 对于 ID 类型，返回 "[ID] id:0x12345678"
 */
std::string CommAddrToString(const CommAddr& commAddr)
{
    const char* funcName = __func__;
    std::string typeStr = GetCommAddrTypeString(commAddr.type);

    switch (commAddr.type) {
        case COMM_ADDR_TYPE_IP_V4: {
            std::array<char, INET_ADDRSTRLEN> buffer;
            const char* result = inet_ntop(AF_INET, &commAddr.addr, buffer.data(), buffer.size());
            if (result == nullptr) {
                HCCL_ERROR("[%s] inet_ntop failed for IPv4 address", funcName);
                return "[" + typeStr + "] conversion failed";
            }
            return "[" + typeStr + "] " + std::string(result);
        }
        case COMM_ADDR_TYPE_IP_V6: {
            std::array<char, INET6_ADDRSTRLEN> buffer;
            const char* result = inet_ntop(AF_INET6, &commAddr.addr6, buffer.data(), buffer.size());
            if (result == nullptr) {
                HCCL_ERROR("[%s] inet_ntop failed for IPv6 address", funcName);
                return "[" + typeStr + "] conversion failed";
            }
            return "[" + typeStr + "] " + std::string(result);
        }
        case COMM_ADDR_TYPE_ID: {
            // ID 类型：返回格式化字符串
            return "[" + typeStr + "] id:0x" + std::to_string(commAddr.id);
        }
        case COMM_ADDR_TYPE_EID: {
            // EID 类型：返回十六进制字符串
            char buffer[64];
            (void)snprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1,
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                commAddr.eid[0], commAddr.eid[1], commAddr.eid[2], commAddr.eid[3],
                commAddr.eid[4], commAddr.eid[5], commAddr.eid[6], commAddr.eid[7],
                commAddr.eid[8], commAddr.eid[9], commAddr.eid[10], commAddr.eid[11],
                commAddr.eid[12], commAddr.eid[13], commAddr.eid[14], commAddr.eid[15]);
            return "[" + typeStr + "] eid:" + std::string(buffer);
        }
        default: {
            // 保留类型或其他：返回通用格式
            return "[" + typeStr + "]";
        }
    }
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
    std::string addrStr = CommAddrToString(commAddr);
    HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], addr[%s]",
        funcName, idx, endpointName, commAddr.type, addrStr.c_str());
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
 * @param localRank 本地 rank
 */
void PrintChannelErrorTableHeader(uint32_t localRank)
{
    HCCL_ERROR("   ________________________________________________CHANNEL_CONNECT_ERROR_INFO________________________________________________");
    HCCL_ERROR("   |  comm error, localRank[%u]", localRank);
    HCCL_ERROR("   |  idx  | localRank |              localAddr               | remoteRank |             remoteAddr             | chHandle  |            Status            | Proto | elapsed |");
    HCCL_ERROR("   |-------|-----------|-------------------------------------|------------|-------------------------------------|-----------|----------------------------|-------|---------|");
}

/**
 * @brief 将 ChannelStatus 状态值转换为可读字符串
 * @param status 状态值
 * @return 状态描述字符串（如 "ChannelStatus::SOCKET_TIMEOUT"）
 */
std::string ChannelStatusToString(int32_t status)
{
    hcomm::ChannelStatus::Value statusValue = static_cast<hcomm::ChannelStatus::Value>(status);
    hcomm::ChannelStatus statusEnum(statusValue);
    return statusEnum.Describe();
}

/**
 * @brief 打印单个 Channel 的错误状态
 * @param idx channel 索引
 * @param localRank 本地 rank
 * @param channelDesc channel 描述符
 * @param channelHandle channel 句柄
 * @param status 连接状态
 * @param elapsedMs 经过的时间（毫秒）
 */
void PrintChannelErrorInfo(
    uint32_t idx,
    uint32_t localRank,
    const HcclChannelDesc& channelDesc,
    ChannelHandle channelHandle,
    int32_t status,
    uint64_t elapsedMs)
{
    std::string localAddr = CommAddrToString(channelDesc.localEndpoint.commAddr);
    std::string remoteAddr = CommAddrToString(channelDesc.remoteEndpoint.commAddr);
    std::string statusStr = ChannelStatusToString(status);

    HCCL_ERROR("   | %5u | %9u | %35s | %10u | %35s | 0x%08llx | %26s | %5d | %7llu |",
        idx,
        localRank,
        localAddr.c_str(),
        channelDesc.remoteRank,
        remoteAddr.c_str(),
        static_cast<unsigned long long>(channelHandle),
        statusStr.c_str(),
        channelDesc.channelProtocol,
        static_cast<unsigned long long>(elapsedMs));
}
