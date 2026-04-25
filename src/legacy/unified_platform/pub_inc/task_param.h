/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef TASK_PARAM_H
#define TASK_PARAM_H

#include <vector>
#include <string>
#include <memory>
#include "securec.h"
#include "hccl/base.h"
#include "const_val.h"
#include "enum_factory.h"
#include "ip_address.h"

namespace Hccl {

MAKE_ENUM(DmaOp, HCCL_DMA_READ, HCCL_DMA_WRITE, HCCL_DMA_NOTIFY_WAIT)

MAKE_ENUM(AlgType, NOT_SPECIFIED, RING, MULTI_RING, MESH, RECURSIVE_HD, BINARY_HD, PAIR_WISE, INVALID_VAL)

MAKE_ENUM(TaskParamType, TASK_SDMA, TASK_RDMA, TASK_REDUCE_INLINE, TASK_REDUCE_TBE, TASK_NOTIFY_RECORD, TASK_NOTIFY_WAIT,
    TASK_SEND_NOTIFY, TASK_SEND_PAYLOAD, TASK_WRITE_WITH_NOTIFY, TASK_WRITE_REDUCE_WITH_NOTIFY, TASK_CCU, TASK_AICPU_KERNEL, TASK_AICPU_REDUCE, TASK_AIV, TASK_UB_INLINE_WRITE, TASK_UB_REDUCE_INLINE, TASK_UB,
    TASK_DPU_KERNEL, TASK_DPU_THREAD_FENCE, TASK_DPU_CHANNEL_FENCE, TASK_DPU_INLINE_WRITE, TASK_DPU_NOTIFY_WAIT, TASK_DPU_WRITE_WITH_NOTIFY)

MAKE_ENUM(DfxLinkType, ONCHIP, HCCS, PCIE, ROCE, SIO, HCCS_SW, STANDARD_ROCE, UB, UBoE, RESERVED)

MAKE_ENUM(CcuProfilinType, CCU_TASK_PROFILING, CCU_WAITCKE_PROFILING, CCU_LOOPGROUP_PROFILING, CCU_MAP_PROFILING)
 
constexpr uint16_t  CCU_MAX_CHANNEL_NUM     = 16;     // 最多16条link
constexpr uint16_t  INVALID_CKE_ID          = 0xFFFF; // CKE ID非法值
constexpr uint16_t  INVALID_VALUE_CHANNELID = 0xFFFF; // channel id非法值
constexpr u64       INVALID_VALUE_NOTIFYID  = 0xFFFFFFFFFFFFFFFF; // NOTIFY id非法值

struct CcuProfilingInfo {
    std::string name;          // CCU任务名或微码名
    uint8_t type;              // 枚举，0为Task粒度，1为WaitCKE，2为LoopGroup，3为channelId->RemoteRankId的映射
    uint8_t dieId;             // CCU任务执行的DieId
    uint8_t missionId;         // CCU任务执行的MissionId
    uint16_t instrId;
    uint8_t reduceOpType;      // 与HcclReduceOp类型保持一致
    uint8_t inputDataType;     // 与HcclDataType类型保持一致
    uint8_t outputDataType;    // 与HcclDataType类型保持一致
    uint64_t dataSize;         // 输入数据大小
    uint32_t ckeId;
    uint32_t mask;
    uint16_t channelId[CCU_MAX_CHANNEL_NUM];    // LoopGroup所包含的搬运指令使用的ChannelId
    uint32_t remoteRankId[CCU_MAX_CHANNEL_NUM]; // LoopGroup所包含的搬运指令的对端
 
    CcuProfilingInfo() : name(""), type(0), dieId(0), missionId(0), instrId(0), reduceOpType(0), inputDataType(0), outputDataType(0), dataSize(0), ckeId(0), mask(0) {
        (void)memset_s(channelId, sizeof(channelId), INVALID_VALUE_CHANNELID, sizeof(channelId));
        (void)memset_s(remoteRankId, sizeof(remoteRankId), INVALID_RANKID, sizeof(remoteRankId));
    }
};

struct ParaDMA {
    const void *src{nullptr};
    const void *dst{nullptr};
    std::size_t size{0};
    u64         notifyID{0};
    u32         notifyValue{0};
    DfxLinkType linkType{DfxLinkType::STANDARD_ROCE};
    DmaOp       dmaOp{DmaOp::HCCL_DMA_READ};
    Eid         locEid{};
    Eid         rmtEid{};
    char        locAddr[64]{0};
    char        rmtAddr[64]{0};
};

struct ParaReduce {
    const void  *src{nullptr};
    const void  *dst{nullptr};
    std::size_t  size{0};
    u64          notifyID{0};
    u32          notifyValue{0};
    DfxLinkType  linkType{DfxLinkType::STANDARD_ROCE};
    HcclReduceOp reduceOp{HcclReduceOp::HCCL_REDUCE_RESERVED};
    HcclDataType dataType{HcclDataType::HCCL_DATA_TYPE_RESERVED};
    Eid         locEid{};
  	Eid         rmtEid{};
};

struct ParaNotify {
    u64 notifyID{0};
    u32 value{0};
};

constexpr u32 CCU_COSTOM_ARGS_LEN = 32;
struct ParaCcu {
    u8  dieId{0};
    u8  missionId{0};
    u8  execMissionId{0};
    u32 instrId{0};
    u64 costumArgs[CCU_COSTOM_ARGS_LEN]{0};
    u64 executeId{0};
    u64 ccuKernelHandle{0};
};

struct ParaAiv{
    HcclCMDType cmdType{HcclCMDType::HCCL_CMD_MAX};
    u32 tag{0};
    u64 count{0};
    u32 numBlocks{0};
    u32 rankSize{0};
    void* flagMem{nullptr};
    u64 flagMemSize{0};
    u32 rank{0};
    u32 sendRecvRemoteRank{0};
    bool isOpbase{false};
    HcclDataType dataType{HcclDataType::HCCL_DATA_TYPE_RESERVED};
};

struct TaskParam {
    TaskParamType taskType;
    u64           beginTime;
    u64           endTime;
    u64           aicpuTaskId{0}; // 关联的aicpu任务id，0表示不关联
    uint16_t      npuDevId{0};
    bool          isMaster{false};
    union {
        ParaDMA    DMA;    // taskType = SDMA/RDMA使用, 包括rtRDMASend写notify
        ParaReduce Reduce; // taskType = inline/CCE Reduce使用
        ParaNotify Notify; // taskType = Noitfy Record/Wait使用
        ParaCcu    Ccu;
        ParaAiv    Aiv; //aiv param
    } taskPara;
    std::shared_ptr<std::vector<CcuProfilingInfo>> ccuDetailInfo; // taskType为TASK_CCU时，ParaCcu的补充profiling信息
    std::string Describe() const
    {
        return StringFormat("TaskParam[taskType[%s] beginTime[%llu] endTime[%llu]]", taskType.Describe().c_str(), beginTime, endTime);
    }
};

} // namespace Hccl

#endif