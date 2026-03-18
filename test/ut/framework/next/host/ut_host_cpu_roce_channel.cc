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
    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer;
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs{std::make_shared<Hccl::Buffer>((uintptr_t)2, 64)};
    std::unique_ptr<CpuRoceEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
};

TEST_F(HostCpuRoceChannelTest, Ut_When_Normal_Expect_HCCL_SUCCESS)
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
    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    ret = impl_->NotifyWait(0, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = impl_->WriteWithNotify((void*)0x0001, (void*)0x0002, 10, 1);
    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    ret = impl_->NotifyWait(1, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    ret = impl_->NotifyRecord(2);
    EXPECT_EQ(ret, HCCL_E_ROCE_CONNECT);
    ret = impl_->NotifyWait(2, 1800);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
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