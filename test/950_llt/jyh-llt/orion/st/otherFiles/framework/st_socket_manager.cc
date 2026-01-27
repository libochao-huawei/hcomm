#define private public
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "socket_manager.h"
#include "socket_handle_manager.h"
#include "communicator_impl.h"
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
        std::cout << "A Test case in SocketManagerTest SetUP" << std::endl;
        hccpSocketHandle = new int(0);
        std::cout << "1";
        MOCKER_CPP(&SocketHandleManager::Create)
            .stubs()
            .with(any(), any())
            .will(returnValue(hccpSocketHandle));
        std::cout << "2";
        MOCKER_CPP(&SocketHandleManager::Get)
            .stubs()
            .with(any(), any())
            .will(returnValue(hccpSocketHandle));
        std::cout << "3";
        SetLinks();
        std::cout << "4";
        std::cout << "A Test case in SocketManagerTest SetUP done" << std::endl;
    }
 
    virtual void TearDown() {
        GlobalMockObject::verify();
        delete hccpSocketHandle;
        std::cout << "A Test case in SocketManagerTest TearDown" << std::endl;
    }

    IpAddress GetAnIpAddress()
    {
        IpAddress ipAddress("1.0.0.0");
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
            IpAddress localIp = GetAnIpAddress();
            IpAddress remoteIp = GetAnIpAddress();
            LinkData tmpLink(portDeploymentType, linkProtocol, localRankId, remoteRankId,
                localIp, remoteIp);
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