/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "host_cpu_roce_channel.h"
#include "../../../endpoints/endpoint.h"
#include "dpu_notify/dpu_notify_manager.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"
#include "exception_handler.h"

// Orion
#include "orion_adapter_hccp.h"
#include "exchange_rdma_buffer_dto.h"
#include "rdma_handle_manager.h"
#include "exchange_rdma_conn_dto.h"
#include "sal.h"

namespace hcomm {
constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒
constexpr u32 MEM_BLOCK_SIZE = 128;
constexpr uint16_t DEFAULT_LISTENING_PORT = 60001;

HostCpuRoceChannel::HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HostCpuRoceChannel::~HostCpuRoceChannel() {
    HcclResult ret = DpuNotifyManager::GetInstance().FreeNotifyIds(notifyNum_, localDpuNotifyIds_);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[HostCpuRoceChannel::~HostCpuRoceChannel] exception occurred, HcclResult=[%d]", ret);
    }
}

HcclResult HostCpuRoceChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    CHK_PTR_NULL(endpointHandle_);
    HCCL_INFO("[HostCpuRoceChannel][%s] Start. endpointHandle[0x%llx]", __func__, reinterpret_cast<uint64_t>(endpointHandle_));
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    // If HIXL, socket is nullptr for now, will be built later.
    notifyNum_ = channelDesc_.notifyNum;

    if (channelDesc_.exchangeAllMems) {  // true for HIXL, false for HCCL
        // 3. Get memHandles from endpoint
        HCCL_INFO("[HostCpuRoceChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalRdmaRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum));
        HCCL_INFO("[HostCpuRoceChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalRdmaRmaBuffer> &localRdmaBuffer = memHandles[i];
            HCCL_INFO("[HostCpuRoceChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag());
            localRmaBuffers_.emplace_back(localRdmaBuffer.get());
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_INFO("[HostCpuRoceChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_PTR_NULL(channelDesc_.memHandles);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            CHK_PTR_NULL(channelDesc_.memHandles[i]);
            auto* localRdmaBuffer = static_cast<Hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
            localRmaBuffers_.emplace_back(localRdmaBuffer);
        }
    }

    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::StartListen()
{
    uint16_t port = channelDesc_.port;
    HCCL_INFO("[HostCpuRoceChannel::%s] Start. EndpointHandle[0x%llx], port[%u]", __func__, reinterpret_cast<uint64_t>(endpointHandle_), port);
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuRoceChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    CHK_RET(HcommEndpointStartListen(endpointHandle_, port, nullptr));
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HostCpuRoceChannel::%s] socket ptr is NULL, rebuild Socket", __func__);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[HostCpuRoceChannel::%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuRoceChannel::%s] channelDesc port is 0, use default port [%u]", __func__, port);
    }
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, port, socketTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. port[%u].", __func__, port);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildConnection()
{
    std::unique_ptr<HostRdmaConnection> conn;
    EXECEPTION_CATCH(
        conn = std::make_unique<HostRdmaConnection>(socket_, rdmaHandle_),
        return HCCL_E_INTERNAL);
    CHK_PTR_NULL(conn);
    CHK_RET(conn->Init());
    Hccl::QpInfo& qpInfo = conn->GetQpInfo();
    qpInfo.serviceLevel = channelDesc_.roceAttr.sl;
    qpInfo.trafficClass = channelDesc_.roceAttr.tc;
    qpInfo.retryCnt = channelDesc_.roceAttr.retryCnt;
    qpInfo.retryInterval = channelDesc_.roceAttr.retryInterval;
    HCCL_INFO("[HostCpuRoceChannel::BuildConnection] QpInfo: serviceLevel[%u], trafficClass[%u], retryCnt[%u], retryInterval[%u].", 
        qpInfo.serviceLevel, qpInfo.trafficClass, qpInfo.retryCnt, qpInfo.retryInterval);
    connections_.emplace_back(std::move(conn));
    connNum_ = connections_.size();
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildNotify()
{
    CHK_RET(DpuNotifyManager::GetInstance().AllocNotifyIds(notifyNum_, localDpuNotifyIds_));
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildBuffer()
{
    bufferNum_ = localRmaBuffers_.size();
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Init()
{
    CHK_RET(ParseInputParam());
    CHK_RET(StartListen());
    CHK_RET(BuildSocket());
    CHK_RET(BuildConnection());
    CHK_RET(BuildNotify());
    CHK_RET(BuildBuffer());
    return HCCL_SUCCESS;
}

// 当前AICPU和框架没有改为返回错误码形式，所有暂时使用该方法转换
ChannelStatus HostCpuRoceChannel::GetStatus()
{
    ChannelStatus status;
    HcclResult ret = GetStatus(status);
    if (ret != HCCL_SUCCESS && ret != HCCL_E_AGAIN) {
        HCCL_ERROR("[HostCpuRoceChannel::GetStatus] get status exception occurred, HcclResult=[%d]", ret);
        return ChannelStatus::FAILED;
    }
    return status;
}

HcclResult HostCpuRoceChannel::GetStatus(ChannelStatus &status) {
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
            // TODO: Prepare Rqe
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

HcclResult HostCpuRoceChannel::CheckSocketStatus() {
    CHK_PTR_NULL(socket_);
    Hccl::SocketStatus socketStatus = socket_->GetStatus(); // socket状态机
    HCCL_DEBUG("[HostCpuRoceChannel::CheckSocketStatus] socket status = %s", socketStatus.Describe().c_str());
    if (socketStatus == Hccl::SocketStatus::OK) {
        rdmaStatus_ = RdmaStatus::SOCKET_OK;
        channelStatus_ = ChannelStatus::SOCKET_OK;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        channelStatus_ = ChannelStatus::SOCKET_TIMEOUT;
    }
    return HCCL_SUCCESS;
}

// 准备资源（创建QP）
HcclResult HostCpuRoceChannel::CreateQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[HostCpuRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        HcclResult ret = conn->CreateQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[HostCpuRoceChannel::IsResReady] all connections resources connected.");
    return HCCL_SUCCESS;
}

// 交换数据
HcclResult HostCpuRoceChannel::ExchangeData()
{
    HCCL_INFO("[HostCpuRoceChannel::%s] Start to SendExchangeData, notifyNum=%u, bufferNum=%u, connNum=%u",
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
        HCCL_ERROR("[HostCpuRoceChannel::%s] Send sendSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] Send size[%llu] of data success. [%llu] bytes sent.",
        __func__, sendSize, sizeof(sendSize));

    // 同步接收数据包尺寸
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(&recvSize), sizeof(recvSize)),
        HCCL_ERROR("[HostCpuRoceChannel::%s] Recv recvSize failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] Receive size[%llu] of data success. [%llu] bytes received.",
        __func__, recvSize, sizeof(recvSize));

    // 同步发送数据
    CHK_PRT_RET(!socket_->Send(reinterpret_cast<void *>(sendData.data()), sendSize),
        HCCL_ERROR("[HostCpuRoceChannel::%s] Send exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] Send Exchange Data success. [%llu] bytes sent.",
        __func__, sendSize);

    // 同步接收数据
    HCCL_INFO("[HostCpuRoceChannel::%s] Start to Receive Exchange Data", __func__);
    recvData.resize(recvSize);
    CHK_PRT_RET(!socket_->Recv(reinterpret_cast<void *>(recvData.data()), recvSize),
        HCCL_ERROR("[HostCpuRoceChannel::%s] Recv exchange data failed", __func__), HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] Receive Exchange Data success. [%llu] bytes received.",
        __func__, recvSize);
    EXCEPTION_HANDLE_END

    // 同步数据解包
    Hccl::BinaryStream recvBinStream(recvData);
    // CHK_RET(HandshakeMsgUnpack(recvBinStream));
    CHK_RET(NotifyVecUnpack(recvBinStream));
    CHK_RET(RmtBufferVecUnpackProc(recvBinStream));
    CHK_RET(ConnVecUnpackProc(recvBinStream));
    
    HCCL_INFO("[HostCpuRoceChannel::%s] Unpack exchange Data success. ", __func__);    
    return HCCL_SUCCESS;
}
 
void HostCpuRoceChannel::NotifyVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << notifyNum_;
    HCCL_INFO("start pack DpuRoceChannel notifyVec");
    u32 pos = 0;
    for (auto &it : localDpuNotifyIds_) {
        binaryStream << it;
        HCCL_INFO("pack notify pos=%u", pos);
        pos++;
    }
}
 
HcclResult HostCpuRoceChannel::BufferVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << bufferNum_;
    HCCL_INFO("[HostCpuRoceChannel::%s] start to pack RmaBuffers", __func__);
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
            Hccl::ExchangeRdmaBufferDto exchangeDto;
            exchangeDto.Serialize(binaryStream);
            HCCL_INFO("pack buffer pos=%u, dto is null %s", pos, exchangeDto.Describe().c_str());
        }
        pos++;
    }
    HCCL_INFO("[HostCpuRoceChannel::%s] pack RmaBuffers finish", __func__);
    return HCCL_SUCCESS;
}
 
HcclResult HostCpuRoceChannel::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << connNum_;
    HCCL_INFO("[HostCpuRoceChannel::%s] start to pack connections", __func__);
    u32 pos = 0;
    for (auto &it : connections_) {
        binaryStream << pos;

        binaryStream << channelDesc_.roceAttr.retryCnt;
        binaryStream << channelDesc_.roceAttr.retryInterval;
        binaryStream << channelDesc_.roceAttr.sl;
        binaryStream << channelDesc_.roceAttr.tc;

        std::unique_ptr<Hccl::Serializable> dto = nullptr;
        CHK_RET(it->GetExchangeDto(dto));
        dto->Serialize(binaryStream);
        HCCL_INFO("pack connection pos=%u, dto %s", pos, dto->Describe().c_str());
        pos++;
    }
    HCCL_INFO("[HostCpuRoceChannel::%s] pack connections finish", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::RmtBufferVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtNum;
    binaryStream >> rmtNum;
 
    HCCL_INFO("[HostCpuRoceChannel::%s] bufferNum_=%u, rmtNum=%u", __func__, bufferNum_, rmtNum);
 
    rmtRmaBuffers_.resize(rmtNum);
    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        if (pos >= rmtNum) {
            HCCL_ERROR("[HostCpuRoceChannel::%s] pos=%u out of range (rmtNum=%u)", __func__, pos, rmtNum);
            return HCCL_E_INTERNAL;
        }
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);

        HCCL_INFO("[HostCpuRoceChannel::%s] pos=%u, dto %s", __func__, pos, dto.Describe().c_str());
        EXECEPTION_CATCH(rmtRmaBuffers_[pos] = std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto),
            HCCL_ERROR("[HostCpuRoceChannel::%s] make_unique<Hccl::RemoteRdmaRmaBuffer> throws an exception!", __func__);
            return HCCL_E_INTERNAL);
        HCCL_INFO("[HostCpuRoceChannel::%s] pos=%u, rmtRmaBuffer=%s", __func__, pos, rmtRmaBuffers_[pos]->Describe().c_str());
    }
 
    return HCCL_SUCCESS;
}
 
HcclResult HostCpuRoceChannel::NotifyVecUnpack(Hccl::BinaryStream &binaryStream)
{
    uint32_t notifySize = 0;
    binaryStream >> notifySize;
    if (notifySize != notifyNum_) {
        HCCL_ERROR("[HostCpuRoceChannel::NotifyVecUnpack] rmtNum=%u is not equal to localNum=%u", notifySize, notifyNum_);
        return HCCL_E_ROCE_CONNECT;
    }
    remoteDpuNotifyIds_.clear();
    u32 pos = 0;
    for (pos = 0; pos < notifySize; pos++) {
        uint32_t notifyId;
        binaryStream >> notifyId;
        remoteDpuNotifyIds_.push_back(notifyId);
    }
    HCCL_INFO("[HostCpuRoceChannel::NotifyVecUnpack] unpack dpuNotify");
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
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
        binaryStream >> channelDesc_.roceAttr.retryCnt;
        binaryStream >> channelDesc_.roceAttr.retryInterval;
        binaryStream >> channelDesc_.roceAttr.sl;
        binaryStream >> channelDesc_.roceAttr.tc;
        rmtConnDto_.Deserialize(binaryStream);
    }
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ModifyQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[HostCpuRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        CHK_RET(conn->ParseRmtExchangeDto(rmtConnDto_));
        Hccl::QpInfo& qpInfo = conn->GetQpInfo();
        qpInfo.serviceLevel = channelDesc_.roceAttr.sl;
        qpInfo.trafficClass = channelDesc_.roceAttr.tc;
        qpInfo.retryCnt = channelDesc_.roceAttr.retryCnt;
        qpInfo.retryInterval = channelDesc_.roceAttr.retryInterval;
        HCCL_INFO("[HostCpuRoceChannel::ModifyQp] QpInfo: serviceLevel[%u], trafficClass[%u], retryCnt[%u], retryInterval[%u].", 
            qpInfo.serviceLevel, qpInfo.trafficClass, qpInfo.retryCnt, qpInfo.retryInterval);
        HcclResult ret = conn->ModifyQp();
        if (ret == HCCL_E_AGAIN) {
            return HCCL_SUCCESS;
        }
        if (ret != HCCL_SUCCESS) {
            return ret;
        }
    }
    HCCL_INFO("[HostCpuRoceChannel::IsResReady] all connections resources connected.");
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char** memTags)
{
    CHK_PRT_RET(remoteMem == nullptr, HCCL_ERROR("[GetRemoteMem] remoteMem is nullptr"), HCCL_E_PTR);
    CHK_PRT_RET(memNum == nullptr, HCCL_ERROR("[GetRemoteMem] memNum is nullptr"), HCCL_E_PTR);
    *memNum = 0;

    uint32_t totalCount = rmtRmaBuffers_.size();
    if (totalCount == 0) {
        HCCL_INFO("[GetRemoteMem] No remote memory regions available");
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
        HCCL_INFO("[HostCpuRoceChannel::%s] rmtBuf[addr[%p], size[%llu]]", 
            __func__, remoteMem[i]->addr, remoteMem[i]->size);
        remoteMems.emplace_back(std::move(hcclMem));
    }

    *memNum = totalCount;
    return HCCL_SUCCESS;
}

std::vector<Hccl::QpInfo> HostCpuRoceChannel::GetQpInfos() const
{
    std::vector<Hccl::QpInfo> qpInfos;
    for (auto& rdmaConn : connections_) {
        qpInfos.emplace_back(rdmaConn->GetQpInfo());
    }
    return qpInfos;
}

std::string HostCpuRoceChannel::Describe() const
{
    std::string msg = "HostCpuRoceChannel{";
    msg += Hccl::StringFormat("notifyNum:%u, dpuNotifyList:[-]", notifyNum_);
    msg += Hccl::StringFormat(", bufferNum:%u, localRmaBuffers: [", bufferNum_);
    for (auto& buf : localRmaBuffers_) {
        msg += buf->Describe();
        msg += ", ";
    }
    msg += Hccl::StringFormat("], connNum:%u, connections:[", connNum_);
    for (auto& conn : connections_) {
        msg += conn->Describe();
        msg += ", ";
    }
    msg += Hccl::StringFormat("], rdmaHandle: %p, %s, ", rdmaHandle_, channelStatus_.Describe().c_str());
    msg += socket_->Describe();
    msg += ", ";
    // msg += attr_.Describe();
    return msg;
}

// TODO: 可能需要错开地址
HcclResult HostCpuRoceChannel::IbvPostRecv() const {
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);

    // 准备wr
    ibv_recv_wr recvWr {};
    ibv_recv_wr *recvbadWr = nullptr;
    ibv_sge recvsgList {};
    recvsgList.addr   = localRmaBuffers_[0]->GetBufferInfo().first; // 本端起始地址
    recvsgList.length = MEM_BLOCK_SIZE;
    recvsgList.lkey   = localRmaBuffers_[0]->GetLkey();             // 本端的访问秘钥
    recvWr.wr_id      = 0;
    recvWr.sg_list    = &recvsgList;
    recvWr.next       = nullptr;
    recvWr.num_sge    = 1;

    HCCL_INFO("[HostCpuRoceChannel::%s] call ibv_post_recv", __func__);
    HCCL_INFO("qp_state = [%u]", qpInfo[0].qp->state);
    int32_t ret = ibv_post_recv(qpInfo[0].qp, &recvWr, &recvbadWr);
    CHK_PRT_RET(ret == ENOMEM,
                HCCL_WARNING("[HostCpuRoceChannel][%s] post recv wqe overflow. ret:%d, "
                             "badWr->wr_id[%llu], badWr->sg_list->addr[%llu]",
                             __func__, ret, recvbadWr->wr_id, recvbadWr->sg_list->addr),
                HCCL_E_AGAIN);

    CHK_PRT_RET(ret != 0,
                HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_post_recv failed. ret:%d, "
                           "badWr->wr_id[%llu], badWr->sg_list->addr[%llu]",
                           __func__, ret, recvbadWr->wr_id, recvbadWr->sg_list->addr),
                HCCL_E_NETWORK);

    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::PrepareNotifyWrResource(
    const uint64_t len, const uint32_t remoteNotifyIdx, struct ibv_send_wr &notifyRecordWr) const
{
    if (remoteNotifyIdx >= remoteDpuNotifyIds_.size()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] remoteNotifyIdx[%u] out of the range of remoteDpuNotifyIds_[%zu].",
                   __func__, remoteNotifyIdx, remoteDpuNotifyIds_.size());
        return HCCL_E_PARA;
    }
    uint32_t dpuNotifyId = remoteDpuNotifyIds_[remoteNotifyIdx];

    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);

    // 构造send_WR
    notifyRecordWr.sg_list->addr                 = localRmaBuffers_[0]->GetBufferInfo().first; // 本端起始地址
    notifyRecordWr.sg_list->length               = 0;                                          // 取的本端长度
    notifyRecordWr.sg_list->lkey                 = localRmaBuffers_[0]->GetLkey();             // 本端的访问秘钥
    notifyRecordWr.opcode       = IBV_WR_RDMA_WRITE_WITH_IMM;
    notifyRecordWr.send_flags   = IBV_SEND_SIGNALED;
    notifyRecordWr.imm_data     = dpuNotifyId;
    notifyRecordWr.next         = nullptr;
    notifyRecordWr.num_sge      = 1;
    notifyRecordWr.wr_id        = 0; // 用户定义工作请求id，建议：设为有意义的值
    notifyRecordWr.wr.rdma.rkey = rmtRmaBuffers_[0]->GetRkey();                               // 远端秘钥
    notifyRecordWr.wr.rdma.remote_addr = static_cast<uint64_t>(rmtRmaBuffers_[0]->GetAddr()); // 远端地址
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyRecord(const uint32_t remoteNotifyIdx)
{
    // 补充rq中消耗的rqe
    // 1. 准备recv_WR
    CHK_RET(IbvPostRecv());

    // 1.构造send_WR
    struct ibv_send_wr  notifyRecordWr {};
    struct ibv_send_wr *sendbadWr = nullptr;
    struct ibv_sge sgList {};
    notifyRecordWr.sg_list      = &sgList;
    CHK_RET(PrepareNotifyWrResource(MEM_BLOCK_SIZE, remoteNotifyIdx, notifyRecordWr));

    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);

    // 3.调用ibv_post_send
    HCCL_INFO("[HostCpuRoceChannel::%s] call ibv_post_send, qp_state = [%u]", __func__, qpInfo[0].qp->state);
    int32_t ret = ibv_post_send(qpInfo[0].qp, &notifyRecordWr, &sendbadWr);
    if (ret != 0 && sendbadWr == nullptr) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_post_send failed while badWr is nullptr", __func__);
        return HCCL_E_INTERNAL;
    }
    CHK_PRT_RET(ret == ENOMEM,
        HCCL_WARNING("[HostCpuRoceChannel][%s] post send wqe overflow. ret:%d, badWr->wr_id[%llu], "
                     "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, sendbadWr->wr_id, sendbadWr->sg_list->addr, sendbadWr->wr.rdma.remote_addr, sendbadWr->wr.ud.remote_qpn),
        HCCL_E_AGAIN);

    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_post_send failed. ret:%d, badWr->wr_id[%llu], "
                   "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, sendbadWr->wr_id, sendbadWr->sg_list->addr, sendbadWr->wr.rdma.remote_addr, sendbadWr->wr.ud.remote_qpn),
        HCCL_E_NETWORK);
    if (wqeNum_ == UINT32_MAX) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] wqeNum_ has reached the maximum value of uint32_t.", __func__);
        return HCCL_E_INTERNAL;
    }
    wqeNum_++;
    HCCL_INFO("[HostCpuRoceChannel::NotifyRecord] NotifyRecord end, wqeNum_=%u", wqeNum_);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    HCCL_INFO("[HostCpuRoceChannel::NotifyWait] NotifyWait start");

    if (localNotifyIdx >= localDpuNotifyIds_.size()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] localNotifyIdx[%u] out of the range of localDpuNotifyIds_[%zu].",
            __func__, localNotifyIdx, localDpuNotifyIds_.size());
        return HCCL_E_PARA;
    }
    uint32_t dpuNotifyId = localDpuNotifyIds_[localNotifyIdx];

    // 1. 准备WR
    struct ibv_wc wc{};
    std::lock_guard<std::mutex> lock(cq_mutex);
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(qpInfo[0].recvCq == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] recvCq is null", __func__), HCCL_E_INTERNAL);
    CHK_PRT_RET(qpInfo[0].qp == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] qp is null", __func__), HCCL_E_INTERNAL);
    CHK_PRT_RET(qpInfo[0].recvCq->context == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] recvCq->context is null", __func__), HCCL_E_INTERNAL);

    HCCL_INFO("[HostCpuRoceChannel::NotifyWait] poll recvCq = %p, localNotifyIdx = %u, notifyId = %u.",
        qpInfo[0].recvCq, localNotifyIdx, dpuNotifyId);

    // 2.轮询rq_cq
    auto startTime = std::chrono::steady_clock::now();
    auto waitTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(timeout));
    while (true) {
        HCCL_INFO("[HostCpuRoceChannel::NotifyWait] start to poll cq");
        
        HCCL_INFO("qp_state = [%u]", qpInfo[0].qp->state);
        auto actualNum = ibv_poll_cq(qpInfo[0].recvCq, 1, &wc);
        CHK_PRT_RET(wc.status != IBV_WC_SUCCESS,
            HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_poll_cq return wc.status is [%d].",
            __func__, wc.status), HCCL_E_NETWORK);

        HCCL_INFO("[HostCpuRoceChannel::NotifyWait] actualNum = %d; imm_data = %u", actualNum, wc.imm_data);

        if (actualNum > 0 && wc.imm_data == dpuNotifyId) {
            HCCL_INFO("[HostCpuRoceChannel::NotifyWait] poll cq success");
            break;
        }

        if ((std::chrono::steady_clock::now() - startTime) >= waitTime) {
            HCCL_ERROR("[HostCpuRoceChannel][%s] call ibv_poll_cq timeout.", __func__);
            return HCCL_E_TIMEOUT;
        }
    }

    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::PrepareWriteWrResource(const void *dst, const void *src, const uint64_t len,
    const uint32_t remoteNotifyIdx, struct ibv_send_wr &writeWithNotifyWr) const
{
    if (remoteNotifyIdx >= remoteDpuNotifyIds_.size()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] remoteNotifyIdx[%u] out of the range of remoteDpuNotifyIds_[%zu].",
            __func__, remoteNotifyIdx, remoteDpuNotifyIds_.size());
        return HCCL_E_PARA;
    }
    uint32_t dpuNotifyId = remoteDpuNotifyIds_[remoteNotifyIdx];

    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);

    // 1. 构造WR
    CHK_PRT_RET(len > UINT32_MAX, HCCL_ERROR("[HostCpuRoceChannel][%s] the len[%llu] exceeds the size of u32.",
        __func__, len), HCCL_E_PARA);

    writeWithNotifyWr.sg_list->addr = reinterpret_cast<uint64_t>(src); // 本端起始地址
    writeWithNotifyWr.sg_list->length = static_cast<uint32_t>(len);
    writeWithNotifyWr.sg_list->lkey = localRmaBuffers_[0]->GetLkey(); // 本端的访问秘钥

    writeWithNotifyWr.opcode              = IBV_WR_RDMA_WRITE_WITH_IMM;
    writeWithNotifyWr.send_flags          = IBV_SEND_SIGNALED;
    writeWithNotifyWr.next                = nullptr;
    writeWithNotifyWr.num_sge             = 1;
    writeWithNotifyWr.wr_id               = 0;
    writeWithNotifyWr.imm_data            = dpuNotifyId;
    writeWithNotifyWr.wr.rdma.rkey        = rmtRmaBuffers_[0]->GetRkey();
    writeWithNotifyWr.wr.rdma.remote_addr = reinterpret_cast<uint64_t>(dst);

    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::WriteWithNotify(
    void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx)
{
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    HCCL_INFO("[HostCpuRoceChannel::WriteWithNotify] WriteWithNotify start");

    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);

    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);

    // 补充rq中消耗的rqe
    CHK_RET(IbvPostRecv());

    // 1. 构造WR
    struct ibv_send_wr writeWithNotifyWr{};
    struct ibv_send_wr *badWr = nullptr;
    struct ibv_sge sgList{};
    writeWithNotifyWr.sg_list = &sgList;
    CHK_RET(PrepareWriteWrResource(dst, src, len, remoteNotifyIdx, writeWithNotifyWr));

    // 2. 调用ibv_post_send
    int32_t ret = ibv_post_send(qpInfo[0].qp, &writeWithNotifyWr, &badWr);
    if (ret != 0 && badWr == nullptr) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_post_send failed while badWr is nullptr", __func__);
        return HCCL_E_INTERNAL;
    }
    CHK_PRT_RET(ret == ENOMEM,
        HCCL_WARNING("[HostCpuRoceChannel][%s] post send wqe overflow. ret:%d, badWr->wr_id[%llu], "
                     "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_AGAIN);

    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_post_send failed. ret:%d, badWr->wr_id[%llu], "
                   "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_NETWORK);
    if (wqeNum_ == UINT32_MAX) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] wqeNum_ has reached the maximum value of uint32_t.", __func__);
        return HCCL_E_INTERNAL;
    }
    wqeNum_++;
    HCCL_INFO("[HostCpuRoceChannel::WriteWithNotify] WriteWithNotify end, wqeNum_=%u", wqeNum_);
    return HCCL_SUCCESS;
}

void HostCpuRoceChannel::BuildRdmaWr(const char *caller, ibv_wr_opcode opcode, void *localAddr,
                                      const void *remoteAddr, uint64_t len, size_t localIdx, size_t rmtIdx,
                                      struct ibv_send_wr &wr, struct ibv_sge &sg) const
{
    wr.sg_list             = &sg;
    wr.sg_list->addr       = reinterpret_cast<uint64_t>(localAddr);
    wr.sg_list->length     = static_cast<uint32_t>(len);
    wr.sg_list->lkey       = localRmaBuffers_[localIdx]->GetLkey();

    wr.opcode              = opcode;
    wr.send_flags          = (fenceFlag_ == true ? (IBV_SEND_SIGNALED | IBV_SEND_FENCE) : IBV_SEND_SIGNALED);
    wr.next                = nullptr;
    wr.num_sge             = 1;
    wr.wr_id               = 0;
    wr.wr.rdma.rkey        = rmtRmaBuffers_[rmtIdx]->GetRkey();
    wr.wr.rdma.remote_addr = reinterpret_cast<uint64_t>(remoteAddr);
}

HcclResult HostCpuRoceChannel::PostAndCheckSend(const char *caller, struct ibv_send_wr &wr)
{
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    struct ibv_send_wr *badWr = nullptr;
    s32 ret = ibv_post_send(qpInfo[0].qp, &wr, &badWr);
    if (ret != 0 && badWr == nullptr) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_post_send failed while badWr is nullptr", caller);
        return HCCL_E_INTERNAL;
    }
    CHK_PRT_RET(ret == ENOMEM,
        HCCL_WARNING("[HostCpuRoceChannel::%s] post send wqe overflow. ret:%d, "
        "badWr->wr_id[%llu], badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
        caller, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_AGAIN);
    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_post_send failed. ret:%d, "
        "badWr->wr_id[%llu], badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
        caller, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_NETWORK);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::PostRdmaOp(const char *caller, ibv_wr_opcode opcode, void *localAddr,
                                           const void *remoteAddr, const uint64_t len)
{
    HCCL_INFO("[HostCpuRoceChannel::%s] START. localAddr[%p], remoteAddr[%p], len[%llu].", caller, localAddr,
              remoteAddr, len);

    CHK_PRT_RET(GetQpInfos().empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", caller), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", caller),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", caller),
                HCCL_E_ROCE_CONNECT);
    if (len > UINT32_MAX) {
        HCCL_WARNING("[HostCpuRoceChannel::%s] len[%llu] exceeds uint32_t max, will be casted to %u.", caller, len, static_cast<uint32_t>(len));
    }
    CHK_PRT_RET(wqeNum_ == UINT32_MAX,
        HCCL_ERROR("[HostCpuRoceChannel::%s] wqeNum_ has reached the maximum value of uint32_t.", caller),
        HCCL_E_INTERNAL);

    // 1. 查找 buffer 索引
    auto startTime = std::chrono::steady_clock::now();
    size_t localIdx = 0;
    CHK_RET(FindLocalBuffer(reinterpret_cast<uint64_t>(localAddr), len, localIdx));
    size_t rmtIdx = 0;
    CHK_RET(FindRemoteBuffer(reinterpret_cast<uint64_t>(remoteAddr), len, rmtIdx));
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    HCCL_INFO("[HostCpuRoceChannel::%s] check buffer takes time [%lld]us", caller, elapsed);

    // 2. 构造 WR 并发送
    struct ibv_send_wr wr{};
    struct ibv_sge sg;
    BuildRdmaWr(caller, opcode, localAddr, remoteAddr, len, localIdx, rmtIdx, wr, sg);
    CHK_RET(PostAndCheckSend(caller, wr));

    wqeNum_++;
    fenceFlag_ = false;
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. wqeNum_[%u]", caller, wqeNum_);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Write(void *dst, const void *src, const uint64_t len)
{
    return PostRdmaOp(__func__, IBV_WR_RDMA_WRITE, const_cast<void *>(src), dst, len);
}

HcclResult HostCpuRoceChannel::Read(void *dst, const void *src, const uint64_t len)
{
    return PostRdmaOp(__func__, IBV_WR_RDMA_READ, dst, src, len);
}

HcclResult HostCpuRoceChannel::FindLocalBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const
{
    uint64_t endAddr = addr + len;
    HCCL_INFO("[HostCpuRoceChannel::%s] START. Finding buffer addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, addr, len, endAddr);
    for (size_t i = 0; i < localRmaBuffers_.size(); ++i) {
        CHK_PTR_NULL(localRmaBuffers_[i]);
        uint64_t bufAddr = localRmaBuffers_[i]->GetBufferInfo().first;
        uint64_t bufSize = localRmaBuffers_[i]->GetBufferInfo().second;
        uint64_t bufEndAddr = bufAddr + bufSize;
        HCCL_INFO("[HostCpuRoceChannel::%s] Comparing with saved localRmaBuffer[%zu]: addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, i, bufAddr, bufSize, bufEndAddr);
        if (addr >= bufAddr && endAddr <= bufEndAddr) {
            targetIdx = i;
            HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. targetIdx[%zu]", __func__, targetIdx);
            return HCCL_SUCCESS;
        }
    }
    HCCL_ERROR("[HostCpuRoceChannel::%s] FAIL. Can not Found Target Buffer addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, addr, len, endAddr);
    return HCCL_E_NOT_FOUND;
}

HcclResult HostCpuRoceChannel::FindRemoteBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const
{
    uint64_t endAddr = addr + len;
    HCCL_INFO("[HostCpuRoceChannel::%s] START. Finding buffer addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, addr, len, endAddr);
    for (size_t i = 0; i < rmtRmaBuffers_.size(); ++i) {
        CHK_PTR_NULL(rmtRmaBuffers_[i]);
        uint64_t bufAddr = static_cast<uint64_t>(rmtRmaBuffers_[i]->GetAddr());
        uint64_t bufSize = rmtRmaBuffers_[i]->GetSize();
        uint64_t bufEndAddr = bufAddr + bufSize;
        HCCL_INFO("[HostCpuRoceChannel::%s] Comparing with saved rmtRmaBuffers[%zu]: addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, i, bufAddr, bufSize, bufEndAddr);
        if (addr >= bufAddr && endAddr <= bufEndAddr) {
            targetIdx = i;
            HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. targetIdx[%zu]", __func__, targetIdx);
            return HCCL_SUCCESS;
        }
    }
    HCCL_ERROR("[HostCpuRoceChannel::%s] FAIL. Can not Found Target Buffer addr[0x%llx], len[0x%llx], addr+len[0x%llx].", __func__, addr, len, endAddr);
    return HCCL_E_NOT_FOUND;
}

HcclResult HostCpuRoceChannel::ChannelFence()
{
    std::lock_guard<std::mutex> lock(sendCq_mutex);
    HCCL_INFO("[HostCpuRoceChannel::%s] ChannelFence start, wqeNum_=%u", __func__, wqeNum_);
    if (wqeNum_ == 0) {
        fenceFlag_ = true;
        HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. wqeNum_[%u].", __func__, wqeNum_);
        return HCCL_SUCCESS;
    }
    std::vector<struct ibv_wc> wc(wqeNum_);
    const std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(qpInfo[0].sendCq == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] sendCq is null", __func__), HCCL_E_INTERNAL);
    CHK_PRT_RET(qpInfo[0].qp == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] qp is null", __func__), HCCL_E_INTERNAL);
    CHK_PRT_RET(qpInfo[0].sendCq->context == nullptr, HCCL_ERROR("[HostCpuRoceChannel::%s] sendCq->context is null", __func__), HCCL_E_INTERNAL);

    auto timeout = std::chrono::milliseconds(FENCE_TIMEOUT_MS);
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto actualNum = ibv_poll_cq(qpInfo[0].sendCq, wqeNum_, wc.data());
        if (actualNum < 0) {
            HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_poll_cq failed. actualNum: %d.", __func__, actualNum);
            return HCCL_E_NETWORK;
        }

        uint32_t actualNum32 = static_cast<uint32_t>(actualNum);
        if (actualNum32 > wqeNum_) {
            HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_poll_cq polled more completions (%d) than expected (%u).",
                __func__, actualNum32, wqeNum_);
            return HCCL_E_INTERNAL;
        } else if (actualNum32 > 0) {
            for (uint32_t i = 0; i < actualNum32; i++) {
                if (wc[i].status != IBV_WC_SUCCESS) {
                    HCCL_ERROR("[HostCpuRoceChannel::%s] ibv_poll_cq error. wc[%u] status: %d.", __func__, i, wc[i].status);
                    return HCCL_E_NETWORK;
                }
            }
            wqeNum_ -= actualNum32; // 减去已经完成的数量，继续等待剩余的完成
            if (wqeNum_ == 0) {
                break; // 所有的wqe都已经完成，退出循环
            }
        }

        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[HostCpuRoceChannel][%s] call ibv_poll_cq timeout.", __func__);
            return HCCL_E_TIMEOUT;
        }
    }

    wqeNum_ = 0; // 所有的wqe都已经完成，重置计数器
    fenceFlag_ = true;
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. wqeNum_[%u].", __func__, wqeNum_);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    CHK_PTR_NULL(notifyNum);
    *notifyNum = notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::GetHcclBuffer(void*& addr, uint64_t& size)
{
    if (rmtRmaBuffers_.empty()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] remote buffer is empty, please check if channel complete exchange data",
                   __func__);
        return HCCL_E_INTERNAL;
    }
    addr = reinterpret_cast<void*>(rmtRmaBuffers_[0]->GetAddr());
    size = static_cast<uint64_t>(rmtRmaBuffers_[0]->GetSize());
    return HCCL_SUCCESS;
}
} // namespace hcomm