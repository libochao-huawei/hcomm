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

// Orion
#include "orion_adapter_hccp.h"
#include "exchange_rdma_buffer_dto.h"
#include "rdma_handle_manager.h"
#include "exchange_rdma_conn_dto.h"
#include "sal.h"

namespace hcomm {
constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒
constexpr u32 MEM_BLOCK_SIZE = 128;

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
    HCCL_INFO("[HostCpuRoceChannel::%s] START. endpointHandle[0x%llx]", __func__, endpointHandle_);
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);

    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);
    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    // socket_ 可为空
    notifyNum_ = channelDesc_.notifyNum;

    if (channelDesc_.exchangeAllMems) {
        // 3. Get memHandles from endpoint
        HCCL_INFO("[HostCpuRoceChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalRdmaRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum));
        HCCL_INFO("[HostCpuRoceChannel][%s] Got memHandleNum[%u]", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalRdmaRmaBuffer> &localRdmaBuffer = memHandle[i];
            HCCL_INFO("[HostCpuRoceChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localRdmaBuffer->GetAddr(), localRdmaBuffer->GetSize(), localRdmaBuffer->GetBuf()->GetMemTag());
            localRmaBuffers_.emplace_back(localRdmaBuffer.get());
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_INFO("[HostCpuRoceChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
            auto *localRdmaBuffer = static_cast<Hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
            localRmaBuffers_.emplace_back(localRdmaBuffer);
        }
    }

    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);
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
    connections_.emplace_back(std::move(conn));
    connNum_ = connections_.size();
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildNotify()
{
    CHK_RET(DpuNotifyManager::GetInstance().AllocNotifyIds(notifyNum_, localDpuNotifyIds_));
    return HCCL_SUCCESS;
}

// NotifyRecord使用的内存
HcclResult HostCpuRoceChannel::BuildBuffer()
{
    // TODO: 追加构造NotifyRecord使用的LocalRdmaRmaBuffer，使用malloc创建Host侧内存
    bufferNum_ = localRmaBuffers_.size();
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[HostCpuRoceChannel][%s] socket ptr is NULL, rebuildSocket", __func__);
    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[HostCpuRoceChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
    }
    HCCL_INFO("[HostCpuRoceChannel][%s] Port[%u] will be used to build socketConfig.", __func__, port);
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, port, socketTag);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::StartSocketListen()
{
    HCCL_INFO("[HostCpuRoceChannel][%s] START", __func__);
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    uint16_t port = channelDesc_.port;
    if (port == 0) {
        port = DEFAULT_LISTENING_PORT;
        HCCL_INFO("[HostCpuRoceChannel][%s] Port unset, default value[%u] is used.", __func__, port);
    }
    CHK_RET(localEpPtr->ServerSocketListen(port));
    HCCL_INFO("[HostCpuRoceChannel][%s] SUCCESS", __func__);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Init()
{
    CHK_RET(ParseInputParam());
    CHK_RET(StartSocketListen());
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
    // HandshakeMsgPack(binaryStream); // attr的数据看上去没有起到作用，先注释
    NotifyVecPack(binaryStream);
    CHK_RET(BufferVecPack(binaryStream));
    CHK_RET(ConnVecPack(binaryStream));

    std::vector<char> sendData{};
    binaryStream.Dump(sendData);
    size_t sendSize = sendData.size();
    std::vector<char> recvData{};
    size_t recvSize = 0;

    // 同步发送数据包尺寸
    socket_->Send(reinterpret_cast<void *>(&sendSize), sizeof(sendSize));
    HCCL_INFO("[HostCpuRoceChannel::%s] Send size[%zu] of data success. [%llu] bytes sent.", 
        __func__, sendSize, sizeof(sendSize));

    // 同步接收数据包尺寸
    socket_->Recv(reinterpret_cast<void *>(&recvSize), sizeof(recvSize));
    HCCL_INFO("[HostCpuRoceChannel::%s] Receive size[%zu] of data success. [%llu] bytes received.", 
        __func__, recvSize, sizeof(recvSize));

    // 同步发送数据
    socket_->Send(reinterpret_cast<void *>(sendData.data()), sendSize);
    HCCL_INFO("[HostCpuRoceChannel::%s] Send Exchange Data success. [%llu] bytes sent.", 
        __func__, sendSize);
    
    // 同步接收数据
    recvData.resize(recvSize);
    socket_->Recv(reinterpret_cast<void *>(recvData.data()), recvSize);
    HCCL_INFO("[HostCpuRoceChannel::%s] Receive Exchange Data success. [%llu] bytes received.", 
        __func__, recvSize);

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
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);
 
        HCCL_INFO("[HostCpuRoceChannel::%s] pos=%u, dto %s", __func__, pos, dto.Describe().c_str());
        if (dto.size == 0) { // size为0，则为 remote 空buffer
            HCCL_INFO("[HostCpuRoceChannel::%s] size is 0, pos=%u, skip.", __func__, pos);
        } else { // size非0，则构造一个remote buffer
            EXECEPTION_CATCH(rmtRmaBuffers_[i] = std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto),
                return HCCL_E_INTERNAL);
            rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto));
            HCCL_INFO("[HostCpuRoceChannel::%s] pos=%u, rmtRmaBuffer=%s", __func__, pos, 
                rmtRmaBuffers_[i]->Describe().c_str());
        }
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
        rmtConnDto_.Deserialize(binaryStream);
    }
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ModifyQp() {
    for (auto &conn : connections_) {
        Hccl::CHECK_NULLPTR(conn,
            Hccl::StringFormat("[HostCpuRoceChannel::%s] failed, connection pointer is nullptr", __func__));
        CHK_RET(conn->ParseRmtExchangeDto(rmtConnDto_));
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
        memTags[i] = const_cast<char*>(rmtRmaBuffer->GetMemTag());
        remoteMem[i] = hcclMem.get();
        HCCL_INFO("[HostCpuRoceChannel::%s] rmtBuf[addr[%p], size[%lu]]", 
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
        HCCL_ERROR("[HostCpuRoceChannel::%s] remoteNotifyIdx[%u] out of the range of remoteDpuNotifyIds_[%u].",
                   __func__, remoteNotifyIdx, remoteDpuNotifyIds_.size());
        return HCCL_E_PARA;
    }
    uint32_t dpuNotifyId = remoteDpuNotifyIds_[remoteNotifyIdx];

    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);

    // 构造send_WR
    notifyRecordWr.sg_list->addr                 = localRmaBuffers_[0]->GetBufferInfo().first + len; // 本端起始地址
    notifyRecordWr.sg_list->length               = len / 2;                                          // 取的本端长度
    notifyRecordWr.sg_list->lkey                 = localRmaBuffers_[0]->GetLkey();                               // 本端的访问秘钥
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

HcclResult HostCpuRoceChannel::NotifyRecord(const uint32_t remoteNotifyIdx) const
{
    uint64_t bufferBlockSize = localRmaBuffers_[0]->GetBufferInfo().second / 2;

    // 补充rq中消耗的rqe
    // 1. 准备recv_WR
    CHK_RET(IbvPostRecv());

    // 1.构造send_WR
    struct ibv_send_wr  notifyRecordWr {};
    struct ibv_send_wr *sendbadWr = nullptr;
    struct ibv_sge sgList {};
    notifyRecordWr.sg_list      = &sgList;
    CHK_RET(PrepareNotifyWrResource(bufferBlockSize, remoteNotifyIdx, notifyRecordWr));

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
    HCCL_INFO("[HostCpuRoceChannel::NotifyRecord] NotifyRecord end");
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    HCCL_INFO("[HostCpuRoceChannel::NotifyWait] NotifyWait start");

    if (localNotifyIdx >= localDpuNotifyIds_.size()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] localNotifyIdx[%u] out of the range of localDpuNotifyIds_[%u].",
            __func__, localNotifyIdx, localDpuNotifyIds_.size());
        return HCCL_E_PARA;
    }
    uint32_t dpuNotifyId = localDpuNotifyIds_[localNotifyIdx];

    // 1. 准备WR
    struct ibv_wc wc{};
    std::lock_guard<std::mutex> lock(cq_mutex);
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    if (qpInfo.empty()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__);
        return HCCL_E_ROCE_CONNECT;
    }
    if (qpInfo[0].recvCq == nullptr) {
        HCCL_INFO("[HostCpuRoceChannel::NotifyWait] recvCq is null");
        return HCCL_E_INTERNAL;
    }
    if (qpInfo[0].recvCq->context == nullptr) {
        HCCL_INFO("[HostCpuRoceChannel::NotifyWait] recvCq->context is null");
        return HCCL_E_INTERNAL;
    }

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
        HCCL_ERROR("[HostCpuRoceChannel::%s] remoteNotifyIdx[%u] out of the range of remoteDpuNotifyIds_[%u].",
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
    writeWithNotifyWr.sg_list->length = len;
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
    void *dst, const void *src, const uint64_t len, const uint32_t remoteNotifyIdx) const
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
    HCCL_INFO("[HostCpuRoceChannel::WriteWithNotify] WriteWithNotify end");
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Write(void *dst, const void *src, const uint64_t len)
{
    HCCL_INFO("[HostCpuRoceChannel::%s] START. dst[%p], src[%p], len[%llu].", __func__, dst, src, len);
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_RET(IbvPostRecv());

    // 1. 构造WR
    size_t localIdx = 0;
    CHK_RET(FindLocalBuffer(reinterpret_cast<uint64_t>(src), len, localIdx));
    size_t rmtIdx = 0;
    CHK_RET(FindRemoteBuffer(reinterpret_cast<uint64_t>(dst), len, rmtIdx));
    struct ibv_send_wr writeWr{};
    struct ibv_send_wr *badWr = nullptr;
    struct ibv_sge sg;
    writeWr.sg_list = &sg;
    writeWr.sg_list->addr = reinterpret_cast<uint64_t>(src); // 源地址
    writeWr.sg_list->length = len;
    writeWr.sg_list->lkey = localRmaBuffers_[localIdx]->GetLkey(); // LKey
    writeWr.opcode = IBV_WR_RDMA_WRITE;
    writeWr.send_flags = (fenceFlag_ == true ? (IBV_SEND_SIGNALED | IBV_SEND_FENCE) : IBV_SEND_SIGNALED);
    writeWr.next = nullptr;
    writeWr.num_sge = 1;
    writeWr.wr_id = 0;
    writeWr.wr.rdma.rkey = rmtRmaBuffers_[rmtIdx]->GetRkey(); // 远端RKey
    writeWr.wr.rdma.remote_addr = reinterpret_cast<uint64_t>(dst); // 远端地址

    // 2. 调用ibv_post_send
    int32_t ret = ibv_post_send(qpInfo[0].qp, &writeWr, &badWr);
    CHK_PRT_CONT(ret == ENOMEM,
        HCCL_WARNING("[HostCpuRoceChannel][%s] post send wqe overflow. ret:%d, badWr->wr_id[%llu], "
                     "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_AGAIN);

    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_post_send failed. ret:%d, badWr->wr_id[%llu], "
                   "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS", __func__);
    fenceFlag_ = false;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::Read(void *dst, const void *src, const uint64_t len)
{
    HCCL_INFO("[HostCpuRoceChannel::%s] START. dst[%p], src[%p], len[%llu].", __func__, dst, src, len);
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__), HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(localRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] localRmaBuffer is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_PRT_RET(rmtRmaBuffers_.empty(), HCCL_ERROR("[HostCpuRoceChannel::%s] rmtRmaBuffers is Empty", __func__),
                HCCL_E_ROCE_CONNECT);
    CHK_RET(IbvPostRecv());

    // 1. 构造WR
    size_t localIdx = 0;
    CHK_RET(FindLocalBuffer(reinterpret_cast<uint64_t>(src), len, localIdx));
    size_t rmtIdx = 0;
    CHK_RET(FindRemoteBuffer(reinterpret_cast<uint64_t>(dst), len, rmtIdx));
    struct ibv_send_wr readWr{};
    struct ibv_send_wr *badWr = nullptr;
    struct ibv_sge sg;
    readWr.sg_list = &sg;
    readWr.sg_list->addr = reinterpret_cast<uint64_t>(dst);
    readWr.sg_list->length = len;
    readWr.sg_list->lkey = localRmaBuffers_[localIdx]->GetLkey(); // LKey
    readWr.opcode = IBV_WR_RDMA_READ;
    readWr.send_flags = (fenceFlag_ == true ? (IBV_SEND_SIGNALED | IBV_SEND_FENCE) : IBV_SEND_SIGNALED);
    readWr.next = nullptr;
    readWr.num_sge = 1;
    readWr.wr_id = 0;
    readWr.wr.rdma.rkey = rmtRmaBuffers_[rmtIdx]->GetRkey(); // 远端RKey
    readWr.wr.rdma.remote_addr = reinterpret_cast<uint64_t>(src); // 远端地址

    // 2. 调用ibv_post_send
    s32 ret = ibv_post_send(qpInfo[0].qp, &readWr, &badWr);
    CHK_PRT_CONT(ret == ENOMEM,
        HCCL_WARNING("[HostCpuRoceChannel][%s] post send wqe overflow. ret:%d, badWr->wr_id[%llu], "
                     "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_AGAIN);

    CHK_PRT_CONT(ret != 0,
        HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_post_send failed. ret:%d, badWr->wr_id[%llu], "
                   "badWr->sg_list->addr[%llu], badWr->wr.rdma.remote_addr[%llu], badWr->wr.ud.remote_qpn[%u]",
            __func__, ret, badWr->wr_id, badWr->sg_list->addr, badWr->wr.rdma.remote_addr, badWr->wr.ud.remote_qpn),
        HCCL_E_NETWORK);
    HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS", __func__);
    fenceFlag_ = false;
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::FindLocalBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const
{
    HCCL_INFO("[HostCpuRoceChannel::%s] START. Finding buffer addr[0x%llx], len[0x%llx].", __func__, addr, len);
    for (size_t i = 0; i < localRmaBuffers_.size(); i++) {
        uint64_t bufAddr = localRmaBuffers_[i]->GetBufferInfo().first;
        uint32_t bufSize = localRmaBuffers_[i]->GetBufferInfo().second;
        HCCL_INFO("[HostCpuRoceChannel::%s] Comparing with saved localRmaBuffers_[%zu] addr[0x%llx], len[0x%llx].", 
            __func__, i, bufAddr, bufSize);
        if (addr >= bufAddr && (addr + len) <= (bufAddr + bufSize)) {
            targetIdx = i;
            HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. targetIdx[%zu].", __func__, targetIdx);
            return HCCL_SUCCESS;
        }
    }
    HCCL_ERROR("[HostCpuRoceChannel::%s] FAIL. Can not Find Target Buffer addr[0x%llx], len[0x%llx].", 
        __func__, addr, len);
    return HCCL_E_NOT_FOUND;
}

HcclResult HostCpuRoceChannel::FindRemoteBuffer(const uint64_t addr, const uint64_t len, size_t &targetIdx) const
{
    HCCL_INFO("[HostCpuRoceChannel::%s] START. Finding buffer addr[0x%llx], len[0x%llx].", __func__, addr, len);
    for (size_t i = 0; i < rmtRmaBuffers_.size(); i++) {
        uint64_t bufAddr = static_cast<uint64_t>(rmtRmaBuffers_[i]->GetAddr());
        uint32_t bufSize = rmtRmaBuffers_[i]->GetSize();
        HCCL_INFO("[HostCpuRoceChannel::%s] Comparing with saved rmtRmaBuffers_[%zu] addr[0x%llx], len[0x%llx].", 
            __func__, i, bufAddr, bufSize);
        if (addr >= bufAddr && (addr + len) <= (bufAddr + bufSize)) {
            targetIdx = i;
            HCCL_INFO("[HostCpuRoceChannel::%s] SUCCESS. targetIdx[%zu].", __func__, targetIdx);
            return HCCL_SUCCESS;
        }
    }
    HCCL_ERROR("[HostCpuRoceChannel::%s] FAIL. Can not Find Target Buffer addr[0x%llx], len[0x%llx].", 
        __func__, addr, len);
    return HCCL_E_NOT_FOUND;
}

HcclResult HostCpuRoceChannel::ChannelFence()
{
    fenceFlag_ = true;
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