/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hcclCommOp.h"

namespace {

const std::map<HcclCMDType, std::pair<Hccl::OpType, std::string>> CMD_OP_TYPE_INFO_MAP = {
    {HcclCMDType::HCCL_CMD_ALLREDUCE, {Hccl::OpType::ALLREDUCE, "OpType::ALLREDUCE"}},
    {HcclCMDType::HCCL_CMD_ALLGATHER, {Hccl::OpType::ALLGATHER, "OpType::ALLGATHER"}},
    {HcclCMDType::HCCL_CMD_REDUCE_SCATTER, {Hccl::OpType::REDUCESCATTER, "OpType::REDUCESCATTER"}},
    {HcclCMDType::HCCL_CMD_SEND, {Hccl::OpType::SEND, "OpType::SEND"}},
    {HcclCMDType::HCCL_CMD_RECEIVE, {Hccl::OpType::RECV, "OpType::RECV"}},
    {HcclCMDType::HCCL_CMD_ALLTOALL, {Hccl::OpType::ALLTOALL, "OpType::ALLTOALL"}},
    {HcclCMDType::HCCL_CMD_ALLTOALLV, {Hccl::OpType::ALLTOALLV, "OpType::ALLTOALLV"}},
    {HcclCMDType::HCCL_CMD_BROADCAST, {Hccl::OpType::BROADCAST, "OpType::BROADCAST"}},
    {HcclCMDType::HCCL_CMD_ALLGATHER_V, {Hccl::OpType::ALLGATHERV, "OpType::ALLGATHERV"}},
    {HcclCMDType::HCCL_CMD_REDUCE_SCATTER_V, {Hccl::OpType::REDUCESCATTERV, "OpType::REDUCESCATTERV"}},
    {HcclCMDType::HCCL_CMD_REDUCE, {Hccl::OpType::REDUCE, "OpType::REDUCE"}},
    {HcclCMDType::HCCL_CMD_ALLTOALLVC, {Hccl::OpType::ALLTOALLVC, "OpType::ALLTOALLVC"}},
    {HcclCMDType::HCCL_CMD_SCATTER, {Hccl::OpType::SCATTER, "OpType::SCATTER"}},
    {HcclCMDType::HCCL_CMD_BATCH_SEND_RECV, {Hccl::OpType::BATCHSENDRECV, "OpType::BATCHSENDRECV"}},
    {HcclCMDType::HCCL_CMD_HALF_ALLTOALLV, {Hccl::OpType::HALFALLTOALLV, "OpType::HALFALLTOALLV"}},
    {HcclCMDType::HCCL_CMD_BARRIER, {Hccl::OpType::BARRIER, "OpType::BARRIER"}},
    {HcclCMDType::HCCL_CMD_GATHER, {Hccl::OpType::GATHER, "OpType::GATHER"}},
    {HcclCMDType::HCCL_CMD_BATCH_GET, {Hccl::OpType::BATCHGET, "OpType::BATCHGET"}},
    {HcclCMDType::HCCL_CMD_BATCH_PUT, {Hccl::OpType::BATCHPUT, "OpType::BATCHPUT"}},
};
}

namespace hccl {

std::shared_ptr<Hccl::DfxOpInfo> ConvertToDfxOpInfo(const HcclDfxOpInfo& dfxOpInfo) {
    auto dfxOpInfoOnce = std::make_shared<Hccl::DfxOpInfo>();
    dfxOpInfoOnce->op_.opMode = static_cast<Hccl::OpMode::Value>(dfxOpInfo.opMode);
    dfxOpInfoOnce->op_.oldOpType = dfxOpInfo.opType; // 存A3的类型
    //  dfxOpInfoOnce->tag_  没有填
    dfxOpInfoOnce->op_.reduceOp = Hccl::HcclReduceOpToReduceOp(static_cast<HcclReduceOp>(dfxOpInfo.reduceOp));
    dfxOpInfoOnce->op_.dataType = Hccl::HcclDataTypeToDataType(static_cast<HcclDataType>(dfxOpInfo.dataType));
    dfxOpInfoOnce->op_.dataCount = dfxOpInfo.dataCount;
    dfxOpInfoOnce->op_.root = dfxOpInfo.root;
    dfxOpInfoOnce->op_.staticAddr = false;
    dfxOpInfoOnce->op_.staticShape = false;
    dfxOpInfoOnce->op_.inputMem = std::make_shared<Hccl::Buffer>(dfxOpInfo.inputMemAddr, dfxOpInfo.inputMemSize);
    dfxOpInfoOnce->op_.outputMem = std::make_shared<Hccl::Buffer>(dfxOpInfo.outputMemAddr, dfxOpInfo.outputMemSize);
    dfxOpInfoOnce->op_.scratchMem = std::make_shared<Hccl::Buffer>(0, 0);

    dfxOpInfoOnce->algTag_ = dfxOpInfo.algTag;
    dfxOpInfoOnce->algType_ = Hccl::AlgType{Hccl::AlgType::MESH}.Describe();
    dfxOpInfoOnce->beginTime_ = dfxOpInfo.beginTime;
    dfxOpInfoOnce->cpuWaitAicpuNotifyId_ = dfxOpInfo.cpuWaitAicpuNotifyId;

    return dfxOpInfoOnce;
}

}