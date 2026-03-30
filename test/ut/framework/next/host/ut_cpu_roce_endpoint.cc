#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "cpu_roce_endpoint.h"
#include "hcomm_c_adpt.h"
#include "rdma_handle_manager.h"
#include "buffer/local_rdma_rma_buffer.h"
#include "ip_address.h"
#include "hccp.h"
#include "buffer.h"

class CpuRoceEndpointTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CpuRoceEndpointTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CpuRoceEndpointTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CpuRoceEndpointTest SetUP" << std::endl;
        Hccl::IpAddress   localIp("1.0.0.0");
        Hccl::IpAddress   remoteIp("2.0.0.0");
        fakeSocket = new Hccl::Socket(nullptr, localIp, listenPort, remoteIp, tag, Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in HostRdmaConnection TearDown" << std::endl;
    }
    Hccl::Socket     *fakeSocket;
    
    u32         listenPort = 100;
    std::string tag        = "test";
    RdmaHandle   rdmaHandle = (void *)0x1000000;
};

TEST_F(CpuRoceEndpointTest, Ut_When_Normal_EXPECT_Return_HCCL_SUCCESS)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle{nullptr};
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Device
TEST_F(CpuRoceEndpointTest, Ut_When_Endpoint_LocType_Device_Expect_Return_HCCL_E_PARA)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    void* endpointHandle = malloc(sizeof(hcomm::CpuRoceEndpoint));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
    free(endpointHandle);
}

// RdmaHandle初始化失败
TEST_F(CpuRoceEndpointTest, Ut_When_RdmaHandle_Init_Fail_Expect_Return_HCCL_E_PTR)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle{nullptr};
    RdmaHandle rdmaHandle2{nullptr};
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle2));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Ip重复监听
TEST_F(CpuRoceEndpointTest, Ut_When_Listen_Repeat_Ip_EXPECT_Return_HCCL_SUCCESS)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle{nullptr};
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 内存注册失败
TEST_F(CpuRoceEndpointTest, Ut_When_Register_Memory_Fail_Expect_Return_HCCL_E_PTR)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle{nullptr};
    MOCKER_CPP(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::CpuRoceEndpoint* endpoint = static_cast<hcomm::CpuRoceEndpoint*>(endpointHandle);
    HcommMem mem;
    mem.type = HCCL_MEM_TYPE_DEVICE;
    mem.addr = malloc(10);
    mem.size = 10;
    ret = endpoint->RegisterMemory(mem, "HcclBuffer", nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(mem.addr);
}

// 内存解注册失败
TEST_F(CpuRoceEndpointTest, Ut_When_Unregister_Memory_Fail_Expect_Return_HCCL_E_PTR)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle{nullptr};
    MOCKER_CPP(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::CpuRoceEndpoint* endpoint = static_cast<hcomm::CpuRoceEndpoint*>(endpointHandle);
    HcommMem mem;
    mem.type = HCCL_MEM_TYPE_DEVICE;
    mem.addr = malloc(10);
    mem.size = 10;
    void* memHandle{nullptr};
    void* mrHandle{nullptr};
    ret = endpoint->UnregisterMemory(memHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
    auto localBufferPtr = std::make_shared<Hccl::Buffer>(666);
    auto localRdmaRmaBuffer = std::make_shared<Hccl::LocalRdmaRmaBuffer>(localBufferPtr, rdmaHandle);
    memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    ret = endpoint->UnregisterMemory(memHandle);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
    free(mem.addr);
}