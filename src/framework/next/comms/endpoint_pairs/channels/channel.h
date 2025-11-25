/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CHANNEL_H
#define CHANNEL_H

#include <memory>
#include <vector>

namespace hcomm {
/**
 * @note 职责：一个EndPointPair上的建立的通信通道的C++抽象接口类声明。
 * 管理该通信通道Channel对上的同步信号Notify、通信队列（如qp、jetty等）等资源管理，负责建立连接，以及注册内存、同步信号等的交换。
 */
class Channel {
public:
    Channel();
    virtual ~Channel() = default;

    // 连接建立
    virtual HcclResult Connect() = 0;

    // 关闭连接
    virtual HcclResult Close() = 0;

    // 获取Channel状态
    virtual ChannelStatus GetStatus() const = 0;

    // 获取Notify数量
    virtual uint32_t GetNotifyNum() const = 0;

protected:
    ChannelHandle handle_{};
    CommEngineType engineType_{};
    ChannelStatus status_{};
};
}
#endif // CHANNEL_H
