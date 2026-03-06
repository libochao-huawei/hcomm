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
#include <memory>

#include "socket.h"
#include "virtual_topo.h"
#include "socket_config.h"
#include "env_func.h"
#include "orion_adapter_hccp.h"

namespace Hccl {

class CommunicatorImpl;
class SocketManager {
public:
    SocketManager(const CommunicatorImpl &communicator, u32 localRank, u32 devicePhyId, u32 deviceLogicId,
                  std::function<shared_ptr<Socket>(IpAddress &localIpAddress, IpAddress &remoteIpAddress,
                                                   u32 listenPort, SocketHandle socketHandle, const std::string &tag,
                                                   SocketRole socketRole, NicType nicType)>
                      socketProducer
                  = nullptr);
    static void SetDeviceServerListenPortMap(const std::unordered_map<u32, u32> &rankListenPortMap);

    static std::unordered_map<u32, u32>& GetDeviceServerListenPortMap();

    void BatchCreateSockets(const vector<LinkData> &links);

    void ServerInit(PortData &localPort);

    void ServerInitAll(const vector<LinkData> &links, u32 &linstenPort) const;

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
    static std::unordered_map<PortData, shared_ptr<Socket>>& GetServerSocketMap();
    u32               localRank;
    u32               devicePhyId;
    u32               deviceLogicId_;
    std::function<shared_ptr<Socket>(IpAddress &localIpAddress, IpAddress &remoteIpAddress, u32 listenPort,
                                     SocketHandle socketHandle, const std::string &tag, SocketRole socketRole,
                                     NicType nicType)>
        socketProducer
        = [](IpAddress &localIpAddress, IpAddress &remoteIpAddress, u32 listenPort, SocketHandle socketHandle,
             const std::string &tag, SocketRole socketRole, NicType nicType) -> shared_ptr<Socket> {
        auto tmpSocket = std::make_shared<Socket>(socketHandle, localIpAddress, listenPort, remoteIpAddress, tag,
                                                  socketRole, nicType);
        HCCL_INFO("create socket with role %u", static_cast<u32>(socketRole));
        return tmpSocket;
    };

    std::unordered_map<SocketConfig, shared_ptr<Socket>> connectedSocketMap;
    std::unordered_map<PortData, vector<RaSocketWhitelist>> socketWlistMap{};

    Socket *GetServerListenSocket(const PortData &localPort) const;
    std::set<LinkData>      availableLinks;
};

} // namespace Hccl

#endif // HCCLV2_SOCKET_MANAGER_H