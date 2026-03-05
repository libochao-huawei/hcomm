/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "sqe_build_a5.h"
#include "sqe_v82.h"
#include "log.h"
#include "communicator_impl_lite_manager.h"

namespace Hccl {

constexpr u32 LOW_BITS = 16;

u32 GetKernelExecTimeoutFromEnvConfig()
{
    const u32 envTimeout  = CommunicatorImplLiteMgr::GetInstance().GetEnvConfig().hcclExecTimeout;
    return envTimeout;
}

void BuildA5SqeNotifyWait(u32 streamId, u32 taskId, u32 notifyId, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;

    sqe->kernelCredit      = RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT;
    sqe->header.type       = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_WAIT);
    sqe->cntFlag           = false;
    sqe->clrFlag           = true;
    sqe->subType           = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_SINGLE_NOTIFY_WAIT);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId     = static_cast<uint16_t>(taskId >> LOW_BITS);
    sqe->header.wrCqe      = 1U;
    sqe->notifyId          = notifyId;
    sqe->timeout           = GetKernelExecTimeoutFromEnvConfig();

    HCCL_INFO("[SQE]NotifyWait: notifyId=%lu, timeout=%us, streamId=%u, taskId=%u", notifyId, sqe->timeout, streamId, taskId);
}

void BuildA5SqeNotifyRecord(u32 streamId, u32 taskId, u32 notifyId, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;
    sqe->header.type       = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_RECORD);
    sqe->kernelCredit      = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->subType           = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_SINGLE_NOTIFY_RECORD);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId     = static_cast<uint16_t>(taskId >> LOW_BITS);
    sqe->header.wrCqe      = 1U;
    sqe->notifyId          = notifyId;

    HCCL_INFO("[SQE]NotifyRecord: notifyId=%lu, streamId=%u, taskId=%u", notifyId, streamId, taskId);
}

void BuildA5SqeCnt1toNNotifyRecord(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_RECORD);
    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->clrFlag = false;
    sqe->cntFlag = true;
    sqe->recordModeBit = 0x0U; //rtCntNotifyRecordMode_t::RECORD_STORE_MODE
    sqe->subType = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_COUNT_NOTIFY_RECORD);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);
    sqe->header.wrCqe  = 1U;
    sqe->notifyId = notifyId;
    sqe->cntValue = cntValue;
}

void BuildA5SqeCnt1toNNotifyWait(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;
    sqe->kernelCredit = RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_WAIT);
    sqe->cntFlag = true;
    sqe->clrFlag = true;
    sqe->bitmap = 1U;
    sqe->subType = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_COUNT_NOTIFY_WAIT);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);    
    sqe->header.wrCqe  = 1U;
    sqe->notifyId = notifyId;
    sqe->cntValue = cntValue;
}

void BuildA5SqeCntNto1NotifyRecord(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_RECORD);
    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->clrFlag = false;
    sqe->cntFlag = true;
    sqe->recordModeBit = 0x2U; // rtCntNotifyRecordMode_t::RECORD_WRITE_BIT_MODE
    sqe->subType = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_COUNT_NOTIFY_RECORD);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);   
    sqe->header.wrCqe  = 1U;
    sqe->notifyId = notifyId; 
    sqe->cntValue = cntValue;
}

void BuildA5SqeCntNto1NotifyWait(u32 streamId, u32 taskId, u32 notifyId, u32 cntValue, uint8_t * const sqeIn)
{
    (void) streamId;
    Rt91095StarsNotifySqe *sqe = (Rt91095StarsNotifySqe *)sqeIn;
    sqe->kernelCredit = RT_STARS_NEVER_TIMEOUT_KERNEL_CREDIT;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_NOTIFY_WAIT);
    sqe->cntFlag = true;
    sqe->clrFlag = true;
    sqe->waitModeBit = 0x1U; // rtCntNotifyWaitMode_t::WAIT_EQUAL_MODE
    sqe->subType = static_cast<uint16_t>(Rt91095NotifySubType::NOTIFY_SUB_TYPE_COUNT_NOTIFY_WAIT);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);
    sqe->header.wrCqe  = 1U;
    sqe->notifyId = notifyId;
    sqe->cntValue = cntValue;
}

void SetSqeHeaderTaskFields(void* sqe, u32 taskId) 
{
    auto header = reinterpret_cast<Rt91095StarsSqeHeader*>(sqe);
    header->rtStreamId     = static_cast<uint16_t>(taskId);
    header->taskId         = static_cast<uint16_t>(taskId >> 16);
}

void BuildA5SqeSdmaCopy(u32 streamId, u32 taskId, u64 dstAddr, u64 srcAddr, u32 size, u32 partId, u32 opcode,
                        uint8_t * const sqeIn)
{
    Rt91095StarsMemcpySqe *sqe = (Rt91095StarsMemcpySqe *)sqeIn;
    SetSqeHeaderTaskFields(sqe, taskId);
    sqe->header.type           = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_SDMA);
    sqe->opcode                = opcode; // opcode为非0，代表 SDMA Reduce Copy; 0代表SDMA Copy
    sqe->kernelCredit          = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->sssv                  = 1U;
    sqe->dssv                  = 1U;
    sqe->sns                   = 1U;
    sqe->dns                   = 1U;
    sqe->mapamPartId           = partId; // 这里走的memcpy，如果走withcfg,需要传入qoscfg
    sqe->header.wrCqe          = 1U;

    sqe->u.strideMode0.lengthMove  = size;
    sqe->u.strideMode0.srcAddrLow  = static_cast<uint32_t>(srcAddr & 0x00000000ffffffffU);
    sqe->u.strideMode0.srcAddrHigh = static_cast<uint32_t>((srcAddr & 0xffffffff00000000U) >> 32); // 高 32bit
    sqe->u.strideMode0.dstAddrLow  = static_cast<uint32_t>(dstAddr & 0x00000000ffffffffU);
    sqe->u.strideMode0.dstAddrHigh = static_cast<uint32_t>((dstAddr & 0xffffffff00000000U) >> 32); // 高 32bit

    HCCL_INFO("[SQE]Memcpy: size=%u, srcAddr=0x%llx, dstAddr=0x%llx, partId=%u, opcode=%u, streamId=%u, taskId=%u",
               size, srcAddr, dstAddr, partId, opcode, streamId, taskId);
}

void BuildA5SqeUbDbSend(u32 streamId, u32 taskId, const UbJettyLiteId &jettyLiteId, u16 piValue, uint8_t * const sqeIn)
{
    (void)streamId;
    Rt91095StarsUbdmaDBmodeSqe *sqe = (Rt91095StarsUbdmaDBmodeSqe *)sqeIn;
    SetSqeHeaderTaskFields(sqe, taskId);
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_UBDMA);

    sqe->mode              = Rt91095UbDmaSqeMode::RT_91095_SQE_DOORBELL_MODE;
    sqe->kernelCredit      = RT_STARS_DEFAULT_KERNEL_CREDIT;

    sqe->doorbellNum = 1U;
    sqe->jettyId1    = jettyLiteId.GetJettyId();
    sqe->funcId1     = jettyLiteId.GetFuncId();
    sqe->piValue1    = piValue;
    sqe->dieId1      = jettyLiteId.GetDieId();
    HCCL_INFO("[SQE]UbDmaSend: dieId=%u, funcId=%u, jettyid=%u, piValue=%u, streamId=%u, taskId=%u",
              jettyLiteId.GetDieId(), jettyLiteId.GetFuncId(), jettyLiteId.GetJettyId(), piValue, streamId, taskId);
}

namespace 
{
void ConstructLHWI(const Rt91095StarsCondIsaRegister_t dstReg, const u64 immd, Rt91095StarsCondOpLHWI_t &opLHWI)
{
    opLHWI.opCode = static_cast<uint32_t>(Hccl::Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_LWI);
    opLHWI.func3 = static_cast<uint32_t>(Rt91095StarsCondIsaLwiFunc3_t::RT_91095_STARS_COND_ISA_LWI_FUNC3_LHWI);
    opLHWI.rd = static_cast<uint32_t>(dstReg);
    opLHWI.immd = static_cast<uint32_t>((immd >> 49U) & 0x7FFFU);  // High15-immd[63:49]
}

void ConstructLLWI(const Rt91095StarsCondIsaRegister_t dstReg, const u64 immd, Rt91095StarsCondOpLLWI_t &opLLWI)
{
    opLLWI.opCode = static_cast<uint32_t>(Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_LWI);
    opLLWI.func3 = static_cast<uint32_t>(Rt91095StarsCondIsaLwiFunc3_t::RT_91095_STARS_COND_ISA_LWI_FUNC3_LLWI);
    opLLWI.rd = static_cast<uint32_t>(dstReg);
    opLLWI.immdHigh = static_cast<uint32_t>((immd >> 32U) & 0x1FFFFU);  // Low49-immd[48:32]
    opLLWI.immdLow = static_cast<uint32_t>(immd & 0xFFFFFFFFU);         // Low49-immd[31:0]
}

void ConstructLoadImm(const Rt91095StarsCondIsaRegister_t dstReg, const u64 addr,
                      const Rt91095StarsCondIsaLoadImmFunc3_t func3, Rt91095StarsCondOpLoadImm_t &loadImm)
{
    loadImm.opCode = static_cast<uint32_t>(Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_LOAD_IMM);
    loadImm.rd = static_cast<uint32_t>(dstReg);
    loadImm.func3 = static_cast<uint32_t>(func3);
    loadImm.immdAddrHigh = static_cast<uint32_t>((addr >> 32U) & 0X1FFFFU); // bit[48:32]
    loadImm.immdAddrLow = static_cast<uint32_t>(addr & 0xFFFFFFFFU); // bit[31:0]
}

void ConstructBranch(const Rt91095StarsCondIsaRegister_t rs1Reg, const Rt91095StarsCondIsaRegister_t rs2Reg,
                     const Rt91095StarsCondIsaBranchFunc3_t func3, const uint8_t instrOffset,
                     Rt91095StarsCondOpBranch_t &opBranch)
{
    opBranch.opCode = static_cast<uint32_t>(Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_BRANCH);
    opBranch.func3 = static_cast<uint32_t>(func3);
    opBranch.rs1 = static_cast<uint32_t>(rs1Reg);
    opBranch.rs2 = static_cast<uint32_t>(rs2Reg);
    opBranch.jumpInstrOffset = instrOffset & 0xFU;  // Jump-immd[3:0]
}

void ConstructStore(const Rt91095StarsCondIsaRegister_t addrReg, const Rt91095StarsCondIsaRegister_t valReg,
                    const uint16_t immdOffset, const Rt91095StarsCondIsaStoreFunc3_t func3, Rt91095StarsCondOpStore_t &opStore)
{
    opStore.opCode = static_cast<uint32_t>(Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_STORE);
    opStore.immdLow = static_cast<uint8_t>(immdOffset & 0x1FU);  // S-immd[4:0]
    opStore.func3 = static_cast<uint32_t>(func3);
    opStore.rs1 = static_cast<uint32_t>(addrReg);
    opStore.rs2 = static_cast<uint32_t>(valReg);
    opStore.immdHigh = static_cast<uint8_t>((immdOffset & 0xFE0U) >> 5U);  // S-immd[11:5]
}

void ConstructNop(Rt91095StarsCondOpNop_t &nop)
{
    nop.opCode = static_cast<uint32_t>(Rt91095StarsCondIsaOpCode_t::RT_91095_STARS_COND_ISA_OP_CODE_NOP);
    nop.rd = static_cast<uint32_t>(Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R0);
    nop.func3 = static_cast<uint32_t>(Rt91095StarsCondIsaOpImmFunc3_t::RT_91095_STARS_COND_ISA_OP_IMM_FUNC3_NOP);
    nop.rs1 = static_cast<uint32_t>(Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R0);
    nop.immd = 0U;
}
} 

void BuildA5SqeCCoreNotifyWait(u32 streamId, u32 taskId, u64 waitAddr, u64 actAddr, bool last, uint8_t * const sqeIn)
{
    Rt91095StarsCCoreSqeNotifyWait* sqe = (Rt91095StarsCCoreSqeNotifyWait *)sqeIn;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_COND);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);

    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->csc = 1U;

    constexpr Rt91095StarsCondIsaRegister_t r0 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R0;
    constexpr Rt91095StarsCondIsaRegister_t r1 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R1;
    constexpr Rt91095StarsCondIsaRegister_t r2 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R2;
    constexpr Rt91095StarsCondIsaRegister_t r3 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R3;

    // load current Turn to r3
    ConstructLoadImm(r3, actAddr, Rt91095StarsCondIsaLoadImmFunc3_t::RT_91095_STARS_COND_ISA_LOAD_IMM_FUNC3_LHU, sqe->ldrImm1);

    // load sendcnt to r2
    ConstructLoadImm(r2, waitAddr, Rt91095StarsCondIsaLoadImmFunc3_t::RT_91095_STARS_COND_ISA_LOAD_IMM_FUNC3_LHU, sqe->ldrImm2);
    uint8_t loadInstrOff = (offsetof(Rt91095StarsCCoreSqeNotifyWait, ldrImm2) -
        offsetof(Rt91095StarsCCoreSqeNotifyWait, ldrImm1));
    loadInstrOff = loadInstrOff / sizeof(uint32_t);

    // r2(sendCnt) < r3(curTurn)，goto reload r2
    ConstructBranch(r2, r3, Rt91095StarsCondIsaBranchFunc3_t::RT_91095_STARS_COND_ISA_BRANCH_FUNC3_BLTU, loadInstrOff, sqe->beq);

    if (last) {
        // load sendcount addr to r1
        ConstructLLWI(r1, waitAddr, sqe->clear.llwi1);
        ConstructLHWI(r1, waitAddr, sqe->clear.lhwi1);
        // the last turn clear sendCnt, r0(0) value store to r1(sendCnt),
        ConstructStore(r1, r0, 0U, Rt91095StarsCondIsaStoreFunc3_t::RT_91095_STARS_COND_ISA_STORE_FUNC3_SH, sqe->clear.sw);
        for (Rt91095StarsCondOpNop_t &nop : sqe->clear.nop) {
            ConstructNop(nop);
        }
    } else {
        for (Rt91095StarsCondOpNop_t &nop : sqe->nop) {
            ConstructNop(nop);
        }
    }

    HCCL_INFO("[SQE]CCoreWait: waitAddr=%llu, actAddr=%llu, last=%u, streamId=%u, taskId=%u, "
        "ISA=%08x %08x %08x %08x %08x %08x %08x",
        waitAddr, actAddr, last, streamId, taskId,
        sqe->ldrImm1, sqe->ldrImm2, sqe->beq, sqe->clear.llwi1, sqe->clear.lhwi1, sqe->clear.sw, sqe->clear.nop[0]);
}

void BuildA5SqeCCoreNotifyRecord(u32 streamId, u32 taskId, u64 writeAddr, u64 valueAddr, uint8_t * const sqeIn)
{
    Rt91095StarsCCoreSqeNotifyRecord* sqe = (Rt91095StarsCCoreSqeNotifyRecord *)sqeIn;
    sqe->header.type = static_cast<uint8_t>(Rt91095StarsSqeType::RT_91095_SQE_TYPE_COND);
    sqe->header.rtStreamId = static_cast<uint16_t>(taskId);
    sqe->header.taskId = static_cast<uint16_t>(taskId >> LOW_BITS);

    sqe->kernelCredit = RT_STARS_DEFAULT_KERNEL_CREDIT;
    sqe->csc = 1U;

    constexpr Rt91095StarsCondIsaRegister_t r1 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R1;
    constexpr Rt91095StarsCondIsaRegister_t r2 = Rt91095StarsCondIsaRegister_t::RT_91095_STARS_COND_ISA_REGISTER_R2;

    ConstructLoadImm(r1, valueAddr, Rt91095StarsCondIsaLoadImmFunc3_t::RT_91095_STARS_COND_ISA_LOAD_IMM_FUNC3_LHU, sqe->ldrImm);
    ConstructLLWI(r2, writeAddr, sqe->llwi1);
    ConstructLHWI(r2, writeAddr, sqe->lhwi1);

    ConstructStore(r2, r1, 0U, Rt91095StarsCondIsaStoreFunc3_t::RT_91095_STARS_COND_ISA_STORE_FUNC3_SH, sqe->sw);
    for (Rt91095StarsCondOpNop_t &nop : sqe->nop) {
        ConstructNop(nop);
    }

    HCCL_INFO("[SQE]CCoreWrite: writeAddr=%p, valueAddr=%p, streamId=%u, taskId=%u, "
        "ISA=%08x %08x %08x %08x %08x",
        writeAddr, valueAddr, streamId, taskId,
        sqe->ldrImm, sqe->llwi1, sqe->lhwi1, sqe->sw, sqe->nop[0]);
}


} // namespace Hccl