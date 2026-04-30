/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#include "host_ub_connection.h"

#include <cstdlib>

#include "hccp_ctx.h"
#include "exception_util.h"
#include "rma_conn_exception.h"
#include "rdma_handle_manager.h"
#include "exchange_ub_conn_dto.h"

namespace Hccl {

constexpr u32 OPBASED_UB_SQ_DEPTH_MAX = 8192;
constexpr u32 UB_SQ_OFFLOAD_DEPTH     = 128;
constexpr u32 UB_SQ_WQEBB_SIZE        = 64;
constexpr u32 UB_MAX_TRANS_SIZE       = 256 * 1024 * 1024; // UB单次最大传输量256*1024*1024 Byte
constexpr u32 WQE_NUM_PER_SQE         = 4; // URMA约束每个SQE包含4个WQEBB

HostUbConnection::HostUbConnection(const RdmaHandle rdmaHandle, const IpAddress &locAddr, const IpAddress &rmtAddr,
                                const OpMode opMode, const HrtUbJfcMode jfcMode)
    : RmaConnection(nullptr, RmaConnType::UB), rdmaHandle(rdmaHandle), locAddr(locAddr), rmtAddr(rmtAddr),
    opMode(opMode), jfcMode(jfcMode), rmtEid(rmtAddr.GetReverseEid()), locEid(locAddr.GetReverseEid())
{
    HCCL_INFO("[HostUbConnection::HostUbConnection] rmtEid=%s", rmtEid.Describe().c_str());

    auto dieIdAndFuncId = RdmaHandleManager::GetInstance().GetDieAndFuncId(rdmaHandle); // 获取dieId和FuncId
    dieId               = dieIdAndFuncId.first;
    funcId              = dieIdAndFuncId.second;

    jfcHandle = RdmaHandleManager::GetInstance().GetJfcHandle(rdmaHandle, cqInfo_, jfcMode);

    sqDepth = OPBASED_UB_SQ_DEPTH_MAX;
    if (opMode == OpMode::OFFLOAD) {
        sqDepth = UB_SQ_OFFLOAD_DEPTH;
    }
    HCCL_INFO("rdmaHandle[%p] locAddr[%s] rmtAddr[%s] opMode[%u] jfcMode[%s] dieId[%u] funcId[%u] jfcHandle[%llu] sqDepth[%u]",
        rdmaHandle, locAddr.Describe().c_str(), rmtAddr.Describe().c_str(), opMode, jfcMode.Describe().c_str(),
        dieId, funcId, jfcHandle, sqDepth);
    if (sqDepth > (UINT32_MAX / UB_SQ_WQEBB_SIZE / WQE_NUM_PER_SQE)) {
        THROW<InternalException>("integer overflow occurs");
    }

    CreateJetty();
}

HostUbTpConnection::HostUbTpConnection(const RdmaHandle rdmaHandle, const IpAddress &locAddr, const IpAddress &rmtAddr,
                                    const OpMode opMode, const HrtUbJfcMode jfcMode)
    : HostUbConnection(rdmaHandle, locAddr, rmtAddr, opMode, jfcMode)
{
    tpProtocol = TpProtocol::TP;
}

HostUbCtpConnection::HostUbCtpConnection(const RdmaHandle rdmaHandle, const IpAddress &locAddr, const IpAddress &rmtAddr,
                                    const OpMode opMode, const HrtUbJfcMode jfcMode)
    : HostUbConnection(rdmaHandle, locAddr, rmtAddr, opMode, jfcMode)
{
    tpProtocol = TpProtocol::CTP;
}

std::vector<char> HostUbConnection::GetUniqueId() const
{
    BinaryStream binaryStream;
    binaryStream << dieId;
    binaryStream << funcId;
    binaryStream << jettyId_;

    u32  jfcPollMode     = 0;     // 待修改，0代表STARS POLL，1代表software Poll
    bool dwqeCacheLocked = false; // 待修改，该jetty是否支持dwqeCachedLocked，默认不支持
    u64  sqCiAddr = 0; // 待修改，软件poll CQ情况下，需要AICPU从该地址中读取CI,依赖UB驱动支持
    binaryStream << jfcPollMode;
    binaryStream << dwqeCacheLocked;
    binaryStream << dbAddr;
    binaryStream << sqCiAddr;
    binaryStream << sqBuffVa;
    binaryStream << sqDepth;
    binaryStream << tpn;
    binaryStream << rmtEid.raw;
    binaryStream << locEid.raw;

    std::vector<char> result;
    binaryStream.Dump(result);
    HCCL_INFO("HostUbConnection::GetUniqueId:%s", Describe().c_str());
    HCCL_INFO("type=%s, jfcPollMode=%u, dwqeCacheLocked=%d, sqCiAddr=0x%llx", rmaConnType.Describe().c_str(),
            jfcPollMode, dwqeCacheLocked, sqCiAddr);
    return result;
}

void HostUbConnection::SetCqInfo(HcclAiRMACQ &cq)
{
    cq.jfcId = cqInfo_.id;
    cq.cqVA = cqInfo_.va;
    cq.cqeSize = cqInfo_.cqeSize;
    cq.cqDepth = cqInfo_.cqDepth;
    cq.dbAddr = cqInfo_.swdbAddr;
}

void HostUbConnection::SetWqInfo(HcclAiRMAWQ &wq)
{
    wq.jettyId = jettyId_;
    wq.dbAddr = dbAddr;
    wq.sqVA = sqBuffVa;
    wq.sqDepth = sqDepth * WQE_NUM_PER_SQE;
    wq.tp_id = tpn;
    memcpy_s(wq.rmtEid, sizeof(wq.rmtEid), rmtEid.raw, sizeof(wq.rmtEid));
}

void HostUbConnection::Connect()
{
    GetStatus();
}

inline uint32_t GetRandomNum()
{
    uint32_t randNum = std::rand();
    return randNum;
}

RmaConnStatus HostUbConnection::GetStatus()
{
    switch (ubConnStatus) {
        case UbConnStatus::INIT: {
            HCCL_INFO("[HostUbConnection][%s] start, status[%s], ubConnStatus[%s].", __func__, status.Describe().c_str(),
                    ubConnStatus.Describe().c_str());

            SetJettyInfo();

            if (!GetTpInfo()) {
                ubConnStatus = UbConnStatus::TP_INFO_GETTING;
                break;
            }

            status       = RmaConnStatus::EXCHANGEABLE;
            ubConnStatus = UbConnStatus::JETTY_CREATED;
            break;
        }
        case UbConnStatus::TP_INFO_GETTING: {
            if (GetTpInfo()) {
                status       = RmaConnStatus::EXCHANGEABLE;
                ubConnStatus = UbConnStatus::JETTY_CREATED;
            }
            break;
        }
        case UbConnStatus::JETTY_CREATED: {
            HCCL_INFO("[HostUbConnection][%s] status[%s] will not change, "
                    "should call ImportRmtDto to change status.",
                    __func__, status.Describe().c_str());
            break;
        }
        case UbConnStatus::JETTY_IMPORTING: {
            SetImportInfo();

            status       = RmaConnStatus::READY;
            ubConnStatus = UbConnStatus::READY;
            break;
        }
        case UbConnStatus::READY:
            break;
        default:
            ThrowAbnormalStatus(std::string(__func__));
    }

    return status;
}

std::unique_ptr<Serializable> HostUbConnection::GetExchangeDto()
{
    if (status != RmaConnStatus::READY && status != RmaConnStatus::EXCHANGEABLE) {
        HCCL_ERROR("[HostUbConnection][%s] status[%s] is not expected.", __func__,
            status.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    if (tpProtocol != TpProtocol::INVALID) {
        jettyImportCfg.localTpHandle = tpInfo.tpHandle;

        HCCL_INFO("[HostUbConnection][%s] tpEnable, localTpHandle[0x%llx] localPsn[%u].", __func__,
                jettyImportCfg.localTpHandle, jettyImportCfg.localPsn);
    }

    std::unique_ptr<ExchangeUbConnDto> dto
        = make_unique<ExchangeUbConnDto>(tokenValue, keySize, jettyImportCfg.localTpHandle, jettyImportCfg.localPsn);
    (void)memcpy_s(dto->qpKey, HRT_UB_QP_KEY_MAX_LEN, repJetty_.key, HRT_UB_QP_KEY_MAX_LEN);
    return std::unique_ptr<Serializable>(dto.release());
}

void HostUbConnection::ParseRmtExchangeDto(const Serializable &rmtDto)
{
    auto dto = dynamic_cast<const ExchangeUbConnDto &>(rmtDto);
    HCCL_INFO("[HostUbConnection][%s] remoteConnDto[%s]", __func__, dto.Describe().c_str());
    remoteTokenValue = dto.tokenValue;
    (void)memcpy_s(remoteQpKey, HRT_UB_QP_KEY_MAX_LEN, dto.qpKey, HRT_UB_QP_KEY_MAX_LEN);

    if (tpProtocol != TpProtocol::INVALID) {
        jettyImportCfg.remoteTpHandle = dto.tpHandle;
        jettyImportCfg.remotePsn      = dto.psn;
        HCCL_INFO("[HostUbConnection][%s] tpEnable, remoteTpHandle[0x%llx], remotePsn[%u].", __func__,
                jettyImportCfg.remoteTpHandle, jettyImportCfg.remotePsn);
    }
}

void HostUbConnection::ImportRmtDto()
{
    if (ubConnStatus == UbConnStatus::READY) {
        HCCL_WARNING("[HostUbConnection][%s] import jetty already, %s.",
                    __func__, Describe().c_str());
        return;
    }

    if (ubConnStatus != UbConnStatus::JETTY_CREATED) {
        HCCL_ERROR("[HostUbConnection][%s] failed, ubConnStatus[%s] is not expected.",
            __func__, ubConnStatus.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    ImportJetty();
    ubConnStatus = UbConnStatus::JETTY_IMPORTING;
}

void HostUbConnection::ThrowAbnormalStatus(std::string funcName)
{
    auto errMsg = StringFormat("[HostUbConnection][%s] failed, [%s].",
        funcName.c_str(), Describe().c_str());
    status = RmaConnStatus::CONN_INVALID;
    ubConnStatus = UbConnStatus::CONN_INVALID;
    THROW<RmaConnException>(errMsg);
}

bool HostUbConnection::CheckRequestResult()
{
    if (reqHandle == 0) {
        return true;
    }

    ReqHandleResult result = HrtRaGetAsyncReqResult(reqHandle);
    if (result == ReqHandleResult::NOT_COMPLETED) {
        return false;
    }

    if (result != ReqHandleResult::COMPLETED) {
        THROW<InternalException>("[HostUbConnection][%s] failed, result[%s] is unexpected.",
            __func__, result.Describe().c_str());
    }

    return true;
}

void HostUbConnection::CreateJetty()
{
    if (sqDepth > UINT32_MAX / UB_SQ_WQEBB_SIZE / WQE_NUM_PER_SQE) {
        THROW<InternalException>("[HostUbConnection][%s] failed, sqDepth[%u] times "
            "UB_SQ_WQEBB_SIZE[%u] overflow uint32 max.", __func__, sqDepth, UB_SQ_WQEBB_SIZE);
    }
    u32 size = static_cast<u32>(sqDepth) * static_cast<u32>(UB_SQ_WQEBB_SIZE) * static_cast<u32>(WQE_NUM_PER_SQE);
    TokenIdHandle tokenIdHandle = RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle).first;
    HrtRaUbCreateJettyParam req {
        jfcHandle, jfcHandle,
        GetUbToken(), tokenIdHandle,
        HrtJettyMode::STANDARD, // peer模式只支持JETTY_MODE_URMA_NORMAL
        0, // HOST展开与AICPU展开传入jetty id为0，申请一个新的jetty
        0, // va由底层分配，此处填0即可。
        size, 0, sqDepth}; // 非CCUv2不需要填写sqeBufIndex

    repJetty_ = HrtRaUbCreateJetty(rdmaHandle, req);
}

void HostUbConnection::SetJettyInfo()
{
    jettyId_     = repJetty_.id;
    jettyHandle_ = repJetty_.handle;
    jettyVa_     = repJetty_.jettyVa;
    sqBuffVa     = repJetty_.sqBuffVa; // hccp提供
    HCCL_INFO("[HostUbConnection][%s] Get sqBuffVa is %llx.", __func__, sqBuffVa);
    keySize      = repJetty_.keySize;
    dbAddr = repJetty_.dbVa;
}

bool HostUbConnection::GetTpInfo()
{
    if (tpProtocol == TpProtocol::INVALID) { // 不感知tp建链，当前默认不支持
        HCCL_ERROR("[HostUbConnection][%s] failed, tpProtocol[%s] is not expected.",
            __func__, tpProtocol.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    int32_t devLogicId = HrtGetDevice();
    TpManager::GetInstance(devLogicId).SetIsHost();
    auto ret = TpManager::GetInstance(devLogicId).GetTpInfo(
        {locAddr, rmtAddr, tpProtocol}, tpInfo);

    switch (ret) {
        case HcclResult::HCCL_SUCCESS:
            GenerateLocalPsn();
            return true;
        case HcclResult::HCCL_E_AGAIN:
            return false;
        case HcclResult::HCCL_E_NOT_FOUND:
        default:
            HCCL_ERROR("[HostUbConnection][%s] failed, hccl result[%d]", __func__, ret);
            ThrowAbnormalStatus(std::string(__func__));
    }
    return true;
}

void HostUbConnection::GenerateLocalPsn()
{
    jettyImportCfg.localPsn = GetRandomNum();
}

void HostUbConnection::ImportJetty()
{
    HrtRaUbJettyImportedInParam in{};
    in.key            = remoteQpKey;
    in.keyLen         = keySize;
    in.tokenValue     = remoteTokenValue;
    in.jettyImportCfg = jettyImportCfg;
    in.jettyImportCfg.protocol = tpProtocol;

    if (tpProtocol != TpProtocol::CTP && tpProtocol != TpProtocol::TP) {
        HCCL_ERROR("[HostUbConnection][%s] failed, tp protocol[%s] is not expected, %s.",
            __func__, tpProtocol.Describe().c_str(), Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    remOutParam_ = RaUbTpImportJetty(rdmaHandle, in.key, in.keyLen, in.tokenValue, in.jettyImportCfg);
}

void HostUbConnection::SetImportInfo()
{
    remoteJettyVa_ = remOutParam_.targetJettyVa;
    remoteJettyHandle_ = remOutParam_.handle;
    tpn = remOutParam_.tpn;
    return;
}

void HostUbConnection::ReleaseTp()
{
    int32_t devLogicId = HrtGetDevice();
    if (tpInfo.tpHandle != 0) {
        (void)TpManager::GetInstance(devLogicId)
            .ReleaseTpInfo({locAddr, rmtAddr, tpProtocol}, tpInfo);
        tpInfo.tpHandle = 0;
    }
}

void HostUbConnection::ReleaseResource()
{
    if (rdmaHandle && remoteJettyHandle_ != 0) {
        HrtRaUbUnimportJetty(rdmaHandle, remoteJettyHandle_);
        remoteJettyHandle_ = 0;
    }

    ReleaseTp();

    if (jettyHandle_ != 0) {
        HrtRaUbDestroyJetty(jettyHandle_);
        jettyHandle_ = 0;
    }
}

HostUbConnection::~HostUbConnection()
{
    DECTOR_TRY_CATCH("HostUbConnection", ReleaseResource());
}

// Suspend接口当前已不使用，由框架调用触发析构流程
bool HostUbConnection::Suspend()
{
    HCCL_WARNING("[HostUbConnection][%s] should not be called.", __func__);
    return true;
}

std::unique_ptr<BaseTask> HostUbConnection::ConstructTaskUbSend(const HrtRaUbSendWrRespParam &sendWrResp,
                                                            const SqeConfig              &config)
{
    unique_ptr<BaseTask> result;
    if (opMode == OpMode::OPBASE) {
        if (config.wqeMode == WqeMode::DWQE) {
            result = make_unique<TaskUbDirectSend>(sendWrResp.funcId, sendWrResp.dieId, sendWrResp.jettyId,
                                                sendWrResp.dwqeSize, sendWrResp.dwqe);
        } else if (config.wqeMode == WqeMode::DB_SEND) {
            result
                = make_unique<TaskUbDbSend>(sendWrResp.jettyId, sendWrResp.funcId, sendWrResp.piVal, sendWrResp.dieId);
        } else if (config.wqeMode == WqeMode::WRITE_VALUE) {
            HCCL_INFO("[HostUbConnection::%s] dbAddr=[%llx], piVal=[%u]", __func__, dbAddr, sendWrResp.piVal);
            result = make_unique<TaskWriteValue>(dbAddr, sendWrResp.piVal);
        } else {
            auto msg = StringFormat("Invalid WqeMode[%s]", config.wqeMode.Describe().c_str());
            THROW<InvalidParamsException>(msg);
        }
    } else if (opMode == OpMode::OFFLOAD) {
        CHK_PRT_THROW(sendWrResp.piVal < piVal,
                    HCCL_ERROR("[HostUbConnection::%s] sendWrResp.piVal[%u] is less than piVal[%u]", __func__, sendWrResp.piVal, piVal),
                    InvalidParamsException, "sendWrResp.piVal or piVal is invalid");
        u32 sendPiVal = sendWrResp.piVal - piVal;
        result = make_unique<TaskUbDbSend>(sendWrResp.jettyId, sendWrResp.funcId, sendPiVal, sendWrResp.dieId);
        HCCL_INFO("[HostUbConnection::%s] sendPiVal[%u] piVal[%u] sendWrResp.piVal[%u]", __func__, sendPiVal, piVal, sendWrResp.piVal);
    } else {
        auto msg = StringFormat("Invalid OpMode[%s]", opMode.Describe().c_str());
        THROW<InvalidParamsException>(msg);
    }

    piVal = sendWrResp.piVal;
    return result;
}

void HostUbConnection::ProcessSlices(const MemoryBuffer &loc, const MemoryBuffer &rmt,
                                    std::function<void(const MemoryBuffer &, const MemoryBuffer &, u32)> processOneSlice,
                                    DataType                                                             dataType) const
{
    HCCL_INFO("[HostUbConnection::%s] start", __func__);

    // reduce操作需要保证切片大小是数据类型大小的整数倍
    u32 sliceSize = UB_MAX_TRANS_SIZE;
    if (dataType != DataType::INVALID) {
        u32 dataTypeSize = DATA_TYPE_SIZE_MAP.at(dataType);
        sliceSize        = UB_MAX_TRANS_SIZE / dataTypeSize * dataTypeSize;
    }

    u32 locBufSize    = loc.size;
    u32 sliceNum      = locBufSize / sliceSize;
    u32 lastSliceSize = locBufSize % sliceSize;
    u64 totalSize = static_cast<u64>(sliceNum) * static_cast<u64>(sliceSize);
    if (loc.addr > UINT64_MAX - totalSize || rmt.addr > UINT64_MAX - totalSize) {
        THROW<InternalException>("integer overflow occurs");
    }
    for (u32 sliceIdx = 0; sliceIdx < sliceNum; sliceIdx++) {
        MemoryBuffer locSlice(loc.addr + sliceIdx * sliceSize, sliceSize, loc.memHandle);
        MemoryBuffer rmtSlice(rmt.addr + sliceIdx * sliceSize, sliceSize, rmt.memHandle);
        // 当前是最后一片，且没有lastSlice时，启用cqe
        u32 cqeEnable = (sliceIdx == sliceNum - 1 && lastSliceSize == 0) ? 1 : 0;
        processOneSlice(locSlice, rmtSlice, cqeEnable);
    }

    if (lastSliceSize > 0) {
        MemoryBuffer lastLocSlice(loc.addr + sliceNum * sliceSize, lastSliceSize, loc.memHandle);
        MemoryBuffer lastRmtSlice(rmt.addr + sliceNum * sliceSize, lastSliceSize, rmt.memHandle);
        processOneSlice(lastLocSlice, lastRmtSlice, 1);
        sliceNum++;
    }

    HCCL_INFO("[HostUbConnection::%s] end, locBufSize[%u], sliceNUm[%u], sliceSize[%u], lastSliceSize[%u]", __func__,
            locBufSize, sliceNum, sliceSize, lastSliceSize);
}

unique_ptr<BaseTask> HostUbConnection::PrepareRead(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                                const SqeConfig &config)
{
    HCCL_INFO("[HostUbConnection::%s] not supported yet.", __func__);
    return nullptr;
}

unique_ptr<BaseTask> HostUbConnection::PrepareReadReduce(const MemoryBuffer &remoteMemBuf,
                                                        const MemoryBuffer &localMemBuf, DataType dataType,
                                                        ReduceOp reduceOp, const SqeConfig &config)
{
    HCCL_INFO("[HostUbConnection::%s] not supported yet.", __func__);
    return nullptr;
}

unique_ptr<BaseTask> HostUbConnection::PrepareWrite(const MemoryBuffer &remoteMemBuf, const MemoryBuffer &localMemBuf,
                                                const SqeConfig &config)
{
    HCCL_INFO("[HostUbConnection::%s] not supported yet.", __func__);
    return nullptr;
}

unique_ptr<BaseTask> HostUbConnection::PrepareWriteReduce(const MemoryBuffer &remoteMemBuf,
                                                        const MemoryBuffer &localMemBuf, DataType dataType,
                                                        ReduceOp reduceOp, const SqeConfig &config)
{
    HCCL_INFO("[HostUbConnection::%s] not supported yet.", __func__);
    return nullptr;
}

unique_ptr<BaseTask> HostUbConnection::PrepareInlineWrite(const MemoryBuffer &remoteMemBuf, u64 data,
                                                        const SqeConfig &config)
{
    HCCL_INFO("[HostUbConnection::%s] not supported yet.", __func__);
    return nullptr;
}

string HostUbConnection::Describe() const
{
    return StringFormat("HostUbConnection[locAddr=%s, rmtAddr=%s, status=%s, dieId=%u, funcId=%u, jettyId=%u, sqBuffVa=%llx, "
                        "sqDepth=%u, tpn=%u, dbAddr=0x%llx]",
                        locAddr.Describe().c_str(), rmtAddr.Describe().c_str(), status.Describe().c_str(), dieId,
                        funcId, jettyId_, sqBuffVa, sqDepth, tpn, dbAddr);
}

void HostUbConnection::AddNop(const Stream &stream)
{
    if (opMode != OpMode::OFFLOAD) {
        HCCL_WARNING("[HostUbConnection][AddNop]Invalid OpMode[%s]", opMode.Describe().c_str());
        return;
    }
    if (sqDepth < piVal) {
        auto msg = StringFormat("Invalid piVal[%u], piVal should be less than or equal to sqDepth[%u]", piVal, sqDepth);
        THROW<InvalidParamsException>(msg);
    }
    if (sqDepth == piVal) {
        return;
    }
    u32 numNop = sqDepth - piVal;
    HrtRaUbPostNops(jettyHandle_, remoteJettyHandle_, numNop);

    HrtUbDbInfo info;
    info.dbNum = 1;
    info.wrCqe = 0; // 默认值是0 不会cqe  如果传1，驱动分发，会给hccl cqe，用于维护ci指针。
    info.info[0].functionId = funcId;
    info.info[0].dieId      = dieId;
    info.info[0].jettyId    = jettyId_;
    info.info[0].piValue    = numNop;
    HrtUbDbSend(info, stream.GetPtr());

    piVal = sqDepth;
}

HrtUbJfcMode HostUbConnection::GetUbJfcMode() const
{
    return jfcMode;
}

JettyHandle& HostUbConnection::GetJettyHandle()
{
    return jettyHandle_;
}

JettyHandle&  HostUbConnection::GetRemoteJettyHandle()
{
    return remoteJettyHandle_;
}

RdmaHandle&  HostUbConnection::GetRdmaHandle()
{
    return rdmaHandle;
}

u32 HostUbConnection::GetPiVal() const
{
    return piVal;
}

u32 HostUbConnection::GetCiVal() const
{
    return ciVal;
}

u32 HostUbConnection::GetSqDepth() const
{
    return sqDepth;
}

uint64_t HostUbConnection::GetCqVa() const
{
    return cqInfo_.va;
}

u64 HostUbConnection::GetJettyVa() const
{
    return jettyVa_;
}

JettyHandle HostUbConnection::GetTJettyVa() const
{
    return remoteJettyVa_;
}

void HostUbConnection::UpdateCiVal(u32 ci)
{
    ciVal = ci;
}

bool IfNeedUpdatingUbCi(const std::vector<HostUbConnection *> &ubConns)
{
    for (auto &ubConn : ubConns) {
        u32 pi      = ubConn->GetPiVal();
        u32 ci      = ubConn->GetCiVal();
        u32 sqDepth = ubConn->GetSqDepth();
        // 考虑pi翻转场景
        u32 extra = pi >= ci ? 0 : sqDepth;

        if (static_cast<double>(pi + extra - ci) >= static_cast<double>(sqDepth) / 2) { 
            // 当pi和ci差距大于sqDepth/2时，更新ci
            return true;
        }
    }
    return false;
}

} // namespace Hccl