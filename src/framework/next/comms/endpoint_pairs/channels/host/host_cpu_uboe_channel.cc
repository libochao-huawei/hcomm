/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "host_cpu_uboe_channel.h"
#include "adapter_rts_common.h"
#include "endpoint.h"
#include "exception_handler.h"
#include "exchange_ub_buffer_dto.h"
#include "local_ub_rma_buffer.h"
#include "hcomm_adapter_urma.h"

namespace hcomm {
HostCpuUboeChannel::HostCpuUboeChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc)
    : endpointHandle_(endpointHandle),
      channelDesc_(channelDesc)
{
}

HostCpuUboeChannel::~HostCpuUboeChannel()
{
    if (socket_ != nullptr) {
        SocketMgr::GetInstance(devicePhyId_).PutSocket(socketConfig_, socket_);
        socket_ = nullptr;
    }
}

HcclResult HostCpuUboeChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_
    CHK_PTR_NULL(endpointHandle_);
    HCCL_INFO("[HostCpuUboeChannel][%s] Start. endpointHandle[0x%llx]", __func__,
        reinterpret_cast<uint64_t>(endpointHandle_));
    Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    uint8_t portNum = localEpPtr->GetPortNum();
    portCtxs_.resize(portNum);
    for (uint8_t i = 0; i < portNum; i++) {
        portCtxs_[i].rdmaHandle_ = localEpPtr->GetRdmaHandleByPortIdx(i);
    }

    localEpDesc_ = localEpPtr->GetEndpointDesc();

    // 2. 从 channelDesc_，获得 remoteEp_, socket_
    remoteEpDesc_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);

    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[HostCpuUboeChannel::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__,
        reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = Hccl::DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuUboeChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(static_cast<HcclResult>(HcommEndpointStartListen(endpointHandle_, port, nullptr)));
    HCCL_INFO("[HostCpuUboeChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::BuildSocket() // 需要解决ip和eid的转换问题
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HostCpuUboeChannel::%s] socket ptr is NULL, rebuild Socket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEpDesc_, remoteEpDesc_, linkData));
    HCCL_INFO("[HostCpuUboeChannel::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = Hccl::DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuUboeChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, port, socketTag);
    CHK_RET(SocketMgr::GetInstance(devicePhyId_).GetSocket(socketConfig, socket_));
    HCCL_INFO("[HostCpuUboeChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::BuildConnection()
{
    for (auto &portCtx : portCtxs_) {
        // 需要为每个端口创建一个连接
        std::unique_ptr<HostUrmaConnection> conn;
        EXECEPTION_CATCH(
            conn = std::make_unique<HostUrmaConnection>(socket_, portCtx.rdmaHandle_), return HCCL_E_INTERNAL);
        CHK_RET(conn->Init());
        portCtx.connection_ = std::move(conn);
    }
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::BuildBuffer()
{
    if (!channelDesc_.exchangeAllMems) { // true for HIXL, false for HCCL
        // 3. 从 channelDesc 的 memHandle，UBOE暂不支持HCCL
        HCCL_ERROR("[HostCpuUboeChannel][BuildBuffer] exchangeAllMems == false.");
        return HCCL_E_NOT_SUPPORT;
    }
    HCCL_INFO("[HostCpuUboeChannel][BuildBuffer] exchangeAllMems == True. Get memHandles from endpoint.");

    std::shared_ptr<Hccl::LocalUbAggregatedRmaBuffer> *memHandles = nullptr;
    uint32_t memHandleNum = 0;
    CHK_RET(static_cast<HcclResult>(
        HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void **>(&memHandles), &memHandleNum)));
    HCCL_INFO("[HostCpuUboeChannel][BuildBuffer] Got memHandleNum[%u].", memHandleNum);
    for (uint32_t i = 0; i < memHandleNum; ++i) {
        std::shared_ptr<Hccl::LocalUbAggregatedRmaBuffer> &localRdmaBuffer = memHandles[i];
        HCCL_INFO("[HostCpuUboeChannel][BuildBuffer] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].", i,
            localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag().c_str());
        localRmaBuffers_.emplace_back(localRdmaBuffer);
    }

    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::Init()
{
    s32 devLogicId;
    CHK_RET(hrtGetDevice(&devLogicId));
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(devLogicId), devicePhyId_));
    CHK_RET(ParseInputParam());
    if (channelDesc_.exchangeAllMems) { // true for HIXL, false for HCCL
        CHK_RET(StartListen());
    }
    CHK_RET(BuildSocket());
    CHK_RET(BuildConnection());
    CHK_RET(BuildBuffer());
    CHK_RET(DlUrmaFunction::GetInstance().DlUrmaFunctionInit());
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = 0;
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    HCCL_ERROR("[HostCpuUboeChannel][GetRemoteMem] not supported.");
    return HCCL_E_NOT_SUPPORT;
}

ChannelStatus HostCpuUboeChannel::GetStatus()
{
    ChannelStatus status;
    HcclResult ret = GetStatus(status);
    if (ret != HCCL_SUCCESS && ret != HCCL_E_AGAIN) {
        HCCL_ERROR("[HostCpuUboeChannel::GetStatus] get status exception occurred, HcclResult=[%d]", ret);
        return ChannelStatus::FAILED;
    }
    return status;
}

HcclResult HostCpuUboeChannel::GetStatus(ChannelStatus &status)
{
    switch (rdmaStatus_) {
        case RdmaStatus::INIT:
            // 检查socket状态
            CHK_RET(CheckSocketStatus());
            break;
        case RdmaStatus::SOCKET_OK:
            // 准备资源
            CHK_RET(CreateJetty());
            rdmaStatus_ = RdmaStatus::JETTY_CREATED;
            break;
        case RdmaStatus::JETTY_CREATED:
            // 发送交换数据
            CHK_RET(ExchangeData());
            rdmaStatus_ = RdmaStatus::DATA_EXCHANGE;
            break;
        case RdmaStatus::DATA_EXCHANGE:
            CHK_RET(ImportJetty());
            rdmaStatus_ = RdmaStatus::JETTY_IMPORTED;
        case RdmaStatus::JETTY_IMPORTED:
            // 暂时仅支持Read/Write，不需要下发rqe
        default:
            rdmaStatus_ = RdmaStatus::CONN_OK;
            channelStatus_ = ChannelStatus::READY;
    }

    status = channelStatus_;
    switch (channelStatus_) {
        case ChannelStatus::READY:
            if (socket_ != nullptr) {
                SocketMgr::GetInstance(devicePhyId_).PutSocket(socketConfig_, socket_);
                socket_ = nullptr;
            }
            return HCCL_SUCCESS;
        case ChannelStatus::SOCKET_TIMEOUT:
            return HCCL_E_NETWORK;
        default:
            return HCCL_E_AGAIN;
    }
}

HcclResult HostCpuUboeChannel::CheckSocketStatus()
{
    CHK_PTR_NULL(socket_);
    Hccl::SocketStatus socketStatus = socket_->GetStatus(); // socket状态机
    HCCL_DEBUG("[HostCpuUboeChannel::CheckSocketStatus] socket status = %s", socketStatus.Describe().c_str());
    if (socketStatus == Hccl::SocketStatus::OK) {
        rdmaStatus_ = RdmaStatus::SOCKET_OK;
        channelStatus_ = ChannelStatus::SOCKET_OK;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        channelStatus_ = ChannelStatus::SOCKET_TIMEOUT;
    }
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::CreateJetty()
{
    for (auto &portCtx : portCtxs_) {
        CHK_PTR_NULL(portCtx.connection_);
        CHK_RET(portCtx.connection_->CreateJetty());
    }

    HCCL_INFO("[HostCpuUboeChannel::IsResReady] all connections resources connected.");
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::ExchangeData()
{
    HCCL_INFO("[HostCpuUboeChannel::%s] Start to SendExchangeData, bufferNum=%u, portNum=%u", __func__,
        localRmaBuffers_.size(), portCtxs_.size());

    // 同步数据打包
    Hccl::BinaryStream binaryStream;
    CHK_RET(BufferVecPack(binaryStream));
    CHK_RET(ConnVecPack(binaryStream));

    std::vector<char> sendData{};
    binaryStream.Dump(sendData);
    uint64_t sendSize = sendData.size();
    std::vector<char> recvData{};
    uint64_t recvSize = 0;

    EXCEPTION_HANDLE_BEGIN
    // 同步发送数据包尺寸
    CHK_PRT_RET(!socket_->Send(reinterpret_cast<void *>(&sendSize), sizeof(sendSize)),
        HCCL_ERROR("[HostCpuUboeChannel::%s] Send sendSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuUboeChannel::%s] Send size[%llu] of data success. [%llu] bytes sent.", __func__, sendSize,
        sizeof(sendSize));

    // 同步接收数据包尺寸
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(&recvSize), sizeof(recvSize)),
        HCCL_ERROR("[HostCpuUboeChannel::%s] Recv recvSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuUboeChannel::%s] Receive size[%llu] of data success. [%llu] bytes received.", __func__, recvSize,
        sizeof(recvSize));

    // 同步发送数据
    CHK_PRT_RET(!socket_->Send(reinterpret_cast<void *>(sendData.data()), sendSize),
        HCCL_ERROR("[HostCpuUboeChannel::%s] Send exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuUboeChannel::%s] Send Exchange Data success. [%llu] bytes sent.", __func__, sendSize);

    // 同步接收数据
    HCCL_INFO("[HostCpuUboeChannel::%s] Start to Receive Exchange Data", __func__);
    recvData.resize(recvSize);
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(recvData.data()), recvSize),
        HCCL_ERROR("[HostCpuUboeChannel::%s] Recv exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuUboeChannel::%s] Receive Exchange Data success. [%llu] bytes received.", __func__, recvSize);
    EXCEPTION_HANDLE_END

    // 同步数据解包
    Hccl::BinaryStream recvBinStream(recvData);
    CHK_RET(RmtBufferVecUnpackProc(recvBinStream));
    CHK_RET(ConnVecUnpackProc(recvBinStream));

    HCCL_INFO("[HostCpuUboeChannel::%s] Unpack exchange Data success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::BufferVecPack(Hccl::BinaryStream &binaryStream)
{
    u32 bufferNum = localRmaBuffers_.size();
    binaryStream << bufferNum;
    HCCL_INFO("[HostCpuUboeChannel::%s] start to pack RmaBuffers", __func__);
    u32 pos = 0;
    for (auto &it : localRmaBuffers_) {
        binaryStream << pos;
        if (it != nullptr) { // 非空的buffer，从buffer中获取 dto
            std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
            if (dto == nullptr) {
                return HCCL_E_INTERNAL;
            }
            dto->Serialize(binaryStream);
            HCCL_INFO("pack buffer pos=%u dto %s", pos, dto->Describe().c_str());
        } else { // 空的buffer，dto所有字段为0(size=0)
            Hccl::ExchangeUbBufferDto exchangeDto;
            exchangeDto.Serialize(binaryStream);
            HCCL_INFO("pack buffer pos=%u, dto is null %s", pos, exchangeDto.Describe().c_str());
        }
        pos++;
    }
    HCCL_INFO("[HostCpuUboeChannel::%s] pack RmaBuffers finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    u8 portNum = portCtxs_.size();
    binaryStream << portNum;
    HCCL_INFO("[HostCpuUboeChannel::%s] start to pack connections", __func__);
    u32 pos = 0;
    for (auto &portCtx : portCtxs_) {
        if (portCtx.connection_ == nullptr) {
            HCCL_ERROR("[HostCpuUboeChannel::%s] portCtx.connection_ is null", __func__);
            return HCCL_E_PTR;
        }
        
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = nullptr;
        CHK_RET(portCtx.connection_->GetExchangeDto(dto));
        dto->Serialize(binaryStream);
        HCCL_INFO("pack connection pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
    HCCL_INFO("[HostCpuUboeChannel::%s] pack connections finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtNum;
    binaryStream >> rmtNum;

    HCCL_INFO("[HostCpuUboeChannel::%s] bufferNum=%u, rmtNum=%u", __func__, localRmaBuffers_.size(), rmtNum);

    std::vector<RdmaHandle> rdmaHandles;
    rdmaHandles.reserve(portCtxs_.size());
    for (auto &portCtx : portCtxs_) {
        rdmaHandles.push_back(portCtx.rdmaHandle_);
    }

    remoteRmaBuffers_.resize(rmtNum);
    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        if (pos >= rmtNum) {
            HCCL_ERROR("[HostCpuUboeChannel::%s] pos=%u out of range (rmtNum=%u)", __func__, pos, rmtNum);
            return HCCL_E_INTERNAL;
        }
        Hccl::ExchangeUbBufferDto dto;
        dto.Deserialize(binaryStream);

        HCCL_INFO("[HostCpuUboeChannel::%s] pos=%u, dto %s", __func__, pos, dto.Describe().c_str());
        EXECEPTION_CATCH(remoteRmaBuffers_[pos] = std::make_unique<Hccl::RemoteUbAggregatedRmaBuffer>(rdmaHandles, dto),
            HCCL_ERROR("[HostCpuUboeChannel::%s] make_unique<Hccl::RemoteUbAggregatedRmaBuffer> throws an exception!",
                __func__);
            return HCCL_E_INTERNAL);
        HCCL_INFO("[HostCpuUboeChannel::%s] pos=%u, remoteRmaBuffers_=%s", __func__, pos,
            remoteRmaBuffers_[pos]->Describe().c_str());
    }

    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u8 portNum = portCtxs_.size();
    u8 rmtPortNum;
    binaryStream >> rmtPortNum;
    HCCL_INFO("start unpack port num, portNum=%u, rmtPortNum=%u", portNum, rmtPortNum);
    if (portNum != rmtPortNum) {
        HCCL_ERROR("portNum=%u is not equal to rmtPortNum=%u", portNum, rmtPortNum);
        return HCCL_E_INTERNAL;
    }

    u32 pos;
    for (uint8_t i = 0; i < rmtPortNum; i++) {
        binaryStream >> pos;
        if (pos != i) {
            HCCL_ERROR(
                "[HostCpuUboeChannel::%s] pos=%u is not equal to i=%u (rmtPortNum=%u)", __func__, pos, i, rmtPortNum);
            return HCCL_E_INTERNAL;
        }
        portCtxs_[i].rmtConnDto_.Deserialize(binaryStream);
    }

    HCCL_INFO("[HostCpuUboeChannel::%s] unpack connections finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuUboeChannel::ImportJetty()
{
    for (auto &portCtx : portCtxs_) {
        CHK_RET(portCtx.connection_->ImportJetty(portCtx.rmtConnDto_));
    }
    HCCL_INFO("[HostCpuUboeChannel::IsResReady] all connections resources connected.");
    return HCCL_SUCCESS;
}

std::string HostCpuUboeChannel::Describe() const
{
    std::string msg = "HostCpuUboeChannel{";
    msg += Hccl::StringFormat("bufferNum:%u, localRmaBuffers: [", localRmaBuffers_.size());
    for (auto &buf : localRmaBuffers_) {
        if (buf == nullptr)
        {
            HCCL_WARNING("[HostCpuUboeChannel::%s] localRmaBuffers_ contains a null pointer", __func__);
            continue;
        }
        
        msg += buf->Describe();
        msg += ", ";
    }
    msg += Hccl::StringFormat("], portNum:%u, portCtxs:[", portCtxs_.size());
    for (auto &portCtx : portCtxs_) {
        if (portCtx.connection_ == nullptr) {
            HCCL_WARNING("[HostCpuUboeChannel::%s] portCtx.connection_ is null", __func__);
            continue;
        }
        msg += portCtx.connection_->Describe();
        msg += Hccl::StringFormat("], rdmaHandle: %p", portCtx.rdmaHandle_);
    }

    msg += Hccl::StringFormat("], rdmaStatus: %s, ", rdmaStatus_.Describe().c_str());
    if (socket_ == nullptr) {
        HCCL_WARNING("[HostCpuUboeChannel::%s] socket_ is null", __func__);
    } else {
        msg += socket_->Describe();
    }
    msg += ", ";
    return msg;
}

} // namespace hcomm