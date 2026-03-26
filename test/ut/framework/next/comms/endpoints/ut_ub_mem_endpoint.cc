#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "cpu_roce_endpoint.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"
#include "rdma_handle_manager.h"
#include "buffer/local_rdma_rma_buffer.h"
#include "ip_address.h"
#include "hccp.h"
#include "buffer.h"
#include "network_api_exception.h"
#include "endpoint.h"

class AivUbMemEndpointTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AivUbMemEndpointTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AivUbMemEndpointTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AivUbMemEndpointTest SetUP" << std::endl;
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

// 内存注册成功
TEST_F(AivUbMemEndpointTest, Ut_When_Register_Memory_NORMAL_Expect_Return_SUCCESS)
{
    Hccl::IpAddress   localIp("1.0.0.0");
    EndpointDesc endpointDesc;
    endpointDesc.protocol = COMM_PROTOCOL_UB_MEM;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    void* endpointHandle{nullptr};
    HcclResult ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hcomm::UbMemEndpoint* endpoint = static_cast<hcomm::UbMemEndpoint*>(endpointHandle);

    HcommMem mem;
    mem.type = HCCL_MEM_TYPE_DEVICE;
    mem.size = 10;
    hrtMalloc(&mem.addr, mem.size);
    void *memHandle;

    ret = endpoint->RegisterMemory(mem, "memTag", &memHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    hrtFree(mem.addr);
}
