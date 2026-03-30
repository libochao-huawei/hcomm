/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dev_rdma_conn_lite.h"

namespace Hccl {
RdmaConnLite::RdmaConnLite(std::vector<char>& uniqueId)
{
    BinaryStream binaryStream(uniqueId);
    binaryStream >> dmaMode_;
    // sqContext
    binaryStream >> sqContext.qpn;
    binaryStream >> sqContext.sqVa;
    binaryStream >> sqContext.wqeSize;
    binaryStream >> sqContext.depth;
    binaryStream >> sqContext.headAddr;
    binaryStream >> sqContext.tailAddr;
    binaryStream >> sqContext.sl;
    binaryStream >> sqContext.dbVa;
    binaryStream >> sqContext.dbMode;
    // cqContext
    binaryStream >> cqContext.cqn;
    binaryStream >> cqContext.cqVa;
    binaryStream >> cqContext.cqeSize;
    binaryStream >> cqContext.cqDepth;
    binaryStream >> cqContext.headAddr;
    binaryStream >> cqContext.tailAddr;
    binaryStream >> cqContext.dbVa;
    binaryStream >> cqContext.dbMode;
}

RdmaConnLite::~RdmaConnLite() {}

std::string RdmaConnLite::Describe()
{
    return StringFormat(
        "RdmaConnLite[QPN=%u, SQ_VA=0x%llx, WQE_SIZE=%u, SQ_DEPTH=%u, SQ_HEAD_ADDR=0x%llx, SQ_TAIL_ADDR=0x%llx, "
        "SL=%u, SQ_DB_VA=0x%llx, SQ_DB_MODE=%d, CQN=%u, CQ_VA=0x%llx, CQE_SIZE=%u, CQ_DEPTH=%u, "
        "CQ_HEAD_ADDR=0x%llx, CQ_TAIL_ADDR=0x%llx, CQ_DB_VA=0x%llx, CQ_DB_MODE=%d]",
        sqContext.qpn, sqContext.sqVa, sqContext.wqeSize, sqContext.depth, sqContext.headAddr, sqContext.tailAddr,
        sqContext.sl, sqContext.dbVa, sqContext.dbMode,
        cqContext.cqn, cqContext.cqVa, cqContext.cqeSize, cqContext.cqDepth,
        cqContext.headAddr, cqContext.tailAddr, cqContext.dbVa, cqContext.dbMode
    );
}
} // namespace Hccl