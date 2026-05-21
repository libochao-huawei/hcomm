#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "next/comms/endpoint_pairs/channels/aiv/aiv_urma_channel.h"
#include "next/comms/endpoint_pairs/channels/aiv/aiv_urma_transport.h"
#undef protected
#undef private
#include "exchange_ub_buffer_dto.h"

using namespace hcomm;
using namespace Hccl;

namespace {
LinkData MakeDefaultLinkData()
{
    BasePortType portType(PortDeploymentType::P2P, ConnectProtoType::PCIE);
    return LinkData(portType, 0, 1, 0, 0);
}

aclError StubAclrtMalloc(void **devPtr, size_t size, aclrtMemMallocPolicy policy)
{
    (void)size;
    (void)policy;
    if (devPtr == nullptr) {
        return ACL_ERROR_RT_PARAM_INVALID;
    }
    *devPtr = reinterpret_cast<void *>(0x12345678);
    return ACL_SUCCESS;
}

aclError StubAclrtMemcpy(void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind)
{
    (void)dst;
    (void)destMax;
    (void)src;
    (void)count;
    (void)kind;
    return ACL_SUCCESS;
}

aclError StubAclrtFree(void *devPtr)
{
    (void)devPtr;
    return ACL_SUCCESS;
}
} // namespace

class AivUrmaChannelTest : public testing::Test {
protected:
    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }

    HcommChannelDesc MakeDefaultDesc()
    {
        HcommChannelDesc desc{};
        desc.roceAttr.queueNum = 1;
        return desc;
    }
};

TEST_F(AivUrmaChannelTest, Ut_Clean_WhenCalled_Returns_SUCCESS)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    EXPECT_EQ(ch.Clean(), HCCL_SUCCESS);
    EXPECT_EQ(ch.Clean(), HCCL_SUCCESS);
}

TEST_F(AivUrmaChannelTest, Ut_Resume_WhenBuildsMocked_Returns_SUCCESS)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    MOCKER_CPP(&AivUrmaChannel::BuildConnection, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildAivUrmaTransport, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    EXPECT_EQ(ch.Resume(), HCCL_SUCCESS);
}

TEST_F(AivUrmaChannelTest, Ut_Init_WhenBuildsMocked_Returns_SUCCESS)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    MOCKER_CPP(&AivUrmaChannel::ParseInputParam, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildSocket, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildAttr, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildConnection, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildBuffer,
        HcclResult(AivUrmaChannel::*)(std::vector<std::shared_ptr<Hccl::Buffer>> &))
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AivUrmaChannel::BuildAivUrmaTransport, HcclResult(AivUrmaChannel::*)())
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

    EXPECT_EQ(ch.Init(), HCCL_SUCCESS);
}

TEST_F(AivUrmaChannelTest, Ut_BuildAttr_WhenCalled_FillsAivOpBaseAttr)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    ch.localEp_.loc.device.devPhyId = 3;

    EXPECT_EQ(ch.BuildAttr(), HCCL_SUCCESS);
    EXPECT_EQ(ch.attr_.devicePhyId, 3);
    EXPECT_EQ(ch.attr_.opMode, OpMode::OPBASE);
    EXPECT_EQ(ch.attr_.opAcceState, AcceleratorState::AIV);
}

TEST_F(AivUrmaChannelTest, Ut_BuildSocket_WhenSocketExists_Returns_SUCCESS)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    ch.socket_ = &socket;

    EXPECT_EQ(ch.BuildSocket(), HCCL_SUCCESS);
}

TEST_F(AivUrmaChannelTest, Ut_BuildSocket_WhenSocketNull_GetsSocketFromSocketMgr)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    ch.socket_ = nullptr;
    ch.localEp_.loc.device.devPhyId = 3;

    MOCKER_CPP(&SocketMgr::GetSocket)
        .stubs()
        .with(any(), outBound(&socket))
        .will(returnValue(HCCL_SUCCESS));

    EXPECT_EQ(ch.BuildSocket(), HCCL_SUCCESS);
    EXPECT_EQ(ch.socket_, &socket);
}

TEST_F(AivUrmaChannelTest, Ut_BuildBuffer_WhenEmpty_Returns_SUCCESS)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs;

    EXPECT_EQ(ch.BuildBuffer(bufs), HCCL_SUCCESS);
    EXPECT_TRUE(ch.commonRes_.bufferVec.empty());
    EXPECT_TRUE(ch.localRmaBuffers_.empty());
}

TEST_F(AivUrmaChannelTest, Ut_UnsupportedDataApis_WhenCalled_Return_NOT_SUPPORT)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    uint32_t notifyNum = 1;

    EXPECT_EQ(ch.GetNotifyNum(&notifyNum), HCCL_SUCCESS);
    EXPECT_EQ(ch.NotifyRecord(0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.NotifyWait(0, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.WriteWithNotify(nullptr, nullptr, 0, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.Write(nullptr, nullptr, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.Read(nullptr, nullptr, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.ChannelFence(), HCCL_E_NOT_SUPPORT);
}

TEST_F(AivUrmaChannelTest, Ut_Init_WhenEndpointNull_Returns_E_PTR)
{
    AivUrmaChannel ch(nullptr, MakeDefaultDesc());

    EXPECT_EQ(ch.Init(), HCCL_E_PTR);
}

TEST_F(AivUrmaChannelTest, Ut_BuildChannelEntityToDevice_WhenDevPtrNull_Returns_E_PTR)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    EXPECT_EQ(ch.BuildChannelEntityToDevice(nullptr), HCCL_E_PTR);
}

TEST_F(AivUrmaChannelTest, Ut_BuildChannelEntityToDevice_WhenTransportNull_Returns_E_PTR)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());
    void *devChannelPtr = nullptr;

    EXPECT_EQ(ch.BuildChannelEntityToDevice(&devChannelPtr), HCCL_E_PTR);
    EXPECT_EQ(devChannelPtr, nullptr);
}

TEST_F(AivUrmaChannelTest, Ut_GetStatus_WhenTransportStatusTimeout_Returns_SOCKET_TIMEOUT)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    BaseMemTransport::CommonLocRes commonRes{};
    DevUbCtpConnection conn(reinterpret_cast<RdmaHandle>(0x1), IpAddress(), IpAddress(), OpMode::OPBASE);
    commonRes.connVec.emplace_back(&conn);
    BaseMemTransport::Attribution attr{};
    LinkData linkData = MakeDefaultLinkData();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    ch.transport_ = std::make_unique<AivUrmaTransport>(
        commonRes, attr, linkData, socket, reinterpret_cast<RdmaHandle>(0x1));
    SocketStatus fakeSocketStatus = SocketStatus::TIMEOUT;

    MOCKER_CPP(&Socket::GetAsyncStatus, SocketStatus(Socket::*)())
        .stubs()
        .with(any())
        .will(returnValue(fakeSocketStatus));

    EXPECT_EQ(ch.GetStatus(), ChannelStatus::SOCKET_TIMEOUT);
}

TEST_F(AivUrmaChannelTest, Ut_BuildChannelEntityToDevice_WhenTransportReady_ReturnsDevicePtr)
{
    EndpointHandle ep = reinterpret_cast<EndpointHandle>(0x1);
    AivUrmaChannel ch(ep, MakeDefaultDesc());

    BaseMemTransport::CommonLocRes commonRes{};
    DevUbCtpConnection conn(reinterpret_cast<RdmaHandle>(0x1), IpAddress(), IpAddress(), OpMode::OPBASE);
    conn.status = RmaConnStatus::READY;
    conn.jettyId = 3;
    conn.dbAddr = 0x1000;
    conn.sqBuffVa = 0x2000;
    conn.sqDepth = 4;
    conn.tpn = 5;
    conn.cqInfo_.id = 6;
    conn.cqInfo_.va = 0x3000;
    conn.cqInfo_.cqeSize = 64;
    conn.cqInfo_.cqDepth = 8;
    conn.cqInfo_.swdbAddr = 0x4000;
    commonRes.connVec.emplace_back(&conn);

    BaseMemTransport::Attribution attr{};
    LinkData linkData = MakeDefaultLinkData();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    ch.transport_ = std::make_unique<AivUrmaTransport>(
        commonRes, attr, linkData, socket, reinterpret_cast<RdmaHandle>(0x1));
    ch.transport_->transportStatus_ = TransportStatus::READY;

    MOCKER(aclrtMalloc)
        .stubs()
        .will(invoke(StubAclrtMalloc));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(invoke(StubAclrtMemcpy));
    MOCKER(aclrtFree)
        .stubs()
        .will(invoke(StubAclrtFree));

    void *devChannelPtr = nullptr;
    EXPECT_EQ(ch.BuildChannelEntityToDevice(&devChannelPtr), HCCL_SUCCESS);
    EXPECT_EQ(devChannelPtr, reinterpret_cast<void *>(0x12345678));
    EXPECT_EQ(ch.devChannelEntity_, reinterpret_cast<void *>(0x12345678));
}

class AivUrmaTransportTest : public testing::Test {
protected:
    static void TearDownTestCase()
    {
        GlobalMockObject::verify();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }

    std::unique_ptr<DevUbCtpConnection> MakeConn()
    {
        return std::make_unique<DevUbCtpConnection>(
            reinterpret_cast<RdmaHandle>(0x1), IpAddress(), IpAddress(), OpMode::OPBASE);
    }

    std::unique_ptr<AivUrmaTransport> MakeTransport(DevUbConnection &conn, Socket &socket)
    {
        commonRes_.connVec.clear();
        commonRes_.bufferVec.clear();
        commonRes_.connVec.emplace_back(&conn);
        attr_.opAcceState = AcceleratorState::AIV;
        LinkData linkData = MakeDefaultLinkData();
        return std::make_unique<AivUrmaTransport>(
            commonRes_, attr_, linkData, socket, reinterpret_cast<RdmaHandle>(0x1));
    }

    BaseMemTransport::CommonLocRes commonRes_{};
    BaseMemTransport::Attribution attr_{};
};

TEST_F(AivUrmaTransportTest, Ut_Construct_WhenConnNull_ThrowsInvalidParamsException)
{
    BaseMemTransport::CommonLocRes commonRes{};
    commonRes.connVec.emplace_back(nullptr);
    BaseMemTransport::Attribution attr{};
    LinkData linkData = MakeDefaultLinkData();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);

    EXPECT_THROW(AivUrmaTransport transport(commonRes, attr, linkData, socket, reinterpret_cast<RdmaHandle>(0x1)),
        InvalidParamsException);
}

TEST_F(AivUrmaTransportTest, Ut_DescribeAndLinkDesc_WhenCalled_ReturnsNonEmpty)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);

    EXPECT_FALSE(transport->Describe().empty());
    EXPECT_FALSE(transport->GetLinkDescInfo().empty());
}

TEST_F(AivUrmaTransportTest, Ut_CheckCommonLocRes_WhenConnValid_Returns)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);

    EXPECT_NO_THROW(transport->CheckLocBuffer(commonRes_));
    EXPECT_NO_THROW(transport->CheckLocConn(commonRes_));
    EXPECT_NO_THROW(transport->CheckCommonLocRes(commonRes_));
}

TEST_F(AivUrmaTransportTest, Ut_HandshakePackUnpack_WhenEmptyMsg_Returns)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;

    transport->HandshakeMsgPack(binaryStream);
    EXPECT_NO_THROW(transport->HandshakeMsgUnpack(binaryStream));
    EXPECT_EQ(transport->rmtOpAcceState_, AcceleratorState::AIV);
}

TEST_F(AivUrmaTransportTest, Ut_HandshakeUnpack_WhenAcceleratorMismatch_Throws)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;
    binaryStream << static_cast<uint32_t>(AcceleratorState::CCU_MS);
    binaryStream << std::vector<char>{};

    EXPECT_THROW(transport->HandshakeMsgUnpack(binaryStream), InvalidParamsException);
}

TEST_F(AivUrmaTransportTest, Ut_HandshakeUnpack_WhenMsgSizeMismatch_Throws)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;
    binaryStream << static_cast<uint32_t>(AcceleratorState::AIV);
    binaryStream << std::vector<char>{'x'};

    EXPECT_THROW(transport->HandshakeMsgUnpack(binaryStream), InvalidParamsException);
}

TEST_F(AivUrmaTransportTest, Ut_BufferVecPack_WhenCalled_FillsBinaryStream)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;

    EXPECT_NO_THROW(transport->BufferVecPack(binaryStream));
}

TEST_F(AivUrmaTransportTest, Ut_IsResReadyAndConnsReady_WhenConnReady_ReturnsTrue)
{
    auto conn = MakeConn();
    conn->status = RmaConnStatus::READY;
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);

    EXPECT_TRUE(transport->IsResReady());
    EXPECT_TRUE(transport->IsConnsReady());
}

TEST_F(AivUrmaTransportTest, Ut_PrepareGetStatus_WhenAlreadyReady_ReturnsFalse)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->transportStatus_ = TransportStatus::READY;

    EXPECT_FALSE(transport->PrepareGetStatus());
    EXPECT_EQ(transport->transportStatus_, TransportStatus::READY);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenInit_SetsSocketOk)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::INIT;

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::SOCKET_OK);
    EXPECT_EQ(transport->transportStatus_, TransportStatus::SOCKET_OK);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenSocketOkAndResNotReady_StaysSocketOk)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::SOCKET_OK;

    MOCKER_CPP(&AivUrmaTransport::IsResReady)
        .stubs()
        .will(returnValue(false));

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::SOCKET_OK);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenSocketOkAndResReady_SendsData)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::SOCKET_OK;

    MOCKER_CPP(&AivUrmaTransport::IsResReady)
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&AivUrmaTransport::SendExchangeData)
        .stubs()
        .will(ignoreReturnValue());

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::SEND_DATA);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenSendData_RecvsData)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::SEND_DATA;

    MOCKER_CPP(&AivUrmaTransport::RecvExchangeData)
        .stubs()
        .will(ignoreReturnValue());

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::RECV_DATA);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenRecvDataNeedsFinish_ProcessesData)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::RECV_DATA;

    MOCKER_CPP(&AivUrmaTransport::RecvDataProcess)
        .stubs()
        .will(returnValue(true));

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::PROCESS_DATA);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenRecvDataNeedsNoFinish_BecomesReady)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::RECV_DATA;

    MOCKER_CPP(&AivUrmaTransport::RecvDataProcess)
        .stubs()
        .will(returnValue(false));

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::RECV_FIN);
    EXPECT_EQ(transport->transportStatus_, TransportStatus::READY);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenProcessDataAndConnsReady_SendsFinish)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::PROCESS_DATA;

    MOCKER_CPP(&AivUrmaTransport::IsConnsReady)
        .stubs()
        .will(returnValue(true));
    MOCKER_CPP(&AivUrmaTransport::SendFinish)
        .stubs()
        .will(ignoreReturnValue());

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::CONN_OK);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenConnOk_RecvsFinish)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::CONN_OK;

    MOCKER_CPP(&AivUrmaTransport::RecvFinish)
        .stubs()
        .will(ignoreReturnValue());

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::SEND_FIN);
}

TEST_F(AivUrmaTransportTest, Ut_ProcessUrmaStatus_WhenSendFin_BecomesReady)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->urmaStatus_ = AivUrmaTransport::UrmaStatus::SEND_FIN;

    transport->ProcessUrmaStatus();

    EXPECT_EQ(transport->urmaStatus_, AivUrmaTransport::UrmaStatus::RECV_FIN);
    EXPECT_EQ(transport->transportStatus_, TransportStatus::READY);
}

TEST_F(AivUrmaTransportTest, Ut_RmtBufferVecUnpackProc_WhenZeroSizeDto_AppendsNullBuffer)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;
    ExchangeUbBufferDto dto{};
    binaryStream << static_cast<uint32_t>(1);
    binaryStream << static_cast<uint32_t>(0);
    dto.Serialize(binaryStream);
    AivUrmaTransport::RemoteBufferVec bufferVec;

    transport->RmtBufferVecUnpackProc(1, binaryStream, bufferVec);

    ASSERT_EQ(bufferVec.size(), 1);
    EXPECT_EQ(bufferVec[0], nullptr);
}

TEST_F(AivUrmaTransportTest, Ut_RmtBufferVecUnpackProc_WhenNumMismatch_Throws)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;
    binaryStream << static_cast<uint32_t>(2);
    AivUrmaTransport::RemoteBufferVec bufferVec;

    EXPECT_THROW(transport->RmtBufferVecUnpackProc(1, binaryStream, bufferVec), InvalidParamsException);
}

TEST_F(AivUrmaTransportTest, Ut_ConnVecUnpackProc_WhenConnNumZero_ReturnsFalse)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->connNum_ = 0;
    BinaryStream binaryStream;
    binaryStream << static_cast<uint32_t>(0);

    EXPECT_FALSE(transport->ConnVecUnpackProc(binaryStream));
}

TEST_F(AivUrmaTransportTest, Ut_ConnVecUnpackProc_WhenConnNumMismatch_Throws)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;
    binaryStream << static_cast<uint32_t>(2);

    EXPECT_THROW(transport->ConnVecUnpackProc(binaryStream), InvalidParamsException);
}

TEST_F(AivUrmaTransportTest, Ut_GetRemoteMems_WhenParamNull_Returns_E_PTR)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    HcclMem *remoteMem = nullptr;
    char **memTags = nullptr;
    uint32_t memNum = 0;

    EXPECT_EQ(transport->GetRemoteMems(nullptr, &memTags, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetRemoteMems(&remoteMem, nullptr, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetRemoteMems(&remoteMem, &memTags, nullptr), HCCL_E_PTR);
}

TEST_F(AivUrmaTransportTest, Ut_GetRemoteMems_WhenNoRemoteBuffer_Returns_E_PARA)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    HcclMem *remoteMem = reinterpret_cast<HcclMem *>(0x1);
    char **memTags = reinterpret_cast<char **>(0x1);
    uint32_t memNum = 0;

    EXPECT_EQ(transport->GetUserRemoteMem(nullptr, &memTags, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetUserRemoteMem(&remoteMem, nullptr, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetUserRemoteMem(&remoteMem, &memTags, nullptr), HCCL_E_PTR);
}

TEST_F(AivUrmaTransportTest, Ut_GetRemoteMems_WhenOnlyReservedRemoteBuffer_Returns_SUCCESS)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    void *rdmaHandle = (void *)0x100;
    RemoteUbRmaBuffer remoteRmaBuffer(rdmaHandle);
    transport->rmtBufferVec_.push_back(remoteRmaBuffer);
    HcclMem *remoteMem = nullptr;
    char **memTags = nullptr;
    uint32_t memNum = 1;

    EXPECT_EQ(transport->GetRemoteMems(&remoteMem, &memTags, &memNum), HCCL_SUCCESS);
    EXPECT_EQ(memNum, 0);
}

TEST_F(AivUrmaTransportTest, Ut_GetStatus_WhenSocketTimeout_Returns_SOCKET_TIMEOUT)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    SocketStatus fakeSocketStatus = SocketStatus::TIMEOUT;

    MOCKER_CPP(&Socket::GetAsyncStatus, SocketStatus(Socket::*)())
        .stubs()
        .with(any())
        .will(returnValue(fakeSocketStatus));

    EXPECT_EQ(transport->GetStatus(), TransportStatus::SOCKET_TIMEOUT);
}

TEST_F(AivUrmaTransportTest, Ut_GetHostChannelEntity_WhenNotReady_ThrowsInternalException)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    ChannelEntity hostChannel{};

    EXPECT_THROW(transport->GetHostChannelEntity(&hostChannel), InternalException);
}

TEST_F(AivUrmaTransportTest, Ut_GetHostChannelEntity_WhenReady_FillsSqAndCqContext)
{
    auto conn = MakeConn();
    conn->status = RmaConnStatus::READY;
    conn->jettyId = 3;
    conn->dbAddr = 0x1000;
    conn->sqBuffVa = 0x2000;
    conn->sqDepth = 4;
    conn->tpn = 5;
    conn->cqInfo_.id = 6;
    conn->cqInfo_.va = 0x3000;
    conn->cqInfo_.cqeSize = 64;
    conn->cqInfo_.cqDepth = 8;
    conn->cqInfo_.swdbAddr = 0x4000;

    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    transport->transportStatus_ = TransportStatus::READY;
    ChannelEntity hostChannel{};

    transport->GetHostChannelEntity(&hostChannel);

    ASSERT_EQ(hostChannel.sqNum, 1);
    ASSERT_NE(hostChannel.sqContextAddr, nullptr);
    EXPECT_EQ(hostChannel.sqContextAddr[0].type, SQ_CONTEXT_TYPE_UB_JFS);
    EXPECT_EQ(hostChannel.sqContextAddr[0].contextInfo.ubJfs.jfsID, conn->jettyId);
    EXPECT_EQ(hostChannel.sqContextAddr[0].contextInfo.ubJfs.dbVa, conn->dbAddr);
    EXPECT_EQ(hostChannel.sqContextAddr[0].contextInfo.ubJfs.sqVa, conn->sqBuffVa);
    EXPECT_EQ(hostChannel.sqContextAddr[0].contextInfo.ubJfs.tpID, conn->tpn);
    EXPECT_EQ(hostChannel.sqContextAddr[0].contextInfo.ubJfs.wqeSize, 64);

    ASSERT_EQ(hostChannel.cqNum, 1);
    ASSERT_NE(hostChannel.cqContextAddr, nullptr);
    EXPECT_EQ(hostChannel.cqContextAddr[0].type, CQ_CONTEXT_TYPE_UB_JFC);
    EXPECT_EQ(hostChannel.cqContextAddr[0].contextInfo.ubJfc.jfcID, conn->cqInfo_.id);
    EXPECT_EQ(hostChannel.cqContextAddr[0].contextInfo.ubJfc.scqVa, conn->cqInfo_.va);
    EXPECT_EQ(hostChannel.cqContextAddr[0].contextInfo.ubJfc.cqeSize, conn->cqInfo_.cqeSize);
    EXPECT_EQ(hostChannel.cqContextAddr[0].contextInfo.ubJfc.cqDepth, conn->cqInfo_.cqDepth);
    EXPECT_EQ(hostChannel.cqContextAddr[0].contextInfo.ubJfc.dbVa, conn->cqInfo_.swdbAddr);
}
