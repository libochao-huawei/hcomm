/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef DEV_AICPU_TS_ROCE_CHANNEL_V2_H
#define DEV_AICPU_TS_ROCE_CHANNEL_V2_H

#include <vector>
#include <memory>
#include <unordered_map>
#include "rma_buffer_lite.h"
#include "rmt_rma_buffer_lite.h"
#include "dev_rdma_conn_lite.h"
#include "notify_lite.h"

namespace Hccl {

MAKE_ENUM(RDMA_NOTIFY_TYPE, DATA_NOTIFY_MEM, ACK_NOTIFY_MEM, DATA_ACK_NOTIFY_MEM, NOTIFY_TYPE_RESERVED);

class DevAicpuTsRoceChannelV2 {
public:
    explicit DevAicpuTsRoceChannelV2(std::vector<char> &uniqueId);

    ~DevAicpuTsRoceChannelV2();

    std::string Describe() const;

private:
    u32 notifyNum_{0};
    u32 bufferNum_{0};
    u32 connNum_{0};

    std::vector<std::unique_ptr<NotifyLite>> localNotifies_{};
    std::vector<RmtRmaBufferLite> remoteNotifies_{};
    std::vector<RmaBufferLite> locBufferVec_{};
    std::vector<RmtRmaBufferLite> rmtBufferVec_{};
    std::vector<std::vector<char>> connUniqueIdVec_{};
    std::vector<std::unique_ptr<RdmaConnLite>> connVec_{};
    std::unique_ptr<RmaBufferLite> notifyValueBuffer_{};

    RmaBufSliceLite GetRmaBufSlicelite(const RmaBufferLite &lite) const;
    RmtRmaBufSliceLite GetRmtRmaBufSliceLite(const RmtRmaBufferLite &lite) const;
    RmtRmaBufSliceLite GetRmtNotifySliceLite(u32 index) const;

    void ParseLocNotifyVec(std::vector<char> &data);

    void ParseRmtNotifyVec(std::vector<char> &data);

    void ParseNotifyValueBuffer(std::vector<char> &data);
 
    void ParseLocBufferVec(std::vector<char> &data);

    void ParseRmtBufferVec(std::vector<char> &data);

    void ParseConnVec(std::vector<char> &data);
};

} // namespace Hccl
#endif