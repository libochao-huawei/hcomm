/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "socket_process.h"
#include "socket_config.h"
#include "socket.h"
#include "ip_address.h"
#include "exception_handler.h"
#include "adapter_rts.h"
#include "../endpoints/endpoint.h"
#include "adapter_rts_common.h"

using namespace std;

namespace hcomm {

SocketProcess &SocketProcess::GetInstance(s32 deviceLogicId)
{
    static SocketProcess socketProcess[MAX_MODULE_DEVICE_NUM];
    if (static_cast<u32>(deviceLogicId) >= MAX_MODULE_DEVICE_NUM) {
        HCCL_WARNING("[SocketProcess][%s] invalid deviceLogicId: %d", __func__, deviceLogicId);
        return socketProcess[0];
    }
    return socketProcess[deviceLogicId];
}

SocketProcess::~SocketProcess()
{
    isInit_ = false;

    unique_lock<std::mutex> lock(mutex_);
    for (auto &socketItem : serverSocketMap_) {
        if (socketItem.second != nullptr) {
            socketItem.second.get()->Destroy();
        }
    }
    serverSocketMap_.clear();

    for (auto &item : tag2socketMap_) {
        if (item.second.first != nullptr) {
            socketMgr_->DestroySocket(item.second.first);
        }
    }
    tag2socketMap_.clear();
    socket2TagMap_.clear();
}

HcclResult SocketProcess::DestroySocketHandle(SocketHandle socketHandle)
{
    Hccl::Socket *socket = static_cast<Hccl::Socket *>(socketHandle);
    if (socket == nullptr) {
        HCCL_ERROR("[SocketProcess][%s] socket is nullptr, please check", __func__);
        return HCCL_E_PARA;
    }

    unique_lock<std::mutex> lock(mutex_);
    auto socket2TagIter = socket2TagMap_.find(socket);
    if (socket2TagIter == socket2TagMap_.end()) {
        HCCL_ERROR("[SocketProcess][%s] socket not found, please check", __func__);
        return HCCL_E_NOT_FOUND;
    }

    string socketTag = socket2TagIter->second;
    auto tag2socketIter = tag2socketMap_.find(socketTag);
    if (tag2socketIter == tag2socketMap_.end()) {
        HCCL_ERROR("[SocketProcess][%s] socketTag not found, please check", __func__);
        return HCCL_E_NOT_FOUND;
    }

    if (tag2socketIter->second.second > 0) {
        tag2socketIter->second.second--;
        HCCL_INFO("[SocketProcess][%s] socket with tag[%s] refCnt: %u", __func__, socketTag.c_str(), 
            tag2socketIter->second.second);
        return HCCL_SUCCESS;
    }

    HCCL_DEBUG("[SocketProcess][%s] destroy socket with tag[%s]", __func__, socketTag.c_str());
    Hccl::Socket* rawSocket = tag2socketIter->second.first;
    tag2socketMap_.erase(tag2socketIter);
    socket2TagMap_.erase(socket2TagIter);
    CHK_RET(socketMgr_->DeleteWhiteList(rawSocket));
    CHK_RET(socketMgr_->DestroySocket(rawSocket));

    return HCCL_SUCCESS;
}

HcclResult SocketProcess::GetSocket(SocketDesc *socketDesc, SocketHandle &socketHandle)
{
    HCCL_INFO("[GetSocket] CMTEST start!!!!!!!!!");
    CHK_PTR_NULL(socketDesc);
    CHK_RET(Init());

    if (!isInit_) {
        HCCL_ERROR("[SocketProcess][%s] SocketProcess not initialized, device may be destroyed", __func__);
        return HCCL_E_INTERNAL;
    }

    Hccl::IpAddress localIpaddr{};
    CHK_RET(CommAddrToIpAddress(socketDesc->localEndpoint.commAddr, localIpaddr));
    Hccl::IpAddress remoteIpaddr{};
    CHK_RET(CommAddrToIpAddress(socketDesc->remoteEndpoint.commAddr, remoteIpaddr));

    string socketTag = string(socketDesc->tag) + "_" + localIpaddr.GetIpStr().c_str() + "_" + remoteIpaddr.GetIpStr().c_str();
    HCCL_INFO("[SocketProcess][%s] socket with tag[%s].", __func__, socketTag.c_str());
    unique_lock<std::mutex> lock(mutex_);
    if (tag2socketMap_.find(socketTag) == tag2socketMap_.end()) {
        CHK_RET(BuildSocket(socketDesc, socketTag));
    } else {
        tag2socketMap_[socketTag].second++;
        HCCL_INFO("[SocketProcess][%s] socket with tag[%s] already exists, num: %u.", __func__, socketTag.c_str(),
            tag2socketMap_[socketTag].second);
    }

    socketHandle = static_cast<SocketHandle>(tag2socketMap_[socketTag].first);
    HCCL_INFO("[SocketProcess][%s] socketHandle = %p", __func__, socketHandle);
    return HCCL_SUCCESS;
}

HcclResult SocketProcess::GetStatus(SocketHandle socketHandle, SocketStates &socketStatus)
{
    Hccl::Socket *socket = static_cast<Hccl::Socket *>(socketHandle);
    if (socket == nullptr || socket2TagMap_.find(socket) == socket2TagMap_.end()) {
        HCCL_ERROR("[SocketProcess][%s] socket is nullptr or not found, please check", __func__);
        return HCCL_E_PARA;
    }

    Hccl::SocketStatus status = socket->GetAsyncStatus();
    if (status == Hccl::SocketStatus::OK) {
        socketStatus = SocketStates::SOCKET_OK;
    } else if (status == Hccl::SocketStatus::TIMEOUT) {
        socketStatus = SocketStates::SOCKET_TIMEOUT;
    } else {
        socketStatus = SocketStates::SOCKET_CONNECTING;
    }

    return HCCL_SUCCESS;
}

HcclResult SocketProcess::SendNoBlock(SocketHandle socketHandle, void *sendbuffer, u64 sendSize, u64 *&sentSize)
{
    Hccl::Socket *socket = static_cast<Hccl::Socket *>(socketHandle);
    if (socket == nullptr || socket2TagMap_.find(socket) == socket2TagMap_.end()) {
        HCCL_ERROR("[SocketProcess][%s] socket is nullptr or not found, please check", __func__);
        return HCCL_E_PARA;
    }
    if (sentSize == nullptr || sendbuffer == nullptr) {
        HCCL_ERROR(
            "[SocketProcess][%s] sentSize is nullptr or sendbuffer is nullptr, please check",
            __func__);
        return HCCL_E_PARA;
    }

    bool result = socket->ISend(reinterpret_cast<u8 *>(sendbuffer), sendSize, *sentSize);
    if (!result) {
        HCCL_ERROR("[SocketProcess::%s] Send size[%llu] of data failed.", __func__, sendSize);
        return HCCL_E_TCP_TRANSFER;
    }
    HCCL_INFO("[SocketProcess::%s] Send size[%llu] of data success. [%zu] bytes sent.", __func__, sendSize, *sentSize);

    return HCCL_SUCCESS;
}

HcclResult SocketProcess::RecvNoBlock(
    SocketHandle socketHandle, void *recvBuffer, u64 recvSize, u64 *&recvedSize)
{
    Hccl::Socket *socket = static_cast<Hccl::Socket *>(socketHandle);
    if (socket == nullptr || socket2TagMap_.find(socket) == socket2TagMap_.end()) {
        HCCL_ERROR("[SocketProcess][%s] socket is nullptr or not found, please check", __func__);
        return HCCL_E_PARA;
    }
    if (recvBuffer == nullptr || recvedSize == nullptr) {
        HCCL_ERROR(
            "[SocketProcess][%s] recvBuffer is nullptr or recvedSize is nullptr, please check",
            __func__);
        return HCCL_E_PARA;
    }

    HcclResult result = socket->IRecv(reinterpret_cast<u8 *>(recvBuffer), recvSize, *recvedSize);
    if (result == HCCL_E_AGAIN) {
        return HCCL_SUCCESS; // 未收到数据，非错误
    }

    HCCL_DEBUG("[SocketProcess::%s] Recv size[%llu] of data success. [%zu] bytes received.",
        __func__, recvSize, *recvedSize);
    
    return HCCL_SUCCESS;
}

HcclResult SocketProcess::Init()
{
    if (isInit_) {
        return HCCL_SUCCESS;
    }

    isInit_ = true;
    EXECEPTION_CATCH(socketMgr_ = std::make_unique<SocketMgr>(), return HCCL_E_PTR);
    s32 devLogicId;
    CHK_RET(hrtGetDevice(&devLogicId));
    CHK_RET(hrtGetDevicePhyIdByIndex(static_cast<u32>(devLogicId), devicePhyId_));
    HCCL_INFO("[SocketProcess] CMTEST Init devicePhyId_ = %d", devicePhyId_);

    return HCCL_SUCCESS;
}

Hccl::SocketRole SocketProcess::ConvertToHcclSocketRole(HcommSocketRole &hcommRole)
{
    switch (hcommRole) {
        case HCOMM_SOCKET_ROLE_CLIENT:
            return Hccl::SocketRole::CLIENT;
        case HCOMM_SOCKET_ROLE_SERVER:
            return Hccl::SocketRole::SERVER;
        case HCOMM_SOCKET_ROLE_RESERVED:
        default:
            HCCL_WARNING("[Convert] Invalid HcommSocketRole: %d, defaulting to CLIENT", hcommRole);
            return Hccl::SocketRole::CLIENT;
    }
}

HcclResult SocketProcess::BuildSocket(SocketDesc *socketDesc, const std::string &socketTag)
{
    if (tag2socketMap_.find(socketTag) != tag2socketMap_.end()) {
        return HCCL_SUCCESS;
    }

    HCCL_INFO("[BuildSocket] CMTEST start!!!!!!!!!");
    // 存疑CommProtocol还没有tcp的protocol，这里是别的协议吗？
    Hccl::LinkData linkData = BuildDefaultLinkData();
    CHK_RET(EndpointDescPairToLinkData(socketDesc->localEndpoint, socketDesc->remoteEndpoint, linkData));
    HCCL_INFO("[SocketProcess][%s] built linkData: %s", __func__, linkData.Describe().c_str());
    Hccl::SocketConfig socketConfig
        = Hccl::SocketConfig(linkData, string(socketDesc->tag), ConvertToHcclSocketRole(socketDesc->role), socketDesc->listenPort);
    auto localListenPair = std::make_pair(socketConfig.link.GetLocalPort(), socketConfig.listeningPort);

    Hccl::IpAddress ipaddr{};
    CHK_RET(CommAddrToIpAddress(socketDesc->localEndpoint.commAddr, ipaddr));
    if (serverSocketMap_.find(localListenPair) == serverSocketMap_.end()) {
        Hccl::SocketHandle serverSocketHandle = Hccl::SocketHandleManager::GetInstance().Get(devicePhyId_, localListenPair.first);
        if (serverSocketHandle == nullptr) {
            serverSocketHandle = Hccl::SocketHandleManager::GetInstance().Create(devicePhyId_, localListenPair.first);
        }
        EXECEPTION_CATCH(serverSocketMap_[localListenPair] = std::make_unique<Hccl::Socket>(
            serverSocketHandle, ipaddr, localListenPair.second, ipaddr, socketDesc->tag,
            Hccl::SocketRole::SERVER, Hccl::NicType::DEVICE_NIC_TYPE), return HCCL_E_PARA);
        HCCL_INFO("[%s] listen_socket_info[%s]", __func__, serverSocketMap_[localListenPair].get()->Describe().c_str());
        EXECEPTION_CATCH(serverSocketMap_[localListenPair].get()->Listen(), return HCCL_E_INTERNAL);
    }
    HCCL_INFO("[SocketProcess][%s] ip[%s] has been listening.", __func__, ipaddr.GetIpStr().c_str());

    Hccl::Socket *socket = nullptr;
    CHK_RET(socketMgr_->GetSocket(socketConfig, socket));
    tag2socketMap_[socketTag].first = socket;
    tag2socketMap_[socketTag].second = 0;
    socket2TagMap_[socket] = socketTag;

    return HCCL_SUCCESS;
}

} // namespace hcomm
