/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <mutex>
#include <set>
#include "socket_manager.h"
#include "socket_handle_manager.h"
#include "communicator_impl.h"
#include "null_ptr_exception.h"
#include "exception_util.h"
#include "stl_util.h"
#include "preempt_port_manager.h"

namespace Hccl {

static std::mutex socketLock;

void SocketManager::BatchCreateSockets(const vector<LinkData> &links)
{
    vector<LinkData> pendingLinks;
    for (auto &link : links) {
        if (Contain(availableLinks, link)) {
            continue;
        }
        pendingLinks.emplace_back(link);
    }

    if (pendingLinks.empty()) {
        return;
    }

    BatchServerInit(pendingLinks);
    BatchAddWhiteList(pendingLinks);
    BatchCreateConnectedSockets(pendingLinks);

    availableLinks.insert(pendingLinks.begin(), pendingLinks.end());
}

void SocketManager::BatchServerInit(const vector<LinkData> &links)
{
    for (auto &link : links) {
        auto portData = link.GetLocalPort();
        ServerInit(portData);
    }
}

void SocketManager::BatchAddWhiteList(const vector<LinkData> &links)
{
    unordered_map<PortData, vector<RaSocketWhitelist>> wlistMap{};

    for (const auto &link : links) {
        // 通过虚拟拓扑获取Peer可能为空，如果为空，需要抛异，NullPtrException
        auto peer = comm->GetRankGraph()->GetPeer(link.GetRemoteRankId());
        if (peer == nullptr) {
            auto msg = StringFormat("Fail to get peer of rank %d!", link.GetRemoteRankId());
            THROW<NullPtrException>(msg);
        }
        RaSocketWhitelist wlistInfo{};;
        wlistInfo.connLimit = 1;
        wlistInfo.remoteIp = link.GetRemoteAddr();

        SocketConfig socketConfig(link.GetRemoteRankId(), link, comm->GetEstablishLinkSocketTag());
        string       hccpSocketTag = socketConfig.GetHccpTag();

        wlistInfo.tag = hccpSocketTag;
        wlistMap[link.GetLocalPort()].push_back(wlistInfo);
    }

    for (auto &i : wlistMap) {
        auto port = i.first;
        AddWhiteList(port, i.second);
        socketWlistMap[port] = i.second;
    }
}

void SocketManager::BatchCreateConnectedSockets(const vector<LinkData> &links)
{
    for (auto &link : links) {
        auto         remoteRank = link.GetRemoteRankId();
        std::string  socketTag  = comm->GetEstablishLinkSocketTag();
        SocketConfig socketConfig(remoteRank, link, socketTag);
        CreateConnectedSocket(socketConfig);
    }
}

void SocketManager::ServerInit(PortData &localPort)
{
    std::lock_guard<std::mutex> lock(socketLock);
    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    auto &rankListenPortMap = SocketManager::GetDeviceServerListenPortMap();
    auto serverSocketInMap = serverSocketMap.find(localPort);
    if (serverSocketInMap != serverSocketMap.end()) {
        HCCL_INFO("[%s] find localPort in serverSocketMap, localPort [%s]", __func__, localPort.Describe().c_str());
        return;
    }

    SocketHandle hccpSocketHandle = SocketHandleManager::GetInstance().Create(devicePhyId, localPort);
    IpAddress    ipAddress        = localPort.GetAddr();
    u32 serverListenPort          = DEFAULT_VALUE_DEVICEPORT;
    auto iter = rankListenPortMap.find(devicePhyId);
    if (iter != rankListenPortMap.end()) {
        serverListenPort = iter->second;
    }
    auto         serverSocket     = socketProducer(ipAddress, ipAddress, serverListenPort, hccpSocketHandle, "server",
                                                   SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);
    bool success = serverSocket->Listen(serverListenPort);
    if (success) {
        HCCL_RUN_INFO("[SocketManager::%s] Device %u listen the port %u success", __func__, deviceLogicId_, serverListenPort);
    } else {
        string msg = StringFormat("[SocketManager::%s] Device %u listen the port %u failed, maybe other process be listen it", __func__, deviceLogicId_, serverListenPort);
        MACRO_THROW(InvalidParamsException, msg);
    }
    serverSocketMap[localPort] = std::move(serverSocket);
}

void SocketManager::ServerInitAll(const vector<LinkData> &links, u32 &linstenPort) const
{
    std::lock_guard<std::mutex> lock(socketLock);
    vector<SocketPortRange> listenPortRanges = EnvConfig::GetInstance().GetHostNicConfig().GetDeviceSocketPortRange();
    if (listenPortRanges.empty()) {
        HCCL_RUN_INFO("[SocketManager::%s] socket port range not configured.", __func__);
        return;
    }

    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    std::set<PortData> newPorts;
    bool useOld = false;  // 旧端口抢占过，就沿用旧端口
    for(uint32_t i = 0; i < links.size(); i++) {
        LinkData link = links[i];
        auto localPort = link.GetLocalPort();
        auto iter = serverSocketMap.find(localPort);
        if (iter != serverSocketMap.end()) {
            linstenPort = serverSocketMap[localPort]->GetListenPort();  
            useOld = true;
            HCCL_INFO("[SocketManager::%s] Device %u use the old device port %u in same process.", __func__, deviceLogicId_, linstenPort);
        } else {
            newPorts.insert(localPort);
        }
    }

    for(auto it = newPorts.begin(); it != newPorts.end(); ++it)
    {
        auto localPort = *it;
        SocketHandle hccpSocketHandle = SocketHandleManager::GetInstance().Create(devicePhyId, localPort);
        IpAddress ipAddress = localPort.GetAddr();
        auto serverSocket = std::make_shared<Socket>(hccpSocketHandle, ipAddress, linstenPort, ipAddress, "server", SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);
        if (!useOld && it == newPorts.begin()) { // 首个链接抢占，其余继承
            PreemptPortManager::GetInstance(deviceLogicId_).ListenPreempt(serverSocket, listenPortRanges, linstenPort);
            HCCL_RUN_INFO("[SocketManager::%s] Device %u listen the preempt port %u", __func__, deviceLogicId_, linstenPort);
        } else {
            serverSocket->Listen(linstenPort);
        }
        serverSocketMap[localPort] = std::move(serverSocket);
    }
}

bool SocketManager::ServerDeInit(PortData &localPort) const
{
    std::lock_guard<std::mutex> lock(socketLock);
    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    auto res = GetServerListenSocket(localPort);
    // 待修改 stop listen maybe needed
    if (res != nullptr) {
        serverSocketMap.erase(localPort);
    }

    return true;
}

Socket *SocketManager::CreateConnectedSocket(SocketConfig &socketConfig)
{
    auto res = GetConnectedSocket(socketConfig);
    if (res != nullptr) {
        return res;
    }

    auto socketHandle = SocketHandleManager::GetInstance().Get(devicePhyId, socketConfig.link.GetLocalPort());
    if (socketHandle == nullptr) {
        THROW<NullPtrException>(StringFormat("socketHandle of is nullptr, devicePhyId=%d, port=%s", devicePhyId,
                                             socketConfig.link.GetLocalPort().Describe().c_str()));
    }
    auto &rankListenPortMap = SocketManager::GetDeviceServerListenPortMap();
    IpAddress  localIpAddress  = socketConfig.link.GetLocalAddr();
    IpAddress  remoteIpAddress = socketConfig.link.GetRemoteAddr();
    SocketRole socketRole      = socketConfig.GetRole();
    string     hccpSocketTag   = socketConfig.GetHccpTag();
    u32 serverListenPort = DEFAULT_VALUE_DEVICEPORT;
    auto iter = rankListenPortMap.find(static_cast<u32>(socketConfig.remoteRank));
    if (iter != rankListenPortMap.end()) {
        serverListenPort = iter->second;
    }

    auto tmpSocket = socketProducer(localIpAddress, remoteIpAddress, serverListenPort, socketHandle, hccpSocketTag,
                                    socketRole, NicType::DEVICE_NIC_TYPE);
    HCCL_INFO("[SocketManager::%s] Device %u connect async the remote rank %d port %u.", __func__, devicePhyId, socketConfig.remoteRank, serverListenPort);
    tmpSocket->ConnectAsync();
    connectedSocketMap[socketConfig] = std::move(tmpSocket);
    return connectedSocketMap[socketConfig].get();
}

bool SocketManager::DestroyConnectedSocket(SocketConfig &socketConfig)
{
    auto socket = GetConnectedSocket(socketConfig);
    if (socket) {
        socket->Destroy();
        int num = availableLinks.erase(socketConfig.link);
        if (num <= 0) {
            THROW<NullPtrException>(StringFormat("availableLinks has no socketConfig.link, link=%s",
                socketConfig.link.Describe().c_str()));
        }
        return true;
    }

    return true;
}

Socket *SocketManager::GetConnectedSocket(SocketConfig &socketConfig) const
{
    auto res = connectedSocketMap.find(socketConfig);
    if (res != connectedSocketMap.end()) {
        return res->second.get();
    }

    return nullptr;
}

void SocketManager::DestroyAll()
{
    for (auto &i : socketWlistMap) {
        auto port = i.first;
        DelWhiteList(port, i.second);
    }
    socketWlistMap.clear();

    for (auto &socket : connectedSocketMap) {
        if (socket.second != nullptr) {
            socket.second->Destroy();
        }
    }
    connectedSocketMap.clear();
    availableLinks.clear();
}

Socket *SocketManager::GetServerListenSocket(const PortData &localPort) const
{
    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    auto res = serverSocketMap.find(localPort);
    if (res != serverSocketMap.end()) {
        return (res->second).get();
    }

    return nullptr;
}

SocketManager::SocketManager(
    const CommunicatorImpl &communicator, u32 localRank, u32 devicePhyId, u32 deviceLogicId,
    std::function<shared_ptr<Socket>(IpAddress &localIpAddress, IpAddress &remoteIpAddress, u32 listenPort,
                                     SocketHandle socketHandle, const std::string &tag, SocketRole socketRole,
                                     NicType nicType)>
        socketProducer)
    : comm(&communicator), localRank(localRank), devicePhyId(devicePhyId), deviceLogicId_(deviceLogicId)
{
    if (socketProducer != nullptr) {
        this->socketProducer = socketProducer;
    }
}

void SocketManager::AddWhiteList(PortData &localPort, vector<RaSocketWhitelist> &wlistInfoVec) const
{
    auto socketHandle = SocketHandleManager::GetInstance().Get(devicePhyId, localPort);
    if (socketHandle == nullptr) {
        THROW<NullPtrException>(StringFormat("socketHandle of is nullptr, devicePhyId=%d, port=%s", devicePhyId,
                                             localPort.Describe().c_str()));
    }
    HrtRaSocketWhiteListAdd(socketHandle, wlistInfoVec);
}

bool SocketManager::DelWhiteList(PortData &localPort, vector<RaSocketWhitelist> &wlistInfoVec) const
{
    auto socketHandle = SocketHandleManager::GetInstance().Get(devicePhyId, localPort);
    if (socketHandle == nullptr) {
        return false;
    }
    HrtRaSocketWhiteListDel(socketHandle, wlistInfoVec);
    return true;
}

void SocketManager::SetDeviceServerListenPortMap(const std::unordered_map<u32, u32> &rankListenPortMap)
{
    std::unordered_map<u32, u32> &staticMap = SocketManager::GetDeviceServerListenPortMap();
    staticMap.clear();
    for (auto &pair : rankListenPortMap) {
        staticMap[pair.first] = pair.second;
    }
    return;
}

std::unordered_map<u32, u32> &SocketManager::GetDeviceServerListenPortMap()
{
    static std::unordered_map<u32, u32> rankListenPortMap;
    return rankListenPortMap;
}


SocketManager::~SocketManager()
{
    DECTOR_TRY_CATCH("SocketManager", DestroyAll());
}

std::unordered_map<PortData, shared_ptr<Socket>> &SocketManager::GetServerSocketMap()
{
    static std::unordered_map<PortData, shared_ptr<Socket>> serverSocketMap;
    return serverSocketMap;
}

} // namespace Hccl