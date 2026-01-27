#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "ub_ci_updater.h"
#include "ub_ci_updater_manager.h"
#include "rma_conn_manager.h"
#include "dev_ub_connection.h"
#include "communicator_impl.h"
#include "rdma_handle_manager.h"
#undef private
#include "socket.h"
#include "runtime_api_exception.h"
#include "invalid_params_exception.h"
using namespace Hccl;

class UbCiUpdaterManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AdapterRts tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AdapterRts tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AdapterRts SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AdapterRts TearDown" << std::endl;
    }
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    std::string tag = "test";
};

TEST_F(UbCiUpdaterManagerTest, UbCiUpdaterManagerSaveConnsCi_ok)
{
    // Given
    // Given: socket time out
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    std::pair<TokenIdHandle, uint32_t> fakeTokenInfo = std::make_pair(0x12345678, 1);
    MOCKER_CPP(&RdmaHandleManager::GetTokenIdInfo).stubs().will(returnValue(fakeTokenInfo));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);

    string opTag = "test";

    // When
    auto devUbConn = make_unique<DevUbConnection>(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    devUbConn->piVal = 100;
    CommunicatorImpl comm;
    RmaConnManager rmaConnManager(comm);
    rmaConnManager.rmaConnectionMap[opTag][linkData] = std::move(devUbConn);
    UbCiUpdaterManager ubCiUpdaterManager(&rmaConnManager);
    ubCiUpdaterManager.SaveConnsCi(opTag);
    std::vector<DevUbConnection::UbCiUpdater *> ubCiUpdaters = ubCiUpdaterManager.Get(opTag);

    // then
    EXPECT_EQ(ubCiUpdaters.size(), 1);
    EXPECT_EQ(ubCiUpdaters[0]->ciVal, 100);
}

TEST_F(UbCiUpdaterManagerTest, UbCiUpdaterManagerUpdateConnsCi_ok)
{
    // Given
    // Given: socket time out
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    std::pair<TokenIdHandle, uint32_t> fakeTokenInfo = std::make_pair(0x12345678, 1);
    MOCKER_CPP(&RdmaHandleManager::GetTokenIdInfo).stubs().will(returnValue(fakeTokenInfo));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);

    string opTag = "test";
    MOCKER(RaUbUpdateCi).stubs();

    // When
    auto devUbConn = make_unique<DevUbConnection>(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    devUbConn->piVal = 100;
    CommunicatorImpl comm;
    RmaConnManager rmaConnManager(comm);
    rmaConnManager.rmaConnectionMap[opTag][linkData] = std::move(devUbConn);
    UbCiUpdaterManager ubCiUpdaterManager(&rmaConnManager);
    ubCiUpdaterManager.SaveConnsCi(opTag);
    ubCiUpdaterManager.UpdateConnsCi(opTag);

    // then
    EXPECT_EQ(ubCiUpdaterManager.ubCiUpdaters[opTag][0]->devUbConnPtr->GetCiVal(), 100);
}