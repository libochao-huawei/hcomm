#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "next/comms/endpoint_pairs/channels/aiv/aiv_urma_channel.h"
#include "next/comms/endpoint_pairs/channels/aiv/aiv_urma_transport.h"
#undef protected
#undef private

using namespace hcomm;
using namespace Hccl;

namespace {
LinkData MakeDefaultLinkData()
{
    BasePortType portType(PortDeploymentType::P2P, ConnectProtoType::PCIE);
    return LinkData(portType, 0, 1, 0, 0);
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

    EXPECT_EQ(ch.NotifyRecord(0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.NotifyWait(0, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.WriteWithNotify(nullptr, nullptr, 0, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.Write(nullptr, nullptr, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.Read(nullptr, nullptr, 0), HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.ChannelFence(), HCCL_E_NOT_SUPPORT);
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

TEST_F(AivUrmaTransportTest, Ut_BufferVecPack_WhenCalled_FillsBinaryStream)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    BinaryStream binaryStream;

    EXPECT_NO_THROW(transport->BufferVecPack(binaryStream));
}

TEST_F(AivUrmaTransportTest, Ut_IsResReady_WhenConnNotReady_ReturnsFalse)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);

    EXPECT_FALSE(transport->IsResReady());
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

TEST_F(AivUrmaTransportTest, Ut_IsSocketReady_WhenSocketOk_ReturnsTrue)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);

    MOCKER_CPP(&Socket::GetAsyncStatus, SocketStatus(Socket::*)())
        .stubs()
        .with(any())
        .will(returnValue(SocketStatus::OK));

    EXPECT_TRUE(transport->IsSocketReady());
    EXPECT_EQ(transport->transportStatus_, TransportStatus::SOCKET_OK);
}

TEST_F(AivUrmaTransportTest, Ut_GetRemoteMem_WhenParamNull_Returns_E_PARA)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    HcclMem *remoteMem = nullptr;
    uint32_t memNum = 0;

    EXPECT_EQ(transport->GetRemoteMem(nullptr, &memNum, nullptr), HCCL_E_PARA);
    EXPECT_EQ(transport->GetRemoteMem(&remoteMem, nullptr, nullptr), HCCL_E_PARA);
}

TEST_F(AivUrmaTransportTest, Ut_GetRemoteMem_WhenNoRemoteBuffer_Returns_SUCCESS)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    HcclMem *remoteMem = reinterpret_cast<HcclMem *>(0x1);
    uint32_t memNum = 1;

    EXPECT_EQ(transport->GetRemoteMem(&remoteMem, &memNum, nullptr), HCCL_SUCCESS);
    EXPECT_EQ(remoteMem, nullptr);
    EXPECT_EQ(memNum, 0);
}

TEST_F(AivUrmaTransportTest, Ut_GetUserRemoteMem_WhenParamNull_Returns_E_PTR)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    CommMem *remoteMem = nullptr;
    char **memTags = nullptr;
    uint32_t memNum = 0;

    EXPECT_EQ(transport->GetUserRemoteMem(nullptr, &memTags, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetUserRemoteMem(&remoteMem, nullptr, &memNum), HCCL_E_PTR);
    EXPECT_EQ(transport->GetUserRemoteMem(&remoteMem, &memTags, nullptr), HCCL_E_PTR);
}

TEST_F(AivUrmaTransportTest, Ut_GetUserRemoteMem_WhenNoRemoteBuffer_Returns_E_PARA)
{
    auto conn = MakeConn();
    Socket socket(nullptr, IpAddress(), 0, IpAddress(), "ut", SocketRole::CLIENT, NicType::DEVICE_NIC_TYPE);
    auto transport = MakeTransport(*conn, socket);
    CommMem *remoteMem = reinterpret_cast<CommMem *>(0x1);
    char **memTags = reinterpret_cast<char **>(0x1);
    uint32_t memNum = 1;

    EXPECT_EQ(transport->GetUserRemoteMem(&remoteMem, &memTags, &memNum), HCCL_E_PARA);
    EXPECT_EQ(remoteMem, nullptr);
    EXPECT_EQ(memTags, nullptr);
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
    ASSERT_NE(hostChannel.SqContextAddr, nullptr);
    EXPECT_EQ(hostChannel.SqContextAddr[0].contextInfo.jfsContext.jfsID, conn->jettyId);
    EXPECT_EQ(hostChannel.SqContextAddr[0].contextInfo.jfsContext.dbVa, conn->dbAddr);
    EXPECT_EQ(hostChannel.SqContextAddr[0].contextInfo.jfsContext.sqVa, conn->sqBuffVa);
    EXPECT_EQ(hostChannel.SqContextAddr[0].contextInfo.jfsContext.tpID, conn->tpn);
    EXPECT_EQ(hostChannel.SqContextAddr[0].contextInfo.jfsContext.wqeSize, 64);

    ASSERT_EQ(hostChannel.cqNum, 1);
    ASSERT_NE(hostChannel.CqContextAddr, nullptr);
    EXPECT_EQ(hostChannel.CqContextAddr[0].contextInfo.jfcContext.jfcID, conn->cqInfo_.id);
    EXPECT_EQ(hostChannel.CqContextAddr[0].contextInfo.jfcContext.scqVa, conn->cqInfo_.va);
    EXPECT_EQ(hostChannel.CqContextAddr[0].contextInfo.jfcContext.cqeSize, conn->cqInfo_.cqeSize);
    EXPECT_EQ(hostChannel.CqContextAddr[0].contextInfo.jfcContext.cqDepth, conn->cqInfo_.cqDepth);
    EXPECT_EQ(hostChannel.CqContextAddr[0].contextInfo.jfcContext.dbVa, conn->cqInfo_.swdbAddr);
}
