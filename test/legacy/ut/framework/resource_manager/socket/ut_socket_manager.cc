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


TEST_F(SocketManagerTest, batch_server_listen_async_conect_sockets_should_ok) {
    MOCKER_CPP(&SocketManager::BatchAddWhiteList).stubs();
    SocketManager socketMgr(localRank, devicePhyId, devicePhyId, "tmp");
 	auto link = links[0];
    Hccl::SocketConfig socketConfig(link.GetRemoteRankId(), link, "test");
    socketMgr.ServerListen(socketConfig);
    auto &serverSocketMap = SocketManager::GetServerSocketMap();
    for (const auto& sock: serverSocketMap) {
        EXPECT_EQ(sock.second->socketStatus, SocketStatus::LISTENING);
    }
    socketMgr.ConnectSockets(socketConfig);
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

TEST_F(SocketManagerTest, test_BatchCreateSockets_with_SocketConfig) {
    SocketManager socketMgr(localRank, devicePhyId, devicePhyId, "tmp");
    auto link = links[0];
    Hccl::SocketConfig socketConfig(link.GetRemoteRankId(), link, "test");
    socketMgr.BatchCreateSockets(socketConfig);
    socketMgr.GetConnectedSocket(socketConfig);
}

TEST_F(SocketManagerTest, Ut_HostListenPortDetect_EmptyRankLevelInfos_Expect_NoThrow)
{
    // Given: rankInfo with empty rankLevelInfos
    NewRankInfo rankInfo;
    rankInfo.rankId = 0;
    rankInfo.deviceId = 0;
    rankInfo.localId = 0;
    // rankLevelInfos is empty by default

    MOCKER(HrtGetDevice).stubs().will(returnValue(0));

    // When & Then: function should iterate zero times and return without error
    EXPECT_NO_THROW(SocketManager::HostListenPortDetect(rankInfo));
}

TEST_F(SocketManagerTest, Ut_HostListenPortDetect_NoTopoGraph_Expect_NoThrow)
{
    // Given: rankInfo with rankLevelInfos but PhyTopo has no graph built
    NewRankInfo rankInfo;
    rankInfo.rankId = 0;
    rankInfo.deviceId = 0;
    rankInfo.localId = 0;

    AddressInfo addrInfo;
    addrInfo.addr = IpAddress("192.168.1.1");
    addrInfo.socketPort_ = 0;
    RankLevelInfo levelInfo;
    levelInfo.netLayer = 0;
    levelInfo.rankAddrs.push_back(addrInfo);
    rankInfo.rankLevelInfos.push_back(levelInfo);

    MOCKER(HrtGetDevice).stubs().will(returnValue(0));

    // When: PhyTopo has no built graph, GetTopoGraph returns nullptr
    // Then: function should skip the nullptr graph, log debug, continue, and return without error
    EXPECT_NO_THROW(SocketManager::HostListenPortDetect(rankInfo));
    // hostPort should remain at default since no HOST link was processed
    EXPECT_EQ(rankInfo.hostPort, DEFAULT_VALUE_TCPPORT);
}

TEST_F(SocketManagerTest, Ut_HostListenPortDetect_MultipleRankLevelInfos_Expect_NoThrow)
{
    // Given: multiple rankLevelInfos, all with no topo graph
    NewRankInfo rankInfo;
    rankInfo.rankId = 0;
    rankInfo.deviceId = 0;
    rankInfo.localId = 0;

    for (u32 i = 0; i < 3; i++) {
        AddressInfo addrInfo;
        addrInfo.addr = IpAddress(StringFormat("192.168.%u.1", i + 1));
        addrInfo.socketPort_ = 0;
        RankLevelInfo levelInfo;
        levelInfo.netLayer = i;
        levelInfo.rankAddrs.push_back(addrInfo);
        rankInfo.rankLevelInfos.push_back(levelInfo);
    }

    MOCKER(HrtGetDevice).stubs().will(returnValue(0));

    // When & Then: iterate all level infos, all graphs nullptr, no throw
    EXPECT_NO_THROW(SocketManager::HostListenPortDetect(rankInfo));
    EXPECT_EQ(rankInfo.hostPort, DEFAULT_VALUE_TCPPORT);
}

TEST_F(SocketManagerTest, Ut_TearDown_HostSocketNull_Expect_EarlyReturn)
{
    // Given: hostSocket_ is nullptr (default state, no HostListenPortDetect called)

    // When & Then: TearDown should return immediately without any side effects
    EXPECT_NO_THROW(SocketManager::TearDown());
}

TEST_F(SocketManagerTest, Ut_HostListenPortDetect_TearDown_Sequence_Expect_NoThrow)
{
    // Given: full sequence HostListenPortDetect → TearDown with no HOST links
    NewRankInfo rankInfo;
    rankInfo.rankId = 0;
    rankInfo.deviceId = 0;
    rankInfo.localId = 0;
    AddressInfo addrInfo;
    addrInfo.addr = IpAddress("10.0.0.1");
    RankLevelInfo levelInfo;
    levelInfo.netLayer = 0;
    levelInfo.rankAddrs.push_back(addrInfo);
    rankInfo.rankLevelInfos.push_back(levelInfo);

    MOCKER(HrtGetDevice).stubs().will(returnValue(0));

    // When: execute the full sequence
    EXPECT_NO_THROW(SocketManager::HostListenPortDetect(rankInfo));
    EXPECT_NO_THROW(SocketManager::TearDown());

    // Then: calling TearDown again should still be safe (idempotent)
    EXPECT_NO_THROW(SocketManager::TearDown());
}