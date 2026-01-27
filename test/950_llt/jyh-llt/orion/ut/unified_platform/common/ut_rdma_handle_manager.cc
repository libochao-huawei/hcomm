#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "rdma_handle_manager.h"


using namespace Hccl;

class RdmaHandleManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "RdmaHandleManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "RdmaHandleManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in RdmaHandleManagerTest SetUP" << std::endl;

        rdmaHandle = new int(0);
        MOCKER(HrtRaRdmaInit).stubs().with(any(), any()).will(returnValue(rdmaHandle));

        BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::RDMA);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete rdmaHandle;
        std::cout << "A Test case in RdmaHandleManagerTest TearDown" << std::endl;
    }

    IpAddress GetAnIpAddress()
    {
        IpAddress ipAddress("1.0.0.0");
        return ipAddress;
    }

    void            *rdmaHandle;
};

TEST_F(RdmaHandleManagerTest, rdma_handle_manager_get_and_create)
{
    // Given
    u32      devicePhyId = 0;
    BasePortType       basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::RDMA);
    PortData           localPortData(0, basePortType, 0, IpAddress());

    // when
    auto res = RdmaHandleManager::GetInstance().Get(devicePhyId, localPortData);

    // then
    EXPECT_EQ(rdmaHandle, res);
}

TEST_F(RdmaHandleManagerTest, rdma_handle_manager_get_twice)
{
    // Given
    u32      devicePhyId = 0;
    BasePortType       basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::RDMA);
    PortData           localPortData(0, basePortType, 0, IpAddress());

    // when
    auto res1 = RdmaHandleManager::GetInstance().Get(devicePhyId, localPortData);
    auto res2 = RdmaHandleManager::GetInstance().Get(devicePhyId, localPortData);

    // then
    EXPECT_EQ(res1, res2);
}

TEST_F(RdmaHandleManagerTest, rdma_handle_manager_get_jfc)
{
    RdmaHandle rdmaHandle = nullptr;
    HrtUbJfcMode mode;
    EXPECT_THROW(RdmaHandleManager::GetInstance().GetJfcHandle(rdmaHandle, mode), InvalidParamsException);
    rdmaHandle = new RdmaHandle();
    EXPECT_THROW(RdmaHandleManager::GetInstance().GetJfcHandle(rdmaHandle, mode), InvalidParamsException);


    RdmaHandle rdmaHandle2 = nullptr;
    EXPECT_THROW(RdmaHandleManager::GetInstance().GetDieAndFuncId(rdmaHandle2), InvalidParamsException);
    delete rdmaHandle;
}

TEST_F(RdmaHandleManagerTest, rdma_handle_manager_get_token_id_handle)
{
    RdmaHandle rdmaHandle = nullptr;
    TokenIdHandle tokenIdHandle;
    EXPECT_THROW(RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle), InvalidParamsException);

    RdmaHandle rdmaHandle1 = (void *)0x12;
    RdmaHandleManager::GetInstance().tokenInfoMap[rdmaHandle1] = make_unique<TokenInfoManager>(0, rdmaHandle1);
    RdmaHandle rdmaHandle2 = (void *)0x1365;
    EXPECT_THROW(RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle2), InvalidParamsException);

    std::pair<TokenIdHandle, uint32_t> expectResult(0, 0);
    EXPECT_EQ(RdmaHandleManager::GetInstance().GetTokenIdInfo(rdmaHandle1), expectResult);
}