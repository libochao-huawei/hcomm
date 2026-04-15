/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AICPU_TS_ROCE_RES_HANDLER_H
#define AICPU_TS_ROCE_RES_HANDLER_H

#include "aicpu_channel_res_handler.h"

class AicpuTsRoceResHandler final : public IAicpuChannelResHandler {
public:
    static AicpuTsRoceResHandler &Instance();

    HcclResult Parse(const void *blob, u64 blobBytes, const HcommDeviceInfo &deviceInfo, ChannelHandle &outHandle) override;
    bool Destroy(ChannelHandle handle) override;

private:
    AicpuTsRoceResHandler() = default;
    ~AicpuTsRoceResHandler() override = default;
    AicpuTsRoceResHandler(const AicpuTsRoceResHandler &) = delete;
    AicpuTsRoceResHandler &operator=(const AicpuTsRoceResHandler &) = delete;
};

#endif // AICPU_TS_ROCE_RES_HANDLER_H
