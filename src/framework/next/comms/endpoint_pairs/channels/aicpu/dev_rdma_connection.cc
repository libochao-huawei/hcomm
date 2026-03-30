/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "./dev_rdma_connection.h"
#include "orion_adapter_rts.h"
#include "exchange_rdma_conn_dto.h"
#include "hccp.h"

namespace hcomm {
constexpr uint32_t TC_TEMP = 132;
constexpr uint32_t SL_TEMP = 4;
constexpr uint32_t RETRY_CNT_TEMP = 7;
constexpr uint32_t RETRY_TIME_TEMP = 20;

DevRdmaConnection::DevRdmaConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle):
    socket_(socket), rdmaHandle_(rdmaHandle) {}

HcclResult DevRdmaConnection::Init()
{
    if (rdmaConnStatus_ != RdmaConnStatus::CLOSED) {
        HCCL_INFO("[DevRdmaConnection][%s] status[%s] is not need init.",
            __func__, rdmaConnStatus_.Describe().c_str());
        return HCCL_SUCCESS;
    }

    GetNdaOps();
    GetDirectFlag();
    GetDmaMode();

    rdmaConnStatus_ = RdmaConnStatus::INIT;
    return HCCL_SUCCESS;
}

DevRdmaConnection::~DevRdmaConnection()
{
    if (rdmaConnStatus_ == RdmaConnStatus::CLOSED || rdmaConnStatus_ == RdmaConnStatus::INIT) {
        return;
    }
    HcclResult ret = DestroyQp();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]call DestroyQp failed: hcclRet -> %d", __func__, ret);
    }
}

std::string DevRdmaConnection::Describe() const
{
    return Hccl::StringFormat("DevRdmaConnection[status=%s]", rdmaConnStatus_.Describe().c_str());
}

static void *NdaAlloc(size_t size) {
    return Hccl::HrtMalloc(size, static_cast<int>(ACL_MEM_TYPE_HIGH_BAND_WIDTH));
}

static void NdaFree(void *ptr) {
    Hccl::HrtFree(ptr);
}

static void NdaMemset(void *dst, int value, size_t count) {
    Hccl::HrtMemsetV2(dst, count, value, count);
}

static int NdaMemcpy(void *dst, size_t dstSize, void *src, size_t srcSize, uint32_t direct) {
    Hccl::rtMemcpyKind_t kind = Hccl::rtMemcpyKind_t::RT_MEMCPY_DEFAULT;
    switch (direct) {
        case MEMCPY_DIRECT_HOST_TO_HOST: {
            kind = Hccl::rtMemcpyKind_t::RT_MEMCPY_HOST_TO_HOST;
            break;
        }
        case MEMCPY_DIRECT_HOST_TO_DEVICE: {
            kind = Hccl::rtMemcpyKind_t::RT_MEMCPY_HOST_TO_DEVICE;
            break;
        }
        case MEMCPY_DIRECT_DEVICE_TO_HOST: {
            kind = Hccl::rtMemcpyKind_t::RT_MEMCPY_DEVICE_TO_HOST;
            break;
        }
        case MEMCPY_DIRECT_DEVICE_TO_DEVICE: {
            kind = Hccl::rtMemcpyKind_t::RT_MEMCPY_DEVICE_TO_DEVICE;
            break;
        }
        default: {
            HCCL_ERROR("[MemcpyKindTranslate]Not support the memory copy type[%d].", direct);
            return -1;
        }
    }
    Hccl::HrtMemcpy(dst, dstSize, src, srcSize, kind);
    return 0;
}

void DevRdmaConnection::GetNdaOps() {
    ndaOps_ = {
        .alloc = NdaAlloc,
        .free = NdaFree,
        .memset_s = NdaMemset,
        .memcpy_s = NdaMemcpy
    };
}

HcclResult DevRdmaConnection::GetDirectFlag() {
    s32 ret = RaNdaGetDirectFlag(rdmaHandle_, &directFlag_);
    if (ret != 0) {
        HCCL_ERROR("[DevRdmaConnection][GetDirectFlag]errNo[0x%016llx] get directFlag fail. "
            "return[%d], params: rdmaHandle[%p], directFlag[%d]",
            HCCL_ERROR_CODE(HCCL_E_INTERNAL), ret, rdmaHandle_, directFlag_);
        return HCCL_E_INTERNAL;
    }
    return HCCL_SUCCESS;
}

void DevRdmaConnection::GetDmaMode() {
    switch (directFlag_) {
        case DIRECT_FLAG_PCIE: {
            dmaMode_ = QBUF_DMA_MODE_DEFAULT;
            break;
        }
        case DIRECT_FLAG_UB: {
            dmaMode_ = QBUF_DMA_MODE_INDEP_UB;
            break;
        }
        default: {
            HCCL_ERROR("[GetDmaMode]Not support the directFlag, use default.");
            dmaMode_ = QBUF_DMA_MODE_MAX;
        }
    }
}

HcclResult DevRdmaConnection::CreateQp()
{
    if (socket_->GetStatus() != Hccl::SocketStatus::OK) {
        HCCL_WARNING("[DevRdmaConnection::CreateQp] socket status is not ok, please");
        return HCCL_E_AGAIN;
    }

    CHK_RET(Hccl::HrtRaNdaCqCreate(rdmaHandle_, &ndaOps_, dmaMode_, &ndaCqInfo_, &cqHandle_));

    CHK_RET(Hccl::HrtRaNdaQpCreate(rdmaHandle_, &ndaOps_, dmaMode_, &ndaQpInfo_, &qpHandle_));

    rdmaConnStatus_ = RdmaConnStatus::QP_CREATED;
    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::DestroyQp()
{
    if (rdmaConnStatus_ == RdmaConnStatus::CLOSED || rdmaConnStatus_ == RdmaConnStatus::INIT) {
        return HCCL_SUCCESS;
    }

    if (cqHandle_ != nullptr) {
        CHK_RET(Hccl::HrtRaNdaCqDestroy(rdmaHandle_, cqHandle_));
        cqHandle_ = nullptr;
    }

    if (qpHandle_ != nullptr) {
        Hccl::HrtRaQpDestroy(qpHandle_);
        qpHandle_ = nullptr;
    }

    rdmaConnStatus_ = RdmaConnStatus::CLOSED;
    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::GetExchangeDto(std::unique_ptr<Hccl::Serializable> &locQpAttrserial)
{
    if (rdmaConnStatus_ != RdmaConnStatus::QP_CREATED && rdmaConnStatus_ != RdmaConnStatus::QP_MODIFIED) {
        HCCL_ERROR("[DevRdmaConnection][%s] status[%s] is not expected.",
            __func__, rdmaConnStatus_.Describe().c_str());
        return HCCL_E_AGAIN;
    }

    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle_, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[DevRdmaConnection::%s]RaGetQpAttr failed, ret[%d]", __func__, ret);
        return HCCL_E_ROCE_CONNECT;
    }
    std::unique_ptr<ExchangeRdmaConnDto> dto= nullptr;
    EXECEPTION_CATCH(
        dto = std::make_unique<ExchangeRdmaConnDto>(localQpAttr.qpn, localQpAttr.psn, localQpAttr.gidIdx),
        return HCCL_E_PTR
    );
    CHK_SAFETY_FUNC_RET(memcpy_s(dto->gid_, HCCP_GID_RAW_LEN, localQpAttr.gid, HCCP_GID_RAW_LEN));
    locQpAttrserial = std::unique_ptr<Hccl::Serializable>(std::move(dto));
    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::ParseRmtExchangeDto(const Hccl::Serializable &rmtQpAttrSerial)
{
    auto dto = dynamic_cast<const ExchangeRdmaConnDto &>(rmtQpAttrSerial);
    HCCL_INFO("[DevRdmaConnection][%s] remoteConnDto[%s]", __func__, dto.Describe().c_str());
    rmtQpAttr_.psn = dto.psn_;
    rmtQpAttr_.qpn = dto.qpn_;
    rmtQpAttr_.gid_idx = dto.gid_idx_;
    CHK_SAFETY_FUNC_RET(memcpy_s(rmtQpAttr_.gid, HCCP_GID_RAW_LEN, dto.gid_, HCCP_GID_RAW_LEN));
    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::ModifyQp()
{
    if (rdmaConnStatus_ == RdmaConnStatus::QP_MODIFIED) {
        HCCL_WARNING("[DevRdmaConnection][%s] modify qp already, status[%s].",
                     __func__, rdmaConnStatus_.Describe().c_str());
        return HCCL_SUCCESS;
    } 
    if (rdmaConnStatus_ != RdmaConnStatus::QP_CREATED) {
        HCCL_ERROR("[DevRdmaConnection][%s] status[%s] is not expected.", __func__,
            rdmaConnStatus_.Describe().c_str());
        return HCCL_E_AGAIN;
    }

    if (!rmtQpAttr_.IsValid()) {
        HCCL_ERROR("[DevRdmaConnection][%s] romate Qp Attr is empty, exchange qp attr first", __func__);
        return HCCL_E_INTERNAL;
    }

    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle_, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[DevRdmaConnection::%s]RaGetQpAttr failed, ret[%d]", __func__, ret);
        return HCCL_E_ROCE_CONNECT;
    }

    struct TypicalQp localQp;
    struct TypicalQp rmtQp;
    localQp.sl = qpInfo_.serviceLevel == 0 ? SL_TEMP : qpInfo_.serviceLevel;
    localQp.tc = qpInfo_.trafficClass == 0 ? TC_TEMP : qpInfo_.trafficClass;
    localQp.retryCnt = qpInfo_.retryCnt == 0 ? RETRY_CNT_TEMP : qpInfo_.retryCnt;
    localQp.retryTime = qpInfo_.retryInterval == 0? RETRY_TIME_TEMP : qpInfo_.retryInterval;
    localQp.qpn = localQpAttr.qpn;
    localQp.psn = localQpAttr.psn;
    localQp.gidIdx = localQpAttr.gidIdx;
    (void)memcpy_s(localQp.gid, HCCP_GID_RAW_LEN, localQpAttr.gid, HCCP_GID_RAW_LEN);
    rmtQp.sl = qpInfo_.serviceLevel == 0 ? SL_TEMP : qpInfo_.serviceLevel;
    rmtQp.tc = qpInfo_.trafficClass == 0 ? TC_TEMP : qpInfo_.trafficClass;
    rmtQp.retryCnt = qpInfo_.retryCnt == 0 ? RETRY_CNT_TEMP : qpInfo_.retryCnt;
    rmtQp.retryTime = qpInfo_.retryInterval == 0? RETRY_TIME_TEMP : qpInfo_.retryInterval;
    rmtQp.qpn = rmtQpAttr_.qpn;
    rmtQp.psn = rmtQpAttr_.psn;
    rmtQp.gidIdx = rmtQpAttr_.gid_idx;
    (void)memcpy_s(rmtQp.gid, HCCP_GID_RAW_LEN, rmtQpAttr_.gid, HCCP_GID_RAW_LEN);
    ret = RaTypicalQpModify(qpHandle_, &localQp, &rmtQp);
    if (ret != 0) {
        HCCL_ERROR("[modify][ra_qp]modify qp failed, ret(%d)", ret);
        return HCCL_E_ROCE_CONNECT;
    }
    rdmaConnStatus_ = RdmaConnStatus::QP_MODIFIED;
    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::BuildSqContext(SqContext* context)
{
    if (context == nullptr) {
        HCCL_ERROR("[BuildSqContext] Invalid null pointer for context.");
        return HCCL_E_PTR;
    }

    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle_, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[DevRdmaConnection::%s]RaGetQpAttr failed, ret[%d]", __func__, ret);
        return HCCL_E_ROCE_CONNECT;
    }

    context->type = 1; // 0-jfs 1-rdma
    context->RdmaSqContext.qpn = localQpAttr.qpn;
    context->RdmaSqContext.sqVa = ndaQpInfo_.sqInfo.qBuf.base;
    context->RdmaSqContext.wqeSize = ndaQpInfo_.sqInfo.qBuf.entrySize;
    context->RdmaSqContext.depth = ndaQpInfo_.sqInfo.qBuf.entryCnt;
    context->RdmaSqContext.headAddr = reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbrPiVa.iov_base);
    context->RdmaSqContext.tailAddr = reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbrCiVa.iov_base);
    context->RdmaSqContext.dbVa = reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbHwVa.iov_base);
    context->RdmaSqContext.sl = qpInfo_.serviceLevel == 0 ? SL_TEMP : qpInfo_.serviceLevel;
    context->RdmaSqContext.dbMode = 0; // 0-hw 1-sw

    HCCL_INFO(
        "[DevRdmaConnection][%s] type=%u, QPN=%u, SQ_VA=0x%llx, WQE_SIZE=%u, "
        "SQ_DEPTH=%u, SQ_HEAD_ADDR=0x%llx, SQ_TAIL_ADDR=0x%llx, "
        "SL=%u, SQ_DB_VA=0x%llx, SQ_DB_MODE=%d", __func__, context->type,
        context->RdmaSqContext.qpn, context->RdmaSqContext.sqVa, context->RdmaSqContext.wqeSize,
        context->RdmaSqContext.depth, context->RdmaSqContext.headAddr, context->RdmaSqContext.tailAddr,
        context->RdmaSqContext.sl, context->RdmaSqContext.dbVa, context->RdmaSqContext.dbMode
    );

    return HCCL_SUCCESS;
}

HcclResult DevRdmaConnection::BuildCqContext(CqContext* context)
{
    if (context == nullptr) {
        HCCL_ERROR("[GetCqContext] Invalid null pointer for context.");
        return HCCL_E_PTR;
    }

    context->type = 1; // 0-jfs 1-rdma
    context->RdmaCqContext.cqn = 0;
    context->RdmaCqContext.cqVa = ndaCqInfo_.cqInfo.qBuf.base;
    context->RdmaCqContext.cqeSize = ndaCqInfo_.cqInfo.qBuf.entrySize;
    context->RdmaCqContext.cqDepth = ndaCqInfo_.cqInfo.qBuf.entryCnt;
    context->RdmaCqContext.headAddr = reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbrPiVa.iov_base);
    context->RdmaCqContext.tailAddr = reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbrCiVa.iov_base);
    context->RdmaCqContext.dbVa = reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbHwVa.iov_base);
    context->RdmaCqContext.dbMode = 0; // 0-hw 1-sw

    HCCL_INFO(
        "[DevRdmaConnection][%s] type=%u, CQN=%u, CQ_VA=0x%llx, CQE_SIZE=%u, CQ_DEPTH=%u, "
        "CQ_HEAD_ADDR=0x%llx, CQ_TAIL_ADDR=0x%llx, CQ_DB_VA=0x%llx, CQ_DB_MODE=%d]",
        __func__, context->type,
        context->RdmaCqContext.cqn, context->RdmaCqContext.cqVa, context->RdmaCqContext.cqeSize,
        context->RdmaCqContext.cqDepth, context->RdmaCqContext.headAddr, context->RdmaCqContext.tailAddr,
        context->RdmaCqContext.dbVa, context->RdmaCqContext.dbMode
    );

    return HCCL_SUCCESS;
}

std::vector<char> DevRdmaConnection::GetSqUniqueId() const
{
    HCCL_INFO("start packing sq uniqueId");
    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpHandle_, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[DevRdmaConnection::%s]RaGetQpAttr failed, ret[%d]", __func__, ret);
        return {};
    }

    Hccl::BinaryStream binaryStream;
    binaryStream << localQpAttr.qpn; // qpn
    binaryStream << ndaQpInfo_.sqInfo.qBuf.base; // sqVa
    binaryStream << ndaQpInfo_.sqInfo.qBuf.entrySize; // wqeSize
    binaryStream << ndaQpInfo_.sqInfo.qBuf.entryCnt; // depth
    binaryStream << reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbrPiVa.iov_base); // headAddr
    binaryStream << reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbrCiVa.iov_base); // tailAddr
    binaryStream << reinterpret_cast<uint64_t >(ndaQpInfo_.sqInfo.dbHwVa.iov_base); // dbVa
    binaryStream << (qpInfo_.serviceLevel == 0 ? SL_TEMP : qpInfo_.serviceLevel); // sl
    binaryStream << 0; // 0-hw 1-sw

    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

std::vector<char> DevRdmaConnection::GetCqUniqueId() const
{
    HCCL_INFO("start packing cq uniqueId");
    Hccl::BinaryStream binaryStream;
    binaryStream << 0; // cqn
    binaryStream << ndaCqInfo_.cqInfo.qBuf.base; // cqVa
    binaryStream << ndaCqInfo_.cqInfo.qBuf.entrySize; // cqeSize
    binaryStream << ndaCqInfo_.cqInfo.qBuf.entryCnt; // cqDepth
    binaryStream << reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbrPiVa.iov_base); // headAddr
    binaryStream << reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbrCiVa.iov_base); // tailAddr
    binaryStream << reinterpret_cast<uint64_t >(ndaCqInfo_.cqInfo.dbHwVa.iov_base); // dbVa
    binaryStream << 0; // 0-hw 1-sw

    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

std::vector<char> DevRdmaConnection::GetUniqueId() const
{
    HCCL_INFO("start packing conn uniqueId");
    Hccl::BinaryStream binaryStream;
    binaryStream << dmaMode_;
    binaryStream << GetSqUniqueId();
    binaryStream << GetCqUniqueId();

    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

} // namespace hcomm