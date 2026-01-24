/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: stream manager header file, provide dpu stream
 */
#ifndef HCCLV2_DPU_STREAM_MANAGER_H
#define HCCLV2_DPU_STREAM_MANAGER_H

#include <memory>
#include "stream_pub.h"

namespace hccl {

class DpuStreamManager {
public:
    explicit DpuStreamManager()
    {
        stream_ = std::make_unique<Stream>(StreamType::STREAM_TYPE_ONLINE, true);
    }

    ~DpuStreamManager() = default;

    Stream *GetStream() const
    {
        return stream_.get();
    }

private:
    std::unique_ptr<Stream> stream_{nullptr};
};

}  // namespace hccl

#endif  // HCCLV2_DPU_STREAM_MANAGER_H