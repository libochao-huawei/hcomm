/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllGatherMeshMem2Mem1D类实现
 * Author: hulinkang
 * Create: 2025-05-12
 */

#include "ccu_context_all_gather_mesh1d_mem2mem.h"
#include "ccu_instruction_all_gather_mesh1d_mem2mem.h"

namespace Hccl {

constexpr int OUTPUT_XN_ID = 1;
constexpr int TOKEN_XN_ID = 2;
constexpr int CKE_IDX_0 = 0;
constexpr int CKE_IDX_1 = 1;
constexpr int CKE_IDX_2 = 2;
constexpr int CKE_IDX_3 = 3;
constexpr uint64_t CCU_MS_SIZE = 4096;
constexpr uint64_t LOCAL_COPY_MS = 8;

CcuContextAllGatherMeshMem2Mem1D::CcuContextAllGatherMeshMem2Mem1D(const CcuCtxArg     &arg,
                                                     const std::vector<CcuTransport *> &transports,
                                                     const CcuTransportGroup           &group)
    : CcuContext(arg, transports, group)
{
    HCCL_INFO("[CcuContextAllGatherMeshMem2Mem1D] Enter Constructor.");
    const CcuCtxArgAllGatherMeshMem2Mem1D *ctxArg = dynamic_cast<const CcuCtxArgAllGatherMeshMem2Mem1D *>(&arg);
    if (ctxArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuContextAllGatherMeshMem2Mem1D::ctxArg ptr is null"));
    }
    rankId_ = ctxArg->rankId_;
    if (ctxArg->dimSize_.size() > 0) {
        rankSize_ = ctxArg->dimSize_[0];
    }

    input_.push_back(CreateVariable());
    uint16_t transportIdx = 0;
    // 按照rank号从小到大遍历transports，遇到本rank就填充本地资源，否则依次取远端资源，要求给框架返回的Link同样是按顺序排列的
    for (uint64_t peerId = 0; peerId < rankSize_; peerId++) {
        if (peerId == rankId_) {
            output_.push_back(CreateVariable());
            token_.push_back(CreateVariable());
        } else {
            HCCL_INFO("[CcuContextAllGatherMeshMem2Mem1D] MyRank[%u], PeerId[%llu], TransportId[%u]",
                rankId_, peerId, transportIdx);
            CHK_PRT_RET(transports[transportIdx] == nullptr,
                HCCL_ERROR("[CcuContextAllGatherMeshMem2Mem1D] Algorithm transport ptr is null"),);
            output_.push_back(CreateVariable((*transports[transportIdx]), OUTPUT_XN_ID));  // 获取transport中id=1的Var来传递output
            token_.push_back(CreateVariable((*transports[transportIdx]), TOKEN_XN_ID));
            transportIdx++;
        }
    }
    offSet_ = CreateVariable();
    sliceSize_ = CreateVariable();
    localGoSize_ = CreateGroupOpSize();
    moConfig.loopCount = 8; // loop展开8次、16次
    moConfig.msInterleave = LOCAL_COPY_MS; // 一个loop 8个MS
    moConfig.memSlice = LOCAL_COPY_MS * CCU_MS_SIZE; // 32k
    if (moRes.executor.size() == 0) {
        moRes.executor = CreateBlockExecutor(moConfig.loopCount);
        moRes.maskSignal = CreateBlockMaskSignal(moConfig.loopCount);
        moRes.ccuBuffer = CreateBlockCcuBuffer(moConfig.loopCount * moConfig.msInterleave);
    }
}

void CcuContextAllGatherMeshMem2Mem1D::CreateLocalCopyLoop()
{
    std::string loopType = "allgather";
    if (registeredLoop.find(loopType) != registeredLoop.end()) {
        return;
    }

    for (uint32_t index = 0; index < 2; index++) { // 需要2个Loop
        CcuRep::Memory              src = CreateMemory();
        CcuRep::Memory              dst = CreateMemory();
        CcuRep::Variable            len = CreateVariable();
        CcuRep::LoopBlock           lb(this, loopType + "_localcopy_loop_" + std::to_string(index));
        lb(src, dst, len);

        CcuRep::MaskSignal sem = moRes.maskSignal[index];
        std::vector<CcuRep::CcuBuffer> bufs;
        for (uint32_t i = 0; i < LOCAL_COPY_MS; i++) {
            bufs.push_back(moRes.ccuBuffer[i]);
        }

        LocalCopy(bufs[0], src, len, sem);
        LocalWait(sem);
        LocalCopy(dst, bufs[0], len, sem);
        LocalWait(sem);
    }
    registeredLoop.insert(loopType);
    return;
}

void CcuContextAllGatherMeshMem2Mem1D::LocalCopyByLoopGroup(CcuRep::Memory dst, CcuRep::Memory src)
{
    CreateLocalCopyLoop();

    CCU_IF(localGoSize_.addrOffset != 0)
    {
        CcuRep::Variable loopParam = CreateVariable();
        loopParam = CcuRep::GetLoopParam(0, moConfig.memSlice * moConfig.loopCount, 0);
        loopParam += localGoSize_.loopParam;

        CcuRep::Variable sliceSize = CreateVariable();
        sliceSize = moConfig.memSlice;
        auto lc   = Loop("allgather_localcopy_loop_0")(src, dst, sliceSize);

        CcuRep::Variable paraCfg = CreateVariable();
        paraCfg = CcuRep::GetParallelParam(moConfig.loopCount - 1, 0, 1);
        CcuRep::Variable offsetCfg = CreateVariable();
        offsetCfg = CcuRep::GetOffsetParam(moConfig.memSlice, moConfig.msInterleave, 1);
        LoopGroup({lc}, {loopParam}, paraCfg, offsetCfg);
    }

    CCU_IF(localGoSize_.parallelParam != 0)
    {
        CcuRep::Condition cond(this, localGoSize_.parallelParam != 0);

        src.addr += localGoSize_.addrOffset;
        dst.addr += localGoSize_.addrOffset;
        auto lc0 = Loop("allgather_localcopy_loop_0")(src, dst, localGoSize_.residual);

        src.addr += localGoSize_.residual;
        dst.addr += localGoSize_.residual;
        CcuRep::Variable sliceSize = CreateVariable();
        sliceSize = moConfig.memSlice;
        auto lc1  = Loop("allgather_localcopy_loop_1")(src, dst, sliceSize);

        CcuRep::Variable loopCfg0 = CreateVariable();
        loopCfg0 = CcuRep::GetLoopParam(0, 0, 1);
        CcuRep::Variable loopCfg1 = CreateVariable();
        loopCfg1 = CcuRep::GetLoopParam(0, 0, 1);
        CcuRep::Variable offsetCfg = CreateVariable();
        offsetCfg = CcuRep::GetOffsetParam(moConfig.memSlice, moConfig.msInterleave, 1);
        LoopGroup({lc0, lc1}, {loopCfg0, loopCfg1}, localGoSize_.parallelParam, offsetCfg);
    }
}

void CcuContextAllGatherMeshMem2Mem1D::Algorithm()
{
    HCCL_INFO("[CcuContextAllGatherMeshMem2Mem1D] AllgatherMesh1D run.");
    uint16_t selfBit = 1 << rankId_;
    uint16_t allBit  = ((1 << rankSize_) - 1) & (~(1 << rankId_));

    Load(input_[0]);
    Load(output_[rankId_]);
    Load(token_[rankId_]);
    Load(offSet_);
    Load(sliceSize_);
    Load(localGoSize_);

    for (auto t : transports) {
        WriteVariableWithSignal(*t, output_[rankId_], OUTPUT_XN_ID, CKE_IDX_1, selfBit); // index = 1，传递output信息
        WriteVariableWithSignal(*t, token_[rankId_], TOKEN_XN_ID, CKE_IDX_2, selfBit);  // index = 2，传递token信息
    }
    GroupWait(*transportGroup, CKE_IDX_1, allBit); // index = 1，传递output信息
    GroupWait(*transportGroup, CKE_IDX_2, allBit); // index = 2，传递token信息

    CcuRep::Memory              src = CreateMemory();
    std::vector<CcuRep::Memory> dst;
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        dst.push_back(CreateMemory());
    }

    u32 transportId = 0;
    CcuRep::MaskSignal locMask = CreateMaskSignal();
    src.addr  = input_[0];
    src.token = token_[rankId_];
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        dst[rankIdx].addr = output_[rankIdx];
        dst[rankIdx].addr += offSet_;
        dst[rankIdx].token = token_[rankIdx];
        if (rankIdx == rankId_) {
            LocalPost(locMask, 1 << rankIdx);
        } else {
            Write(*transports[transportId], dst[rankIdx], src, sliceSize_, locMask, 1 << rankIdx);
            transportId++;
        }
    }
    LocalCopyByLoopGroup(dst[rankId_], src);
    HCCL_INFO("LocalCopyByLoopGroup");
    LocalWait(locMask, (1 << rankSize_) - 1);

    for (auto t : transports) {
        RemotePost(*t, CKE_IDX_0, selfBit);
    }
    GroupWait(*transportGroup, CKE_IDX_0, allBit);
    HCCL_INFO("[CcuContextAllGatherMeshMem2Mem1D] AllgatherMesh1D end.");
    return;
}

std::vector<uint64_t> CcuContextAllGatherMeshMem2Mem1D::GeneArgs(const CcuTaskArg &arg)
{
    const CcuTaskArgAllGatherMeshMem2Mem1D *taskArg = dynamic_cast<const CcuTaskArgAllGatherMeshMem2Mem1D *>(&arg);
    if (taskArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuContextAllGatherMeshMem2Mem1D::taskArg ptr is null"));
    }
    uint64_t inputAddr  = taskArg->inputAddr_;
    uint64_t outputAddr = taskArg->outputAddr_;
    uint64_t tokenInfo  = taskArg->token_;
    uint64_t offset     = taskArg->offSet_;
    uint64_t sliceSize  = taskArg->sliceSize_;
    auto     localGoSize = CalGoSize(sliceSize);

    return {inputAddr,      outputAddr,     tokenInfo,      offset,        sliceSize,
            localGoSize[0], localGoSize[1], localGoSize[2], localGoSize[3]};
}
}
