/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HOST_CPU_URMA_CHANNEL_H
#define HOST_CPU_URMA_CHANNEL_H

#include <mutex>
#include "../channel.h"
#include "urma_types.h"

namespace hcomm {

class HostCpuUrmaChannel : public Channel {
public:

    // 数据面接口
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override;
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override;
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override;
    HcclResult Write(void *dst, const void *src, uint64_t len) override;
    HcclResult Read(void *dst, const void *src, uint64_t len) override;
    HcclResult ChannelFence() override;

private:
    HcclResult UrmaPostJfr();
    HcclResult PrepareNotifyWrResource(const uint32_t remoteNotifyIdx, urma_jfs_wr_t &notifyRecordWr);
    HcclResult PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx,
                                      urma_jfs_wr_t &writeWithNotifyWr);

    urma_jfs_t jfs_;
    urma_jfr_t jfr_;
    urma_jfc_t jfc_;
    urma_target_jetty_t tjetty_;
    uint32_t wqeNum_{0};
    bool fenceFlag_{false};

    std::mutex jfcMutex_;
};

} // namespace hcomm

#endif // HOST_CPU_URMA_CHANNEL_H