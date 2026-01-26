/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllReduceMesh1DOneShot 类实现
 * Author: caichanghua
 * Create: 2025-02-19
 */

#include "ccu_context_all_reduce_mesh1d_one_shot.h"
#include "ccu_instruction_all_reduce_mesh1d_one_shot.h"

namespace Hccl {

constexpr int INPUT_XN_ID  = 0;
constexpr int OUTPUT_XN_ID = 1;
constexpr int TOKEN_XN_ID  = 2;
constexpr int CKE_IDX_0    = 0;
constexpr int CKE_IDX_1    = 1;
constexpr int CKE_IDX_2    = 2;
constexpr int CKE_IDX_3    = 3;

constexpr uint64_t MAIN_BLOCK_LOOP_NUM = 128;
constexpr uint64_t TAIL_BLOCK_LOOP_NUM = 64;
constexpr uint64_t MISSION_NUM_2       = 2;

CcuContextAllReduceMesh1DOneShot::CcuContextAllReduceMesh1DOneShot(const CcuCtxArg &arg,
                                                                   const std::vector<CcuTransport *> &transports,
                                                                   const CcuTransportGroup &group)
    : CcuContext(arg, transports, group)
{
    const CcuCtxArgAllReduceMesh1DOneShot *ctxArg = dynamic_cast<const CcuCtxArgAllReduceMesh1DOneShot *>(&arg);
    if (ctxArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuCtxArgAllReduceMesh1DOneShot::ctxArg ptr is null"));
    }
    notifySignal_ = ctxArg->notifySignal_;
    rankId_ = ctxArg->rankId_;
    rankSize_ = ctxArg->dimSize_[0];
    dataType_ = ctxArg->op_.dataType;
    outputDataType_ = ctxArg->op_.outputDataType;
    reduceOp_ = ctxArg->op_.reduceOp;
    if (outputDataType_ == DataType::INVALID) {
        outputDataType_ = dataType_;
        HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] outputDataType is [INVALID], set outputDataType to[%s]",
            outputDataType_.Describe().c_str());
    }
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] Init, CtxArgs are notifySignal_[%s], rankId[%u], rankSize[%u], dataType[%s], "
        "outputDataType[%s], reduceOp[%s]", notifySignal_.c_str(), rankId_, rankSize_, dataType_.Describe().c_str(),
        outputDataType_.Describe().c_str(), reduceOp_.Describe().c_str());
}

void CcuContextAllReduceMesh1DOneShot::Algorithm()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] AllReduceMesh1DOneShot start");
    InitResource();
    ImportReduceVariables();  // 载入尾块处理 mission 的参数与同步信号
    LoadArgs();  // 加载 taskArg 参数
    Presync();  // 跨卡前同步，交换参数信息

    SyncTailBlock(0);  // 通知尾块处理 mission 参数同步完成，可以开始执行

    DoGroupReduce();

    SyncTailBlock(1);  // 等待尾块处理 mission 任务结束
    Postsync();  // 所有搬运任务结束后，跨卡后同步

    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] AllReduceMesh1DOneShot end");
    return;
}

void CcuContextAllReduceMesh1DOneShot::InitResource()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] InitResource start");
    // 初始化资源
    output_ = CreateVariable();
    uint16_t transportIdx = 0;
    if (transports.size() == 0) {
        THROW<NullPtrException>(StringFormat("CcuContextAllReduceMesh1DOneShot transports is empty"));
    }
    // 按照rank号从小到大遍历transports，遇到本rank就填充本地资源，否则依次取远端资源，要求给框架返回的Link同样是按顺序排列的
    for (uint64_t peerId = 0; peerId < rankSize_; peerId++) {
        if (peerId == rankId_) {
            input_.push_back(CreateVariable());
            token_.push_back(CreateVariable());
        } else {
            HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] MyRank[%u], PeerId[%llu], TransportId[%u]",
                rankId_, peerId, transportIdx);
            CHK_PRT_RET(transports[transportIdx] == nullptr,
                HCCL_ERROR("[CcuContextAllReduceMesh1DOneShot] Algorithm transport ptr is null"),);
            input_.push_back(CreateVariable((*transports[transportIdx]), INPUT_XN_ID));
            token_.push_back(CreateVariable((*transports[transportIdx]), TOKEN_XN_ID));
            transportIdx++;
        }
    }
    groupOpSize_ = CreateGroupOpSize();

    // reduce 的尾块需要的变量
    tailBlockOffSet_ = CreateVariable();
    tailBlockGroupOpSize_ = CreateGroupOpSize();
    mainBlockCtxSignal_ = CreateMaskSignal();

    AllocGoResource(MAIN_BLOCK_LOOP_NUM);
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] InitResource end");
}

void CcuContextAllReduceMesh1DOneShot::ImportReduceVariables()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] ImportReduceVariables start");
    reduceInput_ = ImportVariable(notifySignal_ + "_Input_Reduce_Tail_Block");
    reduceOutput_ = ImportVariable(notifySignal_ + "_Output_Reduce_Tail_Block");
    reducetoken_ = ImportVariable(notifySignal_ + "_Token_Reduce_Tail_Block");
    reduceInputOffSet_ = ImportVariable(notifySignal_ + "_Input_Offset_Reduce_Tail_Block");
    reduceOutputOffSet_ = ImportVariable(notifySignal_ + "_Output_Offset_Reduce_Tail_Block");
    reduceGroupOpSize_.addrOffset = ImportVariable(notifySignal_ + "_AddrOffset_Reduce_Tail_Block");
    reduceGroupOpSize_.loopParam = ImportVariable(notifySignal_ + "_LoopParam_Reduce_Tail_Block");
    reduceGroupOpSize_.parallelParam = ImportVariable(notifySignal_ + "_ParallelParam_Reduce_Tail_Block");
    reduceGroupOpSize_.residual = ImportVariable(notifySignal_ + "_Residual_Reduce_Tail_Block");

    ExportMaskSignal(mainBlockCtxSignal_, notifySignal_ + "_CtxSync_Main_Block");
    tailBlockCtxSignal_ = ImportMaskSignal(notifySignal_ + "_CtxSync_Reduce_Tail_Block");
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] InitResource end");
}

void CcuContextAllReduceMesh1DOneShot::LoadArgs()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] LoadArgs start");
    Load(input_[rankId_]);
    Load(output_);
    Load(token_[rankId_]);
    Load(tailBlockOffSet_);
    Load(groupOpSize_);
    Load(tailBlockGroupOpSize_);

    LocalCtxPostVar(input_[rankId_], reduceInput_, tailBlockCtxSignal_, 0);  // 如果翻译成syncXn，需要有效的信号
    LocalCtxPostVar(output_, reduceOutput_, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(token_[rankId_], reducetoken_, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockOffSet_, reduceInputOffSet_, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockOffSet_, reduceOutputOffSet_, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockGroupOpSize_.addrOffset, reduceGroupOpSize_.addrOffset, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockGroupOpSize_.loopParam, reduceGroupOpSize_.loopParam, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockGroupOpSize_.parallelParam, reduceGroupOpSize_.parallelParam, tailBlockCtxSignal_, 0);
    LocalCtxPostVar(tailBlockGroupOpSize_.residual, reduceGroupOpSize_.residual, tailBlockCtxSignal_, 0);
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] LoadArgs end");
}

void CcuContextAllReduceMesh1DOneShot::Presync()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] Presync start");
    uint16_t selfBit = 1 << rankId_;
    uint16_t allBit  = ((1 << rankSize_) - 1) & (~(1 << rankId_));
    for (auto t : transports) {
        WriteVariableWithSignal(*t, input_[rankId_], INPUT_XN_ID, CKE_IDX_1, selfBit);
        WriteVariableWithSignal(*t, token_[rankId_], TOKEN_XN_ID, CKE_IDX_2, selfBit);
    }

    GroupWait(*transportGroup, CKE_IDX_1, allBit);
    GroupWait(*transportGroup, CKE_IDX_2, allBit);
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] Presync end");
}

void CcuContextAllReduceMesh1DOneShot::Postsync()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] Postsync start");
    uint16_t selfBit = 1 << rankId_;
    uint16_t allBit  = ((1 << rankSize_) - 1) & (~(1 << rankId_));
    for (auto t : transports) {
        RemotePost(*t, CKE_IDX_0, selfBit);
    }
    GroupWait(*transportGroup, CKE_IDX_0, allBit);
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] Postsync end");
}

void CcuContextAllReduceMesh1DOneShot::SyncTailBlock(uint32_t ctxSignalIndex)
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] SyncTailBlock start, ctxSignalIndex[%u]", ctxSignalIndex);
    LocalCtxPost(tailBlockCtxSignal_, 1 << (0 + ctxSignalIndex * MISSION_NUM_2));
    LocalWait(mainBlockCtxSignal_, 1 << (1 + ctxSignalIndex * MISSION_NUM_2));
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] SyncTailBlock end");
}

void CcuContextAllReduceMesh1DOneShot::DoGroupReduce()
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] DoGroupReduce start");
    // 初始化地址寄存器
    std::vector<CcuRep::Memory> reduceSrc;
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        reduceSrc.push_back(CreateMemory());
    }
    CcuRep::Memory reduceDst = CreateMemory();

    // 填充地址
    uint32_t dstId = 0;
    uint32_t curId = 0;
    // SRC
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        if (rankIdx != rankId_) {
            curId = dstId;
            dstId++;
        } else {
            curId = rankSize_ - 1;
        }
        reduceSrc[curId].addr = input_[rankIdx];
        reduceSrc[curId].token = token_[rankIdx];
    }

    // DST
    reduceDst.addr  = output_;
    reduceDst.token = token_[rankId_];

    // 执行 reduce 操作
    GroupReduce(transports, reduceDst, reduceSrc, groupOpSize_, dataType_, outputDataType_, reduceOp_);
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] DoGroupReduce end");
    return;
}

void CcuContextAllReduceMesh1DOneShot::CalcMissionOffset(
    uint64_t sliceSize, uint64_t missionId, uint64_t &missionSize, uint64_t &missionOffset) const
{
    /*
     128+64loop场景下，数据不足512K时全部由Mi-0的一次循环完成，超过512K时，512K整倍数部分由Mi-0完成，余下由Mi1完成
     768K-1024K区间内，Mi-0一次循环，Mi-1两次循环，出现拖尾；其余场景Mi-0的循环次数不少于Mi-1循环次数
     */
    constexpr uint64_t MS_SIZE = 4096;
    uint64_t loopSizeMi0 = MAIN_BLOCK_LOOP_NUM * MS_SIZE;  // Mi-0一次循环的搬运量，在这个范围内的都由Mi-0一次完成
    if (sliceSize <= loopSizeMi0) {
        // 全部由Mi-0完成
        missionSize = missionId == 0 ? sliceSize : 0;
        missionOffset = missionId == 0 ? 0 : sliceSize;
    } else {
        // 小于loopSizeMi0的由Mi-1完成
        uint64_t resSize = sliceSize % loopSizeMi0;
        missionSize = missionId == 0 ? (sliceSize - resSize) : resSize;
        missionOffset = missionId == 0 ? 0 : (sliceSize - resSize);
    }
    return;
}

std::vector<uint64_t> CcuContextAllReduceMesh1DOneShot::GeneArgs(const CcuTaskArg &arg)
{
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] GeneArgs start");
    const CcuTaskArgAllReduceMesh1DOneShot *taskArg    = dynamic_cast<const CcuTaskArgAllReduceMesh1DOneShot *>(&arg);
    if (taskArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuContextAllReduceMesh1DOneShot::taskArg ptr is null"));
    }
    uint64_t                                inputAddr  = taskArg->inputAddr_;
    uint64_t                                outputAddr = taskArg->outputAddr_;
    uint64_t                                tokenInfo  = taskArg->token_;
    uint64_t                                sliceSize  = taskArg->sliceSize_;

    uint64_t mainBlockSize;
    uint64_t tailBlockSize;
    uint64_t mainBlockOffset;
    uint64_t tailBlockOffset;

    CalcMissionOffset(sliceSize, 0, mainBlockSize, mainBlockOffset);
    CalcMissionOffset(sliceSize, 1, tailBlockSize, tailBlockOffset);

    moConfig.loopCount = MAIN_BLOCK_LOOP_NUM;
    auto mainBlockGoSize = CalGoSize(mainBlockSize);
    moConfig.loopCount = TAIL_BLOCK_LOOP_NUM;  // 需要模拟尾块的参数
    auto tailBlockGoSize = CalGoSize(tailBlockSize);
    moConfig.loopCount = MAIN_BLOCK_LOOP_NUM;  // 还原主块的参数

    HCCL_INFO(
        "[CcuContextAllReduceMesh1DOneShot] GeneArgs, taskArg are inputAddr[%llu], outputAddr[%llu], "
        "sliceSize[%llu],  mainBlockSize[%llu], tailBlockSize[%llu], mainBlockOffset[%llu], tailBlockOffset[%llu]",
        inputAddr, outputAddr, sliceSize, mainBlockSize, tailBlockSize, mainBlockOffset, tailBlockOffset);

    std::vector<uint64_t> taskArgList{inputAddr, outputAddr, tokenInfo, tailBlockOffset};

    for (auto goSize : {mainBlockGoSize, tailBlockGoSize}) {
        for (auto val : goSize) {
            taskArgList.push_back(val);
        }
    }
    HCCL_INFO("[CcuContextAllReduceMesh1DOneShot] GeneArgs end");
    return taskArgList;
}

}