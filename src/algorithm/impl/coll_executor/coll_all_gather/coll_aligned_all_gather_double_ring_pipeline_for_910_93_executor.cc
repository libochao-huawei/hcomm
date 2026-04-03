/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_aligned_all_gather_double_ring_pipeline_for_910_93_executor.h"
#include "hccl_types.h"
#include "alg_template_register.h"

namespace hccl {
CollAlignedAllGatherDoubleRingPipelineFor91093Executor::CollAlignedAllGatherDoubleRingPipelineFor91093Executor(
    const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAllGatherRingFor91093Executor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    desc_.level1SupportedAlgos = {
        AlgTypeLevel1::ALG_LEVEL1_NHR,
        AlgTypeLevel1::ALG_LEVEL1_NB,
        AlgTypeLevel1::ALG_LEVEL1_RING,
        AlgTypeLevel1::ALG_LEVEL1_AHC,
        AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE
    };
    desc_.level2SupportedAlgos = {
        AlgTypeLevel2::ALG_LEVEL2_NHR,
        AlgTypeLevel2::ALG_LEVEL2_NB,
        AlgTypeLevel2::ALG_LEVEL2_RING,
        AlgTypeLevel2::ALG_LEVEL2_PIPELINE
    };
}

HcclResult CollAlignedAllGatherDoubleRingPipelineFor91093Executor::CalcStreamNum(u32& streamNum)
{
    // 计算三级流水线所需的流数量
    // 基本流数量计算
    u32 totalStreamNum = (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING ? LEVEL0_PLANE_NUM_IN_NPRING_DOUBLE :
        LEVEL0_PLANE_NUM_IN_NPRING_SINGLE);
    if (workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE) {
        totalStreamNum *= STREAM_NUM_FOR_DMAREDUCE_ONE_RING;
    }

    // 为三级流水线增加额外的流
    // 主流用于L2 NHR，从流用于L1 NHR + L0 DoubleRing
    totalStreamNum += 1; // 增加一个主流用于L2流水线

    streamNum = totalStreamNum - 1;
    HCCL_INFO("[CollAlignedAllGatherDoubleRingPipelineFor91093Executor][CalcStreamNum] tag[%s] streamNum[%u]",
        tag_.c_str(), streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoubleRingPipelineFor91093Executor::KernelRun(const OpParam &param, ExecMem &execMem)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] The AllGatherPipelineExecutor starts, topoType_[%u]",
        __func__, topoType_);

    // 设置L0和L1通信域信息（复制基类GetLevelCommInfo的逻辑）
    logicalLevel0plane_ = COMM_LEVEL0;
    CHK_RET(CheckCommSize(logicalLevel0plane_, COMM_INDEX_0 + 1));
    logicalLevel0CommInfo_ = GetSubCommInfo(logicalLevel0plane_, COMM_INDEX_0);
    u32 commIndex = logicalLevel0CommInfo_.localRank;
    bool isSelectAHC = (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC ||
        algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_AHC_BROKE);
    logicalLevel1plane_ = isSelectAHC ? COMM_LEVEL1_AHC : COMM_LEVEL1;
    CHK_RET(CheckCommSize(logicalLevel1plane_, commIndex + 1));
    logicalLevel1CommInfo_ = GetSubCommInfo(logicalLevel1plane_, commIndex);

    CHK_RET(ActiveSlaveStreams(param.stream));

    // 获取L2通信域信息
    CHK_RET(CheckCommSize(COMM_LEVEL2, COMM_INDEX_0 + 1));
    level2CommInfo_ = GetSubCommInfo(COMM_LEVEL2, COMM_INDEX_0);

    // 准备流水线参数
    CHK_RET(PreparePipelineParameters(param, execMem));

    // 运行三级流水线
    CHK_RET(RunThreeLevelPipeline(param, execMem));

    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoubleRingPipelineFor91093Executor::PreparePipelineParameters(
    const OpParam &param, ExecMem &execMem)
{
    // TODO: 实现流水线参数准备
    // 1. 计算三级通信域信息
    // 2. 分配双缓冲内存
    // 3. 设置主从流
    HCCL_INFO("[CollAlignedAllGatherDoubleRingPipelineFor91093Executor][PreparePipelineParameters] Preparing pipeline parameters");
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoubleRingPipelineFor91093Executor::RunThreeLevelPipeline(
    const OpParam &param, ExecMem &execMem)
{
    HCCL_INFO("[CollAlignedAllGatherDoubleRingPipelineFor91093Executor][RunThreeLevelPipeline] Starting three-level pipeline");

    // 获取算法模板
    std::unique_ptr<AlgTemplateBase> pipelineTemplate = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALL_GATHER_NHR_PIPELINE_FOR_910_93, dispatcher_);
    CHK_SMART_PTR_NULL(pipelineTemplate);

    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_NHR_PIPELINE_FOR_910_93 for three-level pipeline", __func__);

    // 准备模板参数
    // 注意：这里需要获取双缓冲内存、主从流、同步信号等资源
    // 这些资源应该已经在PreparePipelineParameters中分配好了

    // TODO: 实现完整的模板调用
    // 需要传递：opInfo, userRank, count, cclBufferPartOne, cclBufferPartTwo,
    //          level0CommInfo, level1CommInfo, level2CommInfo,
    //          mainStream, subStreams, notifyMain, notifySub

    HCCL_WARNING("[CollAlignedAllGatherDoubleRingPipelineFor91093Executor][RunThreeLevelPipeline] Three-level pipeline not fully implemented yet");
    return HCCL_SUCCESS;
}

REGISTER_EXEC("CollAlignedAllGatherDoubleRingPipelineFor91093Executor",
              AllGatherOpbasePipeline,
              CollAlignedAllGatherDoubleRingPipelineFor91093Executor);

} // namespace hccl