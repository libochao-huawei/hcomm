/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COMM_ADDR_CONVERT_H
#define COMM_ADDR_CONVERT_H

#include <stdint.h>
#include <string>
#include "hccl/hccl_res.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace hcomm {
    std::string ConvertIPv4(const struct in_addr &addr);
    std::string ConvertIPv6(const struct in6_addr &addr6);
    std::string ConvertID(uint32_t id);
    std::string ConvertEID(const uint8_t eid[16]);
    std::string CommAddr2Str(const CommAddr commAddr);

} // namespace hcomm

#ifdef __cplusplus
}
#endif

#endif // COMM_ADDR_CONVERT_H