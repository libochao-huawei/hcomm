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

/**
 * @brief 打印 commAddr 详情（本地或远端端点的通信地址）
 * @param idx channel 索引
 * @param endpointName 端点名称（"localEndpoint" 或 "remoteEndpoint"）
 * @param commAddr 通信地址
 */
void PrintCommAddr(uint32_t idx, const char* endpointName, const CommAddr& commAddr)
{
    const char* funcName = __func__;
    if (commAddr.type == COMM_ADDR_TYPE_IP_V4 || commAddr.type == COMM_ADDR_TYPE_ID) {
        HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], addr[0x%x]",
            funcName, idx, endpointName, commAddr.type, commAddr.id);
    } else if (commAddr.type == COMM_ADDR_TYPE_EID) {
        HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], eid[0x%x:0x%x:0x%x:0x%x]",
            funcName, idx, endpointName, commAddr.type,
            *(uint32_t*)&commAddr.eid[0], *(uint32_t*)&commAddr.eid[4],
            *(uint32_t*)&commAddr.eid[8], *(uint32_t*)&commAddr.eid[12]);
    } else {
        HCCL_INFO("[%s] channelDescs[%u] %s commAddr: type[%d], raws[0x%x:0x%x:0x%x:0x%x]",
            funcName, idx, endpointName, commAddr.type,
            *(uint32_t*)&commAddr.raws[0], *(uint32_t*)&commAddr.raws[4],
            *(uint32_t*)&commAddr.raws[8], *(uint32_t*)&commAddr.raws[12]);
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
