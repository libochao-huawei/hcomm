/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ENDPOINT_PAIR_H
#define ENDPOINT_PAIR_H

#include <memory>
#include <vector>
#include "channels/channel.h"
#include "../endpoints/endpoint.h"

namespace hcomm {
/**
 * @note 职责：通信设备EndPoint对（EndPointPair，或连接）的C++类声明，
 * 管理该EndPoint对上的本地和远端注册内存、多个Channel、以及socket等。两个EndPoint的通信协议一致。
 */
class EndPointPair {
public:
    EndPointPair(EndPointHandle srcEndPoint, EndPointInfo dstEndPoint);
    ~EndPointPair();

    // 创建Channel
    HcclResult CreateChannel(CommEngineType engineType, const std::vector<MemRegion>& memRegions,
        ChannelHandle* channel);

    // 获取源端点
    EndPointHandle GetSrcEndPoint() const { return srcEndPoint_; }

    // 获取目的端点
    EndPointInfo GetDstEndPoint() const { return dstEndPoint_; }

    // 获取远端内存信息
    HcclResult GetRemoteMemoryInfo(std::vector<MemRegion>& remoteMemories);

private:
    EndPointHandle srcEndPoint_{};
    EndPointInfo dstEndPoint_{};
    std::vector<std::shared_ptr<Channel>> channels_{};
    std::vector<MemRegion> remoteMemories_{};
    // 还有socket
};
}
#endif // ENDPOINT_PAIR_H
