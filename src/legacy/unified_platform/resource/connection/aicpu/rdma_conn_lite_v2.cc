/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rdma_conn_lite_v2.h"

namespace Hccl {
void RdmaConnLiteV2::ParseSqContext(std::vector<char>& data)
{
    BinaryStream binaryStream(data);
    binaryStream >> sqContext_.qpn;
    binaryStream >> sqContext_.sqVa;
    binaryStream >> sqContext_.wqeSize;
    binaryStream >> sqContext_.depth;
    binaryStream >> sqContext_.headAddr;
    binaryStream >> sqContext_.tailAddr;
    binaryStream >> sqContext_.sl;
    binaryStream >> sqContext_.dbVa;
    binaryStream >> sqContext_.dbMode;
}

void RdmaConnLiteV2::ParseCqContext(std::vector<char>& data)
{
    BinaryStream binaryStream(data);
    binaryStream >> cqContext_.cqn;
    binaryStream >> cqContext_.cqVa;
    binaryStream >> cqContext_.cqeSize;
    binaryStream >> cqContext_.cqDepth;
    binaryStream >> cqContext_.headAddr;
    binaryStream >> cqContext_.tailAddr;
    binaryStream >> cqContext_.dbVa;
    binaryStream >> cqContext_.dbMode;
}

RdmaConnLiteV2::RdmaConnLiteV2(std::vector<char>& uniqueId) : RmaConnLite()
{
    BinaryStream binaryStream(uniqueId);
    binaryStream >> dmaMode_;

    std::vector<char> sqUniqueId;
    binaryStream >> sqUniqueId;
    ParseSqContext(sqUniqueId);

    std::vector<char> cqUniqueId;
    binaryStream >> cqUniqueId;
    ParseCqContext(cqUniqueId);

    qpVa_ = sqContext_.sqVa;
    sqVa_ = sqContext_.sqVa;
    sqDepth_ = sqContext_.depth;

    // 确定厂商
    GetVendorOps();
}

RdmaConnLiteV2::~RdmaConnLiteV2() {}

std::string RdmaConnLiteV2::Describe()
{
    return StringFormat(
        "RdmaConnLiteV2[QPN=%u, SQ_VA=0x%llx, WQE_SIZE=%u, SQ_DEPTH=%u, SQ_HEAD_ADDR=0x%llx, SQ_TAIL_ADDR=0x%llx, "
        "SL=%u, SQ_DB_VA=0x%llx, SQ_DB_MODE=%d, CQN=%u, CQ_VA=0x%llx, CQE_SIZE=%u, CQ_DEPTH=%u, "
        "CQ_HEAD_ADDR=0x%llx, CQ_TAIL_ADDR=0x%llx, CQ_DB_VA=0x%llx, CQ_DB_MODE=%d]",
        sqContext_.qpn, sqContext_.sqVa, sqContext_.wqeSize, sqContext_.depth, sqContext_.headAddr, sqContext_.tailAddr,
        sqContext_.sl, sqContext_.dbVa, sqContext_.dbMode,
        cqContext_.cqn, cqContext_.cqVa, cqContext_.cqeSize, cqContext_.cqDepth,
        cqContext_.headAddr, cqContext_.tailAddr, cqContext_.dbVa, cqContext_.dbMode
    );
}

void RdmaConnLiteV2::GetVendorOps()
{
    if (vendorOps_ != nullptr) {
        return;
    }
    switch (dmaMode_) {
        case QBUF_DMA_MODE_DEFAULT: {   // PCIe
            vendorOps_ = std::make_unique<RdmaXscdvOps>(&sqContext_, &cqContext_);
            break;
        }
        case QBUF_DMA_MODE_INDEP_UB: {  // UB
            vendorOps_ = std::make_unique<Rdma1825Ops>(&sqContext_, &cqContext_);
            break;
        }
    }
}

void RdmaConnLiteV2::Write(const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[RdmaConnLiteV2::%s] Write start, loc size = %u", __func__, loc.GetSize());

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    vendorOps_->Write(loc, rmt);

    // 构造Doorbell并返回
    vendorOps_->BuildDoorbell(dbAddr, dbValue);

    // TODO 增加错误处理
    HCCL_INFO("[RdmaConnLiteV2::%s] end, dbAddr = %llu, dbValue = %llu, conn[%s]", __func__, dbAddr, dbValue, Describe().c_str());
}

void RdmaConnLite::WriteWithNotify(
    const RmaBufSliceLite &loc, const RmtRmaBufSliceLite &rmt,
    const RmaBufSliceLite &locNotify, const RmtRmaBufSliceLite &notify, u64 &dbAddr, u64 &dbValue)
{
    HCCL_INFO("[RdmaConnLite::%s] start", __func__);

    // Post wqe, vendorOps_用来屏蔽厂商区别; 和UDMA不同，RDMA似乎不需要考虑Slice切分
    vendorOps_->WriteWithNotify(loc, rmt, locNotify, notify);

    // 构造Doorbell并返回
    vendorOps_->BuildDoorbell(dbAddr, dbValue);

    // TODO 增加错误处理
    HCCL_INFO("[RdmaConnLite::%s] end, dbAddr = %llu, dbValue = %llu, conn[%s]", __func__, dbAddr, dbValue, Describe().c_str());
}

} // namespace Hccl
