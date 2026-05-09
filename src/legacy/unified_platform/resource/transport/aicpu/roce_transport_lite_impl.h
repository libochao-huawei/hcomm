/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ROCE_TRANSPORT_LITE_IMPL_H
#define ROCE_TRANSPORT_LITE_IMPL_H

#include <vector>
#include <memory>
#include "base_transport_lite_impl.h"
#include "notify_lite.h"
#include "rma_buffer_lite.h"
#include "rmt_rma_buffer_lite.h"
#include "rdma_conn_lite_v2.h"

namespace Hccl {

class RoceTransportLiteImpl : public BaseTransportLiteImpl {
public:
    explicit RoceTransportLiteImpl(std::vector<char> &uniqueId);
    RoceTransportLiteImpl() = default;
    ~RoceTransportLiteImpl() override;

    void Init(std::vector<char> &uniqueId);

    std::string Describe() const override;

private:
    u32 notifyNum_{0};
    u32 bufferNum_{0};
    u32 connNum_{0};

    std::vector<std::unique_ptr<NotifyLite>> localNotifies_{};
    std::vector<RmtRmaBufferLite> remoteNotifies_{};
    std::vector<RmaBufferLite> locBufferVec_{};
    std::vector<RmtRmaBufferLite> rmtBufferVec_{};
    std::vector<std::vector<char>> connUniqueIdVec_{};
    std::vector<std::unique_ptr<RdmaConnLiteV2>> connVec_{};
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
