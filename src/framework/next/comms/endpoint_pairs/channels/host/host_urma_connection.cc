/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "host_urma_connection.h"
#include "dtype_common.h"
#include "orion_adapter_rts.h"
#include "exchange_ub_conn_dto.h"
#include "hccp.h"
#include "sal.h"

namespace hcomm {
constexpr u32 WAIT_US_COUNT = 1000;
constexpr uint32_t TC_TEMP = 132;
constexpr uint32_t SL_TEMP = 4;
constexpr uint32_t RETRY_CNT_TEMP = 7;
constexpr uint32_t RETRY_TIME_TEMP = 20;

HostUrmaConnection::HostUrmaConnection(Hccl::Socket *socket, RdmaHandle rdmaHandle):
    socket_(socket), rdmaHandle_(rdmaHandle) {}

HcclResult HostUrmaConnection::Init()
{
    if (urmaConnStatus_ != UrmaConnStatus::CLOSED) {
        HCCL_INFO("[HostUrmaConnection][%s] status[%s] is not need init.",
            __func__, urmaConnStatus_.Describe().c_str());
        return HCCL_SUCCESS;
    }

    int qpMode = 0;
    DevType devType;
    CHK_RET(hrtGetDeviceType(devType));
    if (devType == DevType::DEV_TYPE_910_95) {
        qpMode = Hccl::OPBASE_QP_MODE;
    } else {
        HCCL_ERROR("Cannot support this device type!"
                   "errNo[0x%016llx], device type[%d]",
                   HCCL_ERROR_CODE(HcclResult::HCCL_E_NOT_SUPPORT), devType);
        return HCCL_E_NOT_SUPPORT;
    }
    qpInfo_.qpMode = qpMode;
    qpInfo_.rdmaHandle = rdmaHandle_;
    urmaConnStatus_ = UrmaConnStatus::INIT;
    return HCCL_SUCCESS;
}


HostUrmaConnection::~HostUrmaConnection()
{
    if (urmaConnStatus_ == UrmaConnStatus::CLOSED || urmaConnStatus_ == UrmaConnStatus::INIT) {
        return;
    }
    HcclResult ret = DestroyQp();
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[%s]call DestroyQp failed: hcclRet -> %d", __func__, ret);
    }
}

std::string HostUrmaConnection::Describe() const
{
    return Hccl::StringFormat("HostUrmaConnection[status=%s]", urmaConnStatus_.Describe().c_str());
}

HcclResult HostUrmaConnection::CreateQp()
{
    if (socket_->GetStatus() != Hccl::SocketStatus::OK) {
        HCCL_WARNING("[HostUrmaConnection::CreateQp] socket status is not ok, please");
        return HCCL_E_AGAIN;
    }

    // 创建receive & send channel，用于poll cq，避免软件一直轮询cq
    HCCL_INFO("HostUrmaConnection CreateCompChannel");
    s32 ret = RaCreateCompChannel(qpInfo_.rdmaHandle, &sendCompChannel_);
    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[HostUrmaConnection::CreateQp][CreateSendCompChannel]errNo[0x%016llx] RaCreateCompChannel fail. "
        "return[%d], params: rdmaHandle[%p], sendCompChannel[%p]",
        HCCL_ERROR_CODE(HCCL_E_NETWORK), ret, qpInfo_.rdmaHandle, &sendCompChannel_),
        HCCL_E_NETWORK);
    ret = RaCreateCompChannel(qpInfo_.rdmaHandle, &recvCompChannel_);
    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[HostUrmaConnection::CreateQp][CreateReceiveCompChannel]errNo[0x%016llx] RaCreateCompChannel fail. "
        "return[%d], params: rdmaHandle[%p], rcvCompChannel[%p]",
        HCCL_ERROR_CODE(HCCL_E_NETWORK), ret, qpInfo_.rdmaHandle, &recvCompChannel_),
        HCCL_E_NETWORK);
    
    // 创建CQ和QP
    // qp创建时不指定srq/srq cq/srq context，由qp创建时创建独立的sq和rq，并创建对应的cq
    // cq for sq句柄保存在qpInfo_.sendCq中; cq for rq句柄保存在qpInfo_.receiveCq变量中
    HCCL_INFO("HostUrmaConnection CreateCqAndQp");
    CHK_RET(Hccl::HrtRaCreateQpWithCq(qpInfo_.rdmaHandle, -1, -1, sendCompChannel_,
        recvCompChannel_, qpInfo_, isHdcMode_));
    urmaConnStatus_ = UrmaConnStatus::QP_CREATED;
    return HCCL_SUCCESS;
}

HcclResult HostUrmaConnection::DestroyQp()
{
    if (urmaConnStatus_ == UrmaConnStatus::CLOSED || urmaConnStatus_ == UrmaConnStatus::INIT) {
        return HCCL_SUCCESS;
    }

    CHK_RET(Hccl::HrtRaUbDestroyJetty(jettyHandle));
    urmaConnStatus_ = UrmaConnStatus::CLOSED;
    return HCCL_SUCCESS;
}

HcclResult HostUrmaConnection::GetExchangeDto(std::unique_ptr<Hccl::Serializable> &locQpAttrserial)
{
    if (urmaConnStatus_ != UrmaConnStatus::QP_CREATED && urmaConnStatus_ != UrmaConnStatus::QP_MODIFIED) {
        HCCL_ERROR("[HostUrmaConnection][%s] status[%s] is not expected.",
            __func__, urmaConnStatus_.Describe().c_str());
        return HCCL_E_AGAIN;
    }

    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpInfo_.qpHandle, &localQpAttr);
    if (ret != 0) {
        HCCL_ERROR("[HostUrmaConnection::GetExchangeDto]RaGetQpAttr failed, ret(%d)", ret);
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

HcclResult HostUrmaConnection::ParseRmtExchangeDto(const Hccl::Serializable &rmtQpAttrSerial)
{
    auto dto = dynamic_cast<const ExchangeRdmaConnDto &>(rmtQpAttrSerial);
    HCCL_INFO("[HostUrmaConnection][%s] remoteConnDto[%s]", __func__, dto.Describe().c_str());
    rmtQpAttr_.psn = dto.psn_;
    rmtQpAttr_.qpn = dto.qpn_;
    rmtQpAttr_.gid_idx = dto.gid_idx_;
    CHK_SAFETY_FUNC_RET(memcpy_s(rmtQpAttr_.gid, HCCP_GID_RAW_LEN, dto.gid_, HCCP_GID_RAW_LEN));
    return HCCL_SUCCESS;
}

HcclResult HostUrmaConnection::ModifyQp()
{
    if (urmaConnStatus_ == UrmaConnStatus::QP_MODIFIED) {
        HCCL_WARNING("[HostUrmaConnection][%s] modify qp already, status[%s].",
                     __func__, urmaConnStatus_.Describe().c_str());
        return HCCL_SUCCESS;
    } 
    if (urmaConnStatus_ != UrmaConnStatus::QP_CREATED) {
        HCCL_ERROR("[HostUrmaConnection][%s] status[%s] is not expected.", __func__,
            urmaConnStatus_.Describe().c_str());
        return HCCL_E_AGAIN;
    }

    if (!rmtQpAttr_.IsValid()) {
        HCCL_ERROR("[HostUrmaConnection][%s] romate Qp Attr is empty, exchange qp attr first", __func__);
        return HCCL_E_INTERNAL;
    }

    struct QpAttr localQpAttr;
    s32 ret = RaGetQpAttr(qpInfo_.qpHandle, &localQpAttr);
    if (ret != 0) {
        return HCCL_E_ROCE_CONNECT;
    }

    struct TypicalQp localQp;
    struct TypicalQp rmtQp;
    localQp.sl = SL_TEMP;
    localQp.tc = TC_TEMP;
    localQp.retryCnt = RETRY_CNT_TEMP;
    localQp.retryTime = RETRY_TIME_TEMP;
    localQp.qpn = localQpAttr.qpn;
    localQp.psn = localQpAttr.psn;
    localQp.gidIdx = localQpAttr.gidIdx;
    (void)memcpy_s(localQp.gid, HCCP_GID_RAW_LEN, localQpAttr.gid, HCCP_GID_RAW_LEN);
    rmtQp.qpn = rmtQpAttr_.qpn;
    rmtQp.psn = rmtQpAttr_.psn;
    rmtQp.gidIdx = rmtQpAttr_.gid_idx;
    (void)memcpy_s(rmtQp.gid, HCCP_GID_RAW_LEN, rmtQpAttr_.gid, HCCP_GID_RAW_LEN);
    ret = RaTypicalQpModify(qpInfo_.qpHandle, &localQp, &rmtQp);
    if (ret != 0) {
        HCCL_ERROR("[modify][ra_qp]modify qp failed, ret(%d)", ret);
        return HCCL_E_ROCE_CONNECT;
    }
    urmaConnStatus_ = UrmaConnStatus::QP_MODIFIED;
    return HCCL_SUCCESS;
}


} // namespace Hccl