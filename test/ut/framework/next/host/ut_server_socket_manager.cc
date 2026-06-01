#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hcomm_c_adpt.h"
#include "server_socket/server_socket_manager.h"
#define private public
using namespace hcomm;

class ServerSocketManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ServerSocketManagerTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "ServerSocketManagerTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in ServerSocketManagerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in ServerSocketManagerTest TearDown" << std::endl;
    }
};

TEST_F(ServerSocketManagerTest, Ut_When_Device_Socket_Listen_Expect_SUCCESS)
{
    Hccl::IpAddress ipAddr("1.0.0.0");
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::UB);
    Hccl::PortData localPort = Hccl::PortData(0, type, 0, ipAddr);
    HcclResult ret = ServerSocketManager::GetInstance().ServerSocketStartListen(localPort, Hccl::NicType::DEVICE_NIC_TYPE, 0, 60001);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::DEVICE_NIC_TYPE, 60001);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ServerSocketManagerTest, Ut_When_Host_Socket_Listen_Expect_SUCCESS)
{
    Hccl::IpAddress ipAddr("1.0.0.0");
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::RDMA);
    Hccl::PortData localPort = Hccl::PortData(0, type, 0, ipAddr);
    HcclResult ret = ServerSocketManager::GetInstance().ServerSocketStartListen(localPort, Hccl::NicType::HOST_NIC_TYPE, 0, 60001);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::HOST_NIC_TYPE, 60001);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(ServerSocketManagerTest, Ut_When_Stop_Listen_While_Not_Start_Listen_Expect_Fail)
{
    Hccl::IpAddress ipAddr("1.0.0.0");
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::RDMA);
    Hccl::PortData localPort = Hccl::PortData(0, type, 0, ipAddr);
    HcclResult ret = ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::DEVICE_NIC_TYPE, 60001);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(ServerSocketManagerTest, Ut_When_Stop_Listen_While_Listen_Count_Is_Zero_Listen_Expect_Fail)
{
    Hccl::IpAddress ipAddr("1.0.0.0");
    Hccl::DevNetPortType type = Hccl::DevNetPortType(Hccl::ConnectProtoType::RDMA);
    Hccl::PortData localPort = Hccl::PortData(0, type, 0, ipAddr);
    HcclResult ret = ServerSocketManager::GetInstance().ServerSocketStartListen(localPort, Hccl::NicType::HOST_NIC_TYPE, 0, 60001);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ServerSocketManager::GetInstance().hostServerSocketMap_.clear();
    ret = ServerSocketManager::GetInstance().ServerSocketStopListen(localPort, Hccl::NicType::HOST_NIC_TYPE, 60001);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}