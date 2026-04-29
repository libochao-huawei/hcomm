#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "cpu_urma_endpoint.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"
#include "rdma_handle_manager.h"
#include "ip_address.h"
#include "hccp.h"
#include "buffer.h"
#include "endpoint.h"
#include "urma_mem.h"
#include "adapter_rts_common.h"
#include "server_socket_manager.h"
#include "hccp_peer_manager.h"

#define private public
using namespace hcomm;

class CpuUrmaEndpointTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CpuUrmaEndpointTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CpuUrmaEndpointTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CpuUrmaEndpointTest SetUP" << std::endl;
        Hccl::IpAddress localIp("1.0.0.0");
        endpointDesc.protocol = COMM_PROTOCOL_UB;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        rdmaHandle = (void*)0x1000000;
        
        MOCKER(hrtGetDevice).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
        MOCKER(RaSocketSetWhiteListStatus).stubs().will(returnValue(0));
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CpuUrmaEndpointTest TearDown" << std::endl;
    }

    EndpointDesc endpointDesc{};
    RdmaHandle rdmaHandle{nullptr};
};

TEST_F(CpuUrmaEndpointTest, Ut_When_Normal_Init_Expect_HCCL_SUCCESS)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_hrtGetDevice_Fails_Expect_HCCL_E_INTERNAL)
{
    MOCKER(hrtGetDevice).stubs().will(returnValue(HCCL_E_INTERNAL));
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_E_INTERNAL);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_RdmaHandle_Is_Null_Expect_HCCL_E_PTR)
{
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue((void*)nullptr));
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_E_PTR);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_ServerSocketListen_Normal_Expect_HCCL_SUCCESS)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
    MOCKER_CPP(&Hccl::ServerSocketManager::ServerSocketStartListen).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(endpoint->ServerSocketListen(60001), HCCL_SUCCESS);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_ServerSocketStopListen_Normal_Expect_HCCL_SUCCESS)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
    MOCKER_CPP(&Hccl::ServerSocketManager::ServerSocketStopListen).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(endpoint->ServerSocketStopListen(60001), HCCL_SUCCESS);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_RegisterMemory_Normal_Expect_HCCL_SUCCESS)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
    
    HcommMem mem;
    mem.type = COMM_MEM_TYPE_HOST;
    mem.addr = malloc(10);
    mem.size = 10;
    void* memHandle = nullptr;
    MOCKER_CPP(&hcomm::UbRegedMemMgr::RegisterMemory).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(endpoint->RegisterMemory(mem, "test", &memHandle), HCCL_SUCCESS);
    free(mem.addr);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_UnregisterMemory_Normal_Expect_HCCL_SUCCESS)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
    
    void* memHandle = (void*)0x12345678;
    MOCKER_CPP(&hcomm::UbRegedMemMgr::UnregisterMemory).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(endpoint->UnregisterMemory(memHandle), HCCL_SUCCESS);
}

TEST_F(CpuUrmaEndpointTest, Ut_When_GetCcuChannelCtxPool_Expect_Nullptr)
{
    auto endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
    EXPECT_EQ(endpoint->Init(), HCCL_SUCCESS);
    EXPECT_EQ(endpoint->GetCcuChannelCtxPool(), nullptr);
}
