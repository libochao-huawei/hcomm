/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __AICPU_ASYNC_UNFOLDER_H__
#define __AICPU_ASYNC_UNFOLDER_H__

#include <memory>
#include <stdint.h>
#include <vector>

#include "hdc_pub.h"

namespace hccl {

class AicpuAsyncUnfolder {
public:
    AicpuAsyncUnfolder();
    ~AicpuAsyncUnfolder();

    HcclResult Init(const hccl::HDCommunicateParams& tailH2DParams,
        const hccl::HDCommunicateParams& headD2HParams, const hccl::HDCommunicateParams* opH2DRingBufferParams,
        const size_t opH2DRingBufferSize);
private:
    // OpRingBuffer: 以ring buffer形式组织op info用于异步展开
    // 注意: 对每个op info单独维护一个opH2D HDC; 如果用一个HDC维护整个opRingBuffer, 则单个op info更新会导致ReadCache全量刷新
    // 注意: 能够进入AICPU则至少是A3, hccl_communicator.cc中的GetSupportHDCommunicate一定返回true, 即HDC一定可用
    std::shared_ptr<hccl::HDCommunicate> tailH2D_{nullptr}; // uint64_t
    std::shared_ptr<hccl::HDCommunicate> headD2H_{nullptr}; // uint64_t
    std::vector<std::shared_ptr<hccl::HDCommunicate>> opH2DRingBuffer_; // std::vector<OpInfo>
}; // class AicpuAsyncUnfolder

} // namespace hccl

#endif // __AICPU_ASYNC_UNFOLDER_H__