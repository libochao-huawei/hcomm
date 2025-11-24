/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <memory>
#include <vector>
#include <string>
#include "reged_mems/reged_mems.h"

namespace hcomm {
/**
 * @note 职责：通信设备EndPoint的C++抽象接口类，管理通信设备上下文，以及设备上的注册内存。
 */
class EndPoint {
public:
    virtual ~EndPoint() = default;

    // 注册内存
    virtual HcclResult RegisterMemory(const std::vector<MemHandle>& memHandles);

    // 注销内存
    virtual HcclResult UnregisterMemory(MemHandle memHandle) = 0;

    // 获取注册的内存信息
    virtual HcclResult GetRegisteredMemory(std::vector<MemRegion>& memRegions) const = 0;

private:
    std::shared_ptr<RegedMemMgr> regedMemMgr_{};
};
}
#endif // ENDPOINT_H
