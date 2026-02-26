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
#include "hybrid_mode_config.h"

// Orion
#include "orion_adapter_hccp.h"
#include "exchange_rdma_buffer_dto.h"
#include "rdma_handle_manager.h"
#include "exchange_rdma_conn_dto.h"
#include "sal.h"

// Legacy
#include "legacy/common/binary_stream.h"

// Platform
#include "platform/resource/notify/notify_pool_impl.h"
#include "platform/hccp/inc/network/hccp_common.h"

namespace hcomm {

// ========== RoCECapability 序列化实现 ==========
void RoCECapability::Serialize(Hccl::BinaryStream &stream)
{
    stream << magic;
    stream << version;
    stream << totalLength;
    stream << nodeType;
    stream << static_cast<uint8_t>(nicDeploy);
    stream << static_cast<uint8_t>(commStack);
    stream << static_cast<uint8_t>(syncMode);
}

void RoCECapability::Deserialize(Hccl::BinaryStream &stream)
{
    stream >> magic;
    stream >> version;
    stream >> totalLength;
    stream >> nodeType;
    uint8_t nicDeployVal, commStackVal, syncModeVal;
    stream >> nicDeployVal;
    stream >> commStackVal;
    stream >> syncModeVal;
    nicDeploy = static_cast<NicDeployType>(nicDeployVal);
    commStack = static_cast<CommStackType>(commStackVal);
    syncMode = static_cast<SyncMode>(syncModeVal);
}

// ========== HybridExchangeData 序列化实现 ==========
void HybridExchangeData::Serialize(Hccl::BinaryStream &stream)
{
    // QP 信息
    stream << qpn;
    stream << psn;
    for (int i = 0; i < HCCP_GID_RAW_LEN; i++) {
        stream << gid[i];
    }
    stream << gidIdx;
    
    // Buffer 信息
    stream << bufAddr;
    stream << bufRkey;
    stream << bufLkey;
    stream << bufSize;
    
    // Notify 信息
    stream << dpuNotifyId;
    stream << hostNotifyAddr;
    stream << hostNotifyRkey;
    stream << hostNotifyOffset;
}

void HybridExchangeData::Deserialize(Hccl::BinaryStream &stream)
{
    // QP 信息
    stream >> qpn;
    stream >> psn;
    for (int i = 0; i < HCCP_GID_RAW_LEN; i++) {
        stream >> gid[i];
    }
    stream >> gidIdx;
    
    // Buffer 信息
    stream >> bufAddr;
    stream >> bufRkey;
    stream >> bufLkey;
    stream >> bufSize;
    
    // Notify 信息
    stream >> dpuNotifyId;
    stream >> hostNotifyAddr;
    stream >> hostNotifyRkey;
    stream >> hostNotifyOffset;
}
constexpr u32 FENCE_TIMEOUT_MS = 30 * 1000; // 定义最大等待30秒
constexpr u32 MEM_BLOCK_SIZE = 128;

HostCpuRoceChannel::HostCpuRoceChannel(EndpointHandle endpointHandle, HcommChannelDesc channelDesc)
    : endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HostCpuRoceChannel::~HostCpuRoceChannel() {
    if (resourcesCleaned_) {
        return;
    }
    
    if (isHybridMode_) {
        // 混合模式：释放单 Notify ID
        if (localDpuNotifyId_ != INVALID_DPU_NOTIFY_ID) {
            DpuNotifyManager::GetInstance().FreeSingleNotifyId(localDpuNotifyId_);
            localDpuNotifyId_ = INVALID_DPU_NOTIFY_ID;
        }
        
        // 释放 LocalIpcNotify
        if (hybridNotify_) {
            hybridNotify_->Destroy();
            hybridNotify_.reset();
        }
        
        // 释放 Notify Pool
        if (notifyPool_) {
            notifyPool_->DeInit();
            notifyPool_.reset();
        }
    } else {
        // 原生模式：释放批量 Notify IDs
        HcclResult ret = DpuNotifyManager::GetInstance().FreeNotifyIds(notifyNum_, localDpuNotifyIds_);
        if (ret != HCCL_SUCCESS) {
            HCCL_ERROR("[HostCpuRoceChannel::~HostCpuRoceChannel] exception occurred, HcclResult=[%d]", ret);
        }
    }
    
    resourcesCleaned_ = true;
}

HcclResult HostCpuRoceChannel::ParseInputParam()
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    CHK_PTR_NULL(endpointHandle_);
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();
    CHK_PTR_NULL(rdmaHandle_);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    CHK_PTR_NULL(socket_);
    notifyNum_ = channelDesc_.notifyNum;

    // 3. 从 channelDesc 的 memHandle，获得 bufs_
    CHK_PTR_NULL(channelDesc_.memHandles);
    for (uint32_t i = 0; i < channelDesc_.memHandleNum; ++i) {
        CHK_PTR_NULL(channelDesc_.memHandles[i]);
        Hccl::LocalRdmaRmaBuffer* localRdmaBuffer = static_cast<Hccl::LocalRdmaRmaBuffer *>(channelDesc_.memHandles[i]);
        localRmaBuffers_.emplace_back(localRdmaBuffer);
    }

    return HCCL_SUCCESS;
}

// HcclResult HostCpuRoceChannel::BuildAttr()
// {
//     attr_.devicePhyId = localEp_.loc.device.devPhyId;
//     attr_.opMode      = Hccl::OpMode::OPBASE;
//     attr_.handshakeMsg = {'d', 'p', 'u'}; // TODO: 握手消息定义，怎么组？包括 cann版本号，rankTable CRC等字段
//     return HCCL_SUCCESS;
// }

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

HcclResult HostCpuRoceChannel::Init()
{
    CHK_RET(ParseInputParam());
    // CHK_RET(BuildAttr());
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
            // 进行能力协商（混合模式支持）
            CHK_RET(ExchangeCapability());
            rdmaStatus_ = RdmaStatus::CAP_EXCHANGED;
            break;
        case RdmaStatus::CAP_EXCHANGED:
            // 协商通信模式
            CHK_RET(NegotiateMode());
            // 准备资源
            CHK_RET(CreateQp());
            rdmaStatus_ = RdmaStatus::QP_CREATED;
            break;
        case RdmaStatus::QP_CREATED:
            // 根据模式选择数据交换方式
            if (isHybridMode_) {
                CHK_RET(ExchangeDataHybrid());
            } else {
                CHK_RET(ExchangeData());
            }
            rdmaStatus_ = RdmaStatus::DATA_EXCHANGE;
            break;
        case RdmaStatus::DATA_EXCHANGE:
            CHK_RET(ModifyQp());
            rdmaStatus_ = RdmaStatus::QP_MODIFIED;
            break;
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

    // 同步发送数据
    std::vector<char> sendData{};
    binaryStream.Dump(sendData);
    u64 exchangeDataSize = sendData.size();
    // 同步接口必然是true。不需要分两个状态
    socket_->Send(reinterpret_cast<void *>(sendData.data()), exchangeDataSize);

    // 同步接受数据数据
    HCCL_INFO("[HostCpuRoceChannel::%s] Start to Receive Exchange Data", __func__);
    std::vector<char> recvData{};
    recvData.resize(exchangeDataSize);
    socket_->Recv(reinterpret_cast<void *>(recvData.data()), exchangeDataSize);
    HCCL_INFO("[HostCpuRoceChannel::%s] Receive Exchange Data success, size=%llu, exchangeDataSize=%u", __func__,
              recvData.size(), exchangeDataSize);

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
 
    HCCL_INFO("unpack, bufferNum_=%u, rmtNum=%u", bufferNum_,
        rmtNum);
    if (rmtNum != bufferNum_) {
        HCCL_ERROR("bufferNum_=%u is not equal to rmtNum=%u", bufferNum_, rmtNum);
        return HCCL_E_ROCE_CONNECT;
    }
 
    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        Hccl::ExchangeRdmaBufferDto dto;
        dto.Deserialize(binaryStream);
        if (rmtRmaBuffers_.size() > pos) {
            // 对于之前已经加过的资源，无需追加
            continue;
        }
 
        HCCL_INFO("unpack  pos=%u, dto %s", pos, dto.Describe().c_str());
        if (dto.size == 0) { // size为0，则为 remote 空buffer
            HCCL_INFO("unpack nullptr, pos=%u", pos);
            rmtRmaBuffers_.push_back(nullptr);
        } else { // size非0，则构造一个remote buffer
            rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle_, dto));
            HCCL_WARNING("unpack buffer pos=%u, rmtRmaBuffer=%s", pos, rmtRmaBuffers_.back()->Describe().c_str());
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
    
    // 混合模式：使用内存轮询方式
    if (isHybridMode_) {
        HCCL_INFO("[Hybrid][HostCpuRoceChannel] Using hybrid mode NotifyWaitHybrid");
        return NotifyWaitHybrid(localNotifyIdx, timeout);
    }

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
    
    // 混合模式：使用 Write + Notify 方式
    if (isHybridMode_) {
        HCCL_INFO("[Hybrid][HostCpuRoceChannel] Using hybrid mode WriteWithNotifyHybrid");
        // 注意：WriteWithNotifyHybrid 不是 const 方法，需要 const_cast
        return const_cast<HostCpuRoceChannel*>(this)->WriteWithNotifyHybrid(dst, src, len, remoteNotifyIdx);
    }

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

HcclResult HostCpuRoceChannel::Write(void *dst, const void *src, const uint64_t len) const
{
    return HCCL_E_NOT_SUPPORT;
}

HcclResult HostCpuRoceChannel::Read(void *dst, const void *src, const uint64_t len) const 
{
    return HCCL_E_NOT_SUPPORT;
}

HcclResult HostCpuRoceChannel::ChannelFence() const
{
    struct ibv_wc wc{};
    int wcNum = 2;
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    if (qpInfo.empty()) {
        HCCL_ERROR("[HostCpuRoceChannel::%s] qpInfos is Empty", __func__);
        return HCCL_E_ROCE_CONNECT;
    }

    auto timeout = std::chrono::milliseconds(FENCE_TIMEOUT_MS);
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto actualNum = ibv_poll_cq(qpInfo[0].recvCq, wcNum, &wc);
        CHK_PRT_RET(wc.status != IBV_WC_SUCCESS,
            HCCL_ERROR("[HostCpuRoceChannel][%s] ibv_poll_cq return wc.status is [%d].",
            __func__, wc.status), HCCL_E_NETWORK);

        if (actualNum == wcNum) {
            break;
        }

        if ((std::chrono::steady_clock::now() - startTime) >= timeout) {
            HCCL_ERROR("[HostCpuRoceChannel][%s] call ibv_poll_cq timeout.", __func__);
            return HCCL_E_TIMEOUT;
        }
    }

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

// ========== 混合模式（RoCE Cross-Mode）方法实现 ==========

HcclResult HostCpuRoceChannel::ExchangeCapability()
{
    HCCL_INFO("[Hybrid] Starting capability exchange");
    
    // 1. 构造本地能力信息
    RoCECapability localCap;
    localCap.magic = ROCE_HYBRID_MAGIC;
    localCap.version = ROCE_CAPABILITY_VERSION;
    localCap.nodeType = 0;  // TODO: 获取实际设备类型
    localCap.nicDeploy = NicDeployType::NIC_DEPLOY_HOST;
    localCap.commStack = CommStackType::COMM_STACK_HOST_CPU_ROCE;
    localCap.syncMode = SyncMode::SYNC_MODE_WRITE_IMM;
    localCap.totalLength = sizeof(RoCECapability);
    
    // 2. 发送本地能力
    Hccl::BinaryStream sendStream;
    localCap.Serialize(sendStream);
    std::vector<char> sendData;
    sendStream.Dump(sendData);
    
    uint32_t sendSize = sendData.size();
    socket_->Send(&sendSize, sizeof(sendSize));
    socket_->Send(sendData.data(), sendSize);
    
    // 3. 接收对端能力
    uint32_t recvSize = 0;
    socket_->Recv(&recvSize, sizeof(recvSize));
    CHK_PRT_RET(recvSize > 1024 || recvSize == 0,
        HCCL_ERROR("[Hybrid] Invalid capability size: %u", recvSize),
        HCCL_E_PARA);
    
    std::vector<char> recvData(recvSize);
    socket_->Recv(recvData.data(), recvSize);
    
    // 4. 解析对端能力
    Hccl::BinaryStream recvStream(recvData);
    remoteCap_.Deserialize(recvStream);
    
    // 5. 校验魔数和版本
    CHK_PRT_RET(remoteCap_.magic != ROCE_HYBRID_MAGIC,
        HCCL_ERROR("[Hybrid] Magic mismatch, expected 0x%x, got 0x%x", ROCE_HYBRID_MAGIC, remoteCap_.magic),
        HCCL_E_VERSION);
    
    CHK_PRT_RET(remoteCap_.version < ROCE_MIN_SUPPORTED_VERSION,
        HCCL_ERROR("[Hybrid] Version too old, expected >= %u, got %u", ROCE_MIN_SUPPORTED_VERSION, remoteCap_.version),
        HCCL_E_VERSION);
    
    HCCL_INFO("[Hybrid] Capability exchange success, remote commStack=%u", static_cast<uint8_t>(remoteCap_.commStack));
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NegotiateMode()
{
    if (remoteCap_.commStack == CommStackType::COMM_STACK_TRANSPORT_IBVERBS) {
        // 对端是 TransportIbverbs，切换到混合模式
        isHybridMode_ = true;
        // TransportIbverbs 不支持 Write With Immediate，切换到 Write + Notify
        negotiatedSyncMode_ = SyncMode::SYNC_MODE_WRITE_NOTIFY;
        
        // 初始化兼容层资源
        CHK_RET(InitHybridModeResources());
        HCCL_INFO("[Hybrid] Negotiated to hybrid mode (Write + Notify)");
    } else {
        // 对端也是 HostCpuRoceChannel，使用原生模式
        isHybridMode_ = false;
        negotiatedSyncMode_ = SyncMode::SYNC_MODE_WRITE_IMM;
        HCCL_INFO("[Hybrid] Using native mode (Write With Immediate)");
    }
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::InitHybridModeResources()
{
    // 1. 分配单个 DPU Notify ID（供对端 TransportIbverbs 作为立即数发送目标）
    int allocatedId = DpuNotifyManager::GetInstance().AllocSingleNotifyId();
    CHK_PRT_RET(allocatedId < 0,
        HCCL_ERROR("[Hybrid] Failed to allocate DPU Notify ID"),
        HCCL_E_MEMORY);
    localDpuNotifyId_ = static_cast<uint32_t>(allocatedId);
    
    // 2. 创建 LocalIpcNotify 用于混合模式轮询
    // 创建 notify pool（如果尚未创建）
    if (notifyPool_ == nullptr) {
        notifyPool_ = std::make_unique<NotifyPoolImpl>();
        CHK_RET(notifyPool_->Init(localEp_.loc.device.devPhyId));
    }
    
    // 创建 LocalIpcNotify
    hybridNotify_ = std::make_shared<hccl::LocalIpcNotify>();
    CHK_RET(hybridNotify_->Init(localEp_.loc.device.devPhyId, remoteEp_.loc.device.devPhyId,
        hccl::NotifyLoadType::HOST_NOTIFY));
    
    // 获取 notify 偏移量
    CHK_RET(hybridNotify_->GetNotifyOffset(hybridNotifyOffset_));
    
    // 获取 notify 基地址
    // 使用 HrtRaGetNotifyBaseAddr 获取基地址
    uint64_t notifyBaseVa = 0;
    uint64_t notifyTotalSize = 0;
    CHK_RET(HrtRaGetNotifyBaseAddr(rdmaHandle_, &notifyBaseVa, &notifyTotalSize,
        []() -> bool { return false; }));
    
    // 计算实际 notify 地址
    localNotifyBaseAddr_ = notifyBaseVa + hybridNotifyOffset_;
    
    // 获取 notify 的 rkey（用于注册 MR）
    struct MrInfoT mrInfo = {nullptr};
    CHK_RET(HrtRaGetNotifyMrInfo(localEp_.loc.device.devPhyId, rdmaHandle_, &mrInfo));
    localNotifyRkey_ = mrInfo.rkey;
    
    HCCL_INFO("[Hybrid] Allocated DPU Notify ID: %u, notify base=0x%llx, offset=0x%llx, va=0x%llx",
        localDpuNotifyId_, notifyBaseVa, hybridNotifyOffset_, localNotifyBaseAddr_);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::ExchangeDataHybrid()
{
    HCCL_INFO("[Hybrid] Starting hybrid data exchange");
    
    // --- 1. 构造并发送本地数据 ---
    HybridExchangeData localData;
    
    // 填充 QP 信息
    auto qpInfo = connections_[0]->GetQpInfo();
    struct QpAttr localQpAttr;
    RaGetQpAttr(qpInfo.qpHandle, &localQpAttr);
    localData.qpn = localQpAttr.qpn;
    localData.psn = localQpAttr.psn;
    localData.gidIdx = localQpAttr.gidIdx;
    memcpy(localData.gid, localQpAttr.gid, HCCP_GID_RAW_LEN);
    
    // 填充 Buffer 信息
    localData.bufAddr = localRmaBuffers_[0]->GetAddr();
    localData.bufRkey = localRmaBuffers_[0]->GetRkey();
    localData.bufLkey = localRmaBuffers_[0]->GetLkey();
    localData.bufSize = localRmaBuffers_[0]->GetSize();
    
    // 填充 Notify 信息（单 Notify ID，混合模式设计）
    localData.dpuNotifyId = localDpuNotifyId_;
    
    // 填充 Host Notify 地址信息（供对端 TransportIbverbs 写入）
    localData.hostNotifyAddr = localNotifyBaseAddr_;
    localData.hostNotifyRkey = localNotifyRkey_;
    localData.hostNotifyOffset = hybridNotifyOffset_;
    
    // 序列化并发送
    Hccl::BinaryStream stream;
    localData.Serialize(stream);
    std::vector<char> sendData;
    stream.Dump(sendData);
    
    uint32_t sendSize = sendData.size();
    socket_->Send(&sendSize, sizeof(sendSize));
    socket_->Send(sendData.data(), sendSize);
    
    // --- 2. 接收对端数据 ---
    uint32_t recvSize = 0;
    socket_->Recv(&recvSize, sizeof(recvSize));
    CHK_PRT_RET(recvSize > 4096 || recvSize == 0,
        HCCL_ERROR("[Hybrid] Invalid receive size: %u", recvSize),
        HCCL_E_PARA);
    
    std::vector<char> recvData(recvSize);
    socket_->Recv(recvData.data(), recvSize);
    
    // --- 3. 解析对端数据 ---
    Hccl::BinaryStream recvStream(recvData);
    HybridExchangeData remoteData;
    remoteData.Deserialize(recvStream);
    
    // 校验关键字段
    CHK_PRT_RET(remoteData.dpuNotifyId == INVALID_DPU_NOTIFY_ID,
        HCCL_ERROR("[Hybrid] Remote DPU Notify ID is invalid"),
        HCCL_E_PARA);
    
    // 保存对端信息
    rmtConnDto_.qpn_ = remoteData.qpn;
    rmtConnDto_.psn_ = remoteData.psn;
    rmtConnDto_.gid_idx_ = remoteData.gidIdx;
    memcpy(rmtConnDto_.gid_, remoteData.gid, HCCP_GID_RAW_LEN);
    
    // 保存对端 Notify 地址信息（HostCpuRoceChannel 发送时使用）
    remoteHostNotifyAddr_ = remoteData.hostNotifyAddr;
    remoteHostNotifyRkey_ = remoteData.hostNotifyRkey;
    
    // 保存对端 DPU Notify ID（TransportIbverbs 发送时使用）
    // 注意：TransportIbverbs 作为立即数发送此 ID
    
    HCCL_INFO("[Hybrid] Data exchange success, remote QPN=%u", remoteData.qpn);
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::WriteWithNotifyHybrid(
    void *dst, const void *src, uint64_t len, uint32_t remoteNotifyIdx)
{
    CHK_PTR_NULL(src);
    CHK_PTR_NULL(dst);
    HCCL_INFO("[Hybrid] WriteWithNotifyHybrid start, len=%lu", len);
    
    // 参数校验
    CHK_PRT_RET(localRmaBuffers_.empty(),
        HCCL_ERROR("[Hybrid] localRmaBuffer is Empty"),
        HCCL_E_ROCE_CONNECT);
    
    std::vector<Hccl::QpInfo> qpInfo = GetQpInfos();
    CHK_PRT_RET(qpInfo.empty(),
        HCCL_ERROR("[Hybrid] qpInfos is Empty"),
        HCCL_E_ROCE_CONNECT);
    
    // 获取本地 buffer 信息
    uint64_t localAddr = localRmaBuffers_[0]->GetAddr();
    uint32_t localLkey = localRmaBuffers_[0]->GetLkey();
    uint64_t bufferSize = localRmaBuffers_[0]->GetSize();
    
    // 校验数据长度
    CHK_PRT_RET(len > bufferSize,
        HCCL_ERROR("[Hybrid] Data length %lu exceeds buffer size %lu", len, bufferSize),
        HCCL_E_PARA);
    
    // 构造发送 WR 链：数据 WR + Notify WR
    struct ibv_send_wr dataWr{};
    struct ibv_send_wr notifyWr{};
    struct ibv_send_wr *badWr = nullptr;
    struct ibv_sge dataSge{};
    struct ibv_sge notifySge{};
    
    // 1. 数据 WR（RDMA Write）
    dataSge.addr = reinterpret_cast<uint64_t>(src);
    dataSge.length = len;
    dataSge.lkey = localLkey;
    
    dataWr.wr_id = 0;
    dataWr.opcode = IBV_WR_RDMA_WRITE;
    dataWr.send_flags = IBV_SEND_SIGNALED;  // 需要 CQE 确认完成
    dataWr.sg_list = &dataSge;
    dataWr.num_sge = 1;
    dataWr.wr.rdma.remote_addr = reinterpret_cast<uint64_t>(dst);
    dataWr.wr.rdma.rkey = rmtRmaBuffers_[0]->GetRkey();
    
    // 2. Notify WR（写入对端 TransportIbverbs 的 Notify 内存）
    notifySge.addr = reinterpret_cast<uint64_t>(&notifySignalValue_);
    notifySge.length = sizeof(uint64_t);
    notifySge.lkey = localLkey;  // 使用本地 buffer 的 lkey
    
    notifyWr.wr_id = 1;
    notifyWr.opcode = IBV_WR_RDMA_WRITE;
    notifyWr.send_flags = IBV_SEND_SIGNALED;
    notifyWr.sg_list = &notifySge;
    notifyWr.num_sge = 1;
    // Notify 写入对端 hostNotifyAddr 的偏移位置
    notifyWr.wr.rdma.remote_addr = remoteHostNotifyAddr_ + remoteNotifyIdx * sizeof(uint64_t);
    notifyWr.wr.rdma.rkey = remoteHostNotifyRkey_;
    
    // 链接 WR 链：dataWr -> notifyWr
    dataWr.next = &notifyWr;
    notifyWr.next = nullptr;
    
    // 3. 下发 WR 链
    int32_t ret = ibv_post_send(qpInfo[0].qp, &dataWr, &badWr);
    CHK_PRT_RET(ret != 0,
        HCCL_ERROR("[Hybrid] ibv_post_send failed, ret=%d", ret),
        HCCL_E_NETWORK);
    
    HCCL_INFO("[Hybrid] WriteWithNotifyHybrid success");
    return HCCL_SUCCESS;
}

HcclResult HostCpuRoceChannel::NotifyWaitHybrid(uint32_t localNotifyIdx, uint32_t timeout)
{
    HCCL_INFO("[Hybrid] NotifyWaitHybrid start, idx=%u", localNotifyIdx);
    
    // 参数校验：混合模式只支持单 Notify，索引必须为 0
    CHK_PRT_RET(localNotifyIdx != 0,
        HCCL_ERROR("[Hybrid] Hybrid mode only supports single notify, index: %u", localNotifyIdx),
        HCCL_E_PARA);
    
    // 使用配置的超时时间和轮询间隔
    uint32_t pollTimeout = (timeout == 0) ? 
        HybridModeConfig::GetInstance().GetPollTimeoutMs() : timeout;
    uint32_t pollInterval = HybridModeConfig::GetInstance().GetPollIntervalMs();
    
    // 使用原子操作读取 Notify 内存
    std::atomic<uint64_t>* notifyAddr = reinterpret_cast<std::atomic<uint64_t>*>(
        localNotifyBaseAddr_ + localNotifyIdx * sizeof(uint64_t));
    const uint64_t expectedValue = 1;
    
    auto startTime = std::chrono::steady_clock::now();
    auto waitTime = std::chrono::milliseconds(pollTimeout);
    
    while (true) {
        // 使用原子操作读取，确保内存可见性
        if (notifyAddr->load(std::memory_order_acquire) == expectedValue) {
            // 读取成功后清零，为下一次通知做准备
            notifyAddr->store(0, std::memory_order_release);
            HCCL_INFO("[Hybrid] NotifyWaitHybrid success");
            return HCCL_SUCCESS;
        }
        
        // 检查超时
        if ((std::chrono::steady_clock::now() - startTime) >= waitTime) {
            HCCL_ERROR("[Hybrid] NotifyWaitHybrid timeout");
            return HCCL_E_TIMEOUT;
        }
        
        // 低频通信场景：简单睡眠即可
        SaluSleep(pollInterval);
    }
}

} // namespace hcomm