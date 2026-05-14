/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public
#include "socket_manager.h"
#include "socket_handle_manager.h"
#include "communicator_impl.h"
#include "ranktable_stub_clos.h"
#include "preempt_port_manager.h"
#undef protected
#undef private

using namespace Hccl;

class SocketManagerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "SocketManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "SocketManagerTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        hccpSocketHandle = new int(0);
        MOCKER_CPP(&SocketHandleManager::Create)
            .stubs()
            .with(any(), any())
            .will(returnValue(hccpSocketHandle));
        MOCKER_CPP(&SocketHandleManager::Get)
            .stubs()
            .with(any(), any())
            .will(returnValue(hccpSocketHandle));
        MOCKER_CPP(&PreemptPortManager::ListenPreempt)
            .stubs()
            .will(ignoreReturnValue());
        SetLinks();

        std::cout << "A Test case in SocketManagerTest SetUP" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        delete hccpSocketHandle;
        std::cout << "A Test case in SocketManagerTest TearDown" << std::endl;
    }

    IpAddress GetAnIpAddress(RankId rankId=1)
    {
        IpAddress ipAddress(StringFormat("%u.0.0.0", rankId));
        return ipAddress;
    }

    void SetLinks()
    {
        links.clear();
        for (u32 i = 0; i < 4; i++) {
            PortDeploymentType portDeploymentType = PortDeploymentType::DEV_NET;
            LinkProtocol linkProtocol = LinkProtocol::UB_CTP;
            RankId localRankId = 0;
            RankId remoteRankId = 3 - i;
            
            LinkData tmpLink(portDeploymentType, linkProtocol, localRankId, remoteRankId,
                GetAnIpAddress(localRankId), GetAnIpAddress(remoteRankId));
            links.push_back(tmpLink);
        }
    }

    void *hccpSocketHandle;
    IpAddress           localIp;
    IpAddress           remoteIp;
    vector<LinkData>    links;
    CommunicatorImpl impl;
    u32 localRank = 0;
    u32 devicePhyId = 0;
    u32 listenPort = 60001;
};


TEST_F(SocketManagerTest, batch_create_sockets_should_ok) {
    // Given
    MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
    
    // when

    // then
    SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
    socketMgr.BatchCreateSockets(links);
    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    for (const auto& sock: serverSocketMap) {
        EXPECT_EQ(sock.second->socketStatus, SocketStatus::LISTENING);
    }

    for (const auto& sock: socketMgr.connectedSocketMap) {
        if (sock.first.role == SocketRole::CLIENT) {
            EXPECT_EQ(sock.second->socketStatus, SocketStatus::CONNECT_STARTING);
        }
        std::cout << sock.first.remoteRank << " " << sock.second->socketStatus << std::endl;
    }
}

TEST_F(SocketManagerTest, test_ServerDeInit_and_GetServerListenSocket) {
    MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
    SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
    socketMgr.BatchCreateSockets(links);
    for(auto& link : links){
        auto portData = link.GetLocalPort();
        socketMgr.ServerDeInit(portData);
        socketMgr.GetServerListenSocket(portData);
    }
}

TEST_F(SocketManagerTest, Ut_ServerInitAll_Skip_Init_When_Env_not_Config) {
    EnvHostNicConfig envConfig;
    EnvHostNicConfig &fakeEnvConfig = envConfig;
    fakeEnvConfig.hcclDeviceSocketPortRange = CfgField<std::vector<SocketPortRange>>{"HCCL_NPU_SOCKET_PORT_RANGE", {}, 
        [] (const std::string &s) -> std::vector<SocketPortRange> { return CastSocketPortRange(s, "HCCL_NPU_SOCKET_PORT_RANGE"); }};
    fakeEnvConfig.hcclDeviceSocketPortRange.isParsed = true;
    MOCKER_CPP(&EnvConfig::GetHostNicConfig).stubs().will(returnValue(fakeEnvConfig));
    NewRankInfo rankInfo;
    EXPECT_NO_THROW(SocketManager::ServerInitAll(rankInfo));
}

TEST_F(SocketManagerTest, Ut_ServerInitAll_Skip_Init_When_Env_Config) {
    EnvHostNicConfig envConfig;
    EnvHostNicConfig &fakeEnvConfig = envConfig;
    fakeEnvConfig.hcclDeviceSocketPortRange = CfgField<std::vector<SocketPortRange>>{"HCCL_NPU_SOCKET_PORT_RANGE", {{16666, 18888}}, 
        [] (const std::string &s) -> std::vector<SocketPortRange> { return CastSocketPortRange(s, "HCCL_NPU_SOCKET_PORT_RANGE"); }};
    fakeEnvConfig.hcclDeviceSocketPortRange.isParsed = true;
    MOCKER_CPP(&EnvConfig::GetHostNicConfig).stubs().will(returnValue(fakeEnvConfig));

    string topoFilePath{HCOMM_CODE_ROOT_DIR "/test/legacy/ut/framework/communicator/topo2pclos.json"};
    MOCKER_CPP(&CommunicatorImpl::GetTopoFilePath)
        .stubs()
        .will(returnValue(topoFilePath));
    MOCKER(HrtGetDevice)
        .stubs()
        .will(returnValue(0));
    RankGraphBuilder rankGraphBuilder;
    unique_ptr<RankGraph> graph = rankGraphBuilder.Build(RankTable2pClos, topoFilePath, 0);
    EXPECT_NE(nullptr, graph);
    NewRankInfo rankInfo = rankGraphBuilder.GetRankTableInfo()->ranks[0];
    EXPECT_NO_THROW(SocketManager::ServerInitAll(rankInfo));
}

// TEST_F(SocketManagerTest, test_BatchCreateSockets_with_SocketConfig_and_GetConnectedSocket) {
//     MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
//
//     SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
//     LinkData link(PortDeploymentType::DEV_NET, LinkProtocol::UB_CTP, 0, 3,
//         IpAddress("0.0.0.0"), IpAddress("3.0.0.0"));
//     std::string tag = "test_socket_config";
//     SocketConfig socketConfig(link, tag);
//     socketMgr.BatchCreateSockets(socketConfig);
//     Socket *socket = socketMgr.GetConnectedSocket(socketConfig);
//     EXPECT_NE(nullptr, socket);
// }

// TEST_F(SocketManagerTest, test_BatchCreateSockets_with_SocketConfig_reuse_existing_socket) {
//     MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
//
//     SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
//     LinkData link(PortDeploymentType::DEV_NET, LinkProtocol::UB_CTP, 0, 3,
//         IpAddress("0.0.0.0"), IpAddress("3.0.0.0"));
//     std::string tag = "test_socket_config_reuse";
//     SocketConfig socketConfig(link, tag);
//     socketMgr.BatchCreateSockets(socketConfig);
//     Socket *socketFirst = socketMgr.GetConnectedSocket(socketConfig);
//     EXPECT_NE(nullptr, socketFirst);
//     socketMgr.BatchCreateSockets(socketConfig);
//     Socket *socketSecond = socketMgr.GetConnectedSocket(socketConfig);
//     EXPECT_NE(nullptr, socketSecond);
//     EXPECT_EQ(socketFirst, socketSecond);
// }

// TEST_F(SocketManagerTest, test_GetConnectedSocket_returns_null_for_nonexistent_config) {
//     MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
//     SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
//     LinkData link(PortDeploymentType::DEV_NET, LinkProtocol::UB_CTP, 0, 5,
//         IpAddress("0.0.0.0"), IpAddress("5.0.0.0"));
//     std::string tag = "nonexistent_tag";
//     SocketConfig socketConfig(link, tag);
//     Socket *socket = socketMgr.GetConnectedSocket(socketConfig);
//     EXPECT_EQ(nullptr, socket);
// }

// TEST_F(SocketManagerTest, test_CreateConnectedSocket_with_const_SocketConfig) {
//     MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
//     SocketManager socketMgr(impl, localRank, devicePhyId, listenPort);
//     LinkData link(PortDeploymentType::DEV_NET, LinkProtocol::UB_CTP, 0, 2,
//         IpAddress("0.0.0.0"), IpAddress("2.0.0.0"));
//     std::string tag = "test_const_socket_config";
//     const SocketConfig socketConfig(link, tag);
//     Socket *socket = socketMgr.CreateConnectedSocket(socketConfig);
//     EXPECT_NE(nullptr, socket);
// }