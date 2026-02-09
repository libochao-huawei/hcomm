/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "channel_logger.h"
#include "comm_addr_logger.h"
#include "endpoint_logger.h"
#include "hccl_comm_pub.h"
#include "../endpoint_pairs/channels/channel.h"  // ChannelStatus 枚举定义

namespace hcomm {
namespace logger {

void ChannelLogger::PrintBasicFields(uint32_t idx, const HcclChannelDesc& channelDesc)
{
    HCCL_INFO("[%s] channelDescs[%u]: remoteRank[%u], channelProtocol[%d], notifyNum[%u], memHandleNum[%u]",
        __func__, idx, channelDesc.remoteRank, channelDesc.channelProtocol,
        channelDesc.notifyNum, channelDesc.memHandleNum);
}

void ChannelLogger::PrintRoceAttributes(uint32_t idx, const HcclChannelDesc& channelDesc)
{
    if (channelDesc.channelProtocol == COMM_PROTOCOL_ROCE) {
        HCCL_INFO("[%s] channelDescs[%u] roceAttr: queueNum[%u], retryCnt[%u], retryInterval[%u], tc[%u], sl[%u]",
            __func__, idx, channelDesc.roceAttr.queueNum, channelDesc.roceAttr.retryCnt,
            channelDesc.roceAttr.retryInterval, channelDesc.roceAttr.tc, channelDesc.roceAttr.sl);
    }
}

void ChannelLogger::PrintDescInfo(uint32_t idx, const HcclChannelDesc& channelDesc)
{
    // 基本字段
    PrintBasicFields(idx, channelDesc);

    // 本地端点
    EndpointLogger::Print(idx, "localEndpoint", channelDesc.localEndpoint);

    // 远端端点
    EndpointLogger::Print(idx, "remoteEndpoint", channelDesc.remoteEndpoint);

    // ROCE 协议特有属性
    PrintRoceAttributes(idx, channelDesc);
}

void ChannelLogger::PrintDescTableHeader()
{
    HCCL_INFO("   _________________________________________________CHANNEL_DESCRIPTOR_INFO_________________________________________________");
    HCCL_INFO("   |  idx  | remoteRank | Proto |  notifyNum  | memHandleNum |              localAddr               |             remoteAddr             |           ROCE Attr            |");
    HCCL_INFO("   |-------|------------|-------|-------------|--------------|-------------------------------------|-------------------------------------|--------------------------------|");
}

void ChannelLogger::PrintDescInfoRow(uint32_t idx, const HcclChannelDesc& channelDesc)
{
    // 转换地址为字符串
    std::string localAddr = CommAddrLogger::ToString(channelDesc.localEndpoint.commAddr);
    std::string remoteAddr = CommAddrLogger::ToString(channelDesc.remoteEndpoint.commAddr);

    // 格式化 ROCE 属性
    char roceAttrStr[128] = "-";
    if (channelDesc.channelProtocol == COMM_PROTOCOL_ROCE) {
        snprintf_s(roceAttrStr, sizeof(roceAttrStr), sizeof(roceAttrStr) - 1,
            "q:%u r:%u ri:%u tc:%u sl:%u",
            channelDesc.roceAttr.queueNum,
            channelDesc.roceAttr.retryCnt,
            channelDesc.roceAttr.retryInterval,
            channelDesc.roceAttr.tc,
            channelDesc.roceAttr.sl);
    }

    // 输出表格行
    HCCL_INFO("   | %5u | %10u | %5d | %11u | %12u | %35s | %35s | %30s |",
        idx,
        channelDesc.remoteRank,
        channelDesc.channelProtocol,
        channelDesc.notifyNum,
        channelDesc.memHandleNum,
        localAddr.c_str(),
        remoteAddr.c_str(),
        roceAttrStr);
}

void ChannelLogger::PrintErrorTableHeader(uint32_t localRank)
{
    HCCL_ERROR("   ________________________________________________CHANNEL_CONNECT_ERROR_INFO________________________________________________");
    HCCL_ERROR("   |  comm error, localRank[%u]", localRank);
    HCCL_ERROR("   |  idx  | localRank |              localAddr               | remoteRank |             remoteAddr             | chHandle  |            Status            | Proto | elapsed |");
    HCCL_ERROR("   |-------|-----------|-------------------------------------|------------|-------------------------------------|-----------|----------------------------|-------|---------|");
}

std::string ChannelStatusUtils::ToString(int32_t status)
{
    ChannelStatus::Value statusValue = static_cast<ChannelStatus::Value>(status);
    ChannelStatus statusEnum(statusValue);
    return statusEnum.Describe();
}

void ChannelLogger::PrintErrorInfo(
    uint32_t idx,
    uint32_t localRank,
    const HcclChannelDesc& channelDesc,
    ChannelHandle channelHandle,
    int32_t status,
    uint64_t elapsedMs)
{
    std::string localAddr = CommAddrLogger::ToString(channelDesc.localEndpoint.commAddr);
    std::string remoteAddr = CommAddrLogger::ToString(channelDesc.remoteEndpoint.commAddr);
    std::string statusStr = ChannelStatusUtils::ToString(status);

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

void ChannelLogger::PrintChannelErrorDetails(
    uint32_t localRank,
    uint32_t channelNum,
    const HcclChannelDesc* channelDescs,
    ChannelHandle* channelHandles,
    int32_t* statusList,
    int64_t elapsedMs)
{
    // 打印错误详情表格（只打印异常状态的 Channel）
    PrintErrorTableHeader(localRank);

    for (uint32_t i = 0; i < channelNum; ++i) {
        if (statusList[i] != ChannelStatus::READY) {
            PrintErrorInfo(i, localRank, channelDescs[i], channelHandles[i], statusList[i], elapsedMs);
        }
    }

    // 表格外单独打印详细信息（FAILED 或 TIMEOUT 状态）
    for (uint32_t i = 0; i < channelNum; ++i) {
        if (statusList[i] == ChannelStatus::FAILED ||
            statusList[i] == ChannelStatus::SOCKET_TIMEOUT) {
            PrintDescInfo(i, channelDescs[i]);
        }
    }
}

} // namespace logger
} // namespace hcomm
