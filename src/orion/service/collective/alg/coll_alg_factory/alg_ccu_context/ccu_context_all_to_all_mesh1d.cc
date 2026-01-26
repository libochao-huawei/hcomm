/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: CcuContextAllToAllMesh1D类实现
 * Author: chenchao
 * Create: 2025-02-19
 */

#include "ccu_context_all_to_all_mesh1d.h"
#include "ccu_instruction_all_to_all_mesh1d.h"

namespace Hccl {

constexpr int CKE_IDX_0 = 0;
constexpr int CKE_IDX_1 = 1;
constexpr int CKE_IDX_2 = 2;

constexpr uint64_t CCU_MS_SIZE   = 4096;
constexpr uint64_t LOCAL_COPY_MS = 8;

CcuContextAllToAllMesh1D::CcuContextAllToAllMesh1D(const CcuCtxArg &arg, const std::vector<CcuTransport*> &transports,
                                                     const CcuTransportGroup &group)
    : CcuContext(arg, transports, group)
{
    const CcuCtxArgAllToAllMesh1D *ctxArg = dynamic_cast<const CcuCtxArgAllToAllMesh1D *>(&arg);
    if (ctxArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuContextAllToAllMesh1D::ctxArg ptr is null"));
    }
    rankId_ = ctxArg->rankId;
    if (ctxArg->dimSize.size() > 0) {
        rankSize_ = ctxArg->dimSize[0];
    }
    if (transports.size() == 0) {
        THROW<NullPtrException>(StringFormat("CcuContextAllToAllMesh1D transports is empty"));
    }
    loadFromMem_ = ctxArg->loadFromMem;
}

void CcuContextAllToAllMesh1D::CreateLocalCopyLoop()
{
    std::string loopType = "all_to_all";
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

void CcuContextAllToAllMesh1D::LocalCopyByLoopGroup(CcuRep::Memory dst, CcuRep::Memory src)
{
    CreateLocalCopyLoop();
 
    CCU_IF(groupOpSize_.addrOffset != 0)
    {
        CcuRep::Variable loopParam = CreateVariable();
        loopParam = CcuRep::GetLoopParam(0, moConfig.memSlice * moConfig.loopCount, 0);
        loopParam += groupOpSize_.loopParam;
 
        CcuRep::Variable sliceSize = CreateVariable();
        sliceSize = moConfig.memSlice;
        auto lc   = Loop("all_to_all_localcopy_loop_0")(src, dst, sliceSize);
 
        CcuRep::Variable paraCfg = CreateVariable();
        paraCfg = CcuRep::GetParallelParam(moConfig.loopCount - 1, 0, 1);
        CcuRep::Variable offsetCfg = CreateVariable();
        offsetCfg = CcuRep::GetOffsetParam(moConfig.memSlice, moConfig.msInterleave, 1);
        LoopGroup({lc}, {loopParam}, paraCfg, offsetCfg);
    }
 
    CCU_IF(groupOpSize_.parallelParam != 0)
    {
        CcuRep::Condition cond(this, groupOpSize_.parallelParam != 0);
 
        src.addr += groupOpSize_.addrOffset;
        dst.addr += groupOpSize_.addrOffset;
        auto lc0 = Loop("all_to_all_localcopy_loop_0")(src, dst, groupOpSize_.residual);
 
        src.addr += groupOpSize_.residual;
        dst.addr += groupOpSize_.residual;
        CcuRep::Variable sliceSize = CreateVariable();
        sliceSize = moConfig.memSlice;
        auto lc1  = Loop("all_to_all_localcopy_loop_1")(src, dst, sliceSize);
 
        CcuRep::Variable loopCfg0 = CreateVariable();
        loopCfg0 = CcuRep::GetLoopParam(0, 0, 1);
        CcuRep::Variable loopCfg1 = CreateVariable();
        loopCfg1 = CcuRep::GetLoopParam(0, 0, 1);
        CcuRep::Variable offsetCfg = CreateVariable();
        offsetCfg = CcuRep::GetOffsetParam(moConfig.memSlice, moConfig.msInterleave, 1);
        LoopGroup({lc0, lc1}, {loopCfg0, loopCfg1}, groupOpSize_.parallelParam, offsetCfg);
    }
}

void CcuContextAllToAllMesh1D::Algorithm()
{
    HCCL_INFO("[ccuAllToAllMesh1D_context] AllToAllMesh1D run.");
    // 创建Variable，用于交换地址及token
    u32 transportId = 0;
    for (u64 id = 0; id < rankSize_; id++) {
        if (id == rankId_) {
            input_.push_back(CreateVariable());
            output_.push_back(CreateVariable());
            token_.push_back(CreateVariable());
        }
        else { // 非本地，使用远端Variable
            CHK_PRT_RET(transports[transportId] == nullptr,
                HCCL_ERROR("[CcuContextAllToAllMesh1D] Algorithm transport ptr is null"),);
            input_.push_back(CreateVariable((*transports[transportId]), CKE_IDX_0));
            output_.push_back(CreateVariable((*transports[transportId]), CKE_IDX_1));
            token_.push_back(CreateVariable((*transports[transportId]), CKE_IDX_2));
            transportId++;
        }
    }
    sliceSize_   = CreateVariable();
    srcStride_   = CreateVariable();
    srcOffset_   = CreateVariable();
    dstOffset_   = CreateVariable();
    groupOpSize_ = CreateGroupOpSize();

    moConfig.loopCount = 8; // loop展开8次、16次
    moConfig.msInterleave = LOCAL_COPY_MS; // 一个loop 8个MS
    moConfig.memSlice = LOCAL_COPY_MS * CCU_MS_SIZE; // 32k
    if (moRes.executor.size() == 0) {
        moRes.executor = CreateBlockExecutor(moConfig.loopCount);
        moRes.maskSignal = CreateBlockMaskSignal(moConfig.loopCount);
        moRes.ccuBuffer = CreateBlockCcuBuffer(moConfig.loopCount * moConfig.msInterleave);
    }

    // 从SQE load args，本rank需要的input、output地址等信息
    // inputAddr, outputAddr, tokenInfo, srcStride, srcOffset, dstOffset, groupOpSize
    Load(input_[rankId_]);
    Load(output_[rankId_]);
    Load(token_[rankId_]);
    Load(sliceSize_);  // 本轮传输的分片大小
    Load(srcStride_); // 单片数据大小
    Load(srcOffset_);
    Load(dstOffset_);
    Load(groupOpSize_);

    // 前同步。交换信息，将本Rank load的in\out等地址信息写到所有对端的对应Variable中，并同步
    uint16_t selfBit = 1 << rankId_;  // 本rank的mask
    uint16_t allBit  = ((1 << rankSize_) - 1) & (~(1 << rankId_));

    srcOffset_ += input_[rankId_];

    for (auto t : transports) {
        // （transport, param, paramID, SemID, mask）
        WriteVariableWithSignal(*t, output_[rankId_], CKE_IDX_1, CKE_IDX_1, selfBit); // index = 1，传递output信息
        WriteVariableWithSignal(*t, token_[rankId_], CKE_IDX_2, CKE_IDX_2, selfBit);  // index = 2，传递token信息
    }

    GroupWait(*transportGroup, CKE_IDX_1, allBit); // index = 1，传递output信息
    GroupWait(*transportGroup, CKE_IDX_2, allBit); // index = 2，传递token信息

    // 创建GSA， src为本地的各片HBM地址GSA列表，dst为所有对端的HBM地址GSA列表
    std::vector<CcuRep::Memory> src;
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        src.push_back(CreateMemory());
    }
    std::vector<CcuRep::Memory> dst;
    for (uint64_t rankIdx = 0; rankIdx < rankSize_; rankIdx++) {
        dst.push_back(CreateMemory());
    }

    // 考虑stride信息
    for (uint64_t r = 0; r < rankSize_; r++) {
        src[r].token = token_[r];
        dst[r].token = token_[r];

        // src[r] = srcOffset + r*srcStride
        src[r].addr = srcOffset_;
        for (uint64_t i = 0; i < r; i++) {
            src[r].addr += srcStride_;
        }
        // dst[r] = recvBuf[r] + dstOffset
        dst[r].addr = output_[r];
        dst[r].addr += dstOffset_;
    }

    // 创建CKE，源端保序
    CcuRep::MaskSignal locMask = CreateMaskSignal();
    //  all2all 数据搬运
    transportId = 0;
    if (loadFromMem_) {
        for(uint64_t r = 0; r < rankSize_; r++) {
            if (r == rankId_) {
                LocalCopy(dst[r], src[r], sliceSize_, locMask, 1 << r);
            }
            else {
                Write(*transports[transportId], dst[r], src[r], sliceSize_, locMask, 1 << r);
                transportId++;
            }
        }
        LocalWait(locMask, ((1 << rankSize_) - 1));
    } else {
        for(uint64_t r = 0; r < rankSize_; r++) {
            if (r != rankId_) {
                Write(*transports[transportId], dst[r], src[r], sliceSize_, locMask, 1 << r);
                transportId++;
            }
        }
        LocalCopyByLoopGroup(dst[rankId_], src[rankId_]);
        LocalWait(locMask, allBit);
    }

    //  后同步
    for (auto t : transports) {
        if (t == nullptr) {
            THROW<NullPtrException>(StringFormat("CcuContextAllToAllMesh1D::Algorithm transport ptr is null"));
        }
        RemotePost(*t, CKE_IDX_0, selfBit);
    }
    GroupWait(*transportGroup, CKE_IDX_0, allBit);
    HCCL_INFO("[AllToAllAlgo] AllToAllMesh1D end");

    return;
}

std::vector<uint64_t> CcuContextAllToAllMesh1D::GeneArgs(const CcuTaskArg &arg)
{
    const CcuTaskArgAllToAllMesh1D *taskArg = dynamic_cast<const CcuTaskArgAllToAllMesh1D *>(&arg);
    if (taskArg == nullptr) {
        THROW<NullPtrException>(StringFormat("CcuContextAllToAllMesh1D::taskArg ptr is null"));
    }
    uint64_t inputAddr  = taskArg->inputAddr;
    uint64_t outputAddr = taskArg->outputAddr;
    uint64_t tokenInfo  = taskArg->token;

    uint64_t srcStride = taskArg->srcStride;
    uint64_t srcOffset = taskArg->srcOffset;
    uint64_t dstOffset = taskArg->dstOffset;

    uint64_t sliceSize = taskArg->sliceSize;
    auto     goSize    = CalGoSize(sliceSize);
    HCCL_INFO("[AllToAllAlgo] inputAddr[%llu], outputAddr[%llu], sliceSize[%llu], srcStride[%llu], srcOffset[%llu], dstOffset[%llu].",
        inputAddr, outputAddr, sliceSize, srcStride, srcOffset, dstOffset);

    return {inputAddr, outputAddr, tokenInfo, sliceSize, srcStride, srcOffset, dstOffset, goSize[0], goSize[1], goSize[2], goSize[3]};
}

}
