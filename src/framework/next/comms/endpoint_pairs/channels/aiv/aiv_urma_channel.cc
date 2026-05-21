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
HcclResult SecureMemset(void *dest, size_t destMax, int value, size_t count, const char *fieldName)
{
    if (dest == nullptr) {
        HCCL_ERROR("[SecureMemset] dest is nullptr, field[%s]", fieldName);
        return HCCL_E_PTR;
    }
    if (count > destMax) {
        HCCL_ERROR("[SecureMemset] invalid size, field[%s], count[%zu], destMax[%zu]",
            fieldName, count, destMax);
        return HCCL_E_PARA;
    }

    errno_t ret = memset_s(dest, destMax, value, count);
    if (ret != EOK) {
        HCCL_ERROR("[SecureMemset] memset_s failed, field[%s], ret[%d], count[%zu], destMax[%zu]",
            fieldName, ret, count, destMax);
        return HCCL_E_MEMORY;
    }
    return HCCL_SUCCESS;
}

template <typename T>
HcclResult CopyArrayToDevice(const T *hostArray, uint32_t arrayNum, T **deviceArrayPtr,
    std::vector<hccl::DeviceMem> &deviceMemories, const char *arrayName)
{
    if (deviceArrayPtr == nullptr) {
        HCCL_ERROR("[AivUrmaChannel::CopyArrayToDevice] %s deviceArrayPtr is nullptr", arrayName);
        return HCCL_E_PTR;
    }

    if (arrayNum == 0 || hostArray == nullptr) {
        *deviceArrayPtr = nullptr;
        return HCCL_SUCCESS;
    }

    if (arrayNum > SIZE_MAX / sizeof(T)) {
        HCCL_ERROR("[AivUrmaChannel::CopyArrayToDevice] %s copy size overflow, num[%u], elemSize[%zu]",
            arrayName, arrayNum, sizeof(T));
        return HCCL_E_PARA;
    }

    size_t arraySize = static_cast<size_t>(arrayNum) * sizeof(T);
    hccl::DeviceMem arrayDevMem = hccl::DeviceMem::alloc(arraySize);
    CHK_PRT_RET(!arrayDevMem,
        HCCL_ERROR("[AivUrmaChannel::CopyArrayToDevice] DeviceMem::alloc for %s failed, size[%zu]",
            arrayName, arraySize), HCCL_E_MEMORY);
    void *arrayDevPtr = arrayDevMem.ptr();
    deviceMemories.push_back(std::move(arrayDevMem));

    Hccl::HrtMemcpy(arrayDevPtr, arraySize, hostArray, arraySize,
        Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    *deviceArrayPtr = reinterpret_cast<T *>(arrayDevPtr);
    HCCL_INFO("[AivUrmaChannel::CopyArrayToDevice] %s: host[%p] -> dev[%p], num[%u], size[%zu]",
        arrayName, hostArray, arrayDevPtr, arrayNum, arraySize);
    return HCCL_SUCCESS;
}
} // namespace

AivUrmaChannel::AivUrmaChannel(EndpointHandle endpointHandle, const HcommChannelDesc &channelDesc)
    : endpointHandle_(endpointHandle),
      channelDesc_(channelDesc)
{
    channelKind_ = HcommChannelKind::AIV_URMA;
}

AivUrmaChannel::~AivUrmaChannel()
{
    ReleaseDeviceChannelEntity();
}

void AivUrmaChannel::ReleaseDeviceChannelEntity()
{
    deviceMemories_.clear();
    devChannelEntity_ = nullptr;
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
    Endpoint *localEpPtr = reinterpret_cast<Endpoint *>(endpointHandle_);
    CHK_PTR_NULL(localEpPtr);
    localEp_ = localEpPtr->GetEndpointDesc();
    rdmaHandle_ = localEpPtr->GetRdmaHandle();

    socket_ = reinterpret_cast<Hccl::Socket *>(channelDesc_.socket);
    remoteEp_ = channelDesc_.remoteEndpoint;
    notifyNum_ = channelDesc_.notifyNum;

    // 3. 从 channelDesc 的 memHandle，获得 bufs_
    HCCL_INFO("[AivUrmaChannel][%s] exchangeAllMems == false. Get memHandles from channelDesc.", __func__);
    CHK_RET(Makebufs(channelDesc_.memHandles, channelDesc_.memHandleNum, bufs_));

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
    Hccl::SocketHandle socketHandle
        = Hccl::SocketHandleManager::GetInstance().Create(localEp_.loc.device.devPhyId, localPort);
    EXECEPTION_CATCH(serverSocket_ = std::make_unique<Hccl::Socket>(socketHandle, ipAddr, 60001, ipAddr, "server",
                         Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE),
        return HCCL_E_PARA);
    HCCL_INFO("[AivUrmaChannel][%s] listen_socket_info[%s]", __func__, serverSocket_->Describe().c_str());
    EXECEPTION_CATCH(serverSocket_->Listen(), return HCCL_E_INTERNAL);

    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(localEp_, remoteEp_, linkData));
    HCCL_INFO("[AivUrmaChannel][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    std::string socketTag = "AUTOMATIC_SOCKET_TAG";
    bool noRankId = true;
    Hccl::SocketConfig socketConfig = Hccl::SocketConfig(linkData, socketTag, noRankId);
    CHK_RET(SocketMgr::GetInstance(localEp_.loc.device.devPhyId).GetSocket(socketConfig, socket_));

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
    commonRes_.connVec.emplace_back(ubConn.get());
    connections_.push_back(std::move(ubConn));

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
    CHK_RET(SecureMemset(&hostChannel, sizeof(ChannelEntity), 0, sizeof(ChannelEntity), "hostChannel"));

    transport_->GetHostChannelEntity(&hostChannel);
    hostChannel.abiHeader = channelDesc_.header;
    hostChannel.engine = COMM_ENGINE_AIV;
    hostChannel.protocol = channelDesc_.remoteEndpoint.protocol;
    std::vector<hccl::DeviceMem> newDeviceMemories;
    hccl::DeviceMem entityDevMem = hccl::DeviceMem::alloc(sizeof(ChannelEntity));
    CHK_PRT_RET(!entityDevMem,
        HCCL_ERROR("[AivUrmaChannel::%s] DeviceMem::alloc for ChannelEntity failed", __func__), HCCL_E_MEMORY);
    newDeviceMemories.push_back(std::move(entityDevMem));
    void *entityDevPtr = newDeviceMemories.back().ptr();

    ChannelEntity devChannel = hostChannel;
    CHK_RET(CopyArrayToDevice(hostChannel.localNotifyAddr, hostChannel.localNotifyNum,
        &devChannel.localNotifyAddr, newDeviceMemories, "localNotifyAddr"));
    CHK_RET(CopyArrayToDevice(hostChannel.remoteNotifyAddr, hostChannel.remoteNotifyNum,
        &devChannel.remoteNotifyAddr, newDeviceMemories, "remoteNotifyAddr"));
    CHK_RET(CopyArrayToDevice(hostChannel.localBufferAddr, hostChannel.localBufferNum,
        &devChannel.localBufferAddr, newDeviceMemories, "localBufferAddr"));
    CHK_RET(CopyArrayToDevice(hostChannel.remoteBufferAddr, hostChannel.remoteBufferNum,
        &devChannel.remoteBufferAddr, newDeviceMemories, "remoteBufferAddr"));
    CHK_RET(CopyArrayToDevice(hostChannel.sqContextAddr, hostChannel.sqNum,
        &devChannel.sqContextAddr, newDeviceMemories, "sqContextAddr"));
    CHK_RET(CopyArrayToDevice(hostChannel.cqContextAddr, hostChannel.cqNum,
        &devChannel.cqContextAddr, newDeviceMemories, "cqContextAddr"));

    Hccl::HrtMemcpy(entityDevPtr, sizeof(ChannelEntity), &devChannel, sizeof(ChannelEntity),
        Hccl::tagRtMemcpyKind::RT_MEMCPY_HOST_TO_DEVICE);

    ReleaseDeviceChannelEntity();
    deviceMemories_ = std::move(newDeviceMemories);
    devChannelEntity_ = entityDevPtr;
    *devChannelPtr = devChannelEntity_;
    HCCL_INFO("[AivUrmaChannel] Build channel entity to device success, devPtr[%p]", devChannelEntity_);
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
    ReleaseDeviceChannelEntity();
    ReleasePtrArrayDevMems();
    transport_.reset();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::Resume()
{
    BuildConnection();
    BuildAivUrmaTransport();
    return HCCL_SUCCESS;
}

HcclResult AivUrmaChannel::NotifyRecord(const uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::WriteWithNotify(void *dst, const void *src, const uint64_t len,
    uint32_t remoteNotifyIdx)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Write(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Read(void *dst, const void *src, uint64_t len)
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::ChannelFence()
{
    HCCL_INFO("[AivUrmaChannel::%s] not supported yet.", __func__);
    return HCCL_E_NOT_SUPPORT;
}

HcclResult AivUrmaChannel::Init()
{
    /*
        Argue result: make_unique 配合一场捕获的宏 EXCEPTION CATCH
        Attention: const 和引用
    */
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
