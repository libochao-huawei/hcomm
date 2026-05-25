/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include "host_cpu_uboe_connection.h"
#include "rdma_handle_manager.h"
#include "rma_conn_exception.h"
#include "exception_util.h"

using namespace Hccl;

namespace hcomm {

constexpr u32 OPBASED_UB_SQ_DEPTH_MAX = 8192;
// constexpr u32 UB_SQ_OFFLOAD_DEPTH     = 128;
constexpr u32 UB_SQ_WQEBB_SIZE        = 64;
// constexpr u32 UB_MAX_TRANS_SIZE       = 256 * 1024 * 1024; // UB单次最大传输量256*1024*1024 Byte
constexpr u32 WQE_NUM_PER_SQE         = 4; // URMA约束每个SQE包含4个WQEBB
HostCpuUboeConnection::HostCpuUboeConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle):
    socket_(socket), rdmaHandle_(rdmaHandle)
{
    tpProtocol_ = TpProtocol::UBOE;
}

HcclResult HostCpuUboeConnection::Init()
{
    if (urmaConnStatus_ != UrmaConnStatus::CLOSED) {
        HCCL_INFO("[HostCpuUboeConnection][%s] status[%s] is not need init.",
            __func__, urmaConnStatus_.Describe().c_str());
        return HCCL_SUCCESS;
    }

    urmaConnStatus_ = UrmaConnStatus::INIT;
    return HCCL_SUCCESS;
}

HostCpuUboeConnection::~HostCpuUboeConnection()
{
    if (urmaConnStatus_ == UrmaConnStatus::CLOSED || urmaConnStatus_ == UrmaConnStatus::INIT) {
        return;
    }

    DECTOR_TRY_CATCH("HostCpuUboeConnection", ReleaseResource());
}

std::string HostCpuUboeConnection::Describe() const
{
    return Hccl::StringFormat("HostCpuUboeConnection[status=%s]", urmaConnStatus_.Describe().c_str());
}

HcclResult HostCpuUboeConnection::CreateJetty()
{
    if (rdmaHandle_ == nullptr) {
        THROW<InvalidParamsException>("[HostCpuUboeConnection][CreateJetty]rdmaHandle_ is nullptr, please check input.");
    }

    if (socket_->GetStatus() != Hccl::SocketStatus::OK) {
        HCCL_WARNING("[HostCpuUboeConnection::CreateJetty] socket status is not ok, please");
        return HCCL_E_AGAIN;
    }

    // 创建JFC
    HCCL_INFO("HostCpuUboeConnection CreateJfc");
    jfcHandle_ = RdmaHandleManager::GetInstance().GetJfcHandle(rdmaHandle_, cqInfo_, HrtUbJfcMode::NORMAL);

    sqDepth_ = OPBASED_UB_SQ_DEPTH_MAX;
    u32 size = static_cast<u32>(sqDepth_) * static_cast<u32>(UB_SQ_WQEBB_SIZE) * static_cast<u32>(WQE_NUM_PER_SQE);
    TokenIdHandle tokenIdHandle = RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle_).first;
    HrtRaUbCreateJettyParam req {
        jfcHandle_, jfcHandle_,
        GetUbToken(), tokenIdHandle,
        HrtJettyMode::STANDARD,
        0, // HOST展开与AICPU展开传入jetty id为0，申请一个新的jetty
        0, // va由底层分配，此处填0即可。
        size, 0, sqDepth_}; // 非CCUv2不需要填写sqeBufIndex

    // 创建Jetty
    repJetty_ = HrtRaUbCreateJetty(rdmaHandle_, req);
    keySize_      = repJetty_.keySize;
    jettyHandle_ = repJetty_.handle;
    jettyVa_ = repJetty_.jettyVa;

    urmaConnStatus_ = UrmaConnStatus::JETTY_CREATED;
    return HCCL_SUCCESS;
}

void HostCpuUboeConnection::ReleaseResource()
{
    if (urmaConnStatus_ == UrmaConnStatus::CLOSED || urmaConnStatus_ == UrmaConnStatus::INIT) {
        return;
    }

    if (rdmaHandle_ && remoteJettyHandle_ != 0) {
        HrtRaUbUnimportJetty(rdmaHandle_, remoteJettyHandle_);
        remoteJettyHandle_ = 0;
    }

    if (jettyHandle_ != 0) {
        HrtRaUbDestroyJetty(jettyHandle_);
        jettyHandle_ = 0;
    }
    urmaConnStatus_ = UrmaConnStatus::CLOSED;
}

void HostCpuUboeConnection::ParseRmtExchangeDto(const ExchangeUbConnDto &rmtDto)
{
    HCCL_INFO("[HostCpuUboeConnection][%s] remoteConnDto[%s]", __func__, rmtDto.Describe().c_str());
    remoteTokenValue_ = rmtDto.tokenValue;
    errno_t ret = memcpy_s(remoteQpKey_, HRT_UB_QP_KEY_MAX_LEN, rmtDto.qpKey, HRT_UB_QP_KEY_MAX_LEN);
    if (ret != EOK) {
        HCCL_ERROR("[HostCpuUboeConnection][%s] memcpy_s failed, ret=%d", __func__, ret);
        ThrowAbnormalStatus(std::string(__func__));
    }
}

HcclResult HostCpuUboeConnection::ImportJetty(const ExchangeUbConnDto &rmtDto)
{

    if (urmaConnStatus_ == UrmaConnStatus::READY) {
        HCCL_WARNING("[HostCpuUboeConnection][%s] import jetty already, %s.",
                    __func__, Describe().c_str());
        return HCCL_SUCCESS;
    }

    if (urmaConnStatus_ != UrmaConnStatus::JETTY_CREATED) {
        HCCL_ERROR("[HostCpuUboeConnection][%s] failed, urmaConnStatus_[%s] is not expected.",
            __func__, urmaConnStatus_.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    ParseRmtExchangeDto(rmtDto);

    HrtRaUbJettyImportedInParam in{};
    in.key            = remoteQpKey_;
    in.keyLen         = keySize_;
    in.tokenValue     = remoteTokenValue_;

    if (tpProtocol_ != TpProtocol::UBOE) {
        HCCL_ERROR("[HostCpuUboeConnection][%s] failed, tp protocol[%s] is not expected, %s.",
            __func__, tpProtocol_.Describe().c_str(), Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    remOutParam_ = RaUbImportJetty(rdmaHandle_, in.key, in.keyLen, in.tokenValue);
    remoteJettyVa_ = remOutParam_.targetJettyVa;
    remoteJettyHandle_ = remOutParam_.handle;

    urmaConnStatus_ = UrmaConnStatus::READY;
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeConnection::GetExchangeDto(std::unique_ptr<Hccl::Serializable> &locQpAttrserial)
{
    if (urmaConnStatus_ != UrmaConnStatus::READY && urmaConnStatus_ != UrmaConnStatus::JETTY_CREATED) {
        HCCL_ERROR("[HostCpuUboeConnection][%s] urmaConnStatus_[%s] is not expected.", __func__,
            urmaConnStatus_.Describe().c_str());
        ThrowAbnormalStatus(std::string(__func__));
    }

    std::unique_ptr<ExchangeUbConnDto> dto
        = make_unique<ExchangeUbConnDto>(tokenValue_, keySize_, jettyImportCfg_.localTpHandle, jettyImportCfg_.localPsn);
    errno_t ret = memcpy_s(dto->qpKey, HRT_UB_QP_KEY_MAX_LEN, repJetty_.key, HRT_UB_QP_KEY_MAX_LEN);
    if (ret != EOK) {
        HCCL_ERROR("[HostCpuUboeConnection][%s] memcpy_s failed, ret=%d", __func__, ret);
        ThrowAbnormalStatus(std::string(__func__));
    }
    locQpAttrserial = std::move(dto);

    return HCCL_SUCCESS;
}

void HostCpuUboeConnection::ThrowAbnormalStatus(std::string funcName)
{
    auto errMsg = StringFormat("[HostCpuUboeConnection][%s] failed, [%s].",
        funcName.c_str(), Describe().c_str());
    urmaConnStatus_ = UrmaConnStatus::CONN_INVALID;
    THROW<RmaConnException>(errMsg);
}

u64 HostCpuUboeConnection::GetJettyVa()
{
    return jettyVa_;
}

JettyHandle HostCpuUboeConnection::GetTJettyVa()
{
    return remoteJettyVa_;
}

} // namespace hcomm
