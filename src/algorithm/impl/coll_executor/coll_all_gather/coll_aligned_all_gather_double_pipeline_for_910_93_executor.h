/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COLL_ALIGNED_ALLGATHER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H
#define COLL_ALIGNED_ALLGATHER_DOUBLE_RING_PIPELINE_FOR_910_93_EXECUTOR_H

#include "coll_all_gather_ring_for_910_93_executor.h"

namespace hccl {
class CollAlignedAllGatherDoublePipelineFor91093Executor
    : public CollAllGatherRingFor91093Executor {
public:
    explicit CollAlignedAllGatherDoublePipelineFor91093Executor(
        const HcclDispatcher dispatcher,
        std::unique_ptr<TopoMatcher> &topoMatcher);
    ~CollAlignedAllGatherDoublePipelineFor91093Executor() override = default;

    // 流水线编排
    HcclResult Orchestrate(OpParam& param, AlgResourceResponse& algRes) override;

private:
    // 资源计算
    HcclResult CalcStreamNum(u32& streamNum) override;
    HcclResult CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcLevel0CommInfo(TransportMemType inputType, TransportMemType outputType,
        std::vector<LevelNSubCommTransport>& opTransport) override;
    // HcclResult CalcLevel1CommInfo(TransportMemType inputType, TransportMemType outputType,
    //     std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcLevel2CommInfo(TransportMemType inputType, TransportMemType outputType,
        std::vector<LevelNSubCommTransport>& opTransport) override;
    HcclResult CalcTransportMemType(TransportMemType &inputType, TransportMemType &outputType);

    // 算法编排
    u64 CalcLoopMaxCount(const u64 cclBuffSize, const u32 unitSize) override;
    HcclResult RunLoop(OpParam &param);

    // HcclResult KernelRun(const OpParam &param, ExecMem &execMem) override; // --

    // 层级算法调用方法（从模板移动到执行器）
    HcclResult KernelRunInterSuperPod(const OpParam &param, ExecMem &execMem); // 跨超
    HcclResult KernelRunIntraServer(const OpParam &param, ExecMem &execMem); // server内
    HcclResult KernelRunInterServer(const OpParam &param, ExecMem &execMem) override; // 跨server
    HcclResult WriteBackToUserOutput(DeviceMem &dmaBuffer, u32 bufferIndex, std::vector<Stream> &subStreams,
                                     u64 memSliceSize, u32 userRank, ExecMem &execMem);


    virtual std::vector<Slice> PrepareSlicesL2(const OpParam &param, const SubCommInfo &level2CommInfo,
        const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo, u32 perDataSize, u64 inputMemSize) const;
    virtual std::vector<Slice> PrepareSlicesL1(const OpParam &param, const SubCommInfo &level2CommInfo,
        const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo, u32 perDataSize, u64 inputMemSize) const;
    virtual HcclResult PrepareSlicesL0(std::vector<std::vector<Slice>> &multRingsSlice, const OpParam &param,
        const SubCommInfo &level2CommInfo, const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo,
        u32 perDataSize, u64 inputMemSize);
    virtual HcclResult PrepareUserMemSlices(std::vector<std::vector<Slice>> &userMemSlices,
        const std::vector<std::vector<Slice>> &multRingsSlice, const OpParam &param, const SubCommInfo &level2CommInfo,
        const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo, u32 perDataSize, u64 inputMemSize);

    HcclResult DoubleRingAllGather(const std::string &tag, DeviceMem inputMem, DeviceMem outputMem, const u64 count,
        const HcclDataType dataType, const std::vector<std::vector<Slice>> multRingsSliceZero, Stream stream,
        s32 profStage, const u64 baseOffset, const HcomCollOpInfo *opInfo,
        const std::vector<std::vector<Slice>> multRingsUserMemSlice);

    // 成员变量
    u32 unitSize_ = 0;
    u64 totalSize_ = 0; // 输入总数据量
    u32 level0Rank_ = INVALID_VALUE_RANKID;
    u32 level1Rank_ = INVALID_VALUE_RANKID;
    u32 level2Rank_ = INVALID_VALUE_RANKID;
    u32 level0RankSize_ = INVALID_VALUE_RANKSIZE;
    u32 level1RankSize_ = INVALID_VALUE_RANKSIZE;
    u32 level2RankSize_ = INVALID_VALUE_RANKSIZE;
    u32 memIdx_ = 0; // 表示当前算法编排使用CCLIN的第几块（0/1），是RDMA通信的源和目的，也是数据交换的起始
    u32 blockIdx_ = 0; // 表示当前超节点内在处理来自于第几个超节点的数据
    u64 blockSize_ = 0; // 表示来自于一个超节点的数据的大小

    Stream mainStreamL2_; // SDMA+localcopy
    std::vector<Stream> subStreams_;
    Stream mainStreamL1L0_;
    std::vector<Stream> ringSubStreams_;
    std::shared_ptr<LocalNotify> notifyL2ToL1L0_;
    std::shared_ptr<LocalNotify> notifyL1L0ToL2_;
    std::vector<std::shared_ptr<LocalNotify>> notifyRingMain_;
    std::vector<std::shared_ptr<LocalNotify>> notifyRingSub_;
    SubCommInfo level2CommInfo_;           // L2层通信域信息
    DeviceMem dmaBuffers_[2];              // 双缓冲DMA内存

    // 流水线状态
    u32 currentBuffer_{0};                    // 当前缓冲区索引（0或1）
    u32 step_{0};                             // 当前流水线步数
};

} // namespace hccl

#endif