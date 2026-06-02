#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#define private public
#include "channels/aiv/aiv_ub_mem_transport.h"
#include "runtime_api_exception.h"
#include "socket_exception.h"

using namespace hcomm;

static void StubSocketSendAsyncNoop(Hccl::Socket *self, const u8 *sendBuf, u32 size)
{
    (void)self;
    (void)sendBuf;
    (void)size;
}

static void StubSocketRecvAsyncNoop(Hccl::Socket *self, u8 *recvBuf, u32 size)
{
    (void)self;
    (void)recvBuf;
    (void)size;
}

class AivUbMemTransportTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "AivUbMemTransportTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
        std::cout << "AivUbMemTransportTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AivUbMemTransportTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AivUbMemTransportTest TearDown" << std::endl;
    }
};

TEST_F(AivUbMemTransportTest, St_GetStatus_When_SOCKET_OK_Expect_Success)
{
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);
    HcommChannelDesc desc{};
    AivUbMemTransport aivTransport(fakeSocket, desc);

    aivTransport.baseStatus_ = Hccl::TransportStatus::SOCKET_OK;
    aivTransport.aivUbStatus_ = hcomm::AivUbMemTransport::AivUbMemTransportStatus::SOCKET_OK;

    MOCKER_CPP(&AivUbMemTransport::RmtBufferUnpackProc).stubs();
    MOCKER_CPP(&Hccl::Socket::SendAsync, void(Hccl::Socket::*)(const u8 *, u32))
        .stubs().with(any(), any()).will(invoke(StubSocketSendAsyncNoop));
    MOCKER_CPP(&Hccl::Socket::RecvAsync, void(Hccl::Socket::*)(u8 *, u32))
        .stubs().with(any(), any()).will(invoke(StubSocketRecvAsyncNoop));

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::SOCKET_OK);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::SEND_DATA_SIZE);

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::SOCKET_OK);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_DATA_SIZE);

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::SOCKET_OK);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::SEND_MEM_INFO);

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::SOCKET_OK);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_MEM_INFO);

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::SOCKET_OK);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_MEM_FIN);

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::READY);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::READY);
}

TEST_F(AivUbMemTransportTest, Ut_GetStatus_When_RecvData_Fail_Expect_Status_Invalid)
{
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);
    HcommChannelDesc desc{};
    AivUbMemTransport aivTransport(fakeSocket, desc);

    aivTransport.baseStatus_ = Hccl::TransportStatus::SOCKET_OK;
    aivTransport.aivUbStatus_ = hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_MEM_INFO;

    MOCKER_CPP(&AivUbMemTransport::RmtBufferUnpackProc).stubs().will(throws(Hccl::RuntimeApiException("test_fail")));

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::INVALID);
    EXPECT_EQ(aivTransport.aivUbStatus_, hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_MEM_FIN);
}

TEST_F(AivUbMemTransportTest, Ut_GetStatus_When_SocketReady_Fail_Expect_Status_Invalid)
{
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);
    HcommChannelDesc desc{};
    AivUbMemTransport aivTransport(fakeSocket, desc);

    aivTransport.baseStatus_ = Hccl::TransportStatus::SOCKET_OK;
    aivTransport.aivUbStatus_ = hcomm::AivUbMemTransport::AivUbMemTransportStatus::RECV_MEM_INFO;

    MOCKER_CPP(&Hccl::Socket::GetAsyncStatus).stubs().will(throws(Hccl::SocketException("test_fail")));

    EXPECT_EQ(aivTransport.GetStatus(), Hccl::TransportStatus::INVALID);
}
TEST_F(AivUbMemTransportTest, Ut_FillTagVec_When_LocalIpcHandle_Expect_BufferAndTagCopied)
{
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);
    HcommChannelDesc desc{};
    AivUbMemTransport aivTransport(fakeSocket, desc);

    auto rawBuffer = std::make_shared<Hccl::Buffer>(0x45670, 0x300, HCCL_MEM_TYPE_DEVICE, "aiv_user");
    auto localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(rawBuffer);
    HcommMemHandle memHandles[1] = { reinterpret_cast<HcommMemHandle>(localIpcRmaBuffer.get()) };

    std::vector<Hccl::LocalIpcRmaBuffer *> bufferVec;
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> tagVec;
    ASSERT_EQ(aivTransport.FillTagVec(memHandles, 1, bufferVec, tagVec), HCCL_SUCCESS);
    ASSERT_EQ(bufferVec.size(), 1U);
    ASSERT_EQ(tagVec.size(), 1U);
    EXPECT_EQ(bufferVec[0], localIpcRmaBuffer.get());
    EXPECT_EQ(std::string(tagVec[0].data()), rawBuffer->GetMemTag());
}

TEST_F(AivUbMemTransportTest, Ut_FillTagVec_When_TagTooLong_Expect_ReturnHCCL_E_PARA)
{
    Hccl::Socket *fakeSocket = reinterpret_cast<Hccl::Socket *>(0x1);
    HcommChannelDesc desc{};
    AivUbMemTransport aivTransport(fakeSocket, desc);

    std::string longTag(HCCL_RES_TAG_MAX_LEN, static_cast<char>(120));
    auto rawBuffer = std::make_shared<Hccl::Buffer>(0x45680, 0x300, HCCL_MEM_TYPE_DEVICE, longTag.c_str());
    auto localIpcRmaBuffer = std::make_shared<Hccl::LocalIpcRmaBuffer>(rawBuffer);
    HcommMemHandle memHandles[1] = { reinterpret_cast<HcommMemHandle>(localIpcRmaBuffer.get()) };

    std::vector<Hccl::LocalIpcRmaBuffer *> bufferVec;
    std::vector<std::array<char, HCCL_RES_TAG_MAX_LEN>> tagVec;
    EXPECT_EQ(aivTransport.FillTagVec(memHandles, 1, bufferVec, tagVec), HCCL_E_PARA);
}
