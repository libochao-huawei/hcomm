#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include <vector>
#include <iostream>
#include "host_socket_handle_manager.h"
#include "host/host_cpu_roce_channel.h"
#include "topo_common_types.h"
#include "ip_address.h"
#include "op_mode.h"
#include "rdma_handle_manager.h"
#include "host/exchange_rdma_conn_dto.h"

#define private public
using namespace Hccl;

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
        Hccl::DevType dev = Hccl::DevType::DEV_TYPE_910_95;
        MOCKER(HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(HrtGetDeviceType).stubs().will(returnValue(dev));
        MOCKER(HrtGetDevicePhyIdByIndex).stubs().with(any()).will(returnValue(0));
        RdmaHandle rdmaHandle = (void *)0x1000000;
        MOCKER(HrtRaRdmaInit).stubs().with(any(), any()).will(returnValue(rdmaHandle));
        SocketHandle socketHandle = (void *)0x10000;
        MOCKER_CPP(&HostSocketHandleManager::Get).stubs().with(any(), any()).will(returnValue(socketHandle));
        char targetChipVer[CHIP_VERSION_MAX_LEN] = "Ascend910_9591";
        // MOCKER(HrtGetSocVer)
        //     .stubs()
        //     .with(outBoundP(&targetChipVer[0], sizeof(targetChipVer)), any())
        //     .will(returnValue(RT_ERROR_NONE));
        EndpointDesc* endpointDesc = new EndpointDesc();
        endpointDesc->protocol = COMM_PROTOCOL_ROCE;
        endpointDesc->commAddr.type = COMM_ADDR_TYPE_IP_V4;
        IpAddress localIp("1.0.0.0");
        endpointDesc->commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc->loc.locType = ENDPOINT_LOC_TYPE_HOST;
        endpointHandle = static_cast<EndpointHandle>(endpointDesc);
        EndpointDesc endpointDesc2;
        endpointDesc2.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        IpAddress remoteIp("2.0.0.0");
        endpointDesc2.commAddr.addr = remoteIp.GetBinaryAddress().addr;
        endpointDesc2.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        channelDesc.remoteEndpoint = endpointDesc2;
        channelDesc.notifyNum = 2;
        fakeSocket = new Hccl::Socket(nullptr, localIp, 60001, remoteIp, "_0_1_", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
        void* fsocket = static_cast<void*>(fakeSocket);
        channelDesc.memHandles = &(fsocket);
        channelDesc.memHandleNum = 1;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostCpuRoceChannelTest TearDown" << std::endl;
        delete endpointHandle;
        delete fakeSocket;
    }

    std::vector<std::shared_ptr<Buffer>> bufs{std::make_shared<Buffer>((uintptr_t)2, 64)};
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
};

TEST_F(HostCpuRoceChannelTest, Ut_When_HostCpuRoceChannel_Init_Expect_SUCCESS)
{
    // construct
    constexpr u32 notifyNum = 10;
    auto impl_ = make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    // EXPECT_EQ(impl_->attr_.devicePhyId, 0);
    // EXPECT_EQ(impl_->attr_.opMode, OpMode::OPBASE);
    // EXPECT_NE(impl_->socket_, nullptr);
    // EXPECT_EQ(impl_->notifyNum_, notifyNum);
    // EXPECT_EQ(impl_->localDpuNotifyIds_.size(), notifyNum);
    // EXPECT_EQ(impl_->localRmaBuffers_.size(), impl_->bufferNum_);
    // EXPECT_EQ(impl_->connections_.size(), impl_->connNum_);
    // EXPECT_EQ(impl_->GetStatus(), ChannelStatus::INIT);
    // Init
    // ChannelStatus status = ChannelStatus::READY;
    // SocketStatus socketStatus = SocketStatus::OK;
    // MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue(socketStatus));
    // MOCKER_CPP(&HostCpuRoceChannel::SendExchangeData).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::RecvExchangeData).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::SendFinish).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::RecvFinish).stubs().will(returnValue(true));
    // MOCKER_CPP(&Socket::Send, bool(Socket::*)(const void *, u32)).stubs().with(any(), any()).will(returnValue(true));
    // MOCKER_CPP(&Socket::Recv, bool(Socket::*)(void *, u32)).stubs().with(any(), any()).will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::RecvDataProcess).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::HandshakeMsgUnpack).stubs();
    // MOCKER_CPP(&HostCpuRoceChannel::NotifyVecUnpack).stubs();
    // MOCKER_CPP(&HostCpuRoceChannel::RmtBufferVecUnpackProc).stubs();
    // MOCKER_CPP(&HostCpuRoceChannel::ConnVecUnpackProc).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    // MOCKER_CPP(&HostCpuRoceChannel::ConnVecPack).stubs().will(ignoreReturnValue());
    // MOCKER_CPP(&HostCpuRoceChannel::IsConnResConnected).stubs().will(returnValue(true));
    // MOCKER_CPP(&HostCpuRoceChannel::IsConnsReady).stubs().will(returnValue(true));
    
    // std::cout << "step: ---->" << impl_->ConnectSocket() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
    // impl_->ConnectRdma();
    // std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;

    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    hcomm::ChannelStatus status = impl_->GetStatus();
    std::cout << "step: ---->" << impl_->rdmaStatus_.Describe() << std::endl;
}

// TEST_F(HostCpuRoceChannelTest, Ut_When_HostCpuRoceChannel_Deinit_Expect_SUCCESS)
// {
//     constexpr u32 notifyNum = 10;
//     auto impl_ = make_unique<HostCpuRoceChannel>(localEndpoint, remoteEndpoint, notifyNum, bufs);
//     std::cout << "describe: -----------------------------------------\n " << impl_->Describe() << std::endl;
//     MOCKER_CPP(&HostRdmaConnection::DisConnect).stubs();
//     EXPECT_EQ(impl_->DeInit(), HCCL_SUCCESS);
// }

// TEST_F(HostCpuRoceChannelTest, Ut_HostCpuRoceChannel_Pack_And_Unpack)
// {
//     constexpr u32 notifyNum = 10;
//     auto impl_ = make_unique<HostCpuRoceChannel>(localEndpoint, remoteEndpoint, notifyNum, bufs);
//     // MOCKER_CPP(&HostRdmaConnection::GetStatus).stubs().will(returnValue(HCCL_SUCCESS));
//     BinaryStream binaryStream;
//     impl_->HandshakeMsgPack(binaryStream);
//     impl_->NotifyVecPack(binaryStream);
//     impl_->BufferVecPack(binaryStream);
//     // impl_->ConnVecPack(binaryStream);
//     impl_->HandshakeMsgUnpack(binaryStream);
//     impl_->NotifyVecUnpack(binaryStream);
//     impl_->RmtBufferVecUnpackProc(binaryStream);
//     // impl_->ConnVecUnpackProc(binaryStream);
// }