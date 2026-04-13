/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicpu_ts_uboe_channel.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"
#include "hcomm_c_adpt.h"
#include "exception_handler.h"
#include "comm_mems.h"
#include "user_remote_mem_getter.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"
#include "tp_manager.h"
#include "exchange_ub_buffer_dto.h"
#include "exchange_ub_conn_dto.h"

namespace hcomm {

AicpuTsUboeChannel::AicpuTsUboeChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc):
    endpointHandle_(endpointHandle), channelDesc_(channelDesc) {}

HcclResult AicpuTsUboeChannel::Makebufs(HcommMemHandle *memHandles, uint32_t memHandleNum,
    std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    bufs.clear();
    for (uint32_t i = 0; i < memHandleNum; ++i) {
        auto locMemInfo = reinterpret_cast<CommMemInfo *>(memHandles[i]);
        HCCL_INFO("[AicpuTsUboeChannel][%s] tag[%s]", __func__, locMemInfo->memTag);
        bufs.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
            reinterpret_cast<uintptr_t>(locMemInfo->mem.addr), locMemInfo->mem.size,
            hccl::ConvertCommToHcclMemType(locMemInfo->mem.type), locMemInfo->memTag)
        ));
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::ParseInputParam() 
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    Endpoint* localEpPtr = reinterpret_cast<Endpoint*>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    HCCL_INFO("[AicpuTsUboeChannel][%s] localProtocol[%d]", __func__, localEp_.protocol);

    // 2. 从 channelDesc_，获得 remoteEp_, socket_ 和 notifyNum
    remoteEp_ = channelDesc_.remoteEndpoint;
    socket_ = reinterpret_cast<Hccl::Socket*>(channelDesc_.socket);
    notifyNum_ = channelDesc_.notifyNum;

    if (channelDesc_.exchangeAllMems) {
        // 3. Get memHandles from endpoint
        HCCL_INFO("[AicpuTsUboeChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalUbRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(HcommMemGetAllMemHandles(
            endpointHandle_, reinterpret_cast<void**>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AicpuTsUboeChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalUbRmaBuffer> &localUbRmaBuffer = memHandles[i];
            HCCL_INFO("[AicpuTsUboeChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].",
                __func__, i, localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(
                localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str())
            ));
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_INFO("[AicpuTsUboeChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_RET(Makebufs(channelDesc_.memHandles, channelDesc_.memHandleNum, bufs_));
    }

    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::BuildConnection()
{
    Hccl::OpMode        opMode = Hccl::OpMode::OPBASE;
    bool                devUsed  = true;  // aicpu 为 true
    Hccl::LinkProtocol  protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));

    // TODO UBOE OK 本端EID去rmaManager里面查询，对端通过Socket交换
    IpAddress locIpv4Addr;
    IpAddress rmtIpv4Addr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locIpv4Addr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtIpv4Addr));
    HCCL_INFO("[AicpuTsUboeChannel][%s] LinkProtocol[%s], locIpv4Addr[%s], rmtIpv4Addr[%s]", 
        __func__, protocol.Describe().c_str(), locIpv4Addr.Describe().c_str(), rmtIpv4Addr.Describe().c_str());
    RdmaHandleManager::GetInstance().GetEidByIpv4Addr(locIpv4Addr, locAddr_);
    HCCL_INFO("[RmaConnManager::%s] locAddr_[%s]", __func__, locAddr_.Describe().c_str());

    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    Hccl::TpManager::GetInstance(deviceLogicId).Init();

    // TODO UBOE OK 新增AicpuTsUboeChannel，里面增加DevUboeConnection，不在此处增加分支
    std::unique_ptr<Hccl::DevUbConnection> ubConn = 
        std::make_unique<Hccl::DevUbUboeConnection>(rdmaHandle, locAddr_, rmtAddr_, opMode, devUsed, jfcMode, locIpv4Addr, rmtIpv4Addr);
    CHK_SMART_PTR_NULL(ubConn);

    commonRes_.connVec.clear();
    commonRes_.connVec.emplace_back(ubConn.get());
    connections_.clear();
    connections_.push_back(std::move(ubConn));

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::BuildNotify()
{
    localNotifies_.clear();
    commonRes_.notifyVec.clear();
    bool devUsed = true;
    for (uint32_t i = 0; i < notifyNum_; ++i) {
        std::unique_ptr<Hccl::UbLocalNotify> notifyPtr = nullptr;
        EXECEPTION_CATCH(
            notifyPtr = std::make_unique<Hccl::UbLocalNotify>(rdmaHandle_, devUsed),
            return HCCL_E_PTR
        );
        commonRes_.notifyVec.push_back(notifyPtr.get());
        localNotifies_.push_back(std::move(notifyPtr));
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::FillTagVec(std::vector<Hccl::LocalRmaBuffer *> &bufferVec,
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &localUserMemTag)
{
    uint64_t bufferNum = bufferVec.size();
    if (bufferNum == 0) {
        HCCL_WARNING("[AicpuTsUboeChannel][FillTagVec] bufferNum is 0.");
    }
    if (bufferNum > MAX_BUFFER_NUM) {
        HCCL_ERROR("[AicpuTsUboeChannel][FillTagVec] bufferNum[%u] exceeds limit[%u]", bufferNum, MAX_BUFFER_NUM);
        return HCCL_E_PARA;
    }
    HCCL_INFO("[AicpuTsUboeChannel][FillTagVec] bufferNum[%u]", bufferNum);
    localUserMemTag.clear();
    uint32_t index = 0;
    for (auto &localRmaBuffer : bufferVec) {
        std::array<char, HCCL_RES_TAG_MAX_LEN> memTag{};
        if (localRmaBuffer == nullptr) {
            HCCL_ERROR("[AicpuTsUboeChannel][FillTagVec] localRmaBuffer is nullptr. memHandleNum[%u]", index);
            return HCCL_E_PARA;
        } else {
            CHK_PTR_NULL(localRmaBuffer->GetBuf());
            std::string tag = localRmaBuffer->GetBuf()->GetMemTag();
            if (tag.size() >= HCCL_RES_TAG_MAX_LEN) {
                HCCL_ERROR("[AicpuTsUboeChannel][FillTagVec] tagSize exceeds limit[%u]", HCCL_RES_TAG_MAX_LEN);
                return HCCL_E_PARA;
            }
            CHK_SAFETY_FUNC_RET(memcpy_s(memTag.data(), memTag.size(), tag.c_str(), tag.size()));
            HCCL_INFO("[AicpuTsUboeChannel][FillTagVec] memHandleNum[%u] memTag[%s]", index, memTag.data());
        }
        localUserMemTag.push_back(memTag);
        index++;
    }
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    bufferVecTemp_.clear();
    for (size_t i = 0; i < bufs.size(); i++) {
        std::unique_ptr<Hccl::LocalUbRmaBuffer> bufferPtr = nullptr;
        EXECEPTION_CATCH(
            bufferPtr = std::make_unique<Hccl::LocalUbRmaBuffer>(bufs[i], rdmaHandle_),
            return HCCL_E_PTR
        );
        bufferVecTemp_.push_back(bufferPtr.get());
        commonRes_.bufferVec.push_back(bufferPtr.get());
        localRmaBuffers_.push_back(std::move(bufferPtr));
    }
    CHK_RET(FillTagVec(commonRes_.bufferVec, localUserMemTag_));

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AicpuTsUboeChannel][%s] socket ptr is NULL, rebuildSocket", __func__);

    Hccl::IpAddress ipaddr{};
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, ipaddr));
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(static_cast<Hccl::RankId>(localEp_.loc.device.devPhyId), type, 0, ipaddr);
    Hccl::SocketHandle socketHandle = Hccl::SocketHandleManager::GetInstance().Create(localEp_.loc.device.devPhyId, localPort);
    EXECEPTION_CATCH(serverSocket_ = std::make_unique<Hccl::Socket>(socketHandle, ipaddr, 60001, 
        ipaddr, "server", Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE), return HCCL_E_PARA);
    HCCL_INFO("[AicpuTsUboeChannel][%s] listen_socket_info[%s]", __func__, serverSocket_->Describe().c_str());
    EXECEPTION_CATCH(serverSocket_->Listen(), return HCCL_E_INTERNAL);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AicpuTsUboeChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag, noRankId);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));
    isRecvFirst_ = socket_->GetRole() == Hccl::SocketRole::CLIENT ? true : false;

    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::Init()
{
    CHK_RET(ParseInputParam());
    CHK_RET(BuildSocket());
    CHK_RET(BuildNotify());
    localRmaBuffers_.clear();
    commonRes_.bufferVec.clear();
    CHK_RET(BuildBuffer(bufs_));
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    *notifyNum = this->notifyNum_;
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return memTransport_->GetRemoteMem(remoteMem, memNum, memTags);
}

bool AicpuTsUboeChannel::IsSocketReady()
{
    if (socket_ == nullptr) {
        HCCL_ERROR("[%s] socket is nullptr, please check", __func__);
        channelStatus = ChannelStatus::INVALID;
        return false;
    }

    Hccl::SocketStatus socketStatus = socket_->GetAsyncStatus();
    if (socketStatus == Hccl::SocketStatus::OK) {
        channelStatus = ChannelStatus::SOCKET_OK;
        return true;
    } else if (socketStatus == Hccl::SocketStatus::TIMEOUT) {
        channelStatus = ChannelStatus::SOCKET_TIMEOUT;
        return false;
    }

    return false;
}

bool AicpuTsUboeChannel::IsResReady()
{
    for (auto &it : commonRes_.connVec) {
        if (it == nullptr) {
            Hccl::THROW<Hccl::InternalException>("[AicpuTsUboeChannel::%s] failed, connection pointer is nullptr", __func__);
        }
        Hccl::RmaConnType connType = it->GetRmaConnType();
        if (connType != Hccl::RmaConnType::UB) {
            Hccl::THROW<Hccl::InternalException>("[AicpuTsUboeChannel::%s] connection type[%s] is not ub",
                __func__, connType.Describe().c_str());
        }

        auto status = it->GetStatus();
        if (status != Hccl::RmaConnStatus::EXCHANGEABLE &&
            status != Hccl::RmaConnStatus::READY) {
            return false;
        }
    }
    HCCL_INFO("[AicpuTsUboeChannel::%s] all resources ready.", __func__);
    return true;
}

bool AicpuTsUboeChannel::IsConnsReady() //待修改
{
    if (BuildConnection() != HCCL_SUCCESS) {
        HCCL_ERROR("[AicpuTsUboeChannel::%s] failed to build connections", __func__);
        return false;
    }

    for (u32 i = 0; i < connNum_; i++) {
        if (commonRes_.connVec[i]->GetStatus() != Hccl::RmaConnStatus::READY) {
            return false;
        }
    }
    HCCL_INFO("[AicpuTsUboeChannel::%s] conns are ready.", __func__);
    return true;
}

void AicpuTsUboeChannel::EidPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << locAddr_.GetUniqueId();
}

void AicpuTsUboeChannel::NotifyVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << notifyNum_;
    u32 pos = 0;
    for (auto &it : commonRes_.notifyVec) {
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
        dto->Serialize(binaryStream);
        HCCL_INFO("[AicpuTsUboeChannel::%s] pack notify pos=%u, dto %s", 
            __func__, pos, dto->Describe().c_str());
        pos++;
    }
}

void AicpuTsUboeChannel::BufferVecPack(Hccl::BinaryStream &binaryStream, std::vector<Hccl::LocalRmaBuffer *> &bufferVec,
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> &tagVec)
{
    binaryStream << static_cast<u32>(bufferVec.size());
    u32 pos = 0;
    for (auto &it : bufferVec) {
        binaryStream << pos;
        if (it != nullptr) { // 非空的buffer，从buffer中获取 dto
            std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
            dto->Serialize(binaryStream);
            HCCL_INFO("[AicpuTsUboeChannel::%s] pack buffer pos=%u dto %s", 
                __func__, pos, dto->Describe().c_str());
        } else { // 空的buffer，dto所有字段为0(size=0)
            Hccl::ExchangeUbBufferDto exchangeDto;
            exchangeDto.Serialize(binaryStream);
            HCCL_INFO("[AicpuTsUboeChannel::%s] pack buffer pos=%u, dto is null %s", 
                __func__, pos, exchangeDto.Describe().c_str());
        }
        pos++;
    }

    for (const auto& tag : tagVec) {
        // 逐个字节传输
        for (uint32_t i = 0; i < HCCL_RES_TAG_MAX_LEN; ++i) {
            binaryStream << static_cast<u8>(tag[i]);
        }
    }
}

void AicpuTsUboeChannel::ConnVecPack(Hccl::BinaryStream &binaryStream)
{
    binaryStream << connNum_;
    u32 pos = 0;
    for (auto &it : commonRes_.connVec) {
        binaryStream << pos;
        std::unique_ptr<Hccl::Serializable> dto = it->GetExchangeDto();
        dto->Serialize(binaryStream);
        HCCL_INFO("[AicpuTsUboeChannel::%s] pack connection pos=%u, dto %s", 
            __func__, pos, dto->Describe().c_str());
        pos++;
    }
}

void AicpuTsUboeChannel::SendDataSize()
{
    bufferNum_    = commonRes_.bufferVec.size(); // 需要交换的buffer数量
    connNum_      = commonRes_.connVec.size();

    HCCL_INFO("notifyNum=%u, bufferNum=%u, connNum=%u", notifyNum_, bufferNum_, connNum_);

    Hccl::BinaryStream binaryStream;
    EidPack(binaryStream);
    NotifyVecPack(binaryStream);
    BufferVecPack(binaryStream, commonRes_.bufferVec, localUserMemTag_);
    ConnVecPack(binaryStream);

    binaryStream.Dump(sendData_);
    u32 sendSize = sendData_.size();

    // 发送数据包尺寸
    socket_->SendAsync(reinterpret_cast<u8 *>(&sendSize), sizeof(sendSize));
    HCCL_INFO("[AicpuTsUboeChannel::%s] Send size[%u] of data success. [%zu] bytes sent.",
        __func__, sendSize, sizeof(sendSize));
}

void AicpuTsUboeChannel::RecvDataSize()
{
    // 接收数据包尺寸
    socket_->RecvAsync(reinterpret_cast<u8 *>(&recvDataSize_), sizeof(recvDataSize_));
    HCCL_INFO("[AicpuTsUboeChannel::%s] Receive Data Size", __func__);
}

void AicpuTsUboeChannel::SendExchangeData()
{
    socket_->SendAsync(reinterpret_cast<u8 *>(sendData_.data()), sendData_.size());
    HCCL_INFO("[AicpuTsUboeChannel::%s] send data, size=%llu", __func__, sendData_.size());
}

void AicpuTsUboeChannel::RecvExchangeData()
{
    recvData_.resize(recvDataSize_);
    socket_->RecvAsync(reinterpret_cast<u8 *>(recvData_.data()), recvData_.size());
    HCCL_INFO("[AicpuTsUboeChannel::%s] recv data", __func__);
}

bool AicpuTsUboeChannel::RecvDataProcess()
{
    HCCL_INFO("RecvDataProcess: size=%llu, recvDataSize=%u", recvData_.size(), recvDataSize_);
    Hccl::BinaryStream binaryStream(recvData_);
    // TODO UBOE OK 解析对端Eid，存到UboeChannel
    RmtEidUnpackProc(binaryStream, rmtAddr_);
    RmtBufferVecUnpackProc(notifyNum_, binaryStream, rmtNotifyVec, UboeRmtBufType::NOTIFY);
    RmtBufferVecUnpackProc(bufferNum_, binaryStream, rmtBufferVec, UboeRmtBufType::BUFFER);
    return ConnVecUnpackProc(binaryStream);
}

void AicpuTsUboeChannel::RmtEidUnpackProc(Hccl::BinaryStream &binaryStream, Hccl::IpAddress& rmtAddr)
{
    Hccl::IpAddress rmtEidAddr(binaryStream);
    rmtAddr = rmtEidAddr;
    HCCL_INFO("[RmaConnManager::%s] rmtAddr[%s]", __func__, rmtAddr.Describe().c_str());
}

void AicpuTsUboeChannel::RmtBufferVecUnpackProc(u32 locNum, Hccl::BinaryStream &binaryStream, RemoteBufferVec &bufferVec,
                                            UboeRmtBufType type)
{
    u32 rmtNum;
    binaryStream >> rmtNum;
    if (type == UboeRmtBufType::BUFFER && rmtNum > MAX_BUFFER_NUM) {
        MACRO_THROW(Hccl::InvalidParamsException,
            Hccl::StringFormat("[AicpuTsUboeChannel][RmtBufferVecUnpackProc] rmtNum[%u] exceeds limit[%u]",
            rmtNum, MAX_BUFFER_NUM));
    }

    // 允许本端和远端交换内存数量不一致
    HCCL_INFO("unpack %s, locNum=%u, rmtNum=%u", type.Describe().c_str(), locNum, rmtNum);

    for (u32 i = 0; i < rmtNum; i++) {
        u32 pos;
        binaryStream >> pos;
        Hccl::ExchangeUbBufferDto dto;
        dto.Deserialize(binaryStream);
        if (bufferVec.size() > pos) {
            // 对于之前已经加过的资源，无需追加
            continue;
        }
        HCCL_INFO("unpack %s pos=%u, dto %s", type.Describe().c_str(), pos, dto.Describe().c_str());
        if (dto.size == 0) { // size为0，则为 remote 空buffer
            HCCL_INFO("unpack nullptr, pos=%u", pos);
            bufferVec.push_back(nullptr);
        } else { // size非0，则构造一个remote buffer
            bufferVec.push_back(std::make_unique<Hccl::RemoteUbRmaBuffer>(rdmaHandle_, dto));
            HCCL_INFO("unpack buffer pos=%u, rmtRmaBuffer=%s", pos, bufferVec.back()->Describe().c_str());
        }
    }

    rmtMemTagTemp_.clear();
    if (type == UboeRmtBufType::BUFFER) {
        rmtMemTagTemp_.resize(rmtNum);
        for (auto& tag : rmtMemTagTemp_) {
            for (uint32_t i = 0; i < HCCL_RES_TAG_MAX_LEN; ++i) {
                u8 byte;
                binaryStream >> byte;
                tag[i] = static_cast<char>(byte);
            }
        }
    }
    remoteUserMemTag_.insert(remoteUserMemTag_.end(), rmtMemTagTemp_.begin(), rmtMemTagTemp_.end());
}

bool AicpuTsUboeChannel::ConnVecUnpackProc(Hccl::BinaryStream &binaryStream)
{
    u32 rmtConnNum;
    binaryStream >> rmtConnNum;
    HCCL_INFO("start unpack conn connNum=%u, rmtConnNum=%u", connNum_, rmtConnNum);
    if (connNum_ != rmtConnNum) {
        MACRO_THROW(Hccl::InvalidParamsException,
                    Hccl::StringFormat("connNum=%u is not equal to rmtConnNum=%u", connNum_, rmtConnNum));
    }

    bool result = false; // 不需要发送 finish
    for (u32 i = 0; i < rmtConnNum; i++) {
        u32 pos;
        binaryStream >> pos;
        Hccl::ExchangeUbConnDto rmtDto;
        rmtDto.Deserialize(binaryStream);
        HCCL_INFO("unpack connection pos=%u dto %s", pos, rmtDto.Describe().c_str());
        if (commonRes_.connVec[i]->GetStatus() != Hccl::RmaConnStatus::READY) {
            HCCL_INFO("parse and import pos=%u, rmt dto to connection[%s]", pos,
                       commonRes_.connVec[i]->Describe().c_str());
            commonRes_.connVec[i]->ParseRmtExchangeDto(rmtDto);
            commonRes_.connVec[i]->ImportRmtDto();
            result = true; // connection 建链，需要发送finish
        }
    }
    return result;
}

void AicpuTsUboeChannel::SendFinish()
{
    HCCL_INFO("start send Finish Msg [%s]", FINISH_MSG);
    sendFinishMsg_ = std::vector<char>(FINISH_MSG, FINISH_MSG + FINISH_MSG_SIZE);
    socket_->SendAsync(reinterpret_cast<u8 *>(sendFinishMsg_.data()), FINISH_MSG_SIZE);
    HCCL_INFO("end send Finish Msg [%s]", FINISH_MSG);
}

void AicpuTsUboeChannel::RecvFinish()
{
    recvFinishMsg_.resize(FINISH_MSG_SIZE);
    HCCL_INFO("start recv Finish Msg [%s]", FINISH_MSG);
    socket_->RecvAsync(reinterpret_cast<u8 *>(recvFinishMsg_.data()), FINISH_MSG_SIZE);
    HCCL_INFO("end recv Finish Msg [%s]", FINISH_MSG);
}

ChannelStatus AicpuTsUboeChannel::GetStatus()
{
    if (channelStatus == ChannelStatus::READY) {
        return channelStatus;
    } else if (channelStatus == ChannelStatus::INIT) {
        uboeStatus = UboeStatus::INIT;
    }

    if (!IsSocketReady()) {
        return channelStatus;
    }

    switch (uboeStatus) {
        case UboeStatus::INIT:
            uboeStatus = UboeStatus::SEND_SIZE;
            channelStatus = ChannelStatus::SOCKET_OK;
            break;
        case UboeStatus::SEND_SIZE:
            if (IsResReady()) {
                SendDataSize();
                uboeStatus = UboeStatus::RECV_SIZE;
            }
            break;
        case UboeStatus::RECV_SIZE:
            RecvDataSize();
            uboeStatus = isRecvFirst_ ? UboeStatus::RECV_DATA : UboeStatus::SEND_DATA;
            break;
        case UboeStatus::SEND_DATA:
            SendExchangeData();
            uboeStatus = isRecvFirst_ ? UboeStatus::PROCESS_DATA : UboeStatus::RECV_DATA;
            break;
        case UboeStatus::RECV_DATA:
            RecvExchangeData();
            uboeStatus = isRecvFirst_ ? UboeStatus::SEND_DATA : UboeStatus::PROCESS_DATA;
            break;
        case UboeStatus::PROCESS_DATA:
            if (RecvDataProcess()) { // 收消息中，如果设置到connection的建链，则需要发送 finish
                uboeStatus = UboeStatus::SEND_FIN;
            } else { // 不需要发送finish，则将transport状态调整为 ready
                channelStatus = ChannelStatus::READY;
                uboeStatus = UboeStatus::READY;
            }
            break;
        case UboeStatus::SEND_FIN:
            if (IsConnsReady()) {
                SendFinish();
                uboeStatus = UboeStatus::RECV_FIN;
            }
            break;
        case UboeStatus::RECV_FIN:
            RecvFinish();
            uboeStatus = UboeStatus::SET_READY;
            break;
        case UboeStatus::SET_READY:
            channelStatus = ChannelStatus::READY;
            uboeStatus = UboeStatus::READY;
            break;
        default:
            break;
    }
    return channelStatus;
}

// HcclResult SetModuleDataName(Hccl::ModuleData &module, const std::string &name)
// {
//     // int ret = strcpy_s(module.name, sizeof(module.name), name.c_str());
//     // if (ret != 0) {
//     //     HCCL_ERROR("[SetModuleDataName] strcpy_s name %s failed", name.c_str());
//     //     return HCCL_E_INTERNAL;
//     // }
//     // 待适配
//     return HCCL_SUCCESS;
// }

HcclResult AicpuTsUboeChannel::PackOpData(std::vector<char> &data)
{
    // std::vector<Hccl::ModuleData> dataVec;
    // dataVec.resize(Hccl::AicpuResMgrType::__COUNT__);

    // Hccl::AicpuResMgrType resType = Hccl::AicpuResMgrType::STREAM;
    // CHK_RET(SetModuleDataName(dataVec[resType], "AicpuTsUboeChannel"));

    // std::vector<char> result;
    // Hccl::BinaryStream      binaryStream;
    // binaryStream << memTransport_->GetUniqueIdV2();

    // binaryStream.Dump(result);

    // dataVec[resType].data = result;

    // Hccl::AicpuResPackageHelper helper;
    // data = helper.GetPackedData(dataVec);
    // 待适配
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::H2DResPack(std::vector<char>& buffer)
{
    // CHK_RET(PackOpData(buffer));
    // HCCL_INFO("[AicpuTsUboeChannel][%s] Pack Buffer data[%p], Pack Buffer size[%zu].",
    //     __func__, buffer.data(), buffer.size());
    // 待适配
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::Clean()
{
    // 该模式当前不支持N秒快恢
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::Resume()
{
    // 该模式当前不支持N秒快恢
    return HCCL_SUCCESS;
}

HcclResult AicpuTsUboeChannel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    // 待适配
    return HCCL_SUCCESS;
}

} // namespace hcomm
