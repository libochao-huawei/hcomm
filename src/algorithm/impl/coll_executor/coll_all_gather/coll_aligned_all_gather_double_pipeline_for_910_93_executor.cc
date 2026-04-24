/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "coll_aligned_all_gather_double_pipeline_for_910_93_executor.h"
#include "hccl_types.h"
#include "alg_template_register.h"
#include "all_gather_nhr_pub.h"
#include "aligned_all_gather_double_ring_pub.h"
#include "alg_template_base_pub.h"

namespace hccl {
CollAlignedAllGatherDoublePipelineFor91093Executor::CollAlignedAllGatherDoublePipelineFor91093Executor(
    const HcclDispatcher dispatcher,
    std::unique_ptr<TopoMatcher> &topoMatcher)
    : CollAllGatherExecutor(dispatcher, topoMatcher)
{
    DMAReduceFlag_ = workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE;
    desc_.level1SupportedAlgos = {
        AlgTypeLevel1::ALG_LEVEL1_NHR,
        AlgTypeLevel1::ALG_LEVEL1_NB,
        AlgTypeLevel1::ALG_LEVEL1_RING
    };
    desc_.level2SupportedAlgos = {
        AlgTypeLevel2::ALG_LEVEL2_NHR,
        AlgTypeLevel2::ALG_LEVEL2_NB,
        AlgTypeLevel2::ALG_LEVEL2_RING
    };
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::CalcStreamNum(u32& streamNum)
{
    // 计算三级流水线所需的流数量
    HCCL_INFO("[CollAlignedAllGatherDoublePipelineFor91093Executor][CalcStreamNum] topoType_[%u], workflowMode_[%u]",
        topoType_, workflowMode_);
    // 基本流数量计算
    u32 totalStreamNum = (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING ? LEVEL0_PLANE_NUM_IN_NPRING_DOUBLE :
        LEVEL0_PLANE_NUM_IN_NPRING_SINGLE);
    if (workflowMode_ == HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE) { // 工作流模式，双倍的流，用于并行操作
        totalStreamNum *= STREAM_NUM_FOR_DMAREDUCE_ONE_RING; // *2
    }

    // 为三级流水线增加额外的流
    // 主流用于L2 NHR，从流用于L1 NHR + L0 DoubleRing
    totalStreamNum += 1; // 增加一个主流用于L2流水线

    streamNum = totalStreamNum - 1;
    HCCL_INFO("[CollAlignedAllGatherDoublePipelineFor91093Executor][CalcStreamNum] tag[%s] streamNum[%u]",
        tag_.c_str(), streamNum);
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::CalcCommInfo(std::vector<LevelNSubCommTransport>& opTransport)
{
    TransportMemType inputType = TransportMemType::RESERVED;
    TransportMemType outputType = TransportMemType::RESERVED;
    CHK_RET(CalcTransportMemType(inputType, outputType));
    CHK_RET(CalcLevel0CommInfo(inputType, outputType, opTransport));
    CHK_RET(CalcLevel1CommInfo(inputType, outputType, opTransport));
    CHK_RET(CalcLevel2CommInfo(inputType, outputType, opTransport));
    return HCCL_SUCCESS;
}

// level0 ring
HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::CalcLevel0CommInfo(TransportMemType inputType,
    TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commParaInfo(COMM_LEVEL0, CommType::COMM_TAG_RING_INNER);
    CHK_RET(CalcCommPlaneInfo(tag_, commParaInfo, opTransport[COMM_LEVEL0], inputType, outputType));
    return HCCL_SUCCESS;
}

// level2 NHR
HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::CalcLevel2CommInfo(TransportMemType inputType,
    TransportMemType outputType,
    std::vector<LevelNSubCommTransport>& opTransport)
{
    CommParaInfo commParaInfo(COMM_LEVEL2, CommType::COMM_TAG_MAX);
    if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NHR) {
        commParaInfo.commType = CommType::COMM_TAG_NONUNIFORM_HIERARCHICAL_RING;
    } else if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NB) {
        commParaInfo.commType = CommType::COMM_TAG_NONUNIFORM_BRUCK;
    } else {
        commParaInfo.commType = CommType::COMM_TAG_RING_INNER;
    }
    CHK_RET(CalcCommPlaneInfo(tag_, commParaInfo, opTransport[COMM_LEVEL2], inputType, outputType));
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::CalcTransportMemType(TransportMemType &inputType,
    TransportMemType &outputType)
{
    inputType = TransportMemType::CCL_INPUT;
    outputType = TransportMemType::CCL_OUTPUT;
    HCCL_INFO("[CollAlignedAllGatherDoublePipelineFor91093Executor][CalcTransportMemType]" \
        "tag[%s] inputType[%d], outputType[%d]",
        tag_.c_str(), inputType, outputType);
    return HCCL_SUCCESS;
}

// 每次循环处理的数据量，双流水的情况下需要满足每个流水线都能满载
u64 CollAlignedAllGatherDoublePipelineFor91093Executor::CalcLoopMaxCount(const u64 cclBuffSize, const u32 unitSize)
{
    u32 bufferSliceNum = 2U; // 分成两片，做流水ping-pong
    u64 maxCountPerLoop = cclBuffSize / bufferSliceNum / topoAttr_.userRankSize / HCCL_MIN_SLICE_ALIGN * HCCL_MIN_SLICE_ALIGN / unitSize;
    HCCL_INFO("[%s] tag[%s] maxCountPerLoop[%llu]", __func__, tag_.c_str(), maxCountPerLoop);

    return maxCountPerLoop;
}

// 编排
HcclResult hccl::CollAlignedAllGatherDoublePipelineFor91093Executor::Orchestrate(
    OpParam &param, AlgResourceResponse &algRes)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollAlignedAllGatherDoublePipelineFor91093Executor][Orchestrate] begins.");

    HcclUs startut = TIME_NOW();
    tag_ = param.tag;
    algResResp_ = &algRes;

    // 设置L0和L1通信域信息
    CHK_RET(CheckCommSize(COMM_LEVEL0, COMM_INDEX_0 + 1));
    level0CommInfo_ = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    u32 commIndex = level0CommInfo_.localRank;
    CHK_RET(CheckCommSize(COMM_LEVEL1, commIndex + 1));
    level1CommInfo_ = GetSubCommInfo(COMM_LEVEL1, commIndex);
    // 获取L2通信域信息
    CHK_RET(CheckCommSize(COMM_LEVEL2, COMM_INDEX_0 + 1));
    level2CommInfo_ = GetSubCommInfo(COMM_LEVEL2, COMM_INDEX_0);

    // 准备通信资源和流，资源待商榷
    mainStreamL2_ = param.stream;
    subStreams_ = algResResp_->slaveStreams;
    mainStreamL1L0_ = subStreams_.back();
    notifyL2ToL1L0_ = algResResp_->notifiesAux.back(); // 主流通知从流使用Aux
    notifyL1L0ToL2_ = algResResp_->notifiesMain.back(); // 从流通知主流使用Main
    notifyRingMain_.assign(algResResp_->notifiesMain.begin(), algResResp_->notifiesMain.end() - 1);
    for (size_t i = 0; i < notifyRingMain_.size(); ++i) {
        HCCL_INFO("notifyRingMain_[%zu][%p]", i, notifyRingMain_[i].get());
    }
    notifyRingSub_.assign(algResResp_->notifiesAux.begin(), algResResp_->notifiesAux.end() - 1);
    for (size_t i = 0; i < notifyRingSub_.size(); ++i) {
        HCCL_INFO("notifyRingSub_[%zu][%p]", i, notifyRingSub_[i].get());
    }
    ringSubStreams_.assign(subStreams_.begin(), subStreams_.end() - 1);

    // 从流最后一个给了L1L0 当主流用，先从subStreams_里pop出来，后续GetSubStreamInfoOnOneRing里就不会拿到这个流了
    // algResResp_->slaveStreams.pop_back();
    // algResResp_->notifiesMain.pop_back();
    // algResResp_->notifiesAux.pop_back();

    // 计算通信域信息和内存类型
    unitSize_ = SIZE_TABLE[param.DataDes.dataType];
    cclInputSizeHalved_ = algResResp_->cclInputMem.size() / 2;
    cclInputAMem_ = algResResp_->cclInputMem.range(0, cclInputSizeHalved_);
    cclInputBMem_ = algResResp_->cclInputMem.range(cclInputSizeHalved_, cclInputSizeHalved_);
    cclOutputSizeHalved_ = algResResp_->cclOutputMem.size() / 2;
    cclOutputAMem_ = algResResp_->cclOutputMem.range(0, cclOutputSizeHalved_);
    cclOutputBMem_ = algResResp_->cclOutputMem.range(cclOutputSizeHalved_, cclOutputSizeHalved_);

    CHK_RET(RunLoop(param)); // 运行循环，循环内执行三级流水线

    HCCL_INFO("tag[%s], Allgather executor orchestrate success, take time [%lld]us.", tag_.c_str(),
        DURATION_US(TIME_NOW() - startut));

    // algResResp_->notifiesAux.push_back(notifyL2ToL1L0_); // 主流通知从流使用Aux
    // algResResp_->notifiesMain.push_back(notifyL1L0ToL2_); // 从流通知主流使用Main
    // algResResp_->slaveStreams.push_back(mainStreamL1L0_); // 主流放回从流列表
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::RunL2Stage(
    const OpParam &param, ExecMem &execMem, u64 loopIdx, u64 memIdx, u64 bufferSliceNum)
{
    // superpod数量不超过1不需要跨超节点；最后一轮循环处理L1L0的最后一片数据，L2不需要参与通信，跳过L2阶段
    if (level2CommInfo_.localRankSize <= 1 || loopIdx >= bufferSliceNum) {
        return HCCL_SUCCESS;
    }
    // 同步：等待 loopIdx-2 的 L1+L0 通信完成，释放 DMA buffer 后 L2 再复用
    if (loopIdx >= 2) {
        CHK_RET(LocalNotify::Wait(mainStreamL2_, dispatcher_, notifyL1L0ToL2_, INVALID_VALUE_STAGE));
    }

    // Local Copy: UserIn -> Ccl
    u64 curSize = execMem.count * unitSize_;
    DeviceMem srcMem = DeviceMem::create(static_cast<u8 *>(execMem.inputPtr), curSize);
    DeviceMem dstMem = execMem.inputMem.range(0, curSize);
    CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dstMem, srcMem, mainStreamL2_));

    if (DMAReduceFlag_) {
        u64 dstMemOffset = topoAttr_.userRank * curSize;
        DeviceMem dmaDst = execMem.outputMem.range(dstMemOffset, curSize);
        if (level1CommInfo_.localRankSize > 1 || level2CommInfo_.localRankSize > 1) {
            CHK_RET(HcclD2DMemcpyAsync(dispatcher_, dmaDst, dstMem, const_cast<Stream &>(mainStreamL2_)));
        }
    }

    // L2 Rx -> DMA[0/1]
    u64 baseOffset = memIdx == 0 ? 0 : cclInputSizeHalved_;
    CHK_RET(KernelRunInterSuperPod(param, execMem, baseOffset));
    CHK_RET(LocalNotify::Post(mainStreamL2_, dispatcher_, notifyL2ToL1L0_, INVALID_VALUE_STAGE));
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::RunL1L0Stage(
    const OpParam &param, ExecMem &lastExecMem, u64 loopIdx, u64 memIdx, u64 bufferSliceNum)
{
    // 第一轮等待L2处理完
    if (loopIdx < 1) {
        return HCCL_SUCCESS;
    }
    // 同步：等待上一轮的 L2 通信完成
    CHK_RET(LocalNotify::Wait(mainStreamL1L0_, dispatcher_, notifyL2ToL1L0_, INVALID_VALUE_STAGE));

    u64 baseOffset = memIdx == 0 ? cclInputSizeHalved_ : 0;
    if (level1CommInfo_.localRankSize > 1) {
        CHK_RET(KernelRunInterServer(param, lastExecMem, baseOffset));
    }
    CHK_RET(KernelRunIntraServer(param, lastExecMem, baseOffset));

    // loopIdx=bufferSliceNum-1 的Post没有L2消费者(L2在loopIdx=bufferSliceNum不运行)
    if (loopIdx != bufferSliceNum - 1) {
        CHK_RET(LocalNotify::Post(mainStreamL1L0_, dispatcher_, notifyL1L0ToL2_, INVALID_VALUE_STAGE));
    }
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::RunLoop(OpParam &param)
{
    u8* userInputPtr = static_cast<u8 *>(param.inputPtr);
    u8* userOutputPtr = static_cast<u8 *>(param.outputPtr);
    CHK_PTR_NULL(userInputPtr);
    CHK_PTR_NULL(userOutputPtr);

    u64 maxCountPerLoop = CalcLoopMaxCount(algResResp_->cclInputMem.size(), unitSize_);
    CHK_PRT_RET(maxCountPerLoop == 0,
        HCCL_ERROR("[CollAlignedAllGatherDoublePipelineFor91093Executor][RunLoop]tag[%s] userRankSize[%u] maxCountPerLoop[%llu]",
        tag_.c_str(), topoAttr_.userRankSize, maxCountPerLoop), HCCL_E_PARA);
    u64 bufferSliceNum = (param.DataDes.count + maxCountPerLoop - 1) / maxCountPerLoop;
    u64 loopNum = bufferSliceNum + 1;
    u64 countLeft = param.DataDes.count; // 剩余的数据量

    u32 memIdx = 0;
    ExecMem lastExecMem;
    for (u64 loopIdx = 0; loopIdx < loopNum; loopIdx++) {
        u64 curCount = countLeft > maxCountPerLoop ? maxCountPerLoop : countLeft; // 当前循环处理的数据量
        countLeft -= curCount;

        ExecMem execMem;
        execMem.count = curCount;
        execMem.inputMem = memIdx == 0 ? cclInputAMem_ : cclInputBMem_;
        execMem.outputMem = memIdx == 0 ? cclOutputAMem_ : cclOutputBMem_;
        execMem.inputPtr = userInputPtr;
        execMem.outputPtr = userOutputPtr;

        CHK_RET(RunL2Stage(param, execMem, loopIdx, memIdx, bufferSliceNum));
        CHK_RET(RunL1L0Stage(param, lastExecMem, loopIdx, memIdx, bufferSliceNum));

        if (!is310P3Common_) {
            CHK_RET(LaunchTaskExtend(dispatcher_, param.stream, algResResp_->slaveStreams));
        }

        u64 curSize = curCount * unitSize_;
        userInputPtr += curSize;
        userOutputPtr += curSize;
        memIdx = 1 - memIdx; // 双缓冲交替使用
        lastExecMem = execMem;
    }

    CHK_RET(LocalNotify::Wait(mainStreamL2_, dispatcher_, notifyL1L0ToL2_, INVALID_VALUE_STAGE));
    return HCCL_SUCCESS;
}

// 跨超节点
HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::KernelRunInterSuperPod(const OpParam &param, ExecMem &execMem, u64 baseOffset)
{
    std::unique_ptr<AlgTemplateBase> level2AGExecutor;
    if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NB) {
        level2AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_NB in COMM_LEVEL2", __func__);
    } else if (algType_.algoLevel2 == AlgTypeLevel2::ALG_LEVEL2_NHR) {
        level2AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_NHR in COMM_LEVEL2", __func__);
    } else {
        level2AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_RING in COMM_LEVEL2", __func__);
    }
    CHK_SMART_PTR_NULL(level2AGExecutor);

    u64 curDataSegsSliceSize = execMem.count * unitSize_;
    std::vector<Slice> level2DataSegsSlice = PrepareSlicesL2(
        param, level2CommInfo_, level1CommInfo_, level0CommInfo_, unitSize_, curDataSegsSliceSize);
    CHK_RET(level2AGExecutor->Prepare(execMem.outputMem, execMem.outputMem, execMem.inputMem, execMem.count, param.DataDes.dataType,
        mainStreamL2_, HCCL_REDUCE_RESERVED, INVALID_VALUE_RANKID, level2DataSegsSlice, baseOffset));

    CHK_RET(RunTemplate(level2AGExecutor, level2CommInfo_));
    HCCL_INFO("AllGather ring [superpod] level2 AllGather run success, topoType_[%u], agv[%u]", topoType_, isAllGatherV_);
    return HCCL_SUCCESS;
}

// 超节点内的节点间通信 L1nhr
HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::KernelRunInterServer(
    const OpParam &param, ExecMem &execMem, u64 baseOffset)
{
    u64 curDataSegsSliceSize = execMem.count * unitSize_;
    std::vector<Slice> level1DataSegsSlice = PrepareSlicesL1(
        param, level2CommInfo_, level1CommInfo_, level0CommInfo_, unitSize_, curDataSegsSliceSize);

    std::unique_ptr<AlgTemplateBase> level1AGExecutor;
    if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_RING) {
        level1AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_RING, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_RING in COMM_LEVEL1", __func__);
    } else if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_NB) {
        level1AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NB, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_NB in COMM_LEVEL1", __func__);
    } else if (algType_.algoLevel1 == AlgTypeLevel1::ALG_LEVEL1_NHR) {
        level1AGExecutor
            = AlgTemplateRegistry::Instance().GetAlgTemplate(TemplateType::TEMPLATE_ALL_GATHER_NHR, dispatcher_);
        HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALL_GATHER_NHR in COMM_LEVEL1", __func__);
    } else {
        HCCL_ERROR("AllGather ring: unsupported algtype [%s].", AlgTypeToStr(algType_).c_str());
        return HCCL_E_NOT_SUPPORT;
    }
    CHK_SMART_PTR_NULL(level1AGExecutor);
    CHK_RET(level1AGExecutor->Prepare(execMem.outputMem, execMem.outputMem, execMem.inputMem, execMem.count, param.DataDes.dataType,
        mainStreamL1L0_, HCCL_REDUCE_RESERVED, INVALID_VALUE_RANKID, level1DataSegsSlice, baseOffset));

    CHK_RET(RunTemplate(level1AGExecutor, level1CommInfo_));
    HCCL_INFO("AllGather ring [superpod] level1 AllGather run success, topoType_[%u], agv[%u]", topoType_, isAllGatherV_);
    return HCCL_SUCCESS;
}

// Server内的通信
HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::KernelRunIntraServer(
    const OpParam &param, ExecMem &execMem, u64 baseOffset)
{
    // 节点内做AllGather ring
    u64 curDataSegsSliceSize = execMem.count * unitSize_;
    std::vector<std::vector<Slice>> multRingsSlice;
    CHK_RET(PrepareSlicesL0(multRingsSlice, param, level2CommInfo_, level1CommInfo_, level0CommInfo_,
        unitSize_, curDataSegsSliceSize));

    std::vector<std::vector<Slice>> multRingsUserMemSlice;
    CHK_RET(PrepareUserMemSlices(multRingsUserMemSlice, multRingsSlice, param, level2CommInfo_, level1CommInfo_,
        level0CommInfo_, unitSize_, curDataSegsSliceSize));

    HcomCollOpInfo opInfo = {"", execMem.inputPtr, execMem.outputPtr, execMem.count, param.DataDes.dataType, 0,
        HCCL_REDUCE_RESERVED, param.DataDes.strideCount};
    if (!DMAReduceFlag_ && (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING)) {
        opInfo.inputAddr = execMem.inputMem.ptr();
        opInfo.outputAddr = execMem.outputMem.ptr();
    }

    if (DMAReduceFlag_ && (level1CommInfo_.localRankSize > 1 || level2CommInfo_.localRankSize > 1)) {
        // allgather输入放在CCL buffer上，通过设置nullptr指示要从CCL buffer获取输入
        opInfo.inputAddr = nullptr;
    }

    if (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING) {
        CHK_RET(DoubleRingAllGather(param.tag, execMem.inputMem, execMem.outputMem, execMem.count, param.DataDes.dataType,
        multRingsSlice, mainStreamL1L0_, PROF_STAGE_2, baseOffset, &opInfo, multRingsUserMemSlice));
    } else if (topoType_ == TopoType::TOPO_TYPE_NP_SINGLE_RING) {
        CHK_RET(MultiRingAllGather(param.tag, execMem.inputMem, execMem.outputMem, execMem.count, param.DataDes.dataType,
        multRingsSlice, mainStreamL1L0_, PROF_STAGE_2, baseOffset, &opInfo, multRingsUserMemSlice, COMM_LEVEL0));
    } else {
        return HCCL_E_NOT_SUPPORT;
    }
    return HCCL_SUCCESS;
}

std::vector<Slice> CollAlignedAllGatherDoublePipelineFor91093Executor::PrepareSlicesL1(const OpParam &param,
    const SubCommInfo &level2CommInfo, const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo,
    u32 perDataSize, u64 inputMemSize) const
{
    const u32 level0RankSize = level0CommInfo.localRankSize;
    const u32 level0ServerIndex = level0CommInfo.localRank;
    const u32 level1RankSize = level1CommInfo.localRankSize;
    const u32 level2RankSize = level2CommInfo.localRankSize;
    std::vector<Slice> level1DataSegsSlice;
    for (u32 j = 0; j < level1RankSize; j++) {
        for (u32 i = 0; i < level2RankSize; i++) {
            Slice level1Slice;
            level1Slice.size = inputMemSize;
            level1Slice.offset = inputMemSize *
                (i * level1RankSize * level0RankSize + j * level0RankSize + level0ServerIndex);

            HCCL_DEBUG("[CollAlignedAllGatherDoublePipelineFor91093Executor][PrepareSlicesL1] rank[%u], level1index[%u], level2index[%u], slices.offset=%llu, slices.size=%llu",
                level0CommInfo.localRank, j, i, level1Slice.offset, level1Slice.size);

            level1DataSegsSlice.push_back(level1Slice);
        }
    }
    return level1DataSegsSlice;
}

std::vector<Slice> CollAlignedAllGatherDoublePipelineFor91093Executor::PrepareSlicesL2(const OpParam &param,
    const SubCommInfo &level2CommInfo, const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo,
    u32 perDataSize, u64 inputMemSize) const
{
    const u32 level0RankSize = level0CommInfo.localRankSize;
    const u32 level0ServerIndex = level0CommInfo.localRank;
    const u32 level1RankSize = level1CommInfo.localRankSize;
    const u32 level1ServerIndex = level1CommInfo.localRank;
    const u32 level2RankSize = level2CommInfo.localRankSize;
    std::vector<Slice> level2DataSegsSlice;
    for (u32 i = 0; i < level2RankSize; i++) {
        Slice sliceTemp;
        sliceTemp.size = inputMemSize;
        sliceTemp.offset = inputMemSize *
            (i * level1RankSize * level0RankSize + level1ServerIndex * level0RankSize + level0ServerIndex);
        level2DataSegsSlice.push_back(sliceTemp);
    }
    return level2DataSegsSlice;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::PrepareSlicesL0(std::vector<std::vector<Slice>> &multRingsSlice,
    const OpParam &param, const SubCommInfo &level2CommInfo, const SubCommInfo &level1CommInfo,
    const SubCommInfo &level0CommInfo, u32 perDataSize, u64 inputMemSize)
{
    const u32 level0RankSize = level0CommInfo.localRankSize;
    const u32 level1RankSize = level1CommInfo.localRankSize;
    const u32 level2RankSize = level2CommInfo.localRankSize;

    std::vector<Slice> dataSegsSlice;
    CHK_RET(PrepareAllgatherSlice(level0RankSize, inputMemSize, dataSegsSlice));

    // 多环数据切分
    std::vector<std::vector<Slice>> multRingsSliceZero; // 数据基于该rank上环0的偏移
    bool ARSFlag = topoMatcher_->GetARSFlag();
    bool ARSDoubleRing = (ARSFlag && (level0RankSize > FACTOR_TWO) && topoAttr_.isARSDoubleRing);
 
    if (ARSDoubleRing) {
        std::vector<u32> mockNicList;
        mockNicList.reserve(level0RankSize);
        for (u32 rankIndex = 0; rankIndex < level0RankSize; rankIndex++) {
            mockNicList.push_back(rankIndex);
        }
        multRingsSliceZero = PrepareMultiRingSlice(dataSegsSlice, param.tag, false, mockNicList);
    } else if (topoType_ == TopoType::TOPO_TYPE_NP_DOUBLE_RING &&
        !IsSupportUnifiedMarch(param, topoType_, topoAttr_.serverNum, topoAttr_.superPodNum)) {
        multRingsSliceZero = PrepareMultiRingSlice(dataSegsSlice, param.tag, false, topoAttr_.nicList);
    } else {
        multRingsSliceZero.push_back(dataSegsSlice);
    }
    for (u32 ringIndex = 0; ringIndex < multRingsSliceZero.size(); ringIndex++) {
        std::vector<Slice> level2DataSlice;
        CHK_RET(CalculateLevel2AllgatherSlice(inputMemSize, level0RankSize, level1RankSize, level2RankSize,
            multRingsSliceZero, level2DataSlice, ringIndex));
        multRingsSlice.push_back(level2DataSlice);
    }

    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::PrepareUserMemSlices(std::vector<std::vector<Slice>> &userMemSlices,
    const std::vector<std::vector<Slice>> &multRingsSlice, const OpParam &param, const SubCommInfo &level2CommInfo,
    const SubCommInfo &level1CommInfo, const SubCommInfo &level0CommInfo, u32 perDataSize, u64 inputMemSize)
{
    CHK_PRT_RET(0 < param.DataDes.strideCount && param.DataDes.strideCount < param.DataDes.count,
        HCCL_ERROR("[CollAlignedAllGatherDoublePipelineFor91093Executor][KernelRun]strideCount[%llu] is smaller than opCount[%llu]",
        param.DataDes.strideCount, param.DataDes.count),
        HCCL_E_PARA);
    HCCL_DEBUG("[CollAlignedAllGatherDoublePipelineFor91093Executor][KernelRun]strideCount[%llu], opCount[%llu]",
        param.DataDes.strideCount, param.DataDes.count);

    if (!DMAReduceFlag_) {
        userMemSlices = multRingsSlice;
        // 图模式，根据strideCount更新slice的offset
        if (param.DataDes.strideCount != 0) {
            CHK_RET(UpdateOffsetBasedOnStrideCount(param, userMemSlices));
        }
    } else {
        for (u32 ringIndex = 0; ringIndex < multRingsSlice.size(); ringIndex++) {
            std::vector<Slice> userMemSlice;
            for (const auto &cclSlice : multRingsSlice[ringIndex]) {
                Slice tmpSlice;
                u64 count = (param.DataDes.strideCount == 0) ? param.DataDes.count : param.DataDes.strideCount;
                tmpSlice.size = cclSlice.size;
                tmpSlice.offset = (cclSlice.offset / inputMemSize) * count * perDataSize +
                    multRingsSlice[ringIndex][0].offset;
                userMemSlice.push_back(tmpSlice);
                HCCL_DEBUG("rank[%u], ringIndex[%u], tmpSlice.offset=[%llu], size=[%llu]",
                    topoAttr_.userRank, ringIndex, tmpSlice.offset, tmpSlice.size);
            }
            userMemSlices.push_back(userMemSlice);
        }
    }
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::DoubleRingAllGather(
    const std::string &tag, DeviceMem inputMem, DeviceMem outputMem,
    const u64 count, const HcclDataType dataType, const std::vector<std::vector<Slice> > multRingsSliceZero,
    Stream stream, s32 profStage, const u64 baseOffset, const HcomCollOpInfo *opInfo,
    const std::vector<std::vector<Slice>> multRingsUserMemSlice)
{
    HCCL_CONFIG_INFO(HCCL_ALG, "[CollAlignedAllGatherDoublePipelineFor91093Executor]userRank[%u], count[%llu]",
        topoAttr_.userRank, count);

    (void)tag;
    HCCL_INFO("[CollAlignedAllGatherDoublePipelineFor91093Executor][DoubleRingAllGather] DoubleRingAllGather starts");
    HcclResult ret = HCCL_SUCCESS;
    u32 ringNum = multRingsSliceZero.size();
    CHK_RET(CheckCommSize(COMM_LEVEL0, ringNum));
    // 拿到ring环映射关系
    SubCommInfo level0ZeroCommInfo = GetSubCommInfo(COMM_LEVEL0, COMM_INDEX_0);
    auto nicList = topoAttr_.nicList;
    std::vector<std::vector<u32>> multiRingsOrder =
        GetRingsOrderByTopoType(level0ZeroCommInfo.localRankSize, topoType_, nicList);
    // 生成两个ring上的userMemOut_上对应的slices
    std::vector<std::vector<Slice>> userMemOutputSlicesOfDoubleRing;
    CHK_RET(CollectMultiRingsUserMemSlices(ringNum, dataType, opInfo, multRingsSliceZero,
        multiRingsOrder, multRingsUserMemSlice, userMemOutputSlicesOfDoubleRing));
    // 生成两个ring上的rankOrder
    std::vector<std::vector<u32>> rankOrders;
    CHK_RET(CollectMultiRingsRankOrder(ringNum, multiRingsOrder, rankOrders));
    // 初始化executor
    std::unique_ptr<AlgTemplateBase> tempAlg = AlgTemplateRegistry::Instance().GetAlgTemplate(
        TemplateType::TEMPLATE_ALIGNED_ALL_GATHER_DOUBLE_RING, dispatcher_);
    HCCL_CONFIG_INFO(HCCL_ALG, "[%s] Run TEMPLATE_ALIGNED_ALL_GATHER_DOUBLE_RING in COMM_LEVEL0", __func__);
    CHK_SMART_PTR_NULL(tempAlg);
    HCCL_INFO("notifyRingMain_.size[%zu], notifyRingSub_.size[%zu], ringSubStreams_.size[%zu]",
        notifyRingMain_.size(), notifyRingSub_.size(), ringSubStreams_.size());
    CHK_RET(tempAlg->Prepare(const_cast<HcomCollOpInfo *>(opInfo), topoAttr_.userRank, ringSubStreams_,
        notifyRingMain_, notifyRingSub_, rankOrders, userMemOutputSlicesOfDoubleRing));

    ret = tempAlg->Prepare(outputMem, outputMem, inputMem, count, dataType, stream, multRingsSliceZero,
        HCCL_REDUCE_RESERVED, LEVEL0_BRIDGE_RANK_ID, baseOffset);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollAlignedAllGatherDoublePipelineFor91093Executor][DoubleRingAllGather]Double ring "
        "AllGather failed, return[%d]", ret), ret);
    u32 ringIndexOp = COMM_INDEX_0;
    u32 rankSize = level0ZeroCommInfo.localRankSize;
    ret = tempAlg->RegisterProfiler(
        ((ringIndexOp + 1) << PROF_RINGINDEX_OFFSET_OF_PLANEID) +
        (rankSize << PROF_RANKSIZE_OFFSET_OF_PLANEID) + level0ZeroCommInfo.localRank,
        profStage, HCCL_EXEC_STEP_NOT_SET, stream);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollAlignedAllGatherDoublePipelineFor91093Executor][DoubleRingAllGather]Double ring "
        "AllGather failed, return[%d]", ret), ret);

    // 空拷贝用于后续操作附着
    CHK_RET(AlgTemplateBase::ExecEmptyTask(inputMem, outputMem, stream, dispatcher_));
    ret = RunTemplate(tempAlg, level0ZeroCommInfo);
    CHK_PRT_RET(ret != HCCL_SUCCESS,
        HCCL_ERROR("[CollAlignedAllGatherDoublePipelineFor91093Executor][DoubleRingAllGather] Double ring "
                   "AllGather failed, return[%d]", ret), ret);
    // 添加空task,保证执行时不乱序
    CHK_RET(AlgTemplateBase::ExecEmptyTask(inputMem, outputMem, stream, dispatcher_));
    return HCCL_SUCCESS;
}

HcclResult CollAlignedAllGatherDoublePipelineFor91093Executor::GetSubStreamInfoOnOneRing(const u32 ringIndex,
                                         std::vector<Stream>                       &subStreamsInOneRing,
                                         std::vector<std::shared_ptr<LocalNotify>> &mainSignalsInOneRing,
                                         std::vector<std::shared_ptr<LocalNotify>> &subSignalsInOneRing)
{
    u32 ringNum = algResResp_->slaveStreams.size();
    if (ringNum == LEVEL0_PLANE_NUM_IN_NPRING_DOUBLE * STREAM_NUM_FOR_DMAREDUCE_ONE_RING) {
        subStreamsInOneRing.push_back(algResResp_->slaveStreams[ringIndex + 1]);
        mainSignalsInOneRing.push_back(algResResp_->notifiesMain[ringIndex + 1]);
        subSignalsInOneRing.push_back(algResResp_->notifiesAux[ringIndex + 1]);
    } else if (ringNum == LEVEL0_PLANE_NUM_IN_NPRING_SINGLE * STREAM_NUM_FOR_DMAREDUCE_ONE_RING) {
        subStreamsInOneRing.push_back(algResResp_->slaveStreams[ringIndex]);
        mainSignalsInOneRing.push_back(algResResp_->notifiesMain[ringIndex]);
        subSignalsInOneRing.push_back(algResResp_->notifiesAux[ringIndex]);
    }
    return HCCL_SUCCESS;
}

REGISTER_EXEC("AlignedAllGatherDoublePipelineFor91093Executor",
              AlignedAllGatherDoublePipelineFor91093,
              CollAlignedAllGatherDoublePipelineFor91093Executor);

} // namespace hccl