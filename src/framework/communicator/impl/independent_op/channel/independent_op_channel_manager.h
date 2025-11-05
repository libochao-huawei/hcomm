/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef INDEPENDENT_OP_CHANNEL_MANAGER_H
#define INDEPENDENT_OP_CHANNEL_MANAGER_H

#include "hccl_api.h"
#include "hccl_types.h"
#include "transport_pub.h"
#include "hccl_common.h"
#include "hccl_mem_defs.h"
#include "transport_pub.h"
#include "aicpu_operator_pub.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace std {
    template <>
    struct hash<ChannelDesc> {
        size_t operator()(const ChannelDesc& desc) const {
            size_t hash = 0;
            // 仅区分remoteRank和protocol
            hash ^= std::hash<uint32_t>()(desc.remoteRank);
            hash ^= std::hash<int32_t>()(static_cast<int32_t>(desc.protocol));
            return hash;
        }
    };
}

namespace hccl {

struct ChannelDescEqual {
    bool operator()(const ChannelDesc& lcd, const ChannelDesc& rcd) const {
        return lcd.remoteRank == rcd.remoteRank && lcd.protocol == rcd.protocol;
    }
};

class ChannelManager {
public:
    ChannelManager(uint32_t userRank);
    ~ChannelManager();

    HcclResult CheckChannelParam(const std::string &tag, CommEngine engine, const ChannelDesc *channelDesc,
        uint32_t descNum);
    HcclResult RegisterHandle(const std::string& key, CommEngine engine, const ChannelDesc& channelDesc, ChannelHandle channelHandle);
    HcclResult RegisterHandleHDPair(ChannelHandle deviceChannelHandle, ChannelHandle hostChannelHandle);
    HcclResult UnregisterHandle(ChannelHandle channelHandle);
    HcclResult PrepareHandleArray(const std::string &tag, CommEngine engine, const ChannelDesc *channelDesc, 
        uint32_t descNum, ChannelHandle *channelHandleArray, std::vector<ChannelDesc> &needCreateDescs,
        std::vector<uint32_t> &needCreateIndices);
    HcclResult ChannelGetNotifyNum(ChannelHandle channel, uint32_t *notifyNum);
    HcclResult IsChannelExist(ChannelHandle channel);
    HcclResult GetHostChannel(ChannelHandle channel, ChannelHandle &hostChannel);

private:
    std::unordered_map<std::string, ChannelHandle> channelHandleMap_;
    std::unordered_map<ChannelHandle, std::string> keyMap_;
    std::unordered_map<ChannelHandle, CommEngine> engineMap_;
    std::unordered_map<ChannelHandle, ChannelHandle> channelD2HMap_;
    std::unordered_set<ChannelHandle*> channelHandleArraySet_;
    std::unordered_map<ChannelHandle, LINK> linkMap_;
    uint32_t userRank_;
};

} // namespace hccl

#endif  // INDEPENDENT_OP_CHANNEL_MANAGER_H
