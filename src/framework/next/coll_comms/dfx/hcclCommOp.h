/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
//TODO:以后搬走
#ifndef HCCL_COMM_OP_H
#define HCCL_COMM_OP_H

#include <memory>
#include "hccl_common.h"
#include <unordered_map>
#include "buffer.h"
#include "common.h"
#include "op_mode.h"
#include "task_info.h"

namespace hccl {
using RankId = u32;

class HcclDfxOpInfo {
public:
    std::string  opTag;
    RankId       myRank; //hcomm可填
    Hccl::OpMode       opMode;
    HcclCMDType  opType = HcclCMDType::HCCL_CMD_INVALID;
    HcclReduceOp reduceOp = HcclReduceOp::HCCL_REDUCE_RESERVED;
    HcclDataType dataType;
    HcclDataType outputType;
    u64          dataCount{0};
    u32          root = INVALID_VALUE_RANKID;
    u32          numBlocksLimit{0};
    bool         staticAddr{false};
    bool         staticShape{false};
    std::shared_ptr<Hccl::Buffer> inputMem{nullptr};
    std::shared_ptr<Hccl::Buffer> outputMem{nullptr};
    std::shared_ptr<Hccl::Buffer> scratchMem{nullptr};
    std::string  tag_;
    Hccl::AlgType      algType;
    u32          index_{0};
    u64          beginTime_;
    u64          endTime_;
    u32          mainStreamId_; 
    u32          notifyId; //host wait device notifyId
    union {
        struct {
            u64 dataCount;
            HcclDataType dataType;
            HcclDataType dataOutputType;
        } dataDes;
        struct {
            void* counts;
            void* displs;
            HcclDataType dataType;
        } vDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            u64 sendCount;
            u64 recvCount;
        } all2AllDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            void* sendCounts;
            void* recvCounts;
            void* sdispls;
            void* rdispls;
        } all2AllVDataDes;
        struct {
            HcclDataType sendType;
            HcclDataType recvType;
            void* sendCountMatrix;
        } all2AllVCDataDes;
        struct {
            HcclSendRecvItem* sendRecvItemsPtr;
            u32 itemNum;
        } batchSendRecvDataDes;
    };

public:
    std::string Describe() const
    {
        return Hccl::StringFormat(
            "DfxOpInfo: [tag:[%s], algType:[%u], index:[%u], beginTime:[%llu], endTime:[%llu]",
             tag_.c_str(), algType, index_, beginTime_, endTime_);
    }
};

void SetCollopDataDes(Hccl::CollOperator& collOp, const HcclDfxOpInfo& dfxOpInfo);
std::shared_ptr<Hccl::Buffer> CreateBufferShared(const Hccl::Buffer* buffer);
std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo);
HcclResult HcommThreadRegisterDfx(ThreadHandle thread, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback);


}
#endif