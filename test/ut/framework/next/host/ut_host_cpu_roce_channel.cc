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
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    EXPECT_EQ(status, ChannelStatus::FAILED);
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
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
