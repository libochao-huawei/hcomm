/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aiv_urma_channel.h"
#include "../../../endpoints/endpoint.h"
#include "orion_adpt_utils.h"
#include "comm_mems.h"

#include "hcomm_c_adpt.h"
#include "exception_handler.h"

// Orion
#include "coll_alg_param.h"
#include "topo_common_types.h"
#include "virtual_topo.h"
#include "aicpu_res_package_helper.h"
#include "tp_manager.h"
#include "orion_adapter_hccp.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace hcomm {
constexpr uint32_t QUEUE_NUM_MAX = 20;

namespace {
uintptr_t AlignUp(uintptr_t addr, uintptr_t align)
{
    return (addr + align - 1) & ~(align - 1);
}

HcclResult MemCopyHostToDev(void *devPtr, const void *hostPtr, size_t size)
{
    if (devPtr == nullptr || hostPtr == nullptr) {
        HCCL_ERROR("[memCopyHostToDev] nullptr is invalid");
        return HCCL_E_PTR;
    }
    if (size == 0) {
        HCCL_ERROR("[memCopyHostToDev] size is 0");
        return HCCL_E_PARA;
    }

    aclError aclRet = aclrtMemcpy(devPtr, size, hostPtr, size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[memCopyHostToDev] aclrtMemcpy failed, error=%d", aclRet);
        return HCCL_E_MEMORY;
    }

    return HCCL_SUCCESS;
}

size_t CalcSingleChannelSize(const ChannelEntity &ch)
{
    size_t totalSize = sizeof(ChannelEntity);
    totalSize = static_cast<size_t>(AlignUp(totalSize, 8));

    if (ch.localNotifyNum > 0 && ch.localNotifyAddr != nullptr) {
        totalSize += ch.localNotifyNum * sizeof(Notify);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    if (ch.remoteNotifyNum > 0 && ch.remoteNotifyAddr != nullptr) {
        totalSize += ch.remoteNotifyNum * sizeof(Notify);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    if (ch.localBufferNum > 0 && ch.localBufferAddr != nullptr) {
        totalSize += ch.localBufferNum * sizeof(ProtectionInfo);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    if (ch.remoteBufferNum > 0 && ch.remoteBufferAddr != nullptr) {
        totalSize += ch.remoteBufferNum * sizeof(ProtectionInfo);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    if (ch.sqNum > 0 && ch.SqContextAddr != nullptr) {
        totalSize += ch.sqNum * sizeof(SqContext);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    if (ch.cqNum > 0 && ch.CqContextAddr != nullptr) {
        totalSize += ch.cqNum * sizeof(CqContext);
        totalSize = static_cast<size_t>(AlignUp(totalSize, 8));
    }

    return totalSize;
}

HcclResult BuildChannelEntityDev(ChannelEntity *channelEntity, void **outDevPtr)
{
    if (channelEntity == nullptr || outDevPtr == nullptr) {
        HCCL_ERROR("[BuildChannelEntityDev] Invalid nullptr input");
        return HCCL_E_PARA;
    }

    size_t totalSize = CalcSingleChannelSize(*channelEntity);
    if (totalSize == 0) {
        HCCL_ERROR("[BuildChannelEntityDev] totalSize is 0");
        return HCCL_E_PARA;
    }

    uint8_t *devBase = nullptr;
    aclError aclRet = aclrtMalloc(reinterpret_cast<void **>(&devBase), totalSize, ACL_MEM_MALLOC_NORMAL_ONLY);
    if (aclRet != ACL_SUCCESS) {
        HCCL_ERROR("[BuildChannelEntityDev] aclrtMalloc failed, error=%d", aclRet);
        return HCCL_E_MEMORY;
    }

    uint8_t *hostBuf = static_cast<uint8_t *>(malloc(totalSize));
    if (hostBuf == nullptr) {
        (void)aclrtFree(devBase);
        HCCL_ERROR("[BuildChannelEntityDev] Host malloc failed");
        return HCCL_E_MEMORY;
    }
    (void)memset_s(hostBuf, totalSize, 0, totalSize);

    uint8_t *cursor = hostBuf;
    ChannelEntity *dst = reinterpret_cast<ChannelEntity *>(cursor);
    *dst = *channelEntity;
    cursor += sizeof(ChannelEntity);
    cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));

    Notify *srcLocalNotify = channelEntity->localNotifyAddr;
    Notify *srcRemoteNotify = channelEntity->remoteNotifyAddr;
    ProtectionInfo *srcLocalBuffer = channelEntity->localBufferAddr;
    ProtectionInfo *srcRemoteBuffer = channelEntity->remoteBufferAddr;
    SqContext *srcSq = channelEntity->SqContextAddr;
    CqContext *srcCq = channelEntity->CqContextAddr;

    if (channelEntity->localNotifyNum > 0 && srcLocalNotify != nullptr) {
        size_t copySize = channelEntity->localNotifyNum * sizeof(Notify);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcLocalNotify, copySize);
        dst->localNotifyAddr = reinterpret_cast<Notify *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->localNotifyAddr = nullptr;
    }

    if (channelEntity->remoteNotifyNum > 0 && srcRemoteNotify != nullptr) {
        size_t copySize = channelEntity->remoteNotifyNum * sizeof(Notify);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcRemoteNotify, copySize);
        dst->remoteNotifyAddr = reinterpret_cast<Notify *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->remoteNotifyAddr = nullptr;
    }

    if (channelEntity->localBufferNum > 0 && srcLocalBuffer != nullptr) {
        size_t copySize = channelEntity->localBufferNum * sizeof(ProtectionInfo);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcLocalBuffer, copySize);
        dst->localBufferAddr = reinterpret_cast<ProtectionInfo *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->localBufferAddr = nullptr;
    }

    if (channelEntity->remoteBufferNum > 0 && srcRemoteBuffer != nullptr) {
        size_t copySize = channelEntity->remoteBufferNum * sizeof(ProtectionInfo);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcRemoteBuffer, copySize);
        dst->remoteBufferAddr = reinterpret_cast<ProtectionInfo *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->remoteBufferAddr = nullptr;
    }

    if (channelEntity->sqNum > 0 && srcSq != nullptr) {
        size_t copySize = channelEntity->sqNum * sizeof(SqContext);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcSq, copySize);
        dst->SqContextAddr = reinterpret_cast<SqContext *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->SqContextAddr = nullptr;
    }

    if (channelEntity->cqNum > 0 && srcCq != nullptr) {
        size_t copySize = channelEntity->cqNum * sizeof(CqContext);
        uint64_t offset = static_cast<uint64_t>(cursor - hostBuf);
        (void)memcpy_s(cursor, totalSize - (cursor - hostBuf), srcCq, copySize);
        dst->CqContextAddr = reinterpret_cast<CqContext *>(offset);
        cursor += copySize;
        cursor = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(cursor), 8));
    } else {
        dst->CqContextAddr = nullptr;
    }

    HcclResult ret = MemCopyHostToDev(devBase, hostBuf, totalSize);
    free(hostBuf);

    if (ret != HCCL_SUCCESS) {
        (void)aclrtFree(devBase);
        HCCL_ERROR("[BuildChannelEntityDev] Memcpy host to device failed");
        return ret;
    }

    *outDevPtr = devBase;
    HCCL_INFO("[BuildChannelEntityDev] Single channel build success, size=%zu", totalSize);
    return HCCL_SUCCESS;
}
} // namespace

AivUrmaChannel::AivUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc)
    : endpointHandle_(endpointHandle),
      channelDesc_(channelDesc)
{
    channelKind_ = HcommChannelKind::AIV_URMA;
}

HcclResult AivUrmaChannel::Makebufs(HcommMemHandle *memHandles, uint32_t memHandleNum,
    std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    bufs.clear();
    for (uint32_t i = 0; i < memHandleNum; ++i) {
        auto locMemInfo = reinterpret_cast<CommMemInfo *>(memHandles[i]);
        HCCL_INFO("[AivUrmaChannel][%s] tag[%s]", __func__, locMemInfo->memTag);
        bufs.emplace_back(std::move(std::make_shared<Hccl::Buffer>(reinterpret_cast<uintptr_t>(locMemInfo->mem.addr),
            locMemInfo->mem.size, hccl::ConvertCommToHcclMemType(locMemInfo->mem.type), locMemInfo->memTag)));
    }
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::ParseInputParam() 
{
    // 1. 从 endpointHandle_，获得 localEp_ 和 rdmaHandle_
    // TODO: 使用 HcommEndpointGet
    Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    socket_ = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);
    remoteEp_ = channelDesc_.remoteEndpoint;
    notifyNum_ = channelDesc_.notifyNum;
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);

    if (channelDesc_.exchangeAllMems) {
        // 3. Get memHandles from endpoint
        HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == True. Get memHandles from endpoint.", __func__);
        std::shared_ptr<Hccl::LocalUbRmaBuffer> *memHandles = nullptr;
        uint32_t memHandleNum = 0;
        CHK_RET(static_cast<HcclResult>(
            HcommMemGetAllMemHandles(endpointHandle_, reinterpret_cast<void **>(&memHandles), &memHandleNum)));
        HCCL_INFO("[AivUrmaChannel][%s] Got memHandleNum[%u].", __func__, memHandleNum);
        for (uint32_t i = 0; i < memHandleNum; ++i) {
            std::shared_ptr<Hccl::LocalUbRmaBuffer> &localUbRmaBuffer = memHandles[i];
            HCCL_INFO("[AivUrmaChannel][%s] Got memHandle No.%u: addr[0x%llx], size[0x%llx], memTag[%s].", __func__,
                i, localUbRmaBuffer->GetAddr(), localUbRmaBuffer->GetSize(),
                localUbRmaBuffer->GetBuf()->GetMemTag().c_str());
            bufs_.emplace_back(std::move(std::make_shared<Hccl::Buffer>(localUbRmaBuffer->GetAddr(),
                localUbRmaBuffer->GetSize(), localUbRmaBuffer->GetBuf()->GetMemTag().c_str())));
        }
    } else {
        // 3. 从 channelDesc 的 memHandle，获得 bufs_
        HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
        CHK_RET(Makebufs(channelDesc_.memHandles, channelDesc_.memHandleNum, bufs_));
    }

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildSocket()
{
    if (socket_ != nullptr) {
        return HCCL_SUCCESS;
    }
    HCCL_INFO("[AivUrmaChannel][%s] socket ptr is NULL, rebuildSocket", __func__);

    Hccl::IpAddress ipAddr{};
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, ipAddr));
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(static_cast<Hccl::RankId>(localEp_.loc.device.devPhyId), type, 0, ipAddr);
    Hccl::SocketHandle socketHandle = Hccl::SocketHandleManager::GetInstance().Create(localEp_.loc.device.devPhyId, localPort);
    EXECEPTION_CATCH(serverSocket_ = std::make_unique<Hccl::Socket>(socketHandle, ipAddr, 60001, 
        ipAddr, "server", Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE), return HCCL_E_PARA);
    HCCL_INFO("[AivUrmaChannel][%s] listen_socket_info[%s]", __func__, serverSocket_->Describe().c_str());
    EXECEPTION_CATCH(serverSocket_->Listen(), return HCCL_E_INTERNAL);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AivUrmaChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag, noRankId);
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket_));

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildAttr()
{
    attr_.devicePhyId = localEp_.loc.device.devPhyId;
    attr_.opMode = Hccl::OpMode::OPBASE;
    attr_.opAcceState = Hccl::AcceleratorState::AIV;
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildConnection()
{
    Hccl::OpMode opMode = Hccl::OpMode::OPBASE;
    bool devUsed = true;
    Hccl::HrtUbJfcMode jfcMode = Hccl::HrtUbJfcMode::USER_CTL;
    Hccl::LinkProtocol protocol;
    CHK_RET(CommProtocolToLinkProtocol(localEp_.protocol, protocol));

    Hccl::IpAddress locAddr;
    Hccl::IpAddress rmtAddr;
    CHK_RET(CommAddrToIpAddress(localEp_.commAddr, locAddr));
    CHK_RET(CommAddrToIpAddress(remoteEp_.commAddr, rmtAddr));

    s32 deviceLogicId;
    CHK_RET(hrtGetDevice(&deviceLogicId));
    Hccl::TpManager::GetInstance(deviceLogicId).Init();

    std::unique_ptr<Hccl::DevUbConnection> ubConn = nullptr;
    switch (protocol) {
        case Hccl::LinkProtocol::UB_TP:
            EXECEPTION_CATCH(ubConn = std::make_unique<Hccl::DevUbTpConnection>(
                                 rdmaHandle_, locAddr, rmtAddr, opMode, devUsed, jfcMode),
                return HCCL_E_PTR);
            break;
        case Hccl::LinkProtocol::UB_CTP:
            EXECEPTION_CATCH(ubConn = std::make_unique<Hccl::DevUbCtpConnection>(
                                 rdmaHandle_, locAddr, rmtAddr, opMode, devUsed, jfcMode),
                return HCCL_E_PTR);
            break;
        default:
            HCCL_ERROR("%s No LinkProtocol to match", __func__);
            break;
    }
    CHK_SMART_PTR_NULL(ubConn);

    commonRes_.connVec.clear();
    connections_.clear();

    size_t connectNum = channelDesc_.roceAttr.queueNum;
    if (connectNum >= QUEUE_NUM_MAX) {
        HCCL_WARNING("[AivUrmaChannel] connectNum[%u] over queueNumMax[%u]", connectNum, QUEUE_NUM_MAX);
        connectNum = 1;
    }

    for (size_t i = 0; i < connectNum; ++i) {
        commonRes_.connVec.emplace_back(ubConn.get());
        connections_.push_back(std::move(ubConn));
    }

    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildBuffer(std::vector<std::shared_ptr<Hccl::Buffer>> &bufs)
{
    HCCL_INFO("AivUrmaChannel BuildBuffer start size[%u].", bufs.size());
    for (size_t i = 0; i < bufs.size(); i++) {
        std::unique_ptr<Hccl::LocalUbRmaBuffer> bufferPtr = nullptr;
        EXECEPTION_CATCH(bufferPtr = std::make_unique<Hccl::LocalUbRmaBuffer>(bufs[i], rdmaHandle_), return HCCL_E_PTR);
        commonRes_.bufferVec.push_back(bufferPtr.get());
        localRmaBuffers_.push_back(std::move(bufferPtr));
    }
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildAivUrmaTransport()
{

    const Hccl::Socket &socket = *socket_;

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));

    // make_unique / make_shared / release 包一层抛异常的宏
    EXECEPTION_CATCH(transport_ = std::make_unique<Hccl::AivUrmaTransport>(
                         commonRes_, attr_, linkData, socket, rdmaHandle_), // 这里区分是否是优先recv
        return HCCL_E_PTR);
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::BuildChannelEntityToDevice(void **devChannelPtr)
{
    if (devChannelPtr == nullptr) {
        HCCL_ERROR("[AivUrmaChannel] BuildChannelEntityToDevice devChannelPtr is nullptr");
        return HCCL_E_PTR;
    }

    CHK_PTR_NULL(transport_.get());

    ChannelEntity hostChannel;
    memset(&hostChannel, 0, sizeof(ChannelEntity));

    transport_->GetHostChannelEntity(&hostChannel);
    hostChannel.abiHeader = channelDesc_.header;
    hostChannel.engine = COMM_ENGINE_AIV;
    hostChannel.protocol = channelDesc_.remoteEndpoint.protocol;
    HcclResult ret = BuildChannelEntityDev(&hostChannel, devChannelPtr);
    if (ret != HCCL_SUCCESS) {
        HCCL_ERROR("[AivUrmaChannel] BuildChannelEntityDev failed, ret=%d", ret);
        return ret;
    }

    HCCL_INFO("[AivUrmaChannel] Build channel entity to device success");
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::GetNotifyNum(uint32_t *notifyNum) const
{
    HCCL_INFO("AivUrmaChannel GetNotifyNum is not supported.");
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags)
{
    return transport_->GetRemoteMem(remoteMem, memNum, memTags);
}

HcclResult AivUrmaChannel::GetUserRemoteMem(CommMem **remoteMem, char ***memTag, uint32_t *memNum)
{
    return transport_->GetUserRemoteMem(remoteMem, memTag, memNum);
}

HcclResult AivUrmaChannel::Clean()
{
    transport_.reset();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::Resume()
{
    BuildConnection();
    BuildAivUrmaTransport();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::Init()
{
    /*
        Argue result: make_unique 配合一场捕获的宏 EXCEPTION CATCH
        Attention: const 和引用
    */
    // TODO: 处理抛异常
    CHK_RET(ParseInputParam());
    CHK_RET(BuildSocket());
    CHK_RET(BuildAttr());
    CHK_RET(BuildConnection());
    localRmaBuffers_.clear();
    commonRes_.bufferVec.clear();
    CHK_RET(BuildBuffer(bufs_));
    CHK_RET(BuildAivUrmaTransport());
    return HCCL_SUCCESS;
}

ChannelStatus AivUrmaChannel::GetStatus()
{
    Hccl::TransportStatus transportStatus = transport_->GetStatus();
    ChannelStatus out = ChannelStatus::INIT;
    switch (transportStatus) {
        case Hccl::TransportStatus::INIT:
            out = ChannelStatus::INIT;
            break;
        case Hccl::TransportStatus::SOCKET_OK:
            out = ChannelStatus::SOCKET_OK;
            break;
        case Hccl::TransportStatus::SOCKET_TIMEOUT:
            out = ChannelStatus::SOCKET_TIMEOUT;
            break;
        case Hccl::TransportStatus::READY:
            out = ChannelStatus::READY;
            break;
        default:
            HCCL_ERROR("[AivUrmaChannel][%s] Invalid TransportStatus[%d]", __func__, transportStatus);
            out = ChannelStatus::INVALID;
            break;
    }
    return out;
}

} // namespace hcomm
