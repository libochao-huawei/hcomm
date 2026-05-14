/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include "mem_transport_common.h"

// Orion
#include "exchange_rdma_buffer_dto.h"
#include "dev_capability.h"
#include "aicpu_ts_roce_channel_v2.h"
#include "../../../common/orion_adpt_utils.h"
#include "../../../sockets/socket_mgr.h"

namespace hcomm {

constexpr uint16_t DEFAULT_LISTENING_PORT = 60001;
constexpr u32 CHANNEL_ENTITY_TYPE_RDMA = 0;

AicpuTsRoceChannelV2::AicpuTsRoceChannelV2(EndpointHandle endpointHandle, HcommChannelDesc channelDesc, CommEngine engine)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc), engine_(engine)
{
}

AicpuTsRoceChannelV2::~AicpuTsRoceChannelV2()
{
    FreeDeviceMemories();
}

HcclResult AicpuTsRoceChannelV2::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    CHK_PTR_NULL(endpointHandle_);
    HCCL_INFO("[AicpuTsRoceChannelV2][%s] Start. endpointHandle[0x%llx]", __func__, reinterpret_cast<uint64_t>(endpointHandle_));
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum_
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    notifyNum_ = channelDesc_.notifyNum;

    // 3. 初始化 socketMgr_
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__, reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(static_cast<HcclResult>(HcommEndpointStartListen(endpointHandle_, port, nullptr)));
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] socket ptr is NULL, rebuild Socket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, port, socketTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildConnection()
{
    std::unique_ptr<DevRdmaConnectionV2> conn;
    EXECEPTION_CATCH(
        conn = std::make_unique<DevRdmaConnectionV2>(socket_, rdmaHandle_),
        return HCCL_E_INTERNAL);
    CHK_PTR_NULL(conn);
    CHK_RET(conn->Init());
    Hccl::QpInfo& qpInfo = conn->GetQpInfo();
    qpInfo.serviceLevel = channelDesc_.roceAttr.sl;
    qpInfo.trafficClass = channelDesc_.roceAttr.tc;
    qpInfo.retryCnt = channelDesc_.roceAttr.retryCnt;
    qpInfo.retryInterval = channelDesc_.roceAttr.retryInterval;
    HCCL_INFO("[AicpuTsRoceChannelV2::BuildConnection] QpInfo: serviceLevel[%u], trafficClass[%u], retryCnt[%u], retryInterval[%u].", 
        qpInfo.serviceLevel, qpInfo.trafficClass, qpInfo.retryCnt, qpInfo.retryInterval);
    connections_.emplace_back(std::move(conn));
    connNum_ = connections_.size();
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] connection num [%u]", __func__, connNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildNotify()
{
    if (engine_ == COMM_ENGINE_AIV) {
        return HCCL_SUCCESS;
    }

    CHK_PRT_RET(notifyNum_ != RDMA_NOTIFY_NUM,
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] rdma notify num false, actual num [%u], expected num [%u]",
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
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] notify num [%u]", __func__, notifyNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildBuffer()
{
    if (channelDesc_.exchangeAllMems) {
        // Get memHandles from endpoint
        HCCL_INFO("[AicpuTsRoceChannelV2][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalRdmaRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(HcommMemGetAllMemHandles(
            endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AicpuTsRoceChannelV2][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalRdmaRmaBuffer> &localRdmaBuffer = memHandles[i];
            HCCL_INFO("[AicpuTsRoceChannelV2][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag().c_str());
            localRmaBuffers_.emplace_back(localRdmaBuffer.get());
        }
    } else {
        // 从 channelDesc 的 memHandle，获得 localRmaBuffers_
        HCCL_INFO("[AicpuTsRoceChannelV2][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_PTR_NULL(channelDesc_.memHandles);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            CHK_PTR_NULL(channelDesc_.memHandles[i]);
            auto *localRdmaBuffer = reinterpret_cast<Hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
            HCCL_INFO("[AicpuTsRoceChannelV2][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag().c_str());
            localRmaBuffers_.emplace_back(localRdmaBuffer);
        }
    }
    bufferNum_ = localRmaBuffers_.size();
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] buffer num [%u]", __func__, bufferNum_);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildNotifyValueBuffer()
{
    if (engine_ == COMM_ENGINE_AIV) {
        return HCCL_SUCCESS;
    }
    
    u32 notifysize = Hccl::DevCapability::GetInstance().GetNotifySize();
    EXECEPTION_CATCH((notifyValueMem_ = std::make_shared<Hccl::DevBuffer>(notifysize)),
        return HCCL_E_PTR);
    HCCL_DEBUG("create notify value buffer[%p], size[%u]", notifyValueMem_.get(), notifysize);
    u64 notifyValue = 1; // notify值写1表示record
    Hccl::HrtMemcpy(reinterpret_cast<void *>(notifyValueMem_->GetAddr()), notifyValueMem_->GetSize(), &notifyValue, notifysize,
            Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);
    EXECEPTION_CATCH((notifyValueBuffer_ = std::make_unique<Hccl::LocalRdmaRmaBuffer>(notifyValueMem_, rdmaHandle_)),
        return HCCL_E_PTR);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] build notify value buffer success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::Init()
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
ChannelStatus AicpuTsRoceChannelV2::GetStatus()
{
    ChannelStatus status;
    HcclResult ret = GetStatus(status);
    if (ret != HCCL_SUCCESS && ret != HCCL_E_AGAIN) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::GetStatus] get status exception occurred, HcclResult=[%d]", ret);
        return ChannelStatus::FAILED;
    }
    return status;
}

HcclResult AicpuTsRoceChannelV2::GetStatus(ChannelStatus &status) {
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

HcclResult AicpuTsRoceChannelV2::CheckSocketStatus() {
    CHK_PTR_NULL(socket_);
    Hccl::SocketStatus socketStatus = socket_->GetStatus(); // socket状态机
    HCCL_DEBUG("[AicpuTsRoceChannelV2::CheckSocketStatus] socket status = %s", socketStatus.Describe().c_str());
    if (socketStatus == Hccl::SocketStatus::OK) {
        rdmaStatus_ = RdmaStatus::SOCKET_OK;
        channelStatus_ = ChannelStatus::SOCKET_OK;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        channelStatus_ = ChannelStatus::SOCKET_TIMEOUT;
    }
    return HCCL_SUCCESS;
}

// 准备资源（创建QP）
HcclResult AicpuTsRoceChannelV2::CreateQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannelV2::%s] failed, connection pointer is nullptr", __func__));
        HcclResult ret = conn->CreateQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] all connections resources connected.", __func__);
    return HCCL_SUCCESS;
}

// 交换数据
HcclResult AicpuTsRoceChannelV2::ExchangeData()
{
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Start to SendExchangeData, notifyNum=%u, bufferNum=%u, connNum=%u",
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
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] Send sendSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Send size[%llu] of data success. [%llu] bytes sent.",
        __func__, sendSize, sizeof(sendSize));
        
    // 同步接收数据包尺寸
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(&recvSize), sizeof(recvSize)),
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] Recv recvSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Receive size[%llu] of data success. [%llu] bytes received.",
        __func__, recvSize, sizeof(recvSize));

    // 同步发送数据
    CHK_PRT_RET(!socket_->Send(reinterpret_cast<void *>(sendData.data()), sendSize),                
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] Send exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Send Exchange Data success. [%llu] bytes sent.",
        __func__, sendSize);

    // 同步接收数据
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Start to Receive Exchange Data", __func__);
    recvData.resize(recvSize);
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(recvData.data()), recvSize),
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] Recv exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Receive Exchange Data success. [%llu] bytes received.",
        __func__, recvSize);
    EXCEPTION_HANDLE_END

    // 同步数据解包
    Hccl::BinaryStream recvBinStream(recvData);
    CHK_RET(NotifyVecUnpack(recvBinStream));
    CHK_RET(RmtBufferVecUnpackProc(recvBinStream));
    CHK_RET(ConnVecUnpackProc(recvBinStream));
    
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Unpack exchange Data success. ", __func__);    
    return HCCL_SUCCESS;
}
 
void AicpuTsRoceChannelV2::NotifyVecPack(Hccl::BinaryStream &binaryStream)
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
 
HcclResult AicpuTsRoceChannelV2::BufferVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << bufferNum_;
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] start to pack RmaBuffers", __func__);
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
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] pack RmaBuffers finish", __func__);
    return HCCL_SUCCESS;
}
 
HcclResult AicpuTsRoceChannelV2::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << connNum_;
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] start to pack connections", __func__);
    u32 pos = 0;
    for (auto &it : connections_) {
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = nullptr;
        CHK_RET(it->GetExchangeDto(dto));
        dto->Serialize(binaryStream);
        HCCL_INFO("pack connection pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] pack connections finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtNum;
    binaryStream >> rmtNum;
 
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] bufferNum_=%u, rmtNum=%u", __func__, bufferNum_, rmtNum);
 
    rmtRmaBuffers_.resize(rmtNum);
    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        if (pos >= rmtNum) {
            HCCL_ERROR("[AicpuTsRoceChannelV2::%s] pos=%u out of range (rmtNum=%u)", __func__, pos, rmtNum);
            return HCCL_E_INTERNAL;
        }
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);

        HCCL_INFO("[AicpuTsRoceChannelV2::%s] pos=%u, dto %s", __func__, pos, dto.Describe().c_str());
        EXECEPTION_CATCH(rmtRmaBuffers_[pos] = std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto),
            HCCL_ERROR("[AicpuTsRoceChannelV2::%s] make_unique<Hccl::RemoteRdmaRmaBuffer> throws an exception!", __func__);
            return HCCL_E_INTERNAL);
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] pos=%u, rmtRmaBuffer=%s", __func__, pos, rmtRmaBuffers_[pos]->Describe().c_str());
    }
 
    return HCCL_SUCCESS;
}
 
HcclResult AicpuTsRoceChannelV2::NotifyVecUnpack(Hccl::BinaryStream &binaryStream)
{
    if (engine_ == COMM_ENGINE_AIV) {
        return HCCL_SUCCESS;
    }

    uint32_t notifySize = 0;
    binaryStream >> notifySize;
    if (notifySize != notifyNum_) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::NotifyVecUnpack] rmtNum=%u is not equal to localNum=%u", notifySize, notifyNum_);
        return HCCL_E_ROCE_CONNECT;
    }
    remoteNotifies_.clear();
    u32 pos = 0;
    for (pos = 0; pos < notifySize; pos++) {
        binaryStream >> pos;
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);
        HCCL_INFO("unpack  pos=%u, dto %s", pos, dto.Describe().c_str());
        remoteNotifies_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto));
        HCCL_INFO("unpack notify pos=%u, rmtRmaBuffer=%s", pos, remoteNotifies_.back()->Describe().c_str());
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
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

HcclResult AicpuTsRoceChannelV2::ModifyQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannelV2::%s] failed, connection pointer is nullptr", __func__));
        CHK_RET(conn->ParseRmtExchangeDto(rmtConnDto_));
        HcclResult ret = conn->ModifyQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] all connections resources modify success.", __func__);
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetLocNotifyInfo(Notify** notify)
{
    // 目前仅用于aiv模式，无notify
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetRmtNotifyInfo(Notify** notify)
{
    // 目前仅用于aiv模式，无notify
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetRmtBufInfo(ProtectionInfo** protectionInfoPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (bufferNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] No Remote memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    if (protectionInfoPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < bufferNum_; i++) {
        auto& rmtRmaBuffer = rmtRmaBuffers_[i];
        ProtectionInfo protectionInfo;
        protectionInfo.type = CHANNEL_ENTITY_TYPE_RDMA;
        protectionInfo.addr = static_cast<uint64_t>(rmtRmaBuffer->GetAddr());
        protectionInfo.length = rmtRmaBuffer->GetSize();
        protectionInfo.memInfo.rdmaMemInfo.rkey = rmtRmaBuffer->GetRkey();
        rmtBufProtecInfoList_.emplace_back(protectionInfo);
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] rmtBuf[addr[%p], size[%lu]]", 
            __func__, protectionInfo.addr, protectionInfo.length);
    }
    *protectionInfoPtr = rmtBufProtecInfoList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetLocBufInfo(ProtectionInfo** protectionInfoPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (bufferNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] No local memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    if (protectionInfoPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < bufferNum_; i++) {
        auto& locRmaBuffer = localRmaBuffers_[i];
        ProtectionInfo protectionInfo;
        protectionInfo.type = CHANNEL_ENTITY_TYPE_RDMA;
        protectionInfo.addr = static_cast<uint64_t>(locRmaBuffer->GetAddr());
        protectionInfo.length = locRmaBuffer->GetSize();
        protectionInfo.memInfo.rdmaMemInfo.lkey = locRmaBuffer->GetLkey();
        locBufProtecInfoList_.emplace_back(protectionInfo);
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] locBuf[addr[%p], size[%lu]]", 
            __func__, protectionInfo.addr, protectionInfo.length);
    }
    *protectionInfoPtr = locBufProtecInfoList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetSqContext(SqContext** sqContextPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (connNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] No conn available", __func__);
        return HCCL_SUCCESS;
    }

    if (sqContextPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < connNum_; i++) {
        auto &conn = connections_[i];
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannelV2::%s] failed, connection pointer is nullptr", __func__));
        SqContext sqContext;
        CHK_RET(conn->BuildSqContext(&sqContext));
        sqContextList_.emplace_back(sqContext);
    }
    *sqContextPtr = sqContextList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetCqContext(CqContext** cqContextPtr)
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
        return HCCL_E_INTERNAL;
    }

    if (connNum_ == 0) {
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] No conn available", __func__);
        return HCCL_SUCCESS;
    }

    if (cqContextPtr == nullptr) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] input param is null", __func__);
        return HCCL_E_PARA;
    }

    for (uint32_t i = 0; i < connNum_; i++) {
        auto &conn = connections_[i];
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[AicpuTsRoceChannelV2::%s] failed, connection pointer is nullptr", __func__));
        CqContext cqContext;
        CHK_RET(conn->BuildCqContext(&cqContext));
        cqContextList_.emplace_back(cqContext);
    }
    *cqContextPtr = cqContextList_.data();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::BuildAndGetDevChannelEntity(uint64_t* devChannelEntityPtr)
{
    CHK_PTR_NULL(devChannelEntityPtr);

    if (devChannelEntityPtr_ != 0) {
        *devChannelEntityPtr = devChannelEntityPtr_;
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] already built, return cached devPtr=0x%lx", __func__, devChannelEntityPtr_);
        return HCCL_SUCCESS;
    }

    ChannelEntity hostEntity{};
    hostEntity.abiHeader.version   = HCCL_CHANNEL_VERSION;
    hostEntity.abiHeader.magicWord = HCCL_CHANNEL_MAGIC_WORD;
    hostEntity.abiHeader.size      = sizeof(ChannelEntity);
    hostEntity.abiHeader.reserved  = 0;
    hostEntity.engine   = GetCommEngine();
    hostEntity.protocol = GetCommProtocol();

    locBufProtecInfoList_.clear();
    rmtBufProtecInfoList_.clear();
    sqContextList_.clear();
    cqContextList_.clear();
    deviceMemories_.clear();

    CHK_RET(GetNotifyNum(&hostEntity.localNotifyNum));
    CHK_RET(BuildAndGetLocNotifyInfo(&hostEntity.localNotifyAddr));
    hostEntity.remoteNotifyNum = hostEntity.localNotifyNum;
    CHK_RET(BuildAndGetRmtNotifyInfo(&hostEntity.remoteNotifyAddr));

    CHK_RET(GetBufferNum(&hostEntity.localBufferNum));
    CHK_RET(BuildAndGetLocBufInfo(&hostEntity.localBufferAddr));
    hostEntity.remoteBufferNum = hostEntity.localBufferNum;
    CHK_RET(BuildAndGetRmtBufInfo(&hostEntity.remoteBufferAddr));

    CHK_RET(GetQpNum(&hostEntity.sqNum));
    CHK_RET(BuildAndGetSqContext(&hostEntity.SqContextAddr));
    hostEntity.cqNum = hostEntity.sqNum;
    CHK_RET(BuildAndGetCqContext(&hostEntity.CqContextAddr));

    hccl::DeviceMem entityDevMem = hccl::DeviceMem::alloc(sizeof(ChannelEntity));
    CHK_PRT_RET(!entityDevMem,
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] DeviceMem::alloc for ChannelEntity failed", __func__), HCCL_E_MEMORY);
    deviceMemories_.push_back(std::move(entityDevMem));
    void* entityDevPtr = deviceMemories_.back().ptr();

    ChannelEntity devEntity = hostEntity;

    CHK_RET(CopyArrayToDevice(hostEntity.localNotifyAddr, hostEntity.localNotifyNum,
                              &devEntity.localNotifyAddr, "localNotifyAddr"));
    CHK_RET(CopyArrayToDevice(hostEntity.remoteNotifyAddr, hostEntity.remoteNotifyNum,
                              &devEntity.remoteNotifyAddr, "remoteNotifyAddr"));
    CHK_RET(CopyArrayToDevice(hostEntity.localBufferAddr, hostEntity.localBufferNum,
                              &devEntity.localBufferAddr, "localBufferAddr"));
    CHK_RET(CopyArrayToDevice(hostEntity.remoteBufferAddr, hostEntity.remoteBufferNum,
                              &devEntity.remoteBufferAddr, "remoteBufferAddr"));
    CHK_RET(CopyArrayToDevice(hostEntity.SqContextAddr, hostEntity.sqNum,
                              &devEntity.SqContextAddr, "SqContextAddr"));
    CHK_RET(CopyArrayToDevice(hostEntity.CqContextAddr, hostEntity.cqNum,
                              &devEntity.CqContextAddr, "CqContextAddr"));

    Hccl::HrtMemcpy(entityDevPtr, sizeof(ChannelEntity), &devEntity, sizeof(ChannelEntity),
                     Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    *devChannelEntityPtr = reinterpret_cast<uint64_t>(entityDevPtr);
    devChannelEntityPtr_ = reinterpret_cast<uint64_t>(entityDevPtr);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] Success, devPtr=0x%lx", __func__, devChannelEntityPtr_);
    return HCCL_SUCCESS;
}

template<typename T>
HcclResult AicpuTsRoceChannelV2::CopyArrayToDevice(const T* hostArray, uint32_t arrayNum,
    T** deviceArrayPtr, const char* arrayName)
{
    if (arrayNum == 0 || hostArray == nullptr) {
        *deviceArrayPtr = nullptr;
        return HCCL_SUCCESS;
    }

    size_t arraySize = arrayNum * sizeof(T);
    hccl::DeviceMem arrayDevMem = hccl::DeviceMem::alloc(arraySize);
    CHK_PRT_RET(!arrayDevMem,
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] DeviceMem::alloc for %s failed, size=%zu", __func__, arrayName, arraySize),
        HCCL_E_MEMORY);
    void* arrayDevPtr = arrayDevMem.ptr();
    deviceMemories_.push_back(std::move(arrayDevMem));

    Hccl::HrtMemcpy(arrayDevPtr, arraySize, hostArray, arraySize,
                     Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    *deviceArrayPtr = reinterpret_cast<T*>(arrayDevPtr);
    HCCL_INFO("[AicpuTsRoceChannelV2::%s] %s: host=%p -> dev=%p, num=%u, size=%zu",
              __func__, arrayName, hostArray, arrayDevPtr, arrayNum, arraySize);
    return HCCL_SUCCESS;
}

void AicpuTsRoceChannelV2::FreeDeviceMemories()
{
    deviceMemories_.clear();
    devChannelEntityPtr_ = nullptr;
}

std::string AicpuTsRoceChannelV2::Describe() const
{
    std::string msg = "AicpuTsRoceChannelV2{";
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

std::vector<char> AicpuTsRoceChannelV2::GetLocalNotifyUniqueIds() const
{
    HCCL_DEBUG("start packing local notify uniqueIds");
    std::vector<char> result(0);
    for (auto &it : localNotifies_) {
        HCCL_INFO("AicpuTsRoceChannelV2 local notify %s", it->Describe().c_str());
        auto uniqueId = it->GetUniqueId();
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetRemoteNotifyUniqueIds() const
{
    HCCL_DEBUG("start packing remote notify uniqueIds");
    std::vector<char> result(0);
    Hccl::BinaryStream binaryStream;
    for (auto &it : remoteNotifies_) {
        std::vector<char> uniqueId;
        uniqueId = GetSingleRmaBufferUniqueId(
            static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetRkey());
        HCCL_INFO("AicpuTsRoceChannelV2 remote notify %s", it->Describe().c_str());
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    binaryStream.Dump(result);
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetNotifyValueBufferUniqueIds() const
{
    HCCL_DEBUG("start packing notify value buffer uniqueIds");
    std::vector<char> uniqueId;
    uniqueId = GetSingleRmaBufferUniqueId(
        static_cast<uint64_t>(notifyValueBuffer_->GetAddr()), notifyValueBuffer_->GetSize(), notifyValueBuffer_->GetLkey());
    HCCL_INFO("AicpuTsRoceChannelV2 notify value buffer %s", notifyValueBuffer_->Describe().c_str());
    return uniqueId;
}

std::vector<char> AicpuTsRoceChannelV2::GetSingleRmaBufferUniqueId(u64 addr, u64 size, u32 key) const
{
    Hccl::BinaryStream binaryStream;
    binaryStream << addr;
    binaryStream << size;
    binaryStream << key;
    std::vector<char> result;
    binaryStream.Dump(result);
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetRmtBufferUniqueIds() const
{
    HCCL_DEBUG("start packing remote buffer uniqueIds");
    std::vector<char> result(0);
    for (auto &it : rmtRmaBuffers_) {
        std::vector<char> uniqueId;
        if (it != nullptr) {
            uniqueId = GetSingleRmaBufferUniqueId(
                static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetRkey());
            HCCL_INFO("AicpuTsRoceChannelV2::GetRmtBufferUniqueIds, %s", it->Describe().c_str());
        } else {
            uniqueId = GetSingleRmaBufferUniqueId(0, 0, 0); // 填充一个空的buffer
            HCCL_INFO("AicpuTsRoceChannelV2::GetRmtBufferUniqueIds, null buffer");
        }
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetLocBufferUniqueIds() const
{
    HCCL_DEBUG("start packing local buffer uniqueIds");
    std::vector<char> result(0);
    for (auto &it : localRmaBuffers_) {
        std::vector<char> uniqueId;
        if (it != nullptr) {
            uniqueId = GetSingleRmaBufferUniqueId(
                static_cast<uint64_t>(it->GetAddr()), it->GetSize(), it->GetLkey());
            HCCL_INFO("AicpuTsRoceChannelV2::GetLocBufferUniqueIds, %s", it->Describe().c_str());
        } else {
            uniqueId = GetSingleRmaBufferUniqueId(0, 0, 0); // 填充一个空的buffer
            HCCL_INFO("AicpuTsRoceChannelV2::GetLocBufferUniqueIds, null buffer");
        }
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetConnUniqueIds() const
{
    HCCL_DEBUG("start packing all conn uniqueIds");
    std::vector<char> result(0);
    for (auto &it : connections_) {
        HCCL_INFO("AicpuTsRoceChannelV2 %s", it->Describe().c_str());
        auto uniqueId = it->GetUniqueId();
        result.insert(result.end(), uniqueId.begin(), uniqueId.end());
    }
    return result;
}

std::vector<char> AicpuTsRoceChannelV2::GetUniqueId() const
{
    if (channelStatus_ != ChannelStatus::READY) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] channel status[%d] is not ready[%d], please check.",
            __func__, channelStatus_, ChannelStatus::READY);
    }
    u32 type = static_cast<u32>(Hccl::TransportType::ROCE);
    Hccl::BinaryStream binaryStream;
    binaryStream << type;
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

HcclResult AicpuTsRoceChannelV2::PackOpData(std::vector<char> &data) const
{
    std::vector<Hccl::ModuleData> dataVec;
    dataVec.resize(Hccl::AicpuResMgrType::__COUNT__);

    Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM;
    CHK_RET(SetModuleDataName(dataVec[resType], "AicpuTsRoceChannelV2"));

    std::vector<char> result;
    Hccl::BinaryStream      binaryStream;
    binaryStream << GetUniqueId();

    binaryStream.Dump(result);

    dataVec[resType].data = result;

    Hccl::AicpuResPackageHelper helper;
    data = helper.GetPackedData(dataVec);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::H2DResPack(std::vector<char>& buffer)
{
    CHK_RET(PackOpData(buffer));
    HCCL_INFO("[AicpuTsRoceChannelV2][%s] Pack Buffer data[%p], Pack Buffer size[%zu].",
        __func__, buffer.data(), buffer.size());
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = (engine_ == COMM_ENGINE_AIV) ? 0 : notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::GetBufferNum(uint32_t *bufferNum) const
{
    CHK_PTR_NULL(bufferNum);
    *bufferNum = bufferNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::GetQpNum(uint32_t *qpNum) const
{
    CHK_PTR_NULL(qpNum);
    *qpNum = connNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags)
{
    std::lock_guard<std::mutex> lock(remoteMemsMutex_);
    CHK_PRT_RET(remoteMem == nullptr, HCCL_ERROR("[AicpuTsRoceChannelV2::%s] remoteMem is nullptr", __func__), HCCL_E_PTR);
    CHK_PRT_RET(memNum == nullptr, HCCL_ERROR("[AicpuTsRoceChannelV2::%s] memNum is nullptr", __func__), HCCL_E_PTR);
    *remoteMem = nullptr;
    *memNum = 0;

    uint32_t totalCount = static_cast<uint32_t>(rmtRmaBuffers_.size());
    if (totalCount == 0) {
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] No remote memory regions available", __func__);
        return HCCL_SUCCESS;
    }

    remoteMemsPtr_.reset();
    remoteMemsPtr_ = std::make_unique<HcclMem[]>(totalCount);
    CHK_PTR_NULL(remoteMemsPtr_);

    for (uint32_t i = 0; i < totalCount; i++) {
        auto& rmtRmaBuffer = rmtRmaBuffers_[i];
        remoteMemsPtr_[i].type = rmtRmaBuffer->GetMemType();
        remoteMemsPtr_[i].addr = reinterpret_cast<void *>(rmtRmaBuffer->GetAddr());
        remoteMemsPtr_[i].size = rmtRmaBuffer->GetSize();
        memTags[i] = const_cast<char*>(rmtRmaBuffer->GetMemTag().c_str());
        HCCL_INFO("[AicpuTsRoceChannelV2::%s] rmtBuf[addr[%p], size[%llu]]",
            __func__, remoteMemsPtr_[i].addr, remoteMemsPtr_[i].size);
    }

    *memNum = totalCount;
    *remoteMem = remoteMemsPtr_.get();
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::GetUserRemoteMem(CommMem **remoteMem, char ***memTags, uint32_t *memNum)
{
    std::lock_guard<std::mutex> lock(remoteMemsMutex_);
    CHK_PRT_RET(remoteMem == nullptr, HCCL_ERROR("[AicpuTsRoceChannelV2::%s] remoteMem is nullptr", __func__), HCCL_E_PTR);
    CHK_PRT_RET(memTags == nullptr, HCCL_ERROR("[AicpuTsRoceChannelV2::%s] memTags is nullptr", __func__), HCCL_E_PTR);
    CHK_PRT_RET(memNum == nullptr, HCCL_ERROR("[AicpuTsRoceChannelV2::%s] memNum is nullptr", __func__), HCCL_E_PTR);
    *remoteMem = nullptr;
    *memTags = nullptr;
    *memNum = 0;

    if (rmtRmaBuffers_.size() == 0) {
        HCCL_ERROR("[AicpuTsRoceChannelV2::%s] bufferNum is 0.", __func__);
        return HCCL_E_PARA;
    }

    uint32_t userMemCount = static_cast<uint32_t>(rmtRmaBuffers_.size()) - 1;

    if (!cacheValid_) {
        remoteUserMems_.resize(userMemCount);
        tagCopies_.clear();
        tagCopies_.reserve(userMemCount);
        tagPointers_.clear();
        tagPointers_.reserve(userMemCount);
        for (uint32_t i = 0; i < userMemCount; ++i) {
            auto& rmtBuffer = rmtRmaBuffers_[i + 1];
            if (rmtBuffer == nullptr) {
                continue;
            }
            switch (rmtBuffer->GetMemType()) {
                case HCCL_MEM_TYPE_DEVICE:
                    remoteUserMems_[i].type = COMM_MEM_TYPE_DEVICE;
                    break;
                case HCCL_MEM_TYPE_HOST:
                    remoteUserMems_[i].type = COMM_MEM_TYPE_HOST;
                    break;
                default:
                    remoteUserMems_[i].type = COMM_MEM_TYPE_INVALID;
            }
            remoteUserMems_[i].addr = reinterpret_cast<void *>(rmtBuffer->GetAddr());
            remoteUserMems_[i].size = rmtBuffer->GetSize();
            std::string tagCopy = rmtBuffer->GetMemTag();
            tagCopies_.push_back(std::move(tagCopy));
            tagPointers_.push_back(const_cast<char*>(tagCopies_.back().c_str()));
        }
        cacheValid_ = true;
    }

    *remoteMem = remoteUserMems_.data();
    *memTags = tagPointers_.data();
    *memNum = userMemCount;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::Clean()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::Resume()
{
    return HCCL_SUCCESS;
}

HcclResult AicpuTsRoceChannelV2::Serialize(std::shared_ptr<hccl::DeviceMem> &out)
{
    out.reset();
    CHK_PRT_RET(channelStatus_ != ChannelStatus::READY,
        HCCL_ERROR("[AicpuTsRoceChannelV2][%s] channel not ready, status[%d]", __func__, channelStatus_),
        HCCL_E_INTERNAL);

    std::vector<char> hostBuffer;
    CHK_RET(H2DResPack(hostBuffer));

    u64 totalBytes = static_cast<u64>(hostBuffer.size());
    CHK_PRT_RET(totalBytes == 0,
        HCCL_ERROR("[AicpuTsRoceChannelV2][%s] serialized buffer is empty", __func__),
        HCCL_E_INTERNAL);

    hccl::DeviceMem devMem;
    EXECEPTION_CATCH(devMem = hccl::DeviceMem::alloc(totalBytes), return HCCL_E_PTR);

    Hccl::HrtMemcpy(devMem.ptr(), totalBytes, hostBuffer.data(), totalBytes,
        Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    out = std::make_shared<hccl::DeviceMem>(std::move(devMem));

    HCCL_INFO("[AicpuTsRoceChannelV2][%s] serialize success, size[%llu]", __func__, totalBytes);
    return HCCL_SUCCESS;
}

HcommChannelKind AicpuTsRoceChannelV2::GetChannelKind() const
{
    return HcommChannelKind::AICPU_TS_ROCE_V2;
}
} // namespace hcomm


