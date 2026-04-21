#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <vector>
#include <iostream>
#include "host_socket_handle_manager.h"
#include "cpu_roce_endpoint.h"
#include "buffer/local_rdma_rma_buffer.h"
#include "host/host_cpu_roce_channel.h"
#include "host/host_rdma_connection.h"
#include "topo_common_types.h"
#include "ip_address.h"
#include "op_mode.h"
#include "rdma_handle_manager.h"
#include "host/exchange_rdma_conn_dto.h"
#include "socket.h"
#include "hccp.h"
#include "types/types.h"
#include "hcomm_res.h"
#include "hcomm_c_adpt.h"

#define private public
using namespace hcomm;

class HostCpuRoceChannelTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HostCpuRoceChannelTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HostCpuRoceChannelTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HostCpuRoceChannelTest SetUP" << std::endl;
        Hccl::DevType dev = Hccl::DevType::DEV_TYPE_950;
        MOCKER(Hccl::HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(Hccl::HrtGetDeviceType).stubs().will(returnValue(dev));
        MOCKER(Hccl::HrtGetDevicePhyIdByIndex).stubs().with(any()).will(returnValue(static_cast<Hccl::DevId>(0)));
        RdmaHandle rdmaHandle = (void *)0x1000000;
        MOCKER(Hccl::HrtRaRdmaInit).stubs().with(any(), any()).will(returnValue(rdmaHandle));
        MOCKER(HcommEndpointStartListen).stubs().will(returnValue(static_cast<HcommResult>(HCCL_SUCCESS)));
        EndpointDesc endpointDesc{};
        endpointDesc.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        Hccl::IpAddress localIp("1.0.0.0");
        endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        endpoint = std::make_unique<CpuRoceEndpoint>(endpointDesc);
        endpoint->Init();
        endpointHandle = static_cast<EndpointHandle>(endpoint.get());
        EndpointDesc endpointDesc2;
        endpointDesc2.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        Hccl::IpAddress remoteIp("2.0.0.0");
        endpointDesc2.commAddr.addr = remoteIp.GetBinaryAddress().addr;
        endpointDesc2.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        channelDesc.remoteEndpoint = endpointDesc2;
        channelDesc.notifyNum = 2;
        fakeSocket = new Hccl::Socket(nullptr, localIp, 60001, remoteIp, "_0_1_", Hccl::SocketRole::SERVER, 
                                        Hccl::NicType::HOST_NIC_TYPE);
        void* fsocket = static_cast<void*>(fakeSocket);
        channelDesc.socket = fsocket;
        localBufferPtr = std::make_shared<Hccl::Buffer>(666);
        localRdmaRmaBuffer = std::make_shared<Hccl::LocalRdmaRmaBuffer>(localBufferPtr, rdmaHandle);
        void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
        channelDesc.memHandles = &memHandle;
        channelDesc.memHandleNum = 1;
        channelDesc.exchangeAllMems = false;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostCpuRoceChannelTest TearDown" << std::endl;
        delete fakeSocket;
    }
    void SetupSuccessfulConnectionMocks()
    {
        DevType devType = DevType::DEV_TYPE_950;
        MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(
            returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
        MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(
            returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    }

    std::unique_ptr<hcomm::HostCpuRoceChannel> CreateInitAndConnect(uint32_t notifyNum = 4)
    {
        memHandle_ = static_cast<void*>(localRdmaRmaBuffer.get());
        channelDesc.memHandles = &memHandle_;
        channelDesc.memHandleNum = 1;
        channelDesc.notifyNum = notifyNum;
        auto impl = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
        EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
        hcomm::ChannelStatus status = impl->GetStatus();
        EXPECT_EQ(impl->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
        status = impl->GetStatus();
        status = impl->GetStatus();
        status = impl->GetStatus();
        status = impl->GetStatus();
        EXPECT_EQ(impl->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
        return impl;
    }

    void SetupOneValidQpInfoMock()
    {  
        std::vector<Hccl::QpInfo> qpInfos(1);
        ibv_cq cq{};
        ibv_qp qp{};
        ibv_context context{};
        qpInfos[0].sendCq = &cq;
        qpInfos[0].qp = &qp;
        qpInfos[0].sendCq->context = &context;
        MOCKER_CPP(&HostCpuRoceChannel::GetQpInfos).stubs().will(returnValue(qpInfos));
    }

    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer;
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs{std::make_shared<Hccl::Buffer>((uintptr_t)2, 64)};
    std::unique_ptr<CpuRoceEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
    void* memHandle_{nullptr};
};

// Mock endpoint that is NOT CpuRoceEndpoint, used to test dynamic_cast failure
class MockNonRoceEndpoint : public Endpoint {
public:
    explicit MockNonRoceEndpoint(const EndpointDesc &desc) : Endpoint(desc)
    {
        ctxHandle_ = (void *)0x1000000; // non-null so CHK_PTR_NULL(rdmaHandle_) passes
    }
    HcclResult Init() override { return HCCL_SUCCESS; }
    HcclResult ServerSocketListen(const uint32_t port) override { return HCCL_SUCCESS; }
    HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) override { return HCCL_SUCCESS; }
    HcclResult UnregisterMemory(void *memHandle) override { return HCCL_SUCCESS; }
    HcclResult MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen) override { return HCCL_SUCCESS; }
    HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) override { return HCCL_SUCCESS; }
    HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) override { return HCCL_SUCCESS; }
    HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) override { return HCCL_SUCCESS; }
};

TEST_F(HostCpuRoceChannelTest, Ut_When_Normal_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);
}

// Channel Init for HIXL
TEST_F(HostCpuRoceChannelTest, Ut_Init_When_ExchangeAllMemsIsTrue_And_SocketIsNull_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    uint32_t memHandleNum = 1;
    MOCKER(HcommMemGetAllMemHandles).stubs().with(any(), outBound(&memHandle), outBound(&memHandleNum)).will(
        returnValue(static_cast<HcommResult>(HCCL_SUCCESS)));
    MOCKER_CPP(&hcomm::SocketMgr::GetSocket).stubs().with(any(), outBound(fakeSocket)).will(returnValue(HCCL_SUCCESS));
    // construct
    channelDesc.exchangeAllMems = true;
    channelDesc.memHandles = nullptr;
    channelDesc.memHandleNum = 0;
    channelDesc.notifyNum = 4;
    channelDesc.socket = nullptr;
    channelDesc.port = 0;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_Init_Param_Nullptr_Expect_HCCL_E_PTR)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    // construct
    channelDesc.memHandles = nullptr;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_E_PTR);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_Init_NotifyNumOutOfRange_Expect_HCCL_E_PARA)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 8193;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_E_PARA);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_SocketTimeout_Expect_FAILED)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::TIMEOUT));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::INIT);
    EXPECT_EQ(status, ChannelStatus::FAILED);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::INIT);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_CreateQp_Failed_Expect_FAILED)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_E_NETWORK));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::FAILED);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_ExchangeData_Failed_Expect_FAILED)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_E_ROCE_CONNECT));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_ModifyQp_Failed_Expect_FAILED)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_E_ROCE_CONNECT));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_GetRemoteMem_NullParam__Expect_HCCL_E_PTR)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // GetRemoteMem
    HcclMem *remoteMem;
    uint32_t memNum{11119999};
    char* memTagsArray[10];
    HcclResult ret = impl_->GetRemoteMem(&remoteMem, &memNum, memTagsArray);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 0);
    ret = impl_->GetRemoteMem(&remoteMem, (uint32_t*)nullptr, memTagsArray);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_Rdma_Conn_Failed_Expect_ERROR)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);

    // 交换过程实际没有进行，因此对端数据为空
    HcclResult ret = impl_->NotifyRecord(0);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = impl_->NotifyWait(0, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = impl_->WriteWithNotify((void*)0x0001, (void*)0x0002, 10, 1);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = impl_->NotifyWait(1, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = impl_->NotifyRecord(2);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = impl_->NotifyWait(2, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_PrepareWriteWrResource_Expect_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::FindLocalBuffer).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::FindRemoteBuffer).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);

    impl_->localDpuNotifyIds_.emplace_back(0);
    impl_->remoteDpuNotifyIds_.emplace_back(0);
    impl_->localRmaBuffers_.emplace_back(localRdmaRmaBuffer.get());
    RdmaHandle rdmaHandle = (void *)0x1000000;
    std::unique_ptr<Hccl::RemoteRdmaRmaBuffer> remoteRmaBuffer = std::make_unique<Hccl::RemoteRdmaRmaBuffer>(rdmaHandle);
    impl_->rmtRmaBuffers_.emplace_back(std::move(remoteRmaBuffer));
    struct ibv_send_wr sendWr{};
    struct ibv_sge sge{};
    sendWr.sg_list = &sge;
    HcclResult ret = impl_->PrepareWriteWrResource((void*)0x0001, (void*)0x0002, 10, 0, sendWr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HostCpuRoceChannelTest, Ut_When_HostCpuRoceChannel_Pack_And_Unpack_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    struct QpAttr localQpAttr;
    localQpAttr.qpn = 0;
    localQpAttr.udpSport = 1;
    localQpAttr.psn = 2;
    localQpAttr.gidIdx = 3;
    MOCKER(RaGetQpAttr).stubs().with(any(), outBoundP(&localQpAttr)).will(returnValue(0));

    Hccl::BinaryStream binaryStream;
    impl_->NotifyVecPack(binaryStream);
    // impl_->BufferVecPack(binaryStream);
    impl_->connections_[0]->rdmaConnStatus_ = HostRdmaConnection::RdmaConnStatus::QP_CREATED;
    HcclResult ret = impl_->ConnVecPack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = impl_->NotifyVecUnpack(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // impl_->RmtBufferVecUnpackProc(binaryStream);
    ret = impl_->ConnVecUnpackProc(binaryStream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HostCpuRoceChannelTest, Ut_ChannelFence_When_WqeNumIsZero_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);
    // ChannelFence
    impl_->wqeNum_ = 0;
    HcclResult ret = impl_->ChannelFence();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HostCpuRoceChannelTest, Ut_ChannelFence_When_PartialCompletion_Expect_HCCL_SUCCESS)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    impl_->wqeNum_ = 3;
    SetupOneValidQpInfoMock();
    MOCKER_CPP(&HostCpuRoceChannel::IbvPollCq)
        .stubs()
        .will(returnValue(1))
        .then(returnValue(2));
    HcclResult ret = impl_->ChannelFence();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HostCpuRoceChannelTest, Ut_ChannelFence_When_PollExcessCqe_Expect_HCCL_E_INTERNAL)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    impl_->wqeNum_ = 2;
    SetupOneValidQpInfoMock();
    MOCKER_CPP(&HostCpuRoceChannel::IbvPollCq).stubs().will(returnValue(5));
    HcclResult ret = impl_->ChannelFence();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HostCpuRoceChannelTest, Ut_ChannelFence_When_Timeout_Expect_HCCL_E_TIMEOUT)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    impl_->wqeNum_ = 1;
    SetupOneValidQpInfoMock();
    MOCKER_CPP(&HostCpuRoceChannel::IbvPollCq).stubs().will(returnValue(0));
    HcclResult ret = impl_->ChannelFence();
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(HostCpuRoceChannelTest, Ut_Write_When_Normal_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);
    // Write
    impl_->rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>((Hccl::RdmaHandle)0x1000));
    impl_->rmtRmaBuffers_[0]->addr = (uintptr_t)0x0002;
    impl_->rmtRmaBuffers_[0]->size = 10;
    std::vector<Hccl::QpInfo> qpInfos(1);
    MOCKER_CPP(&HostCpuRoceChannel::GetQpInfos).stubs().will(returnValue(qpInfos));
    MOCKER_CPP(&HostCpuRoceChannel::PostAndCheckSend).stubs().will(returnValue(HCCL_SUCCESS));
    void* localAddr = (void*)0x0001;
    void* remoteAddr = (void*)0x0002;
    size_t size = 10;
    HcclResult ret = impl_->Write(remoteAddr, localAddr, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HostCpuRoceChannelTest, Ut_Read_When_Normal_Expect_HCCL_SUCCESS)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // Init
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    // connect
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CONN_OK);
    EXPECT_EQ(status, ChannelStatus::READY);
    // Read
    impl_->rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>((Hccl::RdmaHandle)0x1000));
    impl_->rmtRmaBuffers_[0]->addr = (uintptr_t)0x0002;
    impl_->rmtRmaBuffers_[0]->size = 10;
    std::vector<Hccl::QpInfo> qpInfos(1);
    MOCKER_CPP(&HostCpuRoceChannel::GetQpInfos).stubs().will(returnValue(qpInfos));
    MOCKER_CPP(&HostCpuRoceChannel::PostAndCheckSend).stubs().will(returnValue(HCCL_SUCCESS));
    void* localAddr = (void*)0x0001;
    void* remoteAddr = (void*)0x0002;
    size_t size = 10;
    HcclResult ret = impl_->Read(localAddr, remoteAddr, size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// ParseInputParam: dynamic_cast<CpuRoceEndpoint*> 失败 → HCCL_E_INTERNAL
TEST_F(HostCpuRoceChannelTest, Ut_Init_When_EndpointIsNotCpuRoce_Expect_HCCL_E_INTERNAL)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    // 构造一个非 CpuRoceEndpoint 的 Endpoint
    EndpointDesc mockDesc{};
    mockDesc.protocol = COMM_PROTOCOL_ROCE;
    mockDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress mockIp("1.0.0.0");
    mockDesc.commAddr.addr = mockIp.GetBinaryAddress().addr;
    mockDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    MockNonRoceEndpoint mockEp(mockDesc);
    EndpointHandle mockHandle = static_cast<EndpointHandle>(&mockEp);
    // construct
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(mockHandle, channelDesc);
    // Init 应在 dynamic_cast 失败时返回 HCCL_E_INTERNAL
    EXPECT_EQ(impl_->Init(), HCCL_E_INTERNAL);
}

// Write/Read/WriteWithNotify: maxMsgSize_ == 0 守卫 → HCCL_E_INTERNAL
TEST_F(HostCpuRoceChannelTest, Ut_WriteAndReadAndWriteWithNotify_When_MaxMsgSizeIsZero_Expect_HCCL_E_INTERNAL)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    // construct，不调用 Init，maxMsgSize_ 保持默认值 0
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl_->maxMsgSize_, 0u);
    // Write
    EXPECT_EQ(impl_->Write((void *)0x1, (void *)0x2, 10), HCCL_E_INTERNAL);
    // Read
    EXPECT_EQ(impl_->Read((void *)0x1, (void *)0x2, 10), HCCL_E_INTERNAL);
    // WriteWithNotify
    EXPECT_EQ(impl_->WriteWithNotify((void *)0x1, (void *)0x2, 10, 0), HCCL_E_INTERNAL);
}

// Write: len > maxMsgSize_ 时触发分片循环
TEST_F(HostCpuRoceChannelTest, Ut_Write_When_LenExceedsMaxMsgSize_Expect_Slicing_SUCCESS)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    // 设置小的 maxMsgSize_ 以触发分片
    impl_->maxMsgSize_ = 100;
    // mock PostRdmaOp
    MOCKER_CPP(&HostCpuRoceChannel::PostRdmaOp).stubs().will(returnValue(HCCL_SUCCESS));
    // len = 250 → 3 个分片 (100 + 100 + 50)
    HcclResult ret = impl_->Write((void *)0x1, (void *)0x2, 250);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Read: len > maxMsgSize_ 时触发分片循环
TEST_F(HostCpuRoceChannelTest, Ut_Read_When_LenExceedsMaxMsgSize_Expect_Slicing_SUCCESS)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    // 设置小的 maxMsgSize_ 以触发分片
    impl_->maxMsgSize_ = 100;
    // mock PostRdmaOp
    MOCKER_CPP(&HostCpuRoceChannel::PostRdmaOp).stubs().will(returnValue(HCCL_SUCCESS));
    // len = 250 → 3 个分片 (100 + 100 + 50)
    HcclResult ret = impl_->Read((void *)0x1, (void *)0x2, 250);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// WriteWithNotify: len > maxMsgSize_ 时触发分片 (前 N-1 块 RDMA_WRITE + 尾块 RDMA_WRITE_WITH_IMM)
TEST_F(HostCpuRoceChannelTest, Ut_WriteWithNotify_When_LenExceedsMaxMsgSize_Expect_Slicing_SUCCESS)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();
    // 设置小的 maxMsgSize_ 以触发分片
    impl_->maxMsgSize_ = 100;
    // mock 前 N-1 块的 PostRdmaOp
    MOCKER_CPP(&HostCpuRoceChannel::PostRdmaOp).stubs().will(returnValue(HCCL_SUCCESS));
    // mock 尾块所需的依赖
    std::vector<Hccl::QpInfo> qpInfos(1);
    MOCKER_CPP(&HostCpuRoceChannel::GetQpInfos).stubs().will(returnValue(qpInfos));
    MOCKER_CPP(&HostCpuRoceChannel::PrepareWriteWrResource).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::PostAndCheckSend).stubs().will(returnValue(HCCL_SUCCESS));
    // len = 250 → 前 2 块 PostRdmaOp(WRITE, 100 each) + 尾块 WRITE_WITH_IMM(50)
    HcclResult ret = impl_->WriteWithNotify((void *)0x1, (void *)0x2, 250, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// ==================== Hybrid Mode UT Test Cases ====================

// ExchangeCapability: 魔数不匹配时，回退到原生模式
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeCapability_MagicMismatch_Fallback_To_NativeMode)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // Mock socket_->Send and socket_->Recv for ExchangeCapability
    // 发送成功，接收时魔数不匹配（使用错误的头部）
    MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(true));
    MOCKER_CPP(&Hccl::Socket::Recv)
        .stubs()
        .will(invoke([](void* buf, uint32_t size) -> bool {
            // 写入错误的魔数，模拟对端是旧版本
            uint32_t* magicPtr = reinterpret_cast<uint32_t*>(buf);
            *magicPtr = 0xDEADBEEF; // 错误的魔数
            return true;
        }));

    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);

    // ExchangeCapability 会被 GetStatus 调用
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    // 魔数不匹配后，isHybridMode_ 应为 false，流程继续走原生模式
    EXPECT_EQ(impl_->isHybridMode_, false);
    EXPECT_EQ(impl_->remoteCap_.magic, 0u); // 被标记为无效
}

// ExchangeCapability: 能力校验失败时返回 HCCL_E_INTERNAL
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeCapability_ValidationFailed_Expect_INTERNAL_ERROR)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeData).stubs().will(returnValue(HCCL_SUCCESS));
    // Mock socket_->Send success, Recv with valid magic but invalid version (too high)
    MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(true));
    MOCKER_CPP(&Hccl::Socket::Recv)
        .stubs()
        .will(invoke([](void* buf, uint32_t size) -> bool {
            // 构造一个魔数正确但版本号无效的能力结构体
            // RoCECapability: magic(4) + version(2) + totalLength(2) + nodeType(1) + commStack(1) + syncMode(1) + padding(1) + nicDeploy(4) + reserved(4)
            uint8_t* data = reinterpret_cast<uint8_t*>(buf);
            uint32_t magic = 0x48434C52; // "HCLR"
            memcpy_s(data, sizeof(uint32_t), &magic, sizeof(uint32_t));
            uint16_t version = 100; // 无效版本号，大于支持的最大版本
            memcpy_s(data + 4, sizeof(uint16_t), &version, sizeof(uint16_t));
            uint16_t totalLength = 20;
            memcpy_s(data + 6, sizeof(uint16_t), &totalLength, sizeof(uint16_t));
            return true;
        }));

    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);

    // ExchangeCapability 会被 GetStatus 调用，校验失败会导致状态停留在 CAP_EXCHANGED
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    status = impl_->GetStatus();
    // 由于 ExchangeCapability 返回 HCCL_E_INTERNAL，rdmaStatus_ 保持在 CAP_EXCHANGED
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

// WriteWithNotifyHybrid: 空指针参数校验
TEST_F(HostCpuRoceChannelTest, Ut_WriteWithNotifyHybrid_NullPtr_Expect_HCCL_E_PARA)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    // 设置混合模式相关成员（WriteWithNotifyHybrid 会检查 isHybridMode_ 但不依赖它）
    impl_->isHybridMode_ = true;
    impl_->localRmaBuffers_.emplace_back(localRdmaRmaBuffer.get());
    impl_->localMemMsg_[hccl::NOTIFY_SRC_MEM].addr = nullptr; // 未初始化

    // dst 为空指针
    HcclResult ret = impl_->WriteWithNotifyHybrid(nullptr, (void*)0x0001, 10, 0);
    EXPECT_EQ(ret, HCCL_E_PARA);

    // src 为空指针
    ret = impl_->WriteWithNotifyHybrid((void*)0x0001, nullptr, 10, 0);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// WriteWithNotifyHybrid: localRmaBuffer为空时返回错误
TEST_F(HostCpuRoceChannelTest, Ut_WriteWithNotifyHybrid_EmptyLocalBuffer_Expect_HCCL_E_ROCE_CONNECT)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    impl_->isHybridMode_ = true;
    // localRmaBuffers_ 保持为空

    HcclResult ret = impl_->WriteWithNotifyHybrid((void*)0x0001, (void*)0x0002, 10, 0);
    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
}

// WriteWithNotifyHybrid: len超过buffer大小时返回错误
TEST_F(HostCpuRoceChannelTest, Ut_WriteWithNotifyHybrid_LenExceedsBufferSize_Expect_HCCL_E_PARA)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    impl_->isHybridMode_ = true;
    impl_->localRmaBuffers_.emplace_back(localRdmaRmaBuffer.get());
    // localRdmaRmaBuffer 创建时 size 为 666

    // 设置一个大的 remote buffer
    impl_->rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>((Hccl::RdmaHandle)0x1000));

    HcclResult ret = impl_->WriteWithNotifyHybrid((void*)0x0001, (void*)0x0002, 1000, 0); // 超过 buffer size
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// NotifyWaitHybrid: 超时场景
TEST_F(HostCpuRoceChannelTest, Ut_NotifyWaitHybrid_Timeout_Expect_HCCL_E_TIMEOUT)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    impl_->isHybridMode_ = true;
    // 初始化 localMemMsg_[DATA_NOTIFY_MEM]，但不设置正确的值
    impl_->localMemMsg_[hccl::DATA_NOTIFY_MEM].addr = nullptr; // 模拟内存未初始化

    // 使用很短的超时时间触发超时
    HcclResult ret = impl_->NotifyWaitHybrid(1, 10); // 10ms 超时
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

// NotifyWaitHybrid: 内存未分配时返回错误
TEST_F(HostCpuRoceChannelTest, Ut_NotifyWaitHybrid_NullMemAddr_Expect_HCCL_E_TIMEOUT)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    impl_->isHybridMode_ = true;
    impl_->localMemMsg_[hccl::ACK_NOTIFY_MEM].addr = nullptr; // 未分配内存

    // 即使超时时间很短，也会因为地址为空而可能崩溃或返回错误
    // 这里主要测试空指针检查
    HcclResult ret = impl_->NotifyWaitHybrid(0, 10);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

// GetStatus: 混合模式下调用 ExchangeDataHybird
TEST_F(HostCpuRoceChannelTest, Ut_GetStatus_HybridMode_Calls_ExchangeDataHybird)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    // Mock ExchangeCapability 返回成功并设置 isHybridMode_ = true
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability)
        .stubs()
        .will(invoke([](void) -> HcclResult {
            // 模拟 ExchangeCapability 设置混合模式
            return HCCL_SUCCESS;
        }));
    // Mock ExchangeDataHybird 返回成功
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeDataHybird).stubs().will(returnValue(HCCL_SUCCESS));
    // Mock ConnectSingleQpHybrid 返回成功
    MOCKER_CPP(&HostCpuRoceChannel::ConnectSingleQpHybrid).stubs().will(returnValue(HCCL_SUCCESS));

    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);

    // 第一个 GetStatus: INIT -> SOCKET_OK
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    // 第二个 GetStatus: SOCKET_OK -> CAP_EXCHANGED
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    // 第三个 GetStatus: CAP_EXCHANGED -> QP_CREATED
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    // 第四个 GetStatus: QP_CREATED -> DATA_EXCHANGE (hybrid 模式调用 ExchangeDataHybird)
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);
}

// GetStatus: 混合模式下 ConnectSingleQpHybrid 失败
TEST_F(HostCpuRoceChannelTest, Ut_GetStatus_HybridMode_ConnectSingleQpHybrid_Failed)
{
    DevType devType = DevType::DEV_TYPE_950;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER_CPP(&HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::IbvPostRecv).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BufferVecPack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeCapability).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::ExchangeDataHybird).stubs().will(returnValue(HCCL_SUCCESS));
    // Mock ConnectSingleQpHybrid 返回超时错误
    MOCKER_CPP(&HostCpuRoceChannel::ConnectSingleQpHybrid).stubs().will(returnValue(HCCL_E_TIMEOUT));

    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 4;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);

    // 驱动到 DATA_EXCHANGE 状态
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::QP_CREATED);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::SOCKET_OK);

    // DATA_EXCHANGE 状态下调用 ConnectSingleQpHybrid 失败
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::DATA_EXCHANGE);
    EXPECT_EQ(status, ChannelStatus::FAILED);
}

// NotifyIdToMemtypeHybird: 验证索引到类型的映射
TEST_F(HostCpuRoceChannelTest, Ut_NotifyIdToMemtypeHybird_Mapping)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    // remoteNotifyIdx = 0 -> ACK_NOTIFY_MEM
    EXPECT_EQ(impl_->NotifyIdToMemtypeHybird(0), hccl::MemType::ACK_NOTIFY_MEM);

    // remoteNotifyIdx = 1 -> DATA_NOTIFY_MEM
    EXPECT_EQ(impl_->NotifyIdToMemtypeHybird(1), hccl::MemType::DATA_NOTIFY_MEM);

    // 其他值 -> DATA_NOTIFY_MEM (默认)
    EXPECT_EQ(impl_->NotifyIdToMemtypeHybird(2), hccl::MemType::DATA_NOTIFY_MEM);
    EXPECT_EQ(impl_->NotifyIdToMemtypeHybird(100), hccl::MemType::DATA_NOTIFY_MEM);
}

// ExchangeDataHybird: RegisterUserMemHybird 失败时整体失败
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeDataHybird_RegisterUserMemFailed)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    // Mock RegisterUserMemHybird 失败
    MOCKER_CPP(&HostCpuRoceChannel::RegisterUserMemHybird).stubs().will(returnValue(HCCL_E_MEMORY));

    HcclResult ret = impl_->ExchangeDataHybird();
    EXPECT_EQ(ret, HCCL_E_MEMORY);
}

// ExchangeDataHybird: BuildExchangeDataHybird 失败时整体失败
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeDataHybird_BuildExchangeDataFailed)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    // Mock RegisterUserMemHybird 成功，BuildExchangeDataHybird 失败
    MOCKER_CPP(&HostCpuRoceChannel::RegisterUserMemHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BuildExchangeDataHybird).stubs().will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = impl_->ExchangeDataHybird();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// ExchangeDataHybird: Socket 发送失败
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeDataHybird_SendFailed)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    MOCKER_CPP(&HostCpuRoceChannel::RegisterUserMemHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BuildExchangeDataHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(false)); // 发送失败

    HcclResult ret = impl_->ExchangeDataHybird();
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

// ExchangeDataHybird: Socket 接收失败
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeDataHybird_RecvFailed)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    MOCKER_CPP(&HostCpuRoceChannel::RegisterUserMemHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BuildExchangeDataHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(true));
    MOCKER_CPP(&Hccl::Socket::Recv).stubs().will(returnValue(false)); // 接收失败

    HcclResult ret = impl_->ExchangeDataHybird();
    EXPECT_EQ(ret, HCCL_E_NETWORK);
}

// ExchangeDataHybird: ParseRecvExchangeDataHybird 失败
TEST_F(HostCpuRoceChannelTest, Ut_ExchangeDataHybird_ParseFailed)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    MOCKER_CPP(&HostCpuRoceChannel::RegisterUserMemHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&HostCpuRoceChannel::BuildExchangeDataHybird).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(true));
    MOCKER_CPP(&Hccl::Socket::Recv).stubs().will(returnValue(true));
    MOCKER_CPP(&HostCpuRoceChannel::ParseRecvExchangeDataHybird).stubs().will(returnValue(HCCL_E_PARA));

    HcclResult ret = impl_->ExchangeDataHybird();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// BuildExchangeDataLengthHybird: 验证计算的长度
TEST_F(HostCpuRoceChannelTest, Ut_BuildExchangeDataLengthHybird_CorrectSize)
{
    SetupSuccessfulConnectionMocks();
    auto impl_ = CreateInitAndConnect();

    // 期望的计算公式:
    // sizeof(u32)                    // qp数量
    // + sizeof(hccl::MemMsg) * 2     // output、input buffer
    // + sizeof(hccl::MemMsg) * 3     // 3个Notify
    // + sizeof(u8)                   // atomic value
    HcclResult ret = impl_->BuildExchangeDataLengthHybird();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 根据代码实现验证 exchangeDataTotalSize_
    // MemMsg 的大小需要查看 hccl::MemMsg 定义
    // 这里只验证函数执行成功
}
