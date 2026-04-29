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
#include "dl_urma_function.h"

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
        MOCKER_CPP(&Hccl::TpManager::Init).stubs().will(returnValue());
        
        EndpointDesc endpointDesc{};
        endpointDesc.protocol = COMM_PROTOCOL_UB;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        endpoint = std::make_unique<CpuUrmaEndpoint>(endpointDesc);
        endpoint->Init();
        endpointHandle = static_cast<EndpointHandle>(endpoint.get());
        
        channelDesc.remoteEndpoint = endpointDesc;
        channelDesc.notifyNum = 2;
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

    void SetupConnectionMocks()
    {
        MOCKER_CPP(&Hccl::HostUbConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::HostUbConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteMem).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::READY));
    }

    std::unique_ptr<CpuUrmaEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    RdmaHandle rdmaHandle_{nullptr};
};

TEST_F(HostCpuUrmaChannelTest, Ut_When_Normal_Init_Expect_HCCL_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
}

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

TEST_F(HostCpuUrmaChannelTest, Ut_When_BuildSocket_NullSocket_Expect_SUCCESS)
{
    SetupConnectionMocks();
    channelDesc.socket = nullptr;
    MOCKER_CPP(&hcomm::SocketMgr::GetSocket).stubs().with(any(), outBound(fakeSocket)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_When_BuildConnection_UB_TP_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_FALSE(impl->connections_.empty());
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetNotifyNum_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    uint32_t notifyNum = 0;
    EXPECT_EQ(impl->GetNotifyNum(&notifyNum), HCCL_SUCCESS);
    EXPECT_EQ(notifyNum, 2);
}

TEST_F(HostCpuUrmaChannelTest, Ut_When_Write_LocalSegNotFound_Expect_HCCL_E_INTERNAL)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    // localRmaBuffers_ 为空，GetLocSeg 应失败
    EXPECT_EQ(impl->Write((void*)0x0002, (void*)0x0001, 10), HCCL_E_INTERNAL);
}

TEST_F(HostCpuUrmaChannelTest, Ut_When_Read_LocalSegNotFound_Expect_HCCL_E_INTERNAL)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_EQ(impl->Read((void*)0x0001, (void*)0x0002, 10), HCCL_E_INTERNAL);
}

TEST_F(HostCpuUrmaChannelTest, Ut_ChannelFence_When_WqeNumIsZero_Expect_HCCL_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    impl->wqeNum_ = 0;
    EXPECT_EQ(impl->ChannelFence(), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetStatus_When_TransportReady_Expect_READY)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::READY));
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_EQ(impl->GetStatus(), ChannelStatus::READY);
}

TEST_F(HostCpuUrmaChannelTest, Ut_Clean_And_Resume_Expect_HCCL_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_EQ(impl->Clean(), HCCL_SUCCESS);
    EXPECT_EQ(impl->Resume(), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_BuildAttr_Expect_OpMode_OPBASE)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_EQ(impl->attr_.opMode, Hccl::OpMode::OPBASE);
}

TEST_F(HostCpuUrmaChannelTest, Ut_BuildConnection_UB_CTP_Expect_SUCCESS)
{
    SetupConnectionMocks();
    // 设置协议为 UB_CTP
    endpointDesc.protocol = COMM_PROTOCOL_UBC_CTP;
    channelDesc.remoteEndpoint.protocol = COMM_PROTOCOL_UBC_CTP;
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_FALSE(impl->connections_.empty());
}

TEST_F(HostCpuUrmaChannelTest, Ut_BuildConnection_NullConnection_Expect_HCCL_E_PTR)
{
    SetupConnectionMocks();
    // Mock HostUbConnection 创建失败
    MOCKER_CPP(&Hccl::HostUbConnection::CreateQp).stubs().will(returnValue(HCCL_E_PTR));
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_NE(impl->Init(), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_BuildBuffer_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->bufs_.push_back(localBufferPtr);
    EXPECT_EQ(impl->BuildBuffer(), HCCL_SUCCESS);
    EXPECT_FALSE(impl->localRmaBuffers_.empty());
}

TEST_F(HostCpuUrmaChannelTest, Ut_BuildUbMemTransport_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteMem).stubs().with(any(), any(), any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 确保必要的成员已初始化
    impl->commonRes_.connVec.push_back(impl->connections_[0].get());
    EXPECT_EQ(impl->BuildUbMemTransport(), HCCL_SUCCESS);
    EXPECT_NE(impl->memTransport_, nullptr);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetRemoteMem_Normal_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    HcclMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTags[10] = {nullptr};
    EXPECT_EQ(impl->GetRemoteMem(&remoteMem, &memNum, memTags), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetRemoteMem_NullParam_Expect_HCCL_E_PTR)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    uint32_t memNum = 0;
    char* memTags[10] = {nullptr};
    EXPECT_EQ(impl->GetRemoteMem(nullptr, &memNum, memTags), HCCL_E_PTR);
}

TEST_F(HostCpuUrmaChannelTest, Ut_PrepareNotifyWrResource_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    urma_jfs_wr_t notifyWr{};
    EXPECT_EQ(impl->PrepareNotifyWrResource(1, notifyWr), HCCL_SUCCESS);
    EXPECT_EQ(notifyWr.opcode, URMA_OPC_WRITE_IMM);
    EXPECT_EQ(notifyWr.rw.notify_data, 1u);
}

TEST_F(HostCpuUrmaChannelTest, Ut_PrepareWriteWrResource_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    urma_jfs_wr_t writeWr{};
    EXPECT_EQ(impl->PrepareWriteWrResource((void*)0x0002, (void*)0x0001, 10, 1, writeWr), HCCL_SUCCESS);
    EXPECT_EQ(writeWr.opcode, URMA_OPC_WRITE_IMM);
    EXPECT_EQ(writeWr.rw.notify_data, 1u);
}

TEST_F(HostCpuUrmaChannelTest, Ut_PrepareWriteWrResource_LenExceedsU32_Expect_HCCL_E_PARA)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    urma_jfs_wr_t writeWr{};
    uint64_t largeLen = (uint64_t)UINT32_MAX + 1;
    EXPECT_EQ(impl->PrepareWriteWrResource((void*)0x0002, (void*)0x0001, largeLen, 1, writeWr), HCCL_E_PARA);
}

TEST_F(HostCpuUrmaChannelTest, Ut_NotifyRecord_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    uint32_t initialWqeNum = impl->wqeNum_;
    EXPECT_EQ(impl->NotifyRecord(1), HCCL_SUCCESS);
    EXPECT_EQ(impl->wqeNum_, initialWqeNum + 1);
}

TEST_F(HostCpuUrmaChannelTest, Ut_NotifyWait_Expect_HCCL_E_TIMEOUT)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 模拟 poll 超时
    MOCKER_CPP(&Hccl::HrtUrmaPollJfc).stubs().will(returnValue(0));
    EXPECT_EQ(impl->NotifyWait(1, 100), HCCL_E_TIMEOUT);
}

TEST_F(HostCpuUrmaChannelTest, Ut_WriteWithNotify_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    uint32_t initialWqeNum = impl->wqeNum_;
    EXPECT_EQ(impl->WriteWithNotify((void*)0x0002, (void*)0x0001, 10, 1), HCCL_SUCCESS);
    EXPECT_EQ(impl->wqeNum_, initialWqeNum + 1);
}

TEST_F(HostCpuUrmaChannelTest, Ut_WriteWithNotify_NullPtr_Expect_HCCL_E_PTR)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    EXPECT_EQ(impl->WriteWithNotify(nullptr, (void*)0x0001, 10, 1), HCCL_E_PTR);
    EXPECT_EQ(impl->WriteWithNotify((void*)0x0002, nullptr, 10, 1), HCCL_E_PTR);
}

TEST_F(HostCpuUrmaChannelTest, Ut_Write_Normal_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPostJettySendWr).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer 并设置 remote seg
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    u64 fakeRemoteSeg = 0x12345678;
    MOCKER_CPP(&hcomm::HostCpuUrmaChannel::GetLocSeg).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteSeg).stubs().with(any(), any(), outBoundP(&fakeRemoteSeg)).will(returnValue(HCCL_SUCCESS));
    
    uint32_t initialWqeNum = impl->wqeNum_;
    EXPECT_EQ(impl->Write((void*)0x0002, (void*)0x0001, 10), HCCL_SUCCESS);
    EXPECT_EQ(impl->wqeNum_, initialWqeNum + 1);
}

TEST_F(HostCpuUrmaChannelTest, Ut_Read_Normal_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPostJettySendWr).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer 并设置 remote seg
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    u64 fakeRemoteSeg = 0x12345678;
    MOCKER_CPP(&hcomm::HostCpuUrmaChannel::GetLocSeg).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteSeg).stubs().with(any(), any(), outBoundP(&fakeRemoteSeg)).will(returnValue(HCCL_SUCCESS));
    
    uint32_t initialWqeNum = impl->wqeNum_;
    EXPECT_EQ(impl->Read((void*)0x0001, (void*)0x0002, 10), HCCL_SUCCESS);
    EXPECT_EQ(impl->wqeNum_, initialWqeNum + 1);
}

TEST_F(HostCpuUrmaChannelTest, Ut_Write_LargeData_SplitChunks_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPostJettySendWr).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    u64 fakeRemoteSeg = 0x12345678;
    MOCKER_CPP(&hcomm::HostCpuUrmaChannel::GetLocSeg).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteSeg).stubs().with(any(), any(), outBoundP(&fakeRemoteSeg)).will(returnValue(HCCL_SUCCESS));
    
    // 写入大于 MAX_JETTY_WR_DATA_LEN 的数据，触发分片
    uint64_t largeSize = 2 * 1024 * 1024; // 2MB
    EXPECT_EQ(impl->Write((void*)0x0002, (void*)0x0001, largeSize), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_ChannelFence_WithPendingWqe_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPollJfc).stubs().will(returnValue(1)); // 模拟成功完成一个 wqe
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    impl->wqeNum_ = 3;
    EXPECT_EQ(impl->ChannelFence(), HCCL_SUCCESS);
    EXPECT_EQ(impl->wqeNum_, 0u);
    EXPECT_EQ(impl->fenceFlag_, true);
}

TEST_F(HostCpuUrmaChannelTest, Ut_ChannelFence_PollError_Expect_HCCL_E_NETWORK)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPollJfc).stubs().will(returnValue(-1)); // 模拟 poll 失败
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    impl->wqeNum_ = 1;
    EXPECT_EQ(impl->ChannelFence(), HCCL_E_NETWORK);
}

TEST_F(HostCpuUrmaChannelTest, Ut_ChannelFence_ExceedWqeNum_Expect_HCCL_E_INTERNAL)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    // 返回比 wqeNum_ 更大的完成数
    MOCKER_CPP(&Hccl::HrtUrmaPollJfc).stubs().will(returnValue(10));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    impl->wqeNum_ = 2;
    EXPECT_EQ(impl->ChannelFence(), HCCL_E_INTERNAL);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetStatus_AllStates_Expect_CorrectMapping)
{
    SetupConnectionMocks();
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    
    // 测试各种 TransportStatus 到 ChannelStatus 的映射
    MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::INIT));
    EXPECT_EQ(impl->GetStatus(), ChannelStatus::INIT);
    
    MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::SOCKET_OK));
    EXPECT_EQ(impl->GetStatus(), ChannelStatus::SOCKET_OK);
    
    MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::SOCKET_TIMEOUT));
    EXPECT_EQ(impl->GetStatus(), ChannelStatus::SOCKET_TIMEOUT);
    
    MOCKER_CPP(&Hccl::UbMemTransport::GetSyncStatus).stubs().will(returnValue(Hccl::TransportStatus::READY));
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_EQ(impl->GetStatus(), ChannelStatus::READY);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetLocSeg_AddrNotInRange_Expect_HCCL_E_INTERNAL)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // localRmaBuffers_ 为空
    u64 seg = 0;
    EXPECT_EQ(impl->GetLocSeg((void*)0x0001, 10, &seg), HCCL_E_INTERNAL);
}

TEST_F(HostCpuUrmaChannelTest, Ut_GetLocSeg_AddrInRange_Expect_SUCCESS)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer，地址在范围内
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    u64 seg = 0;
    void* testAddr = (void*)666;
    EXPECT_EQ(impl->GetLocSeg(testAddr, 10, &seg), HCCL_SUCCESS);
}

TEST_F(HostCpuUrmaChannelTest, Ut_ExchangeAllMems_True_Expect_GetAllMemHandles)
{
    SetupConnectionMocks();
    channelDesc.exchangeAllMems = true;
    channelDesc.memHandles = nullptr;
    channelDesc.memHandleNum = 0;
    
    // Mock HcommMemGetAllMemHandles
    void* memHandle = (void*)0x12345678;
    uint32_t memHandleNum = 1;
    MOCKER(HcommMemGetAllMemHandles).stubs().with(any(), outBound(&memHandle), outBound(&memHandleNum))
        .will(returnValue(static_cast<HcommResult>(HCCL_SUCCESS)));
    
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    EXPECT_FALSE(impl->bufs_.empty());
}

TEST_F(HostCpuUrmaChannelTest, Ut_When_FenceFlag_True_Expect_StrongOrder)
{
    SetupConnectionMocks();
    MOCKER_CPP(&Hccl::DlUrmaFunction::DlUrmaFunctionInit).stubs().will(returnValue(HCCL_SUCCESS));
    auto impl = std::make_unique<HostCpuUrmaChannel>(endpointHandle, channelDesc);
    EXPECT_EQ(impl->Init(), HCCL_SUCCESS);
    
    // 添加本地 buffer
    auto localUbRmaBuffer = std::make_shared<Hccl::LocalUbRmaBuffer>(localBufferPtr, rdmaHandle_);
    impl->localRmaBuffers_.push_back(std::move(localUbRmaBuffer));
    
    // 第一次 Write 后 fenceFlag_ 应为 false
    u64 fakeRemoteSeg = 0x12345678;
    MOCKER_CPP(&hcomm::HostCpuUrmaChannel::GetLocSeg).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::UbMemTransport::GetRemoteSeg).stubs().with(any(), any(), outBoundP(&fakeRemoteSeg)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::HrtUrmaPostJettySendWr).stubs().will(returnValue(HCCL_SUCCESS));
    
    impl->fenceFlag_ = true;
    EXPECT_EQ(impl->Write((void*)0x0002, (void*)0x0001, 10), HCCL_SUCCESS);
    EXPECT_EQ(impl->fenceFlag_, false);
}

