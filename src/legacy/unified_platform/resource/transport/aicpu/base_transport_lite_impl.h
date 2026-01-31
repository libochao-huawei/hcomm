/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef BASE_MEM_TRANSPORT_LITE_H
#define BASE_MEM_TRANSPORT_LITE_H

#include <memory>
#include <vector>
#include <unordered_map>
#include "stream_lite.h"
#include "buffer.h"
#include "rma_buffer_lite.h"
#include "mem_transport_common.h"
#include "rmt_rma_buf_slice_lite.h"
namespace Hccl {

MAKE_ENUM(TransferType, WRITE, READ)

class BaseTransportLiteImpl {
public:
    BaseTransportLiteImpl() = default;

    virtual ~BaseTransportLiteImpl() = default;

    virtual std::string Describe() const
    {
        return "BaseTransportLiteImpl";
    }

    struct TransferOp {
        TransferType transType;
        ReduceIn reduceIn;
    };

    virtual Buffer GetRmtBuffer(u32 index)
    {
        (void)index;
        return Buffer(0, 0);
    }

    virtual void Post(u32 index, const StreamLite &stream)
    {
        (void)index;
        (void)stream;
    }

    virtual void Wait(u32 index, const StreamLite &stream)
    {
        (void)index;
        (void)stream;
    }

    virtual void Read(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)stream;
    }

    virtual void Write(const RmaBufferLite &loc, const Buffer &rmt, const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)stream;
    }

    virtual void ReadReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                            const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)reduceIn;
        (void)stream;
    }

    virtual void WriteReduce(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                             const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)reduceIn;
        (void)stream;
    }

    virtual void WriteWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const WithNotifyIn &withNotify,
                                 const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)withNotify;
        (void)stream;
    }

    virtual void WriteReduceWithNotify(const RmaBufferLite &loc, const Buffer &rmt, const ReduceIn &reduceIn,
                                       const WithNotifyIn &withNotify, const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)reduceIn;
        (void)withNotify;
        (void)stream;
    }

    virtual void BatchOneSidedWrite(const std::vector<RmaBufSliceLite> &loc, const std::vector<RmtRmaBufSliceLite> &rmt,
        const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)stream;
    }

    virtual void BatchOneSidedRead(const std::vector<RmaBufSliceLite> &loc, const std::vector<RmtRmaBufSliceLite> &rmt,
        const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)stream;
    }

    virtual void BatchTransfer(const std::vector<RmaBufferLite> &loc, const std::vector<Buffer> &rmt,
                                const std::vector<TransferOp> &transferOp, const StreamLite &stream)
    {
        (void)loc;
        (void)rmt;
        (void)transferOp;
        (void)stream;
    }

private:
};

} // namespace Hccl
#endif