/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ccu_urma_channel.h"

#include "hcomm_adapter_hccp.h"

#include "../../api_c_adpt/hcomm_c_adpt.h"

#include "orion_adpt_utils.h"

#include "exception_handler.h"

// 暂时引入orion
#include "local_ub_rma_buffer.h"

namespace hcomm {

CcuUrmaChannel::CcuUrmaChannel(const EndpointHandle locEndpointHandle,
    const HcommChannelDesc &channelDesc)
    : locEndpointHandle_(locEndpointHandle),
      channelDesc_(channelDesc)
{
}

static HcclResult CreateCcuTransport(UrmaEndpoint *ccuEndpoint,
    const Hccl::LinkData &linkData, Hccl::Socket *socket, void *memHandle,
    std::unique_ptr<CcuTransport> &impl)
{
    HCCL_INFO("[CcuUrmaChannel][%s] begin", __func__);
    // 当前ccu channel不支持按需申请cke
    CHK_PTR_NULL(ccuEndpoint);
    CHK_PTR_NULL(socket);
    CHK_PTR_NULL(memHandle);

    auto ret = HcclResult::HCCL_SUCCESS;
    auto *channelCtxPool = ccuEndpoint->GetCcuChannelCtxPool();
    CHK_PTR_NULL(channelCtxPool);
    ret = channelCtxPool->PrepareCreate({linkData});
    if (ret == HCCL_E_UNAVAIL) {
        HCCL_WARNING("[CcuUrmaChannel][%s] prepare ccu channel ctx failed, "
            "ccu resources unavailable.", __func__);
        return ret;
    }
    CHK_RET(ret);

    CcuChannelCtxPool::CcuChannelCtx channelCtx{};
    CHK_RET(channelCtxPool->GetChannelCtx(linkData, channelCtx));
    const auto &channelInfo = channelCtx.first;
    const auto &ccuJettys = channelCtx.second;

    const auto &locAddr_ = linkData.GetLocalAddr();
    const auto &rmtAddr_ = linkData.GetRemoteAddr();

    CommAddr locAddr{}, rmtAddr{};
    CHK_RET(IpAddressToCommAddr(locAddr_, locAddr));
    CHK_RET(IpAddressToCommAddr(rmtAddr_, rmtAddr));

    CcuTransport::CcuConnectionType type_ =
        linkData.GetLinkProtocol() == Hccl::LinkProtocol::UB_CTP ?
        CcuTransport::CcuConnectionType::UBC_CTP :
        CcuTransport::CcuConnectionType::UBC_TP;

    CcuTransport::CcuConnectionInfo connectionInfo{type_,
        locAddr, rmtAddr, channelInfo, ccuJettys};

    auto *locCclRmaBuffer = reinterpret_cast<Hccl::LocalUbRmaBuffer *>(memHandle);
    HCCL_INFO("[CcuUrmaChannel::CreateTransportByLink] locCclRmaBuffer[%s]", locCclRmaBuffer->Describe().c_str());
    auto *buffer = locCclRmaBuffer->GetBuf();
    CHK_PTR_NULL(buffer);

    const CcuTransport::CclBufferInfo locCclBufInfo {
        buffer->GetAddr(),
        static_cast<uint32_t>(buffer->GetSize()),
        locCclRmaBuffer->GetTokenId(),
        locCclRmaBuffer->GetTokenValue()
    };

    // 调用底层的创建函数 (CcuCreateTransport 通常是全局函数或静态函数)
    ret = CcuCreateTransport(socket, connectionInfo, locCclBufInfo, impl);
    if (ret == HCCL_E_UNAVAIL) {
        HCCL_WARNING("[CcuUrmaChannel][%s] failed, ccu resources unavailable.", __func__);
        return ret;
    }
    CHK_RET(ret);

    HCCL_INFO("[CcuUrmaChannel][%s] end, transport created.", __func__);
    return HCCL_SUCCESS;
}

static HcclResult CheckEndpointDesc(const EndpointDesc &locDesc, const EndpointDesc &rmtDesc)
{
    if (locDesc.protocol != rmtDesc.protocol) {
        HCCL_ERROR("[CcuUrmaChannel][%s] failed, endpoints protocols are not same, "
            "loc[%d] rmt[%d].", __func__, locDesc.protocol, rmtDesc.protocol);
        return HcclResult::HCCL_E_PARA;
    }

    if (locDesc.protocol != COMM_PROTOCOL_UBC_CTP &&
        locDesc.protocol != COMM_PROTOCOL_UBC_TP) {
        HCCL_ERROR("[CcuUrmaChannel][%s] failed, protocol[%d] are not supported in ccu.",
            __func__, locDesc.protocol);
        return HcclResult::HCCL_E_PARA;
    }
    
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::Init()
{
    EXCEPTION_HANDLE_BEGIN
    CHK_PTR_NULL(channelDesc_.socket);
    auto *socket = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);
    // 当前socket在外部统一触发connect，建议之后改为异步建链流程内触发

    CHK_PTR_NULL(locEndpointHandle_);
    void *endpoint{nullptr};
    CHK_RET(HcommEndpointGet(locEndpointHandle_, &endpoint));
    UrmaEndpoint *ccuEndpoint = dynamic_cast<UrmaEndpoint *>(static_cast<Endpoint *>(endpoint));
    CHK_PTR_NULL(ccuEndpoint);
    const auto &locEndpointDesc = ccuEndpoint->GetEndpointDesc();

    CHK_RET(CheckEndpointDesc(locEndpointDesc, channelDesc_.remoteEndpoint));

    auto linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(locEndpointDesc, channelDesc_.remoteEndpoint, linkData));

    if (channelDesc_.memHandleNum != 1) {
        HCCL_ERROR("[CcuUrmaChannel][%s] failed, unsupport memHandleNum[%u].",
            __func__, channelDesc_.memHandleNum);
        return HcclResult::HCCL_E_NOT_SUPPORT;
    }
    CHK_PTR_NULL(channelDesc_.memHandles);

    // 当前建链不支持资源扩容，CCU资源默认固定为16
    HCCL_WARNING("[CcuUrmaChannel][%s] now only support notify num is 16.",
        __func__);
    HCCL_WARNING("[CcuUrmaChannel][%s] now only support to exchange hccl buffer.",
        __func__);
    CHK_RET(CreateCcuTransport(ccuEndpoint, linkData,
        socket, channelDesc_.memHandles[0], impl_));

    hcclBufferInfoPtr_.reset(new (std::nothrow) HcclMem());
    CHK_PTR_NULL(hcclBufferInfoPtr_);
    EXCEPTION_HANDLE_END
    return HCCL_SUCCESS;
}

ChannelStatus CcuUrmaChannel::GetStatus()
{
    if (!impl_) {
        HCCL_ERROR("[CcuUrmaChannel][%s] failed, impl is nullptr.",
            __func__);
        return ChannelStatus::FAILED;
    }

    CcuTransport::TransStatus status = impl_->GetStatus();
    switch (status) {
        case CcuTransport::TransStatus::READY:
            return ChannelStatus::READY;
         case CcuTransport::TransStatus::SOCKET_TIMEOUT:
            HCCL_ERROR("[CcuUrmaChannel][%s] error status[%s].",
                __func__, status.Describe().c_str());
            return ChannelStatus::SOCKET_TIMEOUT;
        case CcuTransport::TransStatus::CONNECT_FAILED:
            HCCL_ERROR("[CcuUrmaChannel][%s] error status[%s].",
                __func__, status.Describe().c_str());
            return ChannelStatus::FAILED;
        default:
            break;
    }
    
    return ChannelStatus::INIT; // todo: AICPU 重新定义基类的状态后，需要修改为CONNECTING
}

uint32_t CcuUrmaChannel::GetDieId() const
{
    if (!impl_) {
        return UINT32_MAX;
    }

    return impl_->GetDieId();
}

uint32_t CcuUrmaChannel::GetChannelId() const
{
    if (!impl_) {
        return UINT32_MAX; 
    }
    return impl_->GetChannelId();
}

HcclResult CcuUrmaChannel::GetLocCkeByIndex(const uint32_t index, uint32_t &locCkeId) const
{
    CHK_PTR_NULL(impl_);
    CHK_RET(impl_->GetLocCkeByIndex(index, locCkeId));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetLocXnByIndex(const uint32_t index, uint32_t &locXnId) const
{
    CHK_PTR_NULL(impl_);
    CHK_RET(impl_->GetLocXnByIndex(index, locXnId));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetRmtCkeByIndex(const uint32_t index, uint32_t &rmtCkeId) const
{
    CHK_PTR_NULL(impl_);
    CHK_RET(impl_->GetRmtCkeByIndex(index, rmtCkeId));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetRmtXnByIndex(const uint32_t index, uint32_t &rmtXnId) const
{
    CHK_PTR_NULL(impl_);
    CHK_RET(impl_->GetRmtXnByIndex(index, rmtXnId));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetRmtBuffer(uint64_t &addr, uint32_t &size,
    uint32_t &tokenId, uint32_t &tokenValue) const
{
    CHK_PTR_NULL(impl_);
    CcuTransport::CclBufferInfo bufInfo{};
    constexpr uint32_t bufNum = 0; // 当前不支持
    CHK_RET(impl_->GetRmtBuffer(bufInfo, bufNum));

    addr = bufInfo.addr;
    size = bufInfo.size;
    tokenId = bufInfo.tokenId;
    tokenValue = bufInfo.tokenValue;
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(impl_);
    CHK_RET(impl_->GetCkeNum(*notifyNum));
    return HcclResult::HCCL_SUCCESS;
}

HcclResult CcuUrmaChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    CHK_PTR_NULL(remoteMem);
    CHK_PTR_NULL(memNum);
    CHK_PTR_NULL(memTags);

    *remoteMem = nullptr;
    *memNum = 0;

    CHK_PTR_NULL(impl_);
    CcuTransport::CclBufferInfo bufInfo{};
    constexpr uint32_t bufNum = 0; // 当前不支持
    CHK_RET(impl_->GetRmtBuffer(bufInfo, bufNum));

    hcclBufferInfoPtr_->type = HCCL_MEM_TYPE_DEVICE;
    hcclBufferInfoPtr_->addr = reinterpret_cast<void *>(bufInfo.addr);
    hcclBufferInfoPtr_->size = static_cast<uint64_t>(bufInfo.size);

    remoteMem[0] = hcclBufferInfoPtr_.get();
    *memNum = 1;
    memTags[0] = const_cast<char *>(memTag_.c_str());
    return HcclResult::HCCL_SUCCESS;
}

}  // namespace hcomm
