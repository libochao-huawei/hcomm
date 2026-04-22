/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_BATCH_SEND_RECV_DIRECT_FULLMESH_EXECUTOR_H
#define COLL_BATCH_SEND_RECV_DIRECT_FULLMESH_EXECUTOR_H

#include "coll_batch_send_recv_executor.h"

namespace hccl {

struct BatchSendRecvItem {
    u32 remoteRank;
    u64 sendLength;
    u64 sendOffset;
    u64 recvLength;
    u64 recvOffset;
    void* sendBuf;
    void* recvBuf;
};

struct BatchSendRecvInfo {
    std::vector<BatchSendRecvItem> items;
    std::set<u32> remoteRanks;
    u32 maxRemoteRankNum;
};

const u32 BATCH_SEND_RECV_DIRECT_FULLMESH_SDMA_CONCURRENT_SIZE = 8;
const u32 BATCH_SEND_RECV_DIRECT_FULLMESH_RDMA_CONCURRENT_SIZE = 1;
const u32 BATCH_SEND_RECV_DIRECT_FULLMESH_BIG_SIZE = 1 * 1024 * 1024;

class CollBatchSendRecvDirectFullmeshExecutor : public CollBatchSendRecvExecutor {
public:
    CollBatchSendRecvDirectFullmeshExecutor(const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollBatchSendRecvDirectFullmeshExecutor() override = default;

    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) override;

protected:
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcLevel0CommInfo(TransportMemType inputType, TransportMemType outputType,
        std::vector<LevelNSubCommTransport>& opTransport) override;

    HcclResult CalcTransportMemType(TransportMemType &inputType, TransportMemType &outputType);
    HcclResult GetLocalSDMAGroupInfo(const u32 userRank, u32& devNumInlocalPod, u32& rankIdxInPod);
    HcclResult ParseBatchSendRecvItems(const OpParam &param);
    HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override;

private:
    BatchSendRecvInfo batchSendRecvInfo_;
};

} // namespace hccl

#endif /* COLL_BATCH_SEND_RECV_DIRECT_FULLMESH_EXECUTOR_H */
