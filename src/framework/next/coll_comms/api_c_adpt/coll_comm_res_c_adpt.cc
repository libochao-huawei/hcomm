/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../rank/my_rank.h"
#include "hccl_comm_pub.h"
#include "exception_handler.h"
#include "env_config.h"
using namespace hccl;
/**
 * @note 职责：集合通信的通信域资源管理的C接口的C到C++适配
 */

/**
 * @note C接口适配参考示例
 * @code {.c}
 * HcclResult HcclThreadAcquire(HcclComm comm, CommEngine engine, uint32_t threadNum,
 *     uint32_t notifyNumPerThread, ThreadHandle *threads) {
 *     return HCCL_SUCCESS;
 * }
 * @endcode
 */

const uint32_t HCCL_CHANNEL_VERSION_ONE = 1;
HcclResult ProcessHcclResPackReq(const HcclChannelDesc &channelDesc, HcclChannelDesc &channelDescFinal)
{
    if (channelDesc.header.size < channelDescFinal.header.size) {
        // 需要前向兼容HcclChannelDesc，末尾部分字段不支持处理
    } else if (channelDesc.header.size > channelDescFinal.header.size) {
        // 需要后向向兼容HcclChannelDesc，末尾部分字段会被忽略
    }
 
    if (channelDesc.header.magicWord != channelDescFinal.header.magicWord) {
        HCCL_ERROR("[%s]channelDescFinal.header.magicWord[%u] not equal to channelDesc.header.magicWord[%u]",
            __func__, channelDescFinal.header.magicWord, channelDesc.header.magicWord);
        return HCCL_E_PARA;
    }
 
    uint32_t copySize = (channelDescFinal.header.size < channelDesc.header.size ?
        channelDescFinal.header.size : channelDesc.header.size) - sizeof(CommAbiHeader);
    CHK_SAFETY_FUNC_RET(memcpy_s(reinterpret_cast<uint8_t *>(&channelDescFinal) + sizeof(CommAbiHeader), copySize,
        reinterpret_cast<const uint8_t *>(&channelDesc) + sizeof(CommAbiHeader), copySize));
 
    if (channelDesc.header.version >= HCCL_CHANNEL_VERSION_ONE) {
        channelDescFinal.remoteRank = channelDesc.remoteRank;
        channelDescFinal.channelProtocol   = channelDesc.channelProtocol;
        channelDescFinal.localEndpoint  = channelDesc.localEndpoint;
        channelDescFinal.remoteEndpoint  = channelDesc.remoteEndpoint;
        channelDescFinal.notifyNum  = channelDesc.notifyNum;
        channelDescFinal.memHandles  = channelDesc.memHandles;
        channelDescFinal.memHandleNum  = channelDesc.memHandleNum;
 
        // 根据协议类型拷贝union中的相应成员
        switch (channelDesc.channelProtocol) {
            case COMM_PROTOCOL_HCCS:
            case COMM_PROTOCOL_PCIE:
            case COMM_PROTOCOL_SIO:
            case COMM_PROTOCOL_UBC_CTP:
                break;
            case COMM_PROTOCOL_ROCE:
                channelDescFinal.roceAttr.queueNum = (channelDesc.roceAttr.queueNum == INVALID_UINT) ? GetExternalInputQpsPerConnection() : channelDesc.roceAttr.queueNum;
                channelDescFinal.roceAttr.retryCnt = (channelDesc.roceAttr.retryCnt == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaRetryCnt() : channelDesc.roceAttr.retryCnt;
                channelDescFinal.roceAttr.retryInterval = (channelDesc.roceAttr.retryInterval == INVALID_UINT) ? EnvConfig::GetExternalInputRdmaTimeOut() : channelDesc.roceAttr.retryInterval;
                channelDescFinal.roceAttr.tc = (channelDesc.roceAttr.tc == 0xFF) ? EnvConfig::GetExternalInputRdmaTrafficClass() : channelDesc.roceAttr.tc;
                channelDescFinal.roceAttr.sl = (channelDesc.roceAttr.sl == 0xFF) ? EnvConfig::GetExternalInputRdmaServerLevel() : channelDesc.roceAttr.sl;
                HCCL_INFO("[%s]queueNum[%u], retryCnt[%u], retryInterval[%u], tc[%u], sl[%u]", __func__,
                    channelDescFinal.roceAttr.queueNum, channelDescFinal.roceAttr.retryCnt, channelDescFinal.roceAttr.retryInterval,
                    channelDescFinal.roceAttr.tc, channelDescFinal.roceAttr.sl);
                break;
            default:
                HCCL_ERROR("[%s]Unsupported protocol[%d] found in HcclChannelDesc.", __func__, channelDesc.channelProtocol);
                return HCCL_E_PARA;
        }
    }
 
    if (channelDesc.header.version > HCCL_CHANNEL_VERSION) {
        // 传入的版本高于当前版本，警告不支持的配置项将被忽略
        HCCL_WARNING("The version of provided [%u] is higher than the current version[%u], "
            "unsupported configuration will be ignored.",
            channelDesc.header.version, HCCL_CHANNEL_VERSION);
    } else if (channelDesc.header.version < HCCL_CHANNEL_VERSION) {
        // 传入的版本低于当前版本，警告高版本支持的配置项将被忽略
        HCCL_WARNING("The version of provided [%u] is lower than the current version[%u], "
            "configurations supported by later versions will be ignored.",
            channelDesc.header.version, HCCL_CHANNEL_VERSION);
    }
 
    // 如果扩展到version=2后
    // 1) 在底层为新的结构体和版本（version为2）上，会正常执行下面的判断处理逻辑；
    // 2) 在底层为旧的结构体和版本（version为1）上，下面的逻辑没有，version的2 > 1的部分会被忽略掉；
    if (channelDesc.header.version >= 2) {
    }
 
    return HCCL_SUCCESS;
}
 
 
constexpr uint32_t CHANNEL_NUM_MAX = 1024 * 1024;  // channel的默认限制最大为1024 * 1024
 
HcclResult HcclChannelAcquire(HcclComm comm, CommEngine engine, 
    const HcclChannelDesc* channelDescs, uint32_t channelNum, ChannelHandle* channels)
{
    EXCEPTION_HANDLE_BEGIN
    for (uint32_t idx = 0; idx < channelNum; idx++) {
        HCCL_INFO("HcclChannelAcquire idx[%u], local[%d], remote[%d]",
            idx, channelDescs[idx].localEndpoint.loc.locType, channelDescs[idx].remoteEndpoint.loc.locType);
    }

    // 入参校验
    CHK_PTR_NULL(comm);
    CHK_PTR_NULL(channelDescs);
    CHK_PTR_NULL(channels);
    CHK_PRT_RET(
        (channelNum == 0 || channelNum > CHANNEL_NUM_MAX), 
        HCCL_ERROR("[%s]Invalid channelNum, channelNum[%u], max channel num[%u]",
        __func__, channelNum, CHANNEL_NUM_MAX), HCCL_E_PARA
    );
 
    HcclResult ret = HCCL_SUCCESS;
    hccl::hcclComm *hcclComm = static_cast<hccl::hcclComm *>(comm);
    std::vector<HcclChannelDesc> channelDescFinals;
    for (uint32_t idx = 0; idx < channelNum; idx++) {
        HcclChannelDesc channelDescFinal;
        HcclChannelDescInit(&channelDescFinal, 1);
        ret = ProcessHcclResPackReq(channelDescs[idx], channelDescFinal);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[%s] Failed check channelDesc, channelDesc idx[%u], group[%s], engine[%d], "
                "channelNum[%llu], ret[%d]", __func__, idx, hcclComm->GetIdentifier().c_str(),
                engine, channelNum, ret);
            return ret;
        }
        channelDescFinals.push_back(channelDescFinal);
    }
 
    if (hcclComm->IsCommunicatorV2()) {  // A5
        hccl::CollComm* collComm = hcclComm->GetCollComm();
        CHK_PTR_NULL(collComm);
        const std::string &commTag = hcclComm->GetIdentifier();
        hccl::MyRank* myRank = collComm->GetMyRank();
        CHK_PTR_NULL(myRank);
 
        CHK_RET(myRank->CreateChannels(engine, commTag, channelDescFinals.data(), channelNum, channels));
    } else {
        auto& channelMgr = hcclComm->GetIndependentOp().GetChannelManager();
        ret = channelMgr.ChannelCommCreate(hcclComm->GetIdentifier(), engine,
            channelDescFinals.data(), channelNum, channels);
    }
 
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s] Failed to acquire channel, group[%s], engine[%d], channelNum[%llu], ret[%d]",
           __func__, hcclComm->GetIdentifier().c_str(), engine, channelNum, ret);
        return ret;
    }
 
    HCCL_RUN_INFO("[%s] acquire channel success, group[%s], engine[%d], channelNum[%llu], ret[%d]", 
        __func__, hcclComm->GetIdentifier().c_str(), engine, channelNum, ret);
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}