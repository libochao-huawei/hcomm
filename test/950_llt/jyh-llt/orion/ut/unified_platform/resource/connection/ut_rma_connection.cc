#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "rma_connection.h"
#include "socket.h"
#include "not_support_exception.h"

using namespace Hccl;

class RmaConnectionTest : public testing::Test {
    protected:
    static void SetUpTestCase()
    {
        std::cout << "DevUbConnection tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "DevUbConnection tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in DevUbConnection SetUP" << std::endl;
        fakeSocket = new Socket(nullptr, localIp, listenPort, remoteIp, tag, SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in DevUbConnection TearDown" << std::endl;
    }

    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    std::string tag = "test";
};

class FakeRmaConnection : public RmaConnection {
public:
    FakeRmaConnection(Socket *socket, const RmaConnType rmaConnType) : RmaConnection(socket, rmaConnType) {}

    void Connect() override {}
    string Describe() const override {}
};

TEST_F(RmaConnectionTest, test_rma_connection_inline_write) {
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);

    SqeConfig sqeConfig;
    FakeRmaConnection rmaConn(fakeSocket, RmaConnType::UB);
    MemoryBuffer memBuffer(0, 0, 0);
    EXPECT_THROW(rmaConn.PrepareInlineWrite(memBuffer, 0, sqeConfig), NotSupportException);
}