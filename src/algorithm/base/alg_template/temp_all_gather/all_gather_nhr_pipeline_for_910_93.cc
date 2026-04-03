/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "all_gather_nhr_pipeline_for_910_93.h"
#include "alg_template_register.h"
#include "all_gather_nhr.h"
#include "aligned_all_gather_double_ring.h"

namespace hccl {

AllGatherNHRPipelineFor91093::AllGatherNHRPipelineFor91093(const HcclDispatcher dispatcher)
    : AlgTemplateBase(dispatcher) {}

AllGatherNHRPipelineFor91093::~AllGatherNHRPipelineFor91093() {}

HcclResult AllGatherNHRPipelineFor91093::Prepare(
    HcomCollOpInfo *opInfo,
    u32 userRank,
    u64 &count,
    DeviceMem &cclBufferPartOne,
    DeviceMem &cclBufferPartTwo,
    SubCommInfo &level0CommInfo,
    SubCommInfo &level1CommInfo,
    SubCommInfo &level2CommInfo,
    Stream &mainStream,
    std::vector<Stream> &subStream,
    std::vector<std::shared_ptr<LocalNotify>> &notifyMain,
    std::vector<std::shared_ptr<LocalNotify>> &notifySub)
{
    opInfo_ = opInfo;
    memSliceCount_ = count;
    userRank_ = userRank;

    u32 unitSize = SIZE_TABLE[opInfo_->dataType];
    u64 memSliceSize = memSliceCount_ * unitSize;

    usrInMemAddr_ = opInfo_->inputAddr;
    usrOutMemAddr_ = opInfo_->outputAddr;

    // 设置三级通信域信息
    level0CommInfo_ = level0CommInfo;
    level1CommInfo_ = level1CommInfo;
    level2CommInfo_ = level2CommInfo;

    // 设置链路信息
    level0Links_ = level0CommInfo.links;
    level1Links_ = level1CommInfo.links;
    level2Links_ = level2CommInfo.links;

    // 设置流资源
    mainStream_ = mainStream;
    subStreams_ = subStream;

    // 设置同步信号
    notifyMain_ = notifyMain;
    notifySub_ = notifySub;

    // 检查资源大小
    u32 level0RankSize = level0CommInfo_.localRankSize;
    if (notifyMain_.size() < level0RankSize) {
        HCCL_ERROR("[AllGatherNHRPipelineFor91093][Prepare]rank[%u] notifyMain_ size[%zu] error, is smaller than level0RankSize[%u]",
            userRank_, notifyMain_.size(), level0RankSize);
        return HCCL_E_INTERNAL;
    }
    if (notifySub_.size() < level0RankSize) {
        HCCL_ERROR("[AllGatherNHRPipelineFor91093][Prepare]rank[%u] notifySub_ size[%zu] error, is smaller than level0RankSize[%u]",
            userRank_, notifySub_.size(), level0RankSize);
        return HCCL_E_INTERNAL;
    }

    // 初始化双缓冲内存
    DeviceMem dmaMem0 = DeviceMem::create(cclBufferPartOne.ptr(), memSliceSize);
    DeviceMem dmaMem1 = DeviceMem::create(cclBufferPartTwo.ptr(), memSliceSize);
    dmaBuffers_[0] = dmaMem0;
    dmaBuffers_[1] = dmaMem1;

    HCCL_INFO("[AllGatherNHRPipelineFor91093][Prepare] Three-level pipeline prepared: "
              "level0RankSize[%u], level1RankSize[%u], level2RankSize[%u], "
              "streamNum[%zu], notifyMainNum[%zu], notifySubNum[%zu]",
              level0CommInfo_.localRankSize, level1CommInfo_.localRankSize, level2CommInfo_.localRankSize,
              subStreams_.size(), notifyMain_.size(), notifySub_.size());

    return HCCL_SUCCESS;
}

HcclResult AllGatherNHRPipelineFor91093::RunAsync()
{
    HCCL_INFO("[AllGatherNHRPipelineFor91093][RunAsync] Three-level NHR pipeline starts for rank[%u]", userRank_);
    return RunThreeLevelPipeline();
}

HcclResult AllGatherNHRPipelineFor91093::RunThreeLevelPipeline()
{
    // 实现三级流水线算法
    // 根据设计文档中的伪代码实现
    HCCL_INFO("[AllGatherNHRPipelineFor91093][RunThreeLevelPipeline] Starting three-level pipeline");

    u32 level2RankSize = level2CommInfo_.localRankSize;
    u32 level1RankSize = level1CommInfo_.localRankSize;
    u32 level0RankSize = level0CommInfo_.localRankSize;

    u32 unitSize = SIZE_TABLE[opInfo_->dataType];
    u64 memSliceSize = memSliceCount_ * unitSize;
    u64 memSliceOffset = opInfo_->count * unitSize;

    // 当前缓冲区索引 (0 或 1)
    u32 currentBuffer = 0;

    // 首处理：填充第一个缓冲区
    // 用户输入 → DMA缓冲[0]
    DeviceMem locSrc = DeviceMem::create(usrInMemAddr_, memSliceSize);
    u64 localOffset = (opInfo_->count * userRank_ * unitSize) % HCCL_MIN_SLICE_ALIGN_910B;
    DeviceMem locDMAInMem = DeviceMem::create(static_cast<u8 *>(dmaBuffers_[currentBuffer].ptr()) + localOffset,
        memSliceSize);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, locDMAInMem, locSrc, mainStream_));

    // TODO: 实现L2 NHR接收
    // L2_NHR_Receive(dmaBuffers_[currentBuffer], level2CommInfo_, mainStream_);

    // 流水线循环
    for (u32 step = 1; step <= level2RankSize; step++) {
        // 从流：处理上一个缓冲区
        CHK_RET(MainRecordSub(currentBuffer));
        CHK_RET(SubWaitMain(currentBuffer));

        // 计算下一个缓冲区索引
        u32 nextBuffer = 1 - currentBuffer;

        // 并行执行：从流处理当前缓冲区，主流准备下一个缓冲区
        // 注意：实际实现中需要使用真正的并行执行机制
        // 这里简化实现，先执行从流，再执行主流

        // 从流分支：L1 NHR + L0 DoubleRing + 数据回写
        if (level1RankSize > 1) {
            // TODO: 调用AllGatherNHR模板执行L1 NHR
            // L1_NHR_Process(dmaBuffers_[currentBuffer], level1CommInfo_, subStreams_);
        }

        if (level0RankSize > 1) {
            // TODO: 调用AlignedAllGatherDoubleRing模板执行L0 DoubleRing
            // L0_DoubleRing_Process(dmaBuffers_[currentBuffer], level0CommInfo_, subStreams_);
        }

        // 数据回写到用户输出
        // TODO: 实现数据回写

        // 主流分支：准备下一个缓冲区（如果不是最后一步）
        if (step < level2RankSize) {
            // 用户输入 → DMA缓冲[nextBuffer]
            DeviceMem nextLocSrc = DeviceMem::create(usrInMemAddr_, memSliceSize);
            u64 nextLocalOffset = (opInfo_->count * userRank_ * unitSize) % HCCL_MIN_SLICE_ALIGN_910B;
            DeviceMem nextLocDMAInMem = DeviceMem::create(
                static_cast<u8 *>(dmaBuffers_[nextBuffer].ptr()) + nextLocalOffset, memSliceSize);
            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, nextLocDMAInMem, nextLocSrc, mainStream_));

            // TODO: L2 NHR接收下一个缓冲区
            // L2_NHR_Receive(dmaBuffers_[nextBuffer], level2CommInfo_, mainStream_);
        }

        CHK_RET(SubRecordMain(currentBuffer));
        CHK_RET(MainWaitSub(currentBuffer));

        // 切换缓冲区
        currentBuffer = nextBuffer;
    }

    HCCL_INFO("[AllGatherNHRPipelineFor91093][RunThreeLevelPipeline] Three-level pipeline finished");
    return HCCL_SUCCESS;
}

HcclResult AllGatherNHRPipelineFor91093::SyncLevels(u32 step, u32 bufferIndex)
{
    // 同步三级流水线
    HCCL_DEBUG("[AllGatherNHRPipelineFor91093][SyncLevels] Syncing levels for step[%u], buffer[%u]", step, bufferIndex);
    // TODO: 实现具体的同步逻辑
    return HCCL_SUCCESS;
}

HcclResult AllGatherNHRPipelineFor91093::MainRecordSub(u32 bufferIndex)
{
    if (bufferIndex >= notifySub_.size()) {
        return HCCL_E_INTERNAL;
    }
    return LocalNotify::Post(mainStream_, dispatcher_, notifySub_[bufferIndex], INVALID_VALUE_STAGE);
}

HcclResult AllGatherNHRPipelineFor91093::SubWaitMain(u32 bufferIndex)
{
    if (bufferIndex >= notifySub_.size()) {
        return HCCL_E_INTERNAL;
    }
    // 从流等待主流信号
    // 注意：这里简化实现，实际可能需要所有从流都等待
    for (auto &stream : subStreams_) {
        CHK_RET(LocalNotify::Wait(stream, dispatcher_, notifySub_[bufferIndex], INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult AllGatherNHRPipelineFor91093::MainWaitSub(u32 bufferIndex)
{
    if (bufferIndex >= notifyMain_.size()) {
        return HCCL_E_INTERNAL;
    }
    return LocalNotify::Wait(mainStream_, dispatcher_, notifyMain_[bufferIndex], INVALID_VALUE_STAGE);
}

HcclResult AllGatherNHRPipelineFor91093::SubRecordMain(u32 bufferIndex)
{
    if (bufferIndex >= notifyMain_.size()) {
        return HCCL_E_INTERNAL;
    }
    // 从流记录完成信号
    // 注意：这里简化实现，实际可能需要所有从流都记录
    for (auto &stream : subStreams_) {
        CHK_RET(LocalNotify::Post(stream, dispatcher_, notifyMain_[bufferIndex], INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult AllGatherNHRPipelineFor91093::GetNslbAdjInfo(const u32 rank, const u32 rankSize,
    const std::vector<LINK> &links, AdjInfo& nslbAdjInfo)
{
    // TODO: 实现NSLB邻接信息获取
    HCCL_DEBUG("[AllGatherNHRPipelineFor91093]GetNslbAdjInfo start");
    return HCCL_SUCCESS;
}

REGISTER_TEMPLATE(TEMPLATE_ALL_GATHER_NHR_PIPELINE_FOR_910_93, AllGatherNHRPipelineFor91093);

} // namespace hccl