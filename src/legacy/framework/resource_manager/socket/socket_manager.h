/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef HCCLV2_SOCKET_MANAGER_H
#define HCCLV2_SOCKET_MANAGER_H

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "socket.h"
#include "virtual_topo.h"
#include "socket_config.h"

namespace Hccl {

class CommunicatorImpl;
class SocketManager {
public:
    SocketManager(const CommunicatorImpl &communicator, u32 localRank, u32 devicePhyId, u32 serverListenPort,
                  std::function<unique_ptr<Socket>(IpAddress &localIpAddress, IpAddress &remoteIpAddress,
                                                   u32 listenPort, SocketHandle socketHandle, const std::string &tag,
                                                   SocketRole socketRole, NicType nicType)>
                      socketProducer
                  = nullptr);

    void BatchCreateSockets(const vector<LinkData> &links);

    void ServerInit(PortData &localPort);

    bool ServerDeInit(PortData &localPort) const;

    Socket *CreateConnectedSocket(SocketConfig &socketConfig);

    bool DestroyConnectedSocket(SocketConfig &socketConfig);

    Socket *GetConnectedSocket(SocketConfig &socketConfig) const;

    void DestroyAll();

    void AddWhiteList(PortData &localPort, vector<RaSocketWhitelist> &wlistInfoVec) const;

    bool DelWhiteList(PortData &localPort, vector<RaSocketWhitelist> &wlistInfoVec) const;

    ~SocketManager();

    SocketManager(const SocketManager &socketManager) = delete;

    SocketManager &operator=(const SocketManager &socketManager) = delete;

private:
    void BatchServerInit(const vector<LinkData> &links);
    void BatchAddWhiteList(const vector<LinkData> &links);
    void BatchCreateConnectedSockets(const vector<LinkData> &links);
    const CommunicatorImpl *comm;
    static std::unordered_map<PortData, unique_ptr<Socket>>& GetServerSocketMap();
    u32               localRank;
    u32               devicePhyId;
    u32               serverListenPort;
    std::function<unique_ptr<Socket>(IpAddress &localIpAddress, IpAddress &remoteIpAddress, u32 listenPort,
                                     SocketHandle socketHandle, const std::string &tag, SocketRole socketRole,
                                     NicType nicType)>
        socketProducer
        = [](IpAddress &localIpAddress, IpAddress &remoteIpAddress, u32 listenPort, SocketHandle socketHandle,
             const std::string &tag, SocketRole socketRole, NicType nicType) -> unique_ptr<Socket> {
        auto tmpSocket = std::make_unique<Socket>(socketHandle, localIpAddress, listenPort, remoteIpAddress, tag,
                                                  socketRole, nicType);
        HCCL_INFO("create socket with role %u", static_cast<u32>(socketRole));
        return tmpSocket;
    };

    std::unordered_map<SocketConfig, unique_ptr<Socket>> connectedSocketMap;
    std::unordered_map<PortData, vector<RaSocketWhitelist>> socketWlistMap{};

    Socket *GetServerListenSocket(PortData &localPort) const;
    std::set<LinkData>      availableLinks;
};

} // namespace Hccl

#endif // HCCLV2_SOCKET_MANAGER_H