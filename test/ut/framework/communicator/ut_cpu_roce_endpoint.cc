#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "cpu_roce_endpoint.h"
#include "hcomm_c_adpt.h"
#include "rdma_handle_manager.h"
#include "ip_address.h"

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

// Device
TEST_F(CpuRoceEndpointTest, Ut_When_Call_GetStatus_Expect_Return_Ready1)
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
TEST_F(CpuRoceEndpointTest, Ut_When_Call_GetStatus_Expect_Return_Ready2)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle = malloc(sizeof(hcomm::CpuRoceEndpoint));
    RdmaHandle rdmaHandle2{nullptr};
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle2));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
    free(endpointHandle);
}

// Ip重复监听
TEST_F(CpuRoceEndpointTest, Ut_When_Call_GetStatus_Expect_Return_Ready3)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle = malloc(sizeof(hcomm::CpuRoceEndpoint));
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    static std::unordered_map<Hccl::IpAddress, std::shared_ptr<Hccl::Socket>, std::hash<Hccl::IpAddress>> serverSocketMap;
    serverSocketMap.insert({localIp, std::shared_ptr<Hccl::Socket>(fakeSocket)});
    MOCKER(&hcomm::CpuRoceEndpoint::GetServerSocketMap).stubs().will(returnValue(serverSocketMap));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    free(endpointHandle);
}

// 内存注册失败
TEST_F(CpuRoceEndpointTest, Ut_When_Call_GetStatus_Expect_Return_Ready4)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    MOCKER(&hcomm::CpuRoceEndpoint::ServerSocketListen).stubs().will(returnValue(HCCL_SUCCESS));
    HcommMem mem;
    mem.type = HCCL_MEM_TYPE_DEVICE;
    mem.addr = malloc(10);
    mem.size = 10;
    void **memHandle;
    // MOCKER(RaRegisterMr).stubs().with(any(), any(), outBoundP(fakeMrHandle)).will(returnValue(0));//s32 ret = RaRegisterMr(rdmaHandle, &mrInfo, &mrHandle);
    // auto endpoint = std::make_unique<hcomm::CpuRoceEndpoint>(endpointDesc);
    // endpoint->Init();
    // endpoint->RegisterMemory(mem, "HcclBuffer", memHandle);
    free(mem.addr);
    free(endpointHandle);
}

// 内存解注册失败
TEST_F(CpuRoceEndpointTest, Ut_When_Call_GetStatus_Expect_Return_Ready5)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    void* endpointHandle = malloc(sizeof(hcomm::CpuRoceEndpoint));
    MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
    MOCKER(&hcomm::CpuRoceEndpoint::ServerSocketListen).stubs().will(returnValue(HCCL_SUCCESS));
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcommMem mem;
    mem.type = HCCL_MEM_TYPE_DEVICE;
    mem.addr = malloc(10);
    mem.size = 10;
    void **memHandle;
    // s32 ret = RaDeregisterMr(rdmaHandle, mrHandle);
    hcomm::CpuRoceEndpoint* endpoint = static_cast<hcomm::CpuRoceEndpoint*>(endpointHandle);
    endpoint->UnregisterMemory(memHandle);
    free(endpointHandle);
}