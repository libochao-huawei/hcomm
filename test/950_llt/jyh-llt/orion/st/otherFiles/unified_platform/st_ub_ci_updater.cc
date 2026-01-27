#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#include "ub_ci_updater.h"
#include "dev_ub_connection.h"
#undef private
#include "socket.h"
#include "runtime_api_exception.h"
#include "invalid_params_exception.h"
using namespace Hccl;

class UbCiUpdaterTest : public testing::Test {
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
        fakeSocket = new Socket(nullptr, localIp, listenPort, remoteIp, tag, SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in AdapterRts TearDown" << std::endl;
    }
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    std::string tag = "test";
};

TEST(UbCiUpdaterTest, ConstructUbCiUpdater_ok)
{
    // Given
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);

    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);

    // then
    DevUbConnection::UbCiUpdater ubCiUpdater(&devUbConnection);
}

TEST(UbCiUpdaterTest, UbCiUpdaterUpdateCi_ok)
{
    // Given
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);

    MOCKER(RaUbUpdateCi).stubs();

    // when
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    DevUbConnection::UbCiUpdater ubCiUpdater(&devUbConnection);
    ubCiUpdater.ciVal = 100;

    // then
    EXPECT_NO_THROW(ubCiUpdater.UpdateCi());
    EXPECT_EQ(ubCiUpdater.devUbConnPtr->GetCiVal(), 100);
}

TEST(UbCiUpdaterTest, UbCiUpdaterSaveCi_ok)
{
    // Given
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);

    // when
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    devUbConnection.piVal = 100;
    DevUbConnection::UbCiUpdater ubCiUpdater(&devUbConnection);
    ubCiUpdater.SaveCi();

    // then
    EXPECT_EQ(ubCiUpdater.ciVal, 100);
}