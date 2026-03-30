/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../endpoints/endpoint.h"
#include "aicpu_res_package_helper.h"
#include "hcomm_c_adpt.h"
#include "exception_handler.h"

// Orion
#include "exchange_rdma_buffer_dto.h"
#include "dev_capability.h"
#include "aicpu_ts_roce_channel.h"
#include "../../../common/orion_adpt_utils.h"
#include "../../../sockets/socket_mgr.h"

namespace hcomm {

constexpr uint16_t DEFAULT_LISTENING_PORT = 60001;

AicpuTsRoceChannel::AicpuTsRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc), engine_(engine)
{
    channelType_ = ChannelType::AICPU_TS_ROCE_CHANNEL;
}

AicpuTsRoceChannel::~AicpuTsRoceChannel() {}

HcclResult AicpuTsRoceChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    CHK_PTR_NULL(endpointHandle_);
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);

    // 2. 从 channelDesc_，获得 remoteEp_
    remoteEp_ = channelDesc_.remoteEndpoint;

    // 3. 初始化 socketMgr_
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[AicpuTsRoceChannel::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__, reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[AicpuTsRoceChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(static_cast<HcclResult>(HcommEndpointStartListen(endpointHandle_, port, nullptr)));
    HCCL_INFO("[AicpuTsRoceChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] socket ptr is NULL, rebuild Socket", __func__);

    CHK_PRT_RET((localEp_.loc.locType != ENDPOINT_LOC_TYPE_HOST) || (remoteEp_.loc.locType != ENDPOINT_LOC_TYPE_HOST),
        HCCL_ERROR("[AicpuTsRoceChannel::BuildSocket] EndpointLoc type is not HOST"), HCCL_E_INTERNAL);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AicpuTsRoceChannel::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[AicpuTsRoceChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, port, socketTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    HCCL_INFO("[AicpuTsRoceChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildConnection()
{
    std::unique_ptr<DevRdmaConnection> conn;
    EXECEPTION_CATCH(
        conn = std::make_unique<DevRdmaConnection>(socket_, rdmaHandle_),
        return HCCL_E_INTERNAL);
    CHK_PTR_NULL(conn);
    CHK_RET(conn->Init());
    Hccl::QpInfo& qpInfo = conn->GetQpInfo();
    qpInfo.serviceLevel = channelDesc_.roceAttr.sl;
    qpInfo.trafficClass = channelDesc_.roceAttr.tc;
    qpInfo.retryCnt = channelDesc_.roceAttr.retryCnt;
    qpInfo.retryInterval = channelDesc_.roceAttr.retryInterval;
    HCCL_INFO("[AicpuTsRoceChannel::BuildConnection] QpInfo: serviceLevel[%u], trafficClass[%u], retryCnt[%u], retryInterval[%u].", 
        qpInfo.serviceLevel, qpInfo.trafficClass, qpInfo.retryCnt, qpInfo.retryInterval);
    connections_.emplace_back(std::move(conn));
    connNum_ = connections_.size();
    HCCL_INFO("[AicpuTsRoceChannel::%s] connection num [%u]", __func__, connNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildNotify()
{
    if (engine_ == COMM_ENGINE_AIV) {
        return HCCL_SUCCESS;
    }

    notifyNum_ = channelDesc_.notifyNum;
    CHK_PRT_RET(notifyNum_ != RDMA_NOTIFY_NUM,
        HCCL_ERROR("[AicpuTsRoceChannel::%s] rdma notify num false, actual num [%u], expected num [%u]",
            __func__, notifyNum_, RDMA_NOTIFY_NUM),
        HCCL_E_PARA);

    localNotifies_.clear();
    bool devUsed = true;
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        std::unique_ptr<Hccl::RdmaLocalNotify> notifyPtr = nullptr;
        EXECEPTION_CATCH(
            notifyPtr = std::make_unique<Hccl::RdmaLocalNotify>(rdmaHandle_, devUsed),
            return HCCL_E_PTR
        );
        localNotifies_.emplace_back(std::move(notifyPtr));
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] notify num [%u]", __func__, notifyNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildBuffer()
{
    if (channelDesc_.exchangeAllMems) {
        // Get memHandles from endpoint
        HCCL_INFO("[AicpuTsRoceChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalRdmaRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(HcommMemGetAllMemHandles(
            endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AicpuTsRoceChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalRdmaRmaBuffer> &localRdmaBuffer = memHandles[i];
            HCCL_INFO("[AicpuTsRoceChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag().c_str());
            localRmaBuffers_.emplace_back(localRdmaBuffer.get());
        }
    } else {
        // 从 channelDesc 的 memHandle，获得 localRmaBuffers_
        HCCL_INFO("[AicpuTsRoceChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_PTR_NULL(channelDesc_.memHandles);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            CHK_PTR_NULL(channelDesc_.memHandles[i]);
            auto *localRdmaBuffer = reinterpret_cast<Hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
            HCCL_INFO("[AicpuTsRoceChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag().c_str());
            localRmaBuffers_.emplace_back(localRdmaBuffer);
        }
    }
    bufferNum_ = localRmaBuffers_.size();
    HCCL_INFO("[AicpuTsRoceChannel::%s] buffer num [%u]", __func__, bufferNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildNotifyValueBuffer()
{
    u32 notifysize = Hccl::DevCapability::GetInstance().GetNotifySize();
    EXECEPTION_CATCH((notifyValueMem_ = std::make_shared<Hccl::DevBuffer>(notifysize)),
        return HCCL_E_PTR);
    HCCL_DEBUG("create notify value buffer[%p], size[%u]", notifyValueMem_, notifysize);
    u64 notifyValue = 1; // notify值写1表示record
    Hccl::HrtMemcpy(reinterpret_cast<void *>(notifyValueMem_->GetAddr()), notifyValueMem_->GetSize(), &notifyValue, notifysize,
            Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);
    EXECEPTION_CATCH((notifyValueBuffer_ = std::make_unique<Hccl::LocalRdmaRmaBuffer>(notifyValueMem_, rdmaHandle_)),
        return HCCL_E_PTR);
    HCCL_INFO("[AicpuTsRoceChannel::%s] build notify value buffer success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Init()
{
    CHK_RET(ParseInputParam());
    if (channelDesc_.exchangeAllMems) {
        CHK_RET(StartListen());
    }
    CHK_RET(BuildSocket());
    CHK_RET(BuildConnection());
    CHK_RET(BuildNotify());
    CHK_RET(BuildBuffer());
    CHK_RET(BuildNotifyValueBuffer());
    return HCCL_SUCCESS;
}

// 当前AICPU和框架没有改为返回错误码形式，所有暂时使用该方法转换
ChannelStatus AicpuTsRoceChannel::GetStatus()
{
    ChannelStatus status;
    HcclResult ret = GetStatus(status);
    if (ret != HCCL_SUCCESS && ret != HCCL_E_AGAIN) {
        HCCL_ERROR("[AicpuTsRoceChannel::GetStatus] get status exception occurred, HcclResult=[%d]", ret);
        return ChannelStatus::FAILED;
    }
    return status;
}

HcclResult AicpuTsRoceChannel::GetStatus(ChannelStatus &status) {
    switch (rdmaStatus_) {
        case RdmaStatus::INIT:
            // 检查socket状态
            CHK_RET(CheckSocketStatus());
            break;
        case RdmaStatus::SOCKET_OK:
            // 准备资源
            CHK_RET(CreateQp());
            rdmaStatus_ = RdmaStatus::QP_CREATED;
            break;
        case RdmaStatus::QP_CREATED:
            // 发送交换数据
            CHK_RET(ExchangeData());
            rdmaStatus_ = RdmaStatus::DATA_EXCHANGE;
            break;
        case RdmaStatus::DATA_EXCHANGE:
            CHK_RET(ModifyQp());
            rdmaStatus_ = RdmaStatus::QP_MODIFIED;
        case RdmaStatus::QP_MODIFIED:
            // ljy: Prepare Rqe
        default:
            rdmaStatus_ = RdmaStatus::CONN_OK;
            channelStatus_ = ChannelStatus::READY;
    }

    status = channelStatus_;
    switch (channelStatus_) {
        case ChannelStatus::READY:
            return HCCL_SUCCESS;
        case ChannelStatus::SOCKET_TIMEOUT:
            return HCCL_E_ROCE_CONNECT;
        default:
            return HCCL_E_AGAIN;
    }
}

HcclResult AicpuTsRoceChannel::CheckSocketStatus() {
    CHK_PTR_NULL(socket_);
    Hccl::SocketStatus socketStatus = socket_->GetStatus(); // socket状态机
    HCCL_DEBUG("[AicpuTsRoceChannel::CheckSocketStatus] socket status = %s", socketStatus.Describe().c_str());
    if (socketStatus == Hccl::SocketStatus::OK) {
        rdmaStatus_ = RdmaStatus::SOCKET_OK;
        channelStatus_ = ChannelStatus::SOCKET_OK;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        channelStatus_ = ChannelStatus::SOCKET_TIMEOUT;
    }
    return HCCL_SUCCESS;
}

// 准备资源（创建QP）
HcclResult AicpuTsRoceChannel::CreateQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        HcclResult ret = conn->CreateQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] all connections resources connected.", __func__);
    return HCCL_SUCCESS;
}

// 交换数据
HcclResult AicpuTsRoceChannel::ExchangeData()
{
    HCCL_INFO("[AicpuTsRoceChannel::%s] Start to SendExchangeData, notifyNum=%u, bufferNum=%u, connNum=%u",
        __func__, notifyNum_, bufferNum_, connNum_);

    // 同步数据打包
    Hccl::BinaryStream binaryStream;
    NotifyVecPack(binaryStream);
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
        HCCL_ERROR("[AicpuTsRoceChannel::%s] Send sendSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannel::%s] Send size[%llu] of data success. [%llu] bytes sent.",
        __func__, sendSize, sizeof(sendSize));
        
    // 同步接收数据包尺寸
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(&recvSize), sizeof(recvSize)),
        HCCL_ERROR("[AicpuTsRoceChannel::%s] Recv recvSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannel::%s] Receive size[%llu] of data success. [%llu] bytes received.",
        __func__, recvSize, sizeof(recvSize));

    // 同步发送数据
    CHK_PRT_RET(!socket_->Send(reinterpret_cast<void *>(sendData.data()), sendSize),                
        HCCL_ERROR("[AicpuTsRoceChannel::%s] Send exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannel::%s] Send Exchange Data success. [%llu] bytes sent.",
        __func__, sendSize);

    // 同步接收数据
    HCCL_INFO("[AicpuTsRoceChannel::%s] Start to Receive Exchange Data", __func__);
    recvData.resize(recvSize);
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(recvData.data()), recvSize),
        HCCL_ERROR("[AicpuTsRoceChannel::%s] Recv exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannel::%s] Receive Exchange Data success. [%llu] bytes received.",
        __func__, recvSize);
    EXCEPTION_HANDLE_END

    // 同步数据解包
    Hccl::BinaryStream recvBinStream(recvData);
    CHK_RET(NotifyVecUnpack(recvBinStream));
    CHK_RET(RmtBufferVecUnpackProc(recvBinStream));
    CHK_RET(ConnVecUnpackProc(recvBinStream));
    
    HCCL_INFO("[AicpuTsRoceChannel::%s] Unpack exchange Data success. ", __func__);    
    return HCCL_SUCCESS;
}
 
void AicpuTsRoceChannel::NotifyVecPack(Hccl::BinaryStream &binaryStream)
{
    if (engine_ == COMM_ENGINE_AIV) {
        return;
    }

    binaryStream << notifyNum_;
    HCCL_INFO("start pack notifyVec");
    u32 pos = 0;
    for (auto &it : localNotifies_) {
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
        dto->Serialize(binaryStream);
        HCCL_INFO("pack notify pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
}
 
HcclResult AicpuTsRoceChannel::BufferVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << bufferNum_;
    HCCL_INFO("[AicpuTsRoceChannel::%s] start to pack RmaBuffers", __func__);
    u32 pos = 0;
    for (auto &it : localRmaBuffers_) {
        binaryStream << pos;
        if (it != nullptr) { // 非空的buffer，从buffer中获取 dto
            std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
            dto->Serialize(binaryStream);
            HCCL_INFO("pack buffer pos=%u dto %s", pos, dto->Describe().c_str());
        } else { // 空的buffer，dto所有字段为0(size=0)
            Hccl::ExchangeRdmaBufferDto exchangeDto;
            exchangeDto.Serialize(binaryStream);
            HCCL_INFO("pack buffer pos=%u, dto is null %s", pos, exchangeDto.Describe().c_str());
        }
        pos++;
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] pack RmaBuffers finish", __func__);
    return HCCL_SUCCESS;
}
 
HcclResult AicpuTsRoceChannel::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << connNum_;
    HCCL_INFO("[AicpuTsRoceChannel::%s] start to pack connections", __func__);
    u32 pos = 0;
    for (auto &it : connections_) {
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = nullptr;
        CHK_RET(it->GetExchangeDto(dto));
        dto->Serialize(binaryStream);
        HCCL_INFO("pack connection pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] pack connections finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtNum;
    binaryStream >> rmtNum;
 
    HCCL_INFO("[AicpuTsRoceChannel::%s] bufferNum_=%u, rmtNum=%u", __func__, bufferNum_, rmtNum);
 
    rmtRmaBuffers_.resize(rmtNum);
    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        if (pos >= rmtNum) {
            HCCL_ERROR("[AicpuTsRoceChannel::%s] pos=%u out of range (rmtNum=%u)", __func__, pos, rmtNum);
            return HCCL_E_INTERNAL;
        }
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);

        HCCL_INFO("[AicpuTsRoceChannel::%s] pos=%u, dto %s", __func__, pos, dto.Describe().c_str());
        EXECEPTION_CATCH(rmtRmaBuffers_[pos] = std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto),
            HCCL_ERROR("[AicpuTsRoceChannel::%s] make_unique<Hccl::RemoteRdmaRmaBuffer> throws an exception!", __func__);
            return HCCL_E_INTERNAL);
        HCCL_INFO("[AicpuTsRoceChannel::%s] pos=%u, rmtRmaBuffer=%s", __func__, pos, rmtRmaBuffers_[pos]->Describe().c_str());
    }
 
    return HCCL_SUCCESS;
}
 
HcclResult AicpuTsRoceChannel::NotifyVecUnpack(Hccl::BinaryStream &binaryStream)
{
    if (engine_ == COMM_ENGINE_AIV) {
        return HCCL_SUCCESS;
    }

    uint32_t notifySize = 0;
    binaryStream >> notifySize;
    if (notifySize != notifyNum_) {
        HCCL_ERROR("[AicpuTsRoceChannel::NotifyVecUnpack] rmtNum=%u is not equal to localNum=%u", notifySize, notifyNum_);
        return HCCL_E_ROCE_CONNECT;
    }
    remoteNotifies_.clear();
    u32 pos = 0;
    for (pos = 0; pos < notifySize; pos++) {
        u32 pos;
        binaryStream >> pos;
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);
        HCCL_INFO("unpack  pos=%u, dto %s", pos, dto.Describe().c_str());
        remoteNotifies_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto));
        HCCL_WARNING("unpack notify pos=%u, rmtRmaBuffer=%s", pos, remoteNotifies_.back()->Describe().c_str());
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtConnNum;
    binaryStream >> rmtConnNum;
    HCCL_INFO("start unpack conn, connNum=%u, rmtConnNum=%u", connNum_, rmtConnNum);
    if (connNum_ != rmtConnNum) {
        HCCL_ERROR("connNum=%u is not equal to rmtConnNum=%u", connNum_, rmtConnNum);
        return HCCL_E_ROCE_CONNECT;
    }

    for (u32 i = 0; i < rmtConnNum; i++) {
        u32 pos;
        binaryStream >> pos;
        rmtConnDto_.Deserialize(binaryStream);
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::ModifyQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        CHK_RET(conn->ParseRmtExchangeDto(rmtConnDto_));
        HcclResult ret = conn->ModifyQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[AicpuTsRoceChannel::%s] all connections resources modify success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetLocNotifyInfo(Notify** notify)
{
    // 目前仅用于aiv模式，无notify
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetRmtNotifyInfo(Notify** notify)
{
    // 目前仅用于aiv模式，无notify
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetRmtBufInfo(ProtectionInfo** protectionInfoPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (bufferNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannel::%s] No Remote memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    if (protectionInfoPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < bufferNum_; i++) {
        auto& rmtRmaBuffer = rmtRmaBuffers_[i];
        ProtectionInfo protectionInfo;
        protectionInfo.type = 0; // 0-RDMA 1-URMA
        protectionInfo.addr = static_cast<uint64_t>(rmtRmaBuffer->GetAddr());
        protectionInfo.length = rmtRmaBuffer->GetSize();
        protectionInfo.RdmaMemProtectionInfo.rkey = rmtRmaBuffer->GetRkey();
        rmtBufProtecInfoList_.emplace_back(protectionInfo);
        HCCL_INFO("[AicpuTsRoceChannel::%s] rmtBuf[addr[%p], size[%lu]]", 
            __func__, protectionInfo.addr, protectionInfo.addr);
    }
    *protectionInfoPtr = rmtBufProtecInfoList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetLocBufInfo(ProtectionInfo** protectionInfoPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (bufferNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannel::%s] No local memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    if (protectionInfoPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < bufferNum_; i++) {
        auto& locRmaBuffer = localRmaBuffers_[i];
        ProtectionInfo protectionInfo;
        protectionInfo.type = 0; // 0-RDMA 1-URMA
        protectionInfo.addr = static_cast<uint64_t>(locRmaBuffer->GetAddr());
        protectionInfo.length = locRmaBuffer->GetSize();
        protectionInfo.RdmaMemProtectionInfo.lkey = locRmaBuffer->GetLkey();
        locBufProtecInfoList_.emplace_back(protectionInfo);
        HCCL_INFO("[AicpuTsRoceChannel::%s] locBuf[addr[%p], size[%lu]]", 
            __func__, protectionInfo.addr, protectionInfo.addr);
    }
    *protectionInfoPtr = locBufProtecInfoList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetSqContext(SqContext** sqContextPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (connNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannel::%s] No conn available", __func__);
        return HCCL_SUCCESS;
    }

    if (sqContextPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < connNum_; i++) {
        auto &conn = connections_[i];
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        SqContext sqContext;
        CHK_RET(conn->BuildSqContext(&sqContext));
        sqContextList_.emplace_back(sqContext);
    }
    *sqContextPtr = sqContextList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::BuildAndGetCqContext(CqContext** cqContextPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (connNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannel::%s] No conn available", __func__);
        return HCCL_SUCCESS;
    }

    if (cqContextPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < connNum_; i++) {
        auto &conn = connections_[i];
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        CqContext cqContext;
        CHK_RET(conn->BuildCqContext(&cqContext));
        cqContextList_.emplace_back(cqContext);
    }
    *cqContextPtr = cqContextList_.data();
    return HCCL_SUCCESS;
}

std::string AicpuTsRoceChannel::Describe() const
{
    std::string msg = "AicpuTsRoceChannel{";
    msg += Hccl::StringFormat("notifyNum:%u, localNotifies:[", notifyNum_);
    for (auto& notify : localNotifies_) {
        msg += notify->Describe();
        msg += ", ";
    }
    msg += "]";
    msg += Hccl::StringFormat(", bufferNum:%u, localRmaBuffers:[", bufferNum_);
    for (auto& buf : localRmaBuffers_) {
        msg += buf->Describe();
        msg += ", ";
    }
    msg += "]";
    msg += Hccl::StringFormat(", connNum:%u, connections:[", connNum_);
    for (auto& conn : connections_) {
        msg += conn->Describe();
        msg += ", ";
    }
    msg += "]";
    msg += Hccl::StringFormat(", rdmaHandle:%p, %s, ", rdmaHandle_, channelStatus_.Describe().c_str());
    msg += socket_->Describe();
    msg += "}";
    return msg;
}

std::vector<char> AicpuTsRoceChannel::GetLocalNotifyUniqueIds() const
{
    HCCL_INFO("start packing local notify uniqueIds");
    std::vector<char> result(0);
    for (auto &it : localNotifies_) {
        HCCL_INFO("AicpuTsRoceChannel local notify %s", it->Describe().c_str());
        auto uniqueId = it->GetUniqueId();
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetRemoteNotifyUniqueIds() const
{
    HCCL_INFO("start packing remote notify uniqueIds");
    std::vector<char> result(0);
    Hccl::BinaryStream binaryStream;
    for (auto &it : remoteNotifies_) {
        std::vector<char> uniqueId;
        uniqueId = GetSingleRmaBufferUniqueId(
            static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetRkey());
        HCCL_INFO("AicpuTsRoceChannel remote notify %s", it->Describe().c_str());
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetNotifyValueBufferUniqueIds() const
{
    HCCL_INFO("start packing notify value buffer uniqueIds");
    std::vector<char> uniqueId;
    uniqueId = GetSingleRmaBufferUniqueId(
        static_cast<uint64_t>(notifyValueBuffer_->GetAddr()), notifyValueBuffer_->GetSize(), notifyValueBuffer_->GetLkey());
    HCCL_INFO("AicpuTsRoceChannel notify value buffer %s", notifyValueBuffer_->Describe().c_str());
    return uniqueId;
}

std::vector<char> AicpuTsRoceChannel::GetSingleRmaBufferUniqueId(u64 addr, u64 size, u32 key) const
{
    Hccl::BinaryStream binaryStream;
    binaryStream << addr;
    binaryStream << size;
    binaryStream << key;
    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetRmtBufferUniqueIds() const
{
    HCCL_INFO("start packing remote buffer uniqueIds");
    std::vector<char> result(0);
    for (auto &it : rmtRmaBuffers_) {
        std::vector<char> uniqueId;
        if (it != nullptr) {
            uniqueId = GetSingleRmaBufferUniqueId(
                static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetRkey());
            HCCL_INFO("AicpuTsRoceChannel::GetRmtBufferUniqueIds, %s", it->Describe().c_str());
        } else {
            uniqueId = GetSingleRmaBufferUniqueId(0, 0, 0); // 填充一个空的buffer
            HCCL_INFO("AicpuTsRoceChannel::GetRmtBufferUniqueIds, null buffer");
        }
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetLocBufferUniqueIds() const
{
    HCCL_INFO("start packing local buffer uniqueIds");
    std::vector<char> result(0);
    for (auto &it : localRmaBuffers_) {
        std::vector<char> uniqueId;
        if (it != nullptr) {
            uniqueId = GetSingleRmaBufferUniqueId(
                static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetLkey());
            HCCL_INFO("AicpuTsRoceChannel::GetLocBufferUniqueIds, %s", it->Describe().c_str());
        } else {
            uniqueId = GetSingleRmaBufferUniqueId(0, 0, 0); // 填充一个空的buffer
            HCCL_INFO("AicpuTsRoceChannel::GetLocBufferUniqueIds, null buffer");
        }
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetConnUniqueIds() const
{
    HCCL_INFO("start packing all conn uniqueIds");
    std::vector<char> result(0);
    for (auto &it : connections_) {
        HCCL_INFO("AicpuTsRoceChannel %s", it->Describe().c_str());
        auto uniqueId = it->GetUniqueId();
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannel::GetUniqueId() const
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannel::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
    }
    Hccl::BinaryStream binaryStream;
    binaryStream << notifyNum_;
    binaryStream << bufferNum_;
    binaryStream << connNum_;
 
    auto locNotifyUniqueIds = GetLocalNotifyUniqueIds();
    binaryStream << locNotifyUniqueIds;

    auto rmtNotifyUniqueIds = GetRemoteNotifyUniqueIds();
    binaryStream << rmtNotifyUniqueIds;

    auto notifyValueBufferUniqueIds = GetNotifyValueBufferUniqueIds();
    binaryStream << notifyValueBufferUniqueIds;
 
    auto locBufferUniqueIds = GetLocBufferUniqueIds();
    binaryStream << locBufferUniqueIds;
 
    auto rmtBufferUniqueIds = GetRmtBufferUniqueIds();
    binaryStream << rmtBufferUniqueIds;
 
    auto connUniqueIds = GetConnUniqueIds();
    binaryStream << connUniqueIds;
 
    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

static HcclResult SetModuleDataName(Hccl::ModuleData &module, const std::string &name)
{
    int ret = strcpy_s(module.name, sizeof(module.name), name.c_str());
    if (ret != 0) {
        HCCL_ERROR("[SetModuleDataName] strcpy_s name %s failed", name.c_str());
        return HCCL_E_INTERNAL;
    }

    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::PackOpData(std::vector<char> &data) const
{
    std::vector<Hccl::ModuleData> dataVec;
    dataVec.resize(Hccl::AicpuResMgrType::__COUNT__);

    Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM;
    CHK_RET(SetModuleDataName(dataVec[resType], "AicpuTsRoceChannel"));

    std::vector<char> result;
    Hccl::BinaryStream      binaryStream;
    binaryStream << GetUniqueId();

    binaryStream.Dump(result);

    dataVec[resType].data = result;

    Hccl::AicpuResPackageHelper helper;
    data = helper.GetPackedData(dataVec);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::H2DResPack(std::vector<char>& buffer)
{
    CHK_RET(PackOpData(buffer));
    HCCL_INFO("[AicpuTsRoceChannel][%s] Pack Buffer data[%p], Pack Buffer size[%zu].",
        __func__, buffer.data(), buffer.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = (engine_ == COMM_ENGINE_AIV) ? 0 : notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::GetBufferNum(uint32_t *bufferNum) const
{
    CHK_PTR_NULL(bufferNum);
    *bufferNum = bufferNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::GetQpNum(uint32_t *qpNum) const
{
    CHK_PTR_NULL(qpNum);
    *qpNum = connNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags)
{
    CHK_PRT_RET(remoteMem == nullptr, HCCL_ERROR("[AicpuTsRoceChannel::%s] remoteMem is nullptr", __func__), HCCL_E_PTR);
    CHK_PRT_RET(memNum == nullptr, HCCL_ERROR("[AicpuTsRoceChannel::%s] memNum is nullptr", __func__), HCCL_E_PTR);
    *memNum = 0;

    uint32_t totalCount = rmtRmaBuffers_.size();
    if (totalCount == 0) {
        HCCL_INFO("[AicpuTsRoceChannel::%s] No remote memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    for (uint32_t i = 0; i < totalCount; i++) {
        auto& rmtRmaBuffer = rmtRmaBuffers_[i];
        std::unique_ptr<HcclMem> hcclMem{};
        EXECEPTION_CATCH(hcclMem = std::make_unique<HcclMem>(), return HCCL_E_PARA);
        
        hcclMem->type = rmtRmaBuffer->GetMemType();
        hcclMem->addr = reinterpret_cast<void *>(rmtRmaBuffer->GetAddr());
        hcclMem->size = rmtRmaBuffer->GetSize();
        memTags[i] = const_cast<char*>(rmtRmaBuffer->GetMemTag().c_str());
        remoteMem[i] = hcclMem.get();
        HCCL_INFO("[AicpuTsRoceChannel::%s] rmtBuf[addr[%p], size[%lu]]", 
            __func__, remoteMem[i]->addr, remoteMem[i]->size);
        remoteMems.emplace_back(std::move(hcclMem));
    }

    *memNum = totalCount;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Clean()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannel::Resume()
{
    return HCCL_SUCCESS;
}
} // namespace hcomm


