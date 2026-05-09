/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCLV2_RDMA_CONN_LITE_V2_H_
#define HCCLV2_RDMA_CONN_LITE_V2_H_

#include "rma_conn_lite.h"
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
    int8_t dbMode;
};

struct RdmaCqContextLite {
    uint32_t cqn;
    uint64_t cqVa;
    uint32_t cqeSize;
    uint32_t cqDepth;
    uint64_t headAddr;
    uint64_t tailAddr;
    uint64_t dbVa;
    int8_t dbMode;
};

namespace Hccl {
class RdmaConnLiteV2 : public RmaConnLite {
public:
    explicit RdmaConnLiteV2(std::vector<char>& uniqueId);
    ~RdmaConnLiteV2() override;

    std::string Describe() override;

private:
    void ParseSqContext(std::vector<char>& data);
    void ParseCqContext(std::vector<char>& data);

    uint32_t            dmaMode_{0};
    RdmaSqContextLite sqContext{};
    RdmaCqContextLite cqContext{};
};

} // namespace Hccl

#endif
