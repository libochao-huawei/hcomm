#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "socket_handle_manager.h"
#include "internal_exception.h"

using namespace Hccl;

class SocketHandleManagerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "SocketHandleManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "SocketHandleManagerTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in SocketHandleManagerTest SetUP" << std::endl;
        hccpSocketHandle = new int(0);
        MOCKER(HrtRaSocketInit)
        .stubs()
        .with(any(), any())
        .will(returnValue(hccpSocketHandle));

        BasePortType basePortType(PortDeploymentType::P2P, ConnectProtoType::UB);
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        delete hccpSocketHandle;
        std::cout << "A Test case in SocketHandleManagerTest TearDown" << std::endl;
    }

    IpAddress GetAnIpAddress() {
        IpAddress ipAddress("1.0.0.0");
        return ipAddress;
    }

    void *hccpSocketHandle;
};


TEST_F(SocketHandleManagerTest, should_return_valid_ptr_when_calling_create_with_valid_params) {
    // Given
    u32 devicePhyId = 0;
    BasePortType       basePortType(PortDeploymentType::P2P, ConnectProtoType::UB);
    PortData           localPortData(0, basePortType, 0, IpAddress());

    // when
    auto res = SocketHandleManager::GetInstance().Create(devicePhyId, localPortData);

    // then
    EXPECT_EQ(hccpSocketHandle, res);
}

TEST_F(SocketHandleManagerTest, should_return_ptr_when_calling_create_with_valid_params) {
    // Given
    u32 devicePhyId = 0;
    PortData           localPortData(0, PortDeploymentType::P2P, LinkProtoType::INVALID, 0, IpAddress());

    // then
    EXPECT_THROW(SocketHandleManager::GetInstance().Create(devicePhyId, localPortData), InternalException);
}