#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <vector>
#include <iostream>
#include "cpu_urma_endpoint.h"
#include "host_cpu_urma_channel.h"
#include "buffer/local_ub_rma_buffer.h"
#include "host_ub_connection.h"
#include "ip_address.h"
#include "op_mode.h"
#include "rdma_handle_manager.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"
#include "socket.h"
#include "urma_api.h"
#include "dlurma_function.h"

#define private public
using namespace hcomm;

class HostCpuUrmaChannelTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HostCpuUrmaChannelTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HostCpuUrmaChannelTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HostCpuUrmaChannelTest SetUP" << std::endl;
        Hccl::IpAddress localIp("1.0.0.0");
        Hccl::IpAddress remoteIp("2.0.0.0");

        MOCKER(hrtGetDevice).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));

        RdmaHandle rdmaHandle = (void*)0x1000000;
        MOCKER(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(rdmaHandle));
        MOCKER(RaSocketSetWhiteListStatus).stubs().will(returnValue(0));
        MOCKER(HcommEndpointStartListen).stubs().will(returnValue(static_cast<HcommResult>(HCCL_SUCCESS)));

        EndpointDesc epDesc{};
        epDesc.protocol = COMM_PROTOCOL_UBC_CTP;
        epDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        epDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        epDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        endpoint = std::make_unique<CpuUrmaEndpoint>(epDesc);
        endpoint->Init();
        endpointHandle = static_cast<EndpointHandle>(endpoint.get());

        channelDesc.remoteEndpoint = epDesc;
        channelDesc.exchangeAllMems = false;
        channelDesc.port = 60001;

        fakeSocket = new Hccl::Socket(nullptr, localIp, 60001, remoteIp, "_0_1_",
                                      Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
        channelDesc.socket = static_cast<void*>(fakeSocket);

        localBufferPtr = std::make_shared<Hccl::Buffer>(666);
        rdmaHandle_ = rdmaHandle;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostCpuUrmaChannelTest TearDown" << std::endl;
        delete fakeSocket;
    }

    std::unique_ptr<CpuUrmaEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    RdmaHandle rdmaHandle_{nullptr};
};

TEST_F(HostCpuUrmaChannelTest, Ut_When_ParseInputParam_NullEndpoint_Expect_HCCL_E_PTR)
{
    channelDesc.socket = nullptr;
    auto impl = std::make_unique<HostCpuUrmaChannel>(nullptr, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_E_PTR);
}

TEST_F(HostCpuUrmaChannelTest, Ut_When_StartListen_Fail_Expect_Error)
{
    MOCKER(HcommEndpointStartListen).stubs().will(returnValue(static_cast<HcommResult>(HCCL_E_INTERNAL)));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_NE(impl->Init(), HCCL_SUCCESS);
}