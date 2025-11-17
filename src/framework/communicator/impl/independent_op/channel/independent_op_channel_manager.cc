/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "independent_op_channel_manager.h"
#include <unordered_set>
#include <string>
#include "log.h"

namespace hccl {

ChannelManager::ChannelManager(uint32_t userRank) : userRank_(userRank)
{
}

ChannelManager::~ChannelManager()
{
}

HcclResult ChannelManager::CheckChannelParam(const std::string &tag, CommEngine engine,
    const ChannelDesc *channelDesc, uint32_t descNum)
{
    std::unordered_set<ChannelDesc, std::hash<ChannelDesc>, ChannelDescEqual> descSet;

    for (uint32_t descIdx = 0; descIdx < descNum; ++descIdx) {
        // 检查ChannelDesc是否有重复元素
        CHK_PRT_RET(descSet.find(channelDesc[descIdx]) != descSet.end(),
            HCCL_ERROR("[%s]Duplicate item found in ChannelDesc.", __func__), HCCL_E_PARA);
        descSet.insert(channelDesc[descIdx]);
        // 检查RemoteRank有效性
        CHK_PRT_RET(channelDesc[descIdx].remoteRank == userRank_,
            HCCL_ERROR("[%s]Local Rank found in ChannelDesc.", __func__), HCCL_E_PARA);
        // 检查是否有不支持协议
        CHK_PRT_RET(channelDesc[descIdx].protocol != COMM_PROTOCOL_HCCS &&
            channelDesc[descIdx].protocol != COMM_PROTOCOL_ROCE,
            HCCL_ERROR("[%s]Unsupported protocol found in ChannelDesc.", __func__), HCCL_E_PARA);
        
        // 检查engine支持情况
        if (engine != COMM_ENGINE_HOSTCPU && engine != COMM_ENGINE_HOSTCPU_TS && 
            engine != COMM_ENGINE_AICPU && engine != COMM_ENGINE_AICPU_TS) {
            HCCL_ERROR("[%s]Unsupported engine for Channel.", __func__);
            return HCCL_E_PARA;
        }
    }
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::RegisterHandle(const std::string &tag, CommEngine engine, 
    const ChannelDesc &channelDesc, ChannelHandle channelHandle)
{
    std::string channelKey = tag + ":" + std::to_string(engine) + ":" + std::to_string(channelDesc.remoteRank) + 
                            ":" + std::to_string(channelDesc.protocol);

    CHK_PRT_RET((channelHandleMap_.find(channelKey) != channelHandleMap_.end()),
        HCCL_ERROR("[%s]Channel already exists, tag[%s], engine[%d], remoteRank[%d], protocol[%d].", 
        __func__, tag.c_str(), engine, channelDesc.remoteRank, channelDesc.protocol), HCCL_E_PARA);
    channelHandleMap_[channelKey] = channelHandle;
    keyMap_[channelHandle] = channelKey;
    engineMap_[channelHandle] = engine;
    HCCL_INFO("[%s]Register channel handle[%llu]", __func__, channelHandle);
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::PrepareHandleArray(const std::string& tag, CommEngine engine, const ChannelDesc *channelDesc, 
    uint32_t descNum, ChannelHandle* channelHandleArray, std::vector<ChannelDesc>& needCreateDescs, 
    std::vector<uint32_t>& needCreateIndices)
{
    needCreateDescs.clear();
    needCreateIndices.clear();
    
    for (uint32_t descIdx = 0; descIdx < descNum; descIdx++) {
        // 组合channelKey
        std::string channelKey = tag + ":" + std::to_string(engine) + ":" + std::to_string(channelDesc[descIdx].remoteRank) + 
                                ":" + std::to_string(channelDesc[descIdx].protocol);
        if (channelHandleMap_.find(channelKey) != channelHandleMap_.end()) {
            channelHandleArray[descIdx] = channelHandleMap_[channelKey];
            continue;
        }
        channelHandleArray[descIdx] = 0;
        needCreateDescs.push_back(channelDesc[descIdx]);
        needCreateIndices.push_back(descIdx);
    }
    
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::IsChannelExist(ChannelHandle channel)
{
    CHK_PRT_RET((keyMap_.find(channel) == keyMap_.end()),
        HCCL_ERROR("[%s]ChannelHandle is not exist.", __func__), HCCL_E_PARA);
    HCCL_INFO("[%s]ChannelHandle exist, ChannelHandle[%llu]", __func__, channel);
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::UnregisterHandle(ChannelHandle channel)
{
    CHK_PRT_RET((keyMap_.find(channel) == keyMap_.end()),
        HCCL_ERROR("[%s]ChannelHandle is not exist.", __func__), HCCL_E_PARA);
    
    channelHandleMap_.erase(keyMap_[channel]);
    keyMap_.erase(channel);
    if (engineMap_[channel] == COMM_ENGINE_AICPU ||
        engineMap_[channel] == COMM_ENGINE_AICPU_TS) {
        channelD2HMap_.erase(channel);
    }
    engineMap_.erase(channel);
    
    HCCL_INFO("[%s]Unregister channel handle success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::RegisterHandleHDPair(ChannelHandle deviceChannelHandle, ChannelHandle hostChannelHandle)
{
    CHK_PRT_RET((deviceChannelHandle == 0 || hostChannelHandle == 0),
        HCCL_ERROR("[%s]ChannelHandle is 0.", __func__), HCCL_E_PARA);
    CHK_PRT_RET((channelD2HMap_.find(deviceChannelHandle) != channelD2HMap_.end()),
        HCCL_ERROR("[%s]deviceChannelHandle has existed in channelD2HMap_.", __func__), HCCL_E_PARA);

    channelD2HMap_[deviceChannelHandle] = hostChannelHandle;
    return HCCL_SUCCESS;
}

HcclResult ChannelManager::GetHostChannel(ChannelHandle channel, ChannelHandle &hostChannel)
{
    if (engineMap_[channel] == COMM_ENGINE_AICPU ||
        engineMap_[channel] == COMM_ENGINE_AICPU_TS) {
        CHK_PRT_RET((channelD2HMap_.find(channel) != channelD2HMap_.end()),
            HCCL_ERROR("[%s]device channel handle has existed in channelD2HMap_.", __func__), HCCL_E_PARA);
        hostChannel = channelD2HMap_[channel];
    } else {
        hostChannel = channel;
    }
    return HCCL_SUCCESS;
}

} // namespace hccl