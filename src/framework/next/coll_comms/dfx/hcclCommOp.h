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

struct HcclDfxOpInfo {
    //DfxOpInfo_base
    char                tag_[256];
    u32                 index_{0};
    u64                 beginTime_{0};
    u64                 endTime_{0};
    //CollOperator
    char                opTag[256];
    bool                staticAddr{false};
    bool                staticShape{false};
    RankId              myRank;
    //baseCollOperator
    u32                 opMode{0};
    u32                 opType{0};//通过map找dfxopinfo
    u32                 reduceOp{0};
    u32                 dataType{0};
    u32                 outputType{0};
    u64                 dataCount{0};
    u32                 root = INVALID_VALUE_RANKID;
    u32                 numBlocksLimit{0};
    void*               inputMemPtr{nullptr};
    u64                 inputMemSize{0};
    void*               outputMemPtr{nullptr};
    u64                 outputMemSize{0};
    void*               scratchMemPtr{nullptr};
    u64                 scratchMemSize{0};
    //task_exception
    u32          notifyId{0}; //host wait device notifyId
    union {
        struct {
            u64 dataCount{0};
            u32 dataType{0};
            u32 dataOutputType{0};
            u64 strideCount{0};
        } dataDes;
        struct {
            void* counts;
            void* displs;
            u32 dataType{0};
        } vDataDes;
        struct {
            u32 sendType{0};
            u32 recvType{0};
            u64 sendCount{0};
            u64 recvCount{0};
        } all2AllDataDes;
        struct {
            u32 sendType{0};
            u32 recvType{0};
            void* sendCounts;
            void* recvCounts;
            void* sdispls;
            void* rdispls;
        } all2AllVDataDes;
        struct {
            u32 sendType{0};
            u32 recvType{0};
            void* sendCountMatrix;
        } all2AllVCDataDes;
        struct {
            HcclSendRecvItem* sendRecvItemsPtr;
            u32 itemNum{0};
        } batchSendRecvDataDes;
    };
};

void SetCollopDataDes(Hccl::CollOperator& collOp, const HcclDfxOpInfo& dfxOpInfo);
std::shared_ptr<Hccl::Buffer> CreateBufferShared(const Hccl::Buffer* buffer);
std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo);

}
int32_t HcommThreadRegisterDfx(ThreadHandle thread, std::function<HcclResult(u32, u32, const Hccl::TaskParam&, u64)> callback);
#endif