/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_RDMA_CONN_LITE_H_
#define HCCLV2_RDMA_CONN_LITE_H_

#include "binary_stream.h"
#include "exception_util.h"
#include "not_support_exception.h"
#include "log.h"

struct RdmaSqContextLite {
    uint32_t qpn;
    uint64_t sqVa;
    uint32_t wqeSize;
    uint32_t depth;
    uint64_t headAddr;
    uint64_t tailAddr;
    uint8_t sl;
    uint64_t dbVa;
    int8_t dbMode; // 0-hw/1-sw
};

struct RdmaCqContextLite{
    uint32_t cqn;
    uint64_t cqVa;
    uint32_t cqeSize;
    uint32_t cqDepth;
    uint64_t headAddr;
    uint64_t tailAddr;
    uint64_t dbVa;
    int8_t dbMode; // 0-hw/1-sw
};

namespace Hccl {
class RdmaConnLite {
public:
    explicit RdmaConnLite(std::vector<char>& uniqueId);
    ~RdmaConnLite();

    std::string Describe();

private:
    void ParseSqContext(std::vector<char>& data);
    void ParseCqContext(std::vector<char>& data);

    uint32_t            dmaMode_{0};
    RdmaSqContextLite   sqContext{};
    RdmaCqContextLite   cqContext{};
};

} // namespace Hccl

#endif