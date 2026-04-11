/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCOMM_AICPU_TS_ROCE_MEM_VIEW_H
#define HCOMM_AICPU_TS_ROCE_MEM_VIEW_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace hcomm {

// 本机已注册设备内存的轻量视图（AicpuTsRoceChannel 使用，不依赖 Hccl::Buffer）。
struct AicpuTsRoceMemView {
    uintptr_t addr{0};
    std::size_t size{0};
    std::string tag{};
};

} // namespace hcomm

#endif // HCOMM_AICPU_TS_ROCE_MEM_VIEW_H
