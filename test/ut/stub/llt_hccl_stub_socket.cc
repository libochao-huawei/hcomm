/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <cstdint>
#include <cstring>
#include <string>
#include "../../../src/legacy/unified_platform/resource/socket/socket.h"
namespace Hccl {
    void Socket::Destroy()
    {
        isDestroyed = true;
    }
    virtual Socket::~Socket() {}
    virtual Socket::void Listen() {}
    virtual Socket::void Connect() {}
    void Socket::Close() {}
    SocketStatus Socket::GetStatus() {}
}  // namespace Hccl
