/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_transport_.h"

#include "exception_handler.h"

namespace hcomm {

constexpr uint32_t FINISH_MSG_SIZE = 128;
constexpr char FINISH_MSG[FINISH_MSG_SIZE] = "Transport exchange data ready!";

HcclResult CcuCreateTransport(Hccl::Socket *socket, const CcuTransport::CcuConnectionInfo &ccuConnectionInfo,
    const CcuTransport::CclBufferInfo &cclBufferInfo, std::unique_ptr<CcuTransport> &ccuTransport)
{
    CHK_PTR_NULL(socket);
    std::unique_ptr<CcuConnection> ccuConnection{nullptr};
    if (ccuConnectionInfo.type == CcuTransport::CcuConnectionType::UBC_CTP) {
        ccuConnection.reset(new (std::nothrow) CcuCtpConnection(
            ccuConnectionInfo.locAddr, ccuConnectionInfo.rmtAddr,
            ccuConnectionInfo.channelInfo, ccuConnectionInfo.ccuJettys));
    } else {
        ccuConnection.reset(new (std::nothrow) CcuRtpConnection(
            ccuConnectionInfo.locAddr, ccuConnectionInfo.rmtAddr,
            ccuConnectionInfo.channelInfo, ccuConnectionInfo.ccuJettys));
    }
    CHK_PTR_NULL(ccuConnection);
    CHK_RET(ccuConnection->Init());

    ccuTransport.reset(new (std::nothrow)
        CcuTransport(socket, std::move(ccuConnection), cclBufferInfo));
    CHK_PTR_NULL(ccuTransport);
    CHK_RET(ccuTransport->Init());

    return HcclResult::HCCL_SUCCESS;
}

CcuTransport::CcuTransport(Hccl::Socket *socket, std::unique_ptr<CcuConnection> &&connection,
    const CclBufferInfo &locCclBufInfo)
    : socket_(socket), ccuConnection_(std::move(connection)), locCclBufInfo_(locCclBufInfo)
{
}

HcclResult CcuTransport::Init()
{
    dieId_      = ccuConnection_->GetDieId();
    devLogicId_ = ccuConnection_->GetDevLogicId();
    auto ret = AppendCkes(INIT_CKE_NUM);
    if (ret == HcclResult::HCCL_E_UNAVAIL) {
        return ret;
    }
    CHK_RET(ret);

    ret = AppendXns(INIT_XN_NUM);
    if (ret == HcclResult::HCCL_E_UNAVAIL) {
        return ret;
    }
    CHK_RET(ret);

    transStatus_ = TransStatus::INIT;
    return HCCL_SUCCESS;
}

CcuTransport::TransStatus CcuTransport::GetStatus()
{
    if (transStatus_ == TransStatus::READY
        || transStatus_ == TransStatus::CONNECT_FAILED
        || transStatus_ == TransStatus::SOCKET_TIMEOUT) {
        return transStatus_;
    }

    if (StatusMachine() != HcclResult::HCCL_SUCCESS) {
        HCCL_ERROR("[CcuTransport][%s] failed, %s.",
            __func__, transStatus_.Describe().c_str());
        transStatus_ = TransStatus::CONNECT_FAILED;
    }

    return transStatus_;
}

HcclResult CcuTransport::AppendCkes(uint32_t ckesNum)
{
    std::vector<ResInfo> resInfo;
    auto ret = CcuDevMgrImp::AllocCke(devLogicId_, dieId_, ckesNum, resInfo);
    CHK_PRT_RET(ret == HcclResult::HCCL_E_UNAVAIL,
        HCCL_WARNING("[CcuTransport][%s] failed, the resource is not enough.",
            __func__),
        ret);
    CHK_RET(ret);

    const uint32_t resSize = resInfo.size();
    for (uint32_t i = 0; i < resSize; i++) {
        const uint32_t ckeNum     = resInfo[i].num;
        const uint32_t ckesSartId = resInfo[i].startId;
        for (uint32_t j = 0; j < ckeNum; j++) {
            locRes_.ckes.emplace_back(ckesSartId + j);
        }
    }
    ckesRes_.push_back(resInfo);
    return HCCL_SUCCESS;
}

HcclResult CcuTransport::AppendXns(uint32_t xnsNum)
{
    std::vector<ResInfo> resInfo;
    auto ret = CcuDevMgrImp::AllocXn(devLogicId_, dieId_, xnsNum, resInfo);
    CHK_PRT_RET(ret == HcclResult::HCCL_E_UNAVAIL,
        HCCL_WARNING("[CcuTransport][%s] failed, the resource is not enough.",
            __func__),
        ret);
    CHK_RET(ret);

    const uint32_t resSize = resInfo.size();
    for (uint32_t i = 0; i < resSize; i++) {
        uint32_t xnNum     = resInfo[i].num;
        uint32_t xnsSartId = resInfo[i].startId;
        for (uint32_t j = 0; j < xnNum; j++) {
            locRes_.xns.emplace_back(xnsSartId + j);
        }
    }
    xnsRes_.push_back(resInfo);
    return HCCL_SUCCESS;
}

HcclResult CcuTransport::StatusMachine()
{
    EXCEPTION_HANDLE_BEGIN
    Hccl::SocketStatus socketStatus = socket_->GetAsyncStatus();
    if (socketStatus == Hccl::SocketStatus::INIT) {
        HCCL_ERROR("[CcuTransport][GetStatus]socket timeout or no link, please check");
        return HcclResult::HCCL_E_INTERNAL;
    }
    
    if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        transStatus_ = TransStatus::SOCKET_TIMEOUT;
        return HcclResult::HCCL_SUCCESS; // 操作成功，置成错误状态
    }
    
    if (socketStatus != Hccl::SocketStatus::OK) {
        return HcclResult::HCCL_SUCCESS; // 操作成功，保持当前状态
    }
    EXCEPTION_HANDLE_END

    switch (transStatus_) {
        case CcuTransport::TransStatus::INIT: {
            auto connStatus = ccuConnection_->GetStatus();
            if (connStatus == CcuConnStatus::CONN_INVALID) {
                HCCL_ERROR("[CcuTransport][GetStatus] connection status[%s] failed."
                    " please check.", connStatus.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }

            if (connStatus == CcuConnStatus::EXCHANGEABLE
                || connStatus == CcuConnStatus::CONNECTED) {
                // connection完成本端资源创建或复用时，发送本端资源信息
                CHK_RET(SendConnAndTransInfo());
                transStatus_ = TransStatus::SEND_ALL_INFO;
            }

            // connection状态非错误但未达到目标状态时，transport保持当前状态
            break;
        }
        case CcuTransport::TransStatus::SEND_ALL_INFO:
            CHK_RET(RecvConnAndTransInfo());
            transStatus_ = TransStatus::RECV_ALL_INFO;
            break;
        case CcuTransport::TransStatus::RECV_ALL_INFO:
            CHK_RET(RecvDataProcess());
            CHK_RET(ccuConnection_->ImportJetty());
            transStatus_ = TransStatus::SEND_FIN;
            break;
        case CcuTransport::TransStatus::SEND_FIN: {
            auto connStatus = ccuConnection_->GetStatus();
            if (connStatus == CcuConnStatus::CONN_INVALID) {
                HCCL_ERROR("[CcuTransport][GetStatus] connection status[%s] failed."
                    " please check", connStatus.Describe().c_str());
                return HcclResult::HCCL_E_INTERNAL;
            }

            if (connStatus == CcuConnStatus::CONNECTED) {
                CHK_RET(SendFinish());
                transStatus_ = CcuTransport::TransStatus::RECVING_FIN;
            }
            break;
        }
        case CcuTransport::TransStatus::RECVING_FIN:
            CHK_RET(RecvFinish());
            transStatus_ = CcuTransport::TransStatus::RECV_FIN;
            break;
        case CcuTransport::TransStatus::RECV_FIN:
            CHK_RET(CheckFinish());
            transStatus_ = CcuTransport::TransStatus::READY;
            break;
        case CcuTransport::TransStatus::SEND_TRANS_RES:
            CHK_RET(SendTransInfo());
            transStatus_ = CcuTransport::TransStatus::RECVING_TRANS_RES;
            break;
        case CcuTransport::TransStatus::RECVING_TRANS_RES:
            CHK_RET(RecvTransInfo());
            transStatus_ = CcuTransport::TransStatus::RECV_TRANS_RES;
            break;
        case CcuTransport::TransStatus::RECV_TRANS_RES:
            CHK_RET(RecvTransInfoProcess());
            transStatus_ = CcuTransport::TransStatus::SEND_FIN;
            break;
        default:
            HCCL_ERROR("[CcuTransport][%s] failed, error status[%s].",
                __func__, transStatus_.Describe().c_str());
            transStatus_ = CcuTransport::TransStatus::CONNECT_FAILED;
            break;
    }
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::SendConnAndTransInfo()
{
    Hccl::BinaryStream binaryStream;
    CHK_RET(HandshakeMsgPack(binaryStream));
    CHK_RET(ConnInfoPack(binaryStream));
    CHK_RET(TransResPack(binaryStream));
    CHK_RET(CclBufferInfoPack(binaryStream));
    binaryStream.Dump(sendData_);
    // 当前socket失败会抛异常，需要统一整改
    EXCEPTION_HANDLE_BEGIN
    socket_->SendAsync(reinterpret_cast<u8 *>(sendData_.data()), sendData_.size());
    EXCEPTION_HANDLE_END
    exchangeDataSize_ = sendData_.size();
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::RecvConnAndTransInfo()
{
    recvData_.resize(exchangeDataSize_);
    EXCEPTION_HANDLE_BEGIN
    socket_->RecvAsync(reinterpret_cast<u8 *>(recvData_.data()), recvData_.size());
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::RecvDataProcess()
{
    Hccl::BinaryStream binaryStream(recvData_);
    CHK_RET(HandshakeMsgUnpack(binaryStream));
    CHK_RET(ConnInfoUnpackProc(binaryStream));
    CHK_RET(TransResUnpackProc(binaryStream));
    CHK_RET(CclBufferInfoUnpack(binaryStream));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::SendTransInfo()
{
    Hccl::BinaryStream binaryStream;
    TransResPack(binaryStream);
    binaryStream.Dump(sendTrans_);
    EXCEPTION_HANDLE_BEGIN
    socket_->SendAsync(reinterpret_cast<u8 *>(sendTrans_.data()), sendTrans_.size());
    EXCEPTION_HANDLE_END
    exchangeDataSize_ = sendTrans_.size();
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::RecvTransInfo()
{
    recvTrans_.resize(exchangeDataSize_);
    EXCEPTION_HANDLE_BEGIN
    socket_->RecvAsync(reinterpret_cast<u8 *>(recvTrans_.data()), recvTrans_.size());
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::RecvTransInfoProcess()
{
    Hccl::BinaryStream binaryStream(recvTrans_);
    TransResUnpackProc(binaryStream);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::HandshakeMsgPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << attr_.handshakeMsg;
    HCCL_INFO("[CcuTransport][%s] start pack handshakeMsg, attr.handshakeMsg.size[%zu]",
        __func__, attr_.handshakeMsg.size());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::ConnInfoPack(Hccl::BinaryStream &binaryStream) const
{
    std::vector<char> dtoData{};
    CHK_RET(ccuConnection_->Serialize(dtoData));
    binaryStream << dtoData;
    HCCL_INFO("[CcuTransport][%s] start pack connInfo, dtoData.size[%u]", __func__, dtoData.size());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::TransResPack(Hccl::BinaryStream &binaryStream)
{
    const uint32_t locCkesSize = locRes_.ckes.size();
    binaryStream << locCkesSize;
    const uint32_t locCkeSize = locRes_.ckes.size();
    for (uint32_t i = 0; i < locCkeSize; i++) {
        binaryStream << locRes_.ckes[i];
    }

    const uint32_t locXnsSize = locRes_.xns.size();
    binaryStream << locXnsSize;
    for (uint32_t i = 0; i < locRes_.xns.size(); i++) {
        binaryStream << locRes_.xns[i];
    }
    HCCL_INFO("Send ckesSize[%u], xnsSize[%u]", locCkesSize, locXnsSize);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::CclBufferInfoPack(Hccl::BinaryStream &binaryStream) const
{
    locCclBufInfo_.Pack(binaryStream);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::HandshakeMsgUnpack(Hccl::BinaryStream &binaryStream)
{
    binaryStream >> rmtHandshakeMsg_;

    if (attr_.handshakeMsg.size() != rmtHandshakeMsg_.size()) {
        HCCL_ERROR("handshakeMsg size=%u is not equal to rmt=%u",
            attr_.handshakeMsg.size(), rmtHandshakeMsg_.size());
        return HcclResult::HCCL_E_INTERNAL;
    }
    HCCL_INFO("[CcuTransport][%s] start unpack handshakeMsg", __func__);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::ConnInfoUnpackProc(Hccl::BinaryStream &binaryStream) const
{
    std::vector<char> dtoData{};
    binaryStream >> dtoData;
    CHK_RET(ccuConnection_->Deserialize(dtoData));
    HCCL_INFO("[CcuTransport][%s] start unpack connInfo, dtoData.size[%u]", __func__, dtoData.size());
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::TransResUnpackProc(Hccl::BinaryStream &binaryStream)
{
    uint32_t resSize{0};
    binaryStream >> resSize;
    rmtRes_.ckes.clear();
    for (uint32_t i = 0; i < resSize; i++) {
        uint32_t cke{0};
        binaryStream >> cke;
        rmtRes_.ckes.push_back(cke);
    }
    HCCL_INFO("Recv ckesSize[%u]", resSize);

    binaryStream >> resSize;
    rmtRes_.xns.clear();
    for (uint32_t i = 0; i < resSize; i++) {
        uint32_t xn{0};
        binaryStream >> xn;
        rmtRes_.xns.push_back(xn);
    }
    HCCL_INFO("Recv xnsSize[%u]", resSize);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::CclBufferInfoUnpack(Hccl::BinaryStream &binaryStream)
{
    rmtCclBufInfo_.Unpack(binaryStream);
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::SendFinish()
{
    sendFinishMsg_ = std::vector<char>(FINISH_MSG, FINISH_MSG + FINISH_MSG_SIZE);
    EXCEPTION_HANDLE_BEGIN
    socket_->SendAsync(reinterpret_cast<u8 *>(sendFinishMsg_.data()), FINISH_MSG_SIZE);
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::RecvFinish()
{
    recvFinishMsg_.resize(FINISH_MSG_SIZE);
    EXCEPTION_HANDLE_BEGIN
    socket_->RecvAsync(reinterpret_cast<u8 *>(recvFinishMsg_.data()), FINISH_MSG_SIZE);
    EXCEPTION_HANDLE_END
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::CheckFinish()
{
    const std::string sendFinishMsgStr(sendFinishMsg_.begin(), sendFinishMsg_.end());
    const std::string recvFinishMsgStr(recvFinishMsg_.begin(), recvFinishMsg_.end());
    if (sendFinishMsgStr != recvFinishMsgStr) {
        HCCL_ERROR("[CcuTransport][RecvFinish]msgRecv[%s] and msgSend[%s] are not equal",
            recvFinishMsgStr.c_str(), sendFinishMsgStr.c_str());
        return HcclResult::HCCL_E_INTERNAL;
    }

    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::ReleaseTransRes()
{
    for (uint32_t i = 0; i < ckesRes_.size(); i++) {
        if (ckesRes_[i].empty()) {
            continue;
        }
        auto ret = CcuDevMgrImp::ReleaseCke(devLogicId_, dieId_, ckesRes_[i]);
        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[CcuTransport][%s] release ckes failed but passed, "
                "devLogicId[%d] dieId[%u].", __func__, devLogicId_, dieId_);
        }
    }
    ckesRes_.clear();

    for (uint32_t i = 0; i < xnsRes_.size(); i++) {
        if (xnsRes_[i].empty()) {
            continue;
        }
        auto ret = CcuDevMgrImp::ReleaseXn(devLogicId_, dieId_, xnsRes_[i]);
        if (ret != HcclResult::HCCL_SUCCESS) {
            HCCL_ERROR("[CcuTransport][%s] release xns failed but passed, "
                "devLogicId[%d] dieId[%u].", __func__, devLogicId_, dieId_);
        }
    }
    xnsRes_.clear();

    return HcclResult::HCCL_SUCCESS;
}

uint32_t CcuTransport::GetDieId() const
{
    return dieId_;
}

uint32_t CcuTransport::GetChannelId() const
{
    return ccuConnection_->GetChannelId();
}

HcclResult CcuTransport::GetLocCkeByIndex(const uint32_t index, uint32_t &locCkeId) const
{
    CHK_PRT_RET(locRes_.ckes.empty(),
        HCCL_ERROR("[CcuTransport][%s] failed, local resources is empty.",
            __func__),
        HcclResult::HCCL_E_PARA);

    CHK_PRT_RET(index >= locRes_.ckes.size(),
        HCCL_ERROR("[CcuTransport][%s] failed, index[%u] is larger than size[%u].",
            __func__, index, locRes_.ckes.size()),
        HcclResult::HCCL_E_PARA);

    locCkeId = locRes_.ckes[index];
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::GetLocXnByIndex(const uint32_t index, uint32_t &locXnId) const
{
    CHK_PRT_RET(locRes_.xns.empty(),
        HCCL_ERROR("[CcuTransport][%s] failed, local resources is empty.",
            __func__),
        HcclResult::HCCL_E_PARA);

    CHK_PRT_RET(index >= locRes_.xns.size(),
        HCCL_ERROR("[CcuTransport][%s] failed, index[%u] is larger than size[%u].",
            __func__, index, locRes_.xns.size()),
        HcclResult::HCCL_E_PARA);

    locXnId = locRes_.xns[index];
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::GetRmtCkeByIndex(const uint32_t index, uint32_t &rmtCkeId) const
{
    CHK_PRT_RET(rmtRes_.ckes.empty(),
        HCCL_ERROR("[CcuTransport][%s] failed, local resources is empty.",
            __func__),
        HcclResult::HCCL_E_PARA);

    CHK_PRT_RET(index >= rmtRes_.ckes.size(),
        HCCL_ERROR("[CcuTransport][%s] failed, index[%u] is larger than size[%u].",
            __func__, index, rmtRes_.ckes.size()),
        HcclResult::HCCL_E_PARA);

    rmtCkeId = rmtRes_.ckes[index];
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::GetRmtXnByIndex(const uint32_t index, uint32_t &rmtXnId) const
{
    CHK_PRT_RET(rmtRes_.xns.empty(),
        HCCL_ERROR("[CcuTransport][%s] failed, local resources is empty.",
            __func__),
        HcclResult::HCCL_E_PARA);
    
    CHK_PRT_RET(index >= rmtRes_.xns.size(),
        HCCL_ERROR("[CcuTransport][%s] failed, index[%u] is larger than size[%u].",
            __func__, index, rmtRes_.xns.size()),
        HcclResult::HCCL_E_PARA);

    rmtXnId = rmtRes_.xns[index];
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuTransport::GetLocBuffer(CclBufferInfo &bufferInfo, const uint32_t &bufNum) const
{
    (void)bufNum;
    bufferInfo = locCclBufInfo_;
    return HCCL_SUCCESS;
}

HcclResult CcuTransport::GetRmtBuffer(CclBufferInfo &bufferInfo, const uint32_t &bufNum) const
{
    (void)bufNum;
    bufferInfo = rmtCclBufInfo_;
    return HCCL_SUCCESS;
}

HcclResult CcuTransport::GetCkeNum(uint32_t &ckeNum) const
{
    ckeNum = locRes_.ckes.size();
    return HcclResult::HCCL_SUCCESS;
}

CcuTransport::~CcuTransport()
{
    (void)ReleaseTransRes();
}

std::string CcuTransport::Describe() const
{
    std::string description = "";

    description = Hccl::StringFormat("DieId: %u, ", dieId_);
    description += transStatus_.Describe();
    description += Hccl::StringFormat(", LocRes: {%u Ckes, %u Xns}, ", locRes_.ckes.size(),
                                locRes_.xns.size());
    description += Hccl::StringFormat("RmtRes: {%u Ckes, %u Xns}, ", rmtRes_.ckes.size(),
                                rmtRes_.xns.size());
    description += Hccl::StringFormat("CkesRes size: %u, ", ckesRes_.size());
    description += Hccl::StringFormat("XnsRes size: %u.", xnsRes_.size());
    return description;
}

void CcuTransport::Clean()
{
    transStatus_ = TransStatus::INIT;
    sendData_.clear();
    ccuConnection_->Clean();
}

} // namespace hcomm