#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "rank_graph_interface.h"
#include "rank_graph_v2.h"
#include "hcomm_c_adpt.h"
#include "my_rank.h"
#include "channel_process.h"
#define private public
using namespace hccl;
using namespace hcomm;

class MyRankTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MyRankTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "MyRankTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in MyRankTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in MyRankTest TearDown" << std::endl;
    }
};

TEST_F(MyRankTest, Ut_When_QueryListenPort_Listen_Port_Expect_SUCCESS)
{
    std::cout << "Ut_When_QueryListenPort_Listen_Port_Expect_SUCCESS1111111" << std::endl;
    uint32_t devPort = 60001;
    MOCKER_CPP(&Hccl::IRankGraph::GetDevicePort).stubs().with(any(), outBoundP(&devPort)).will(returnValue(HCCL_SUCCESS));
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());
    EndpointDesc localEp;
    localEp.protocol = COMM_PROTOCOL_ROCE;
    localEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    localEp.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    localEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointDesc rmtEp;
    rmtEp.protocol = COMM_PROTOCOL_ROCE;
    rmtEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp.commAddr.addr = Hccl::IpAddress("2.0.0.0").GetBinaryAddress().addr;
    rmtEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    uint32_t listenPort;
    HcommChannelDesc desc;
    HcclResult ret = myRank.QueryListenPort(0, 1, localEp, rmtEp, listenPort, desc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(listenPort, devPort);
    EXPECT_EQ(desc.role, HCOMM_SOCKET_ROLE_SERVER);

    EndpointDesc rmtEp2;
    rmtEp2.protocol = COMM_PROTOCOL_ROCE;
    rmtEp2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp2.commAddr.addr = Hccl::IpAddress("0.0.0.0").GetBinaryAddress().addr;
    rmtEp2.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    ret = myRank.QueryListenPort(0, 2, localEp, rmtEp2, listenPort, desc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(listenPort, devPort);
    EXPECT_EQ(desc.role, HCOMM_SOCKET_ROLE_CLIENT);
}

TEST_F(MyRankTest, Ut_When_QueryListenPort_InValid_Port_Expect_E_PARA)
{
    uint32_t devPort = 1919000;
    MOCKER_CPP(&Hccl::IRankGraph::GetDevicePort).stubs().with(any(), outBoundP(&devPort)).will(returnValue(HCCL_SUCCESS));
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());
    EndpointDesc localEp;
    localEp.protocol = COMM_PROTOCOL_ROCE;
    localEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    localEp.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    localEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointDesc rmtEp;
    rmtEp.protocol = COMM_PROTOCOL_ROCE;
    rmtEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp.commAddr.addr = Hccl::IpAddress("2.0.0.0").GetBinaryAddress().addr;
    rmtEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    uint32_t listenPort;
    HcommChannelDesc desc;
    HcclResult ret = myRank.QueryListenPort(0, 1, localEp, rmtEp, listenPort, desc);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(MyRankTest, Ut_When_BatchCreateChannels_Expect_SUCCESS)
{
    uint32_t devPort = 60001;
    MOCKER_CPP(&Hccl::IRankGraph::GetDevicePort).stubs().with(any(), outBoundP(&devPort)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::SocketManager::GetConnectedSocket).stubs().with(any()).will(returnValue((Hccl::Socket*)0xab));
    MOCKER_CPP(&hccl::CommMems::GetTagMemoryHandles).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::EndpointMgr::RegisterMemory).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuResContainer::Init).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    ChannelHandle channelHandle = 0xab;
    MOCKER_CPP(&HcommCollectiveChannelCreate).stubs().with(any(), any(), any(), any(), outBoundP(&channelHandle)).will(returnValue(HCCL_SUCCESS));
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());
    HcclMem cclBuffer;
    cclBuffer.addr = (void*)0xab;
    cclBuffer.size = 1024;
    cclBuffer.type = HCCL_MEM_TYPE_DEVICE;
    EXPECT_EQ(myRank.Init(cclBuffer, 0, 2), HCCL_SUCCESS);
    EndpointDesc localEp;
    localEp.protocol = COMM_PROTOCOL_UB_MEM;
    localEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    localEp.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    localEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointDesc rmtEp;
    rmtEp.protocol = COMM_PROTOCOL_UB_MEM;
    rmtEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp.commAddr.addr = Hccl::IpAddress("2.0.0.0").GetBinaryAddress().addr;
    rmtEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    EndpointDesc rmtEp2;
    rmtEp2.protocol = COMM_PROTOCOL_UB_MEM;
    rmtEp2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp2.commAddr.addr = Hccl::IpAddress("0.0.0.0").GetBinaryAddress().addr;
    rmtEp2.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    HcclChannelDesc channelDesc[3];
    channelDesc[0].channelProtocol = COMM_PROTOCOL_UB_MEM;
    channelDesc[0].remoteRank = 1;
    channelDesc[0].notifyNum = 2;
    channelDesc[0].localEndpoint = localEp;
    channelDesc[0].remoteEndpoint = rmtEp;
    channelDesc[1].channelProtocol = COMM_PROTOCOL_UB_MEM;
    channelDesc[1].remoteRank = 1;
    channelDesc[1].notifyNum = 2;
    channelDesc[1].localEndpoint = localEp;
    channelDesc[1].remoteEndpoint = rmtEp;
    channelDesc[2].channelProtocol = COMM_PROTOCOL_UB_MEM;
    channelDesc[2].remoteRank = 2;
    channelDesc[2].notifyNum = 2;
    channelDesc[2].localEndpoint = localEp;
    channelDesc[2].remoteEndpoint = rmtEp2;

    std::vector<HcommChannelDesc> hcommDesc(3);
    EXPECT_EQ(myRank.BatchCreateSockets(channelDesc, 1, "test", hcommDesc), HCCL_SUCCESS);
    std::vector<ChannelHandle> hostChannelHandles(3);
    ChannelHandle *hostChannelHandleList = hostChannelHandles.data();
    EXPECT_EQ(myRank.BatchCreateChannels(COMM_ENGINE_AICPU_TS, channelDesc, 1, hcommDesc, hostChannelHandleList), HCCL_SUCCESS);

    EXPECT_EQ(myRank.BatchCreateSockets(channelDesc, 2, "test", hcommDesc), HCCL_SUCCESS);
    EXPECT_EQ(myRank.BatchCreateChannels(COMM_ENGINE_AICPU_TS, channelDesc, 2, hcommDesc, hostChannelHandleList), HCCL_SUCCESS);

    EXPECT_EQ(myRank.BatchCreateSockets(channelDesc, 3, "test", hcommDesc), HCCL_SUCCESS);
    EXPECT_EQ(myRank.BatchCreateChannels(COMM_ENGINE_AICPU_TS, channelDesc, 3, hcommDesc, hostChannelHandleList), HCCL_SUCCESS);
}
/* 
// Helper: 初始化 MyRank 并可选创建若干通道（复用已有的 mocking 风格）
static std::unique_ptr<MyRank> CreateMyRankWithChannels(uint32_t channelNum,
    std::vector<ChannelHandle> &outHostChannelHandles)
{
    // 常用 mock
    uint32_t devPort = 60001;
    MOCKER_CPP(&Hccl::IRankGraph::GetDevicePort).stubs().with(any(), outBoundP(&devPort)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::SocketManager::GetConnectedSocket).stubs().with(any()).will(returnValue((Hccl::Socket*)0xab));
    MOCKER_CPP(&hccl::CommMems::GetTagMemoryHandles).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::EndpointMgr::RegisterMemory).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuResContainer::Init).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    ChannelHandle channelHandle = 0xab;
    MOCKER_CPP(&HcommCollectiveChannelCreate).stubs().with(any(), any(), any(), any(), outBoundP(&channelHandle)).will(returnValue(HCCL_SUCCESS));

    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    std::unique_ptr<MyRank> myRank = std::make_unique<MyRank>(binHandle, 0, config, callbacks, rankGraph.get());

    HcclMem cclBuffer;
    cclBuffer.addr = (void*)0xab;
    cclBuffer.size = 1024;
    cclBuffer.type = HCCL_MEM_TYPE_DEVICE;
    EXPECT_EQ(myRank->Init(cclBuffer, 0, 2), HCCL_SUCCESS);

    // 如果不需要创建通道，直接返回
    if (channelNum == 0) {
        return myRank;
    }

    // 构造 endpoint 和 channelDesc（与已有测试保持一致）
    EndpointDesc localEp;
    localEp.protocol = COMM_PROTOCOL_UB_MEM;
    localEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    localEp.commAddr.addr = Hccl::IpAddress("1.0.0.0").GetBinaryAddress().addr;
    localEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    EndpointDesc rmtEp;
    rmtEp.protocol = COMM_PROTOCOL_UB_MEM;
    rmtEp.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp.commAddr.addr = Hccl::IpAddress("2.0.0.0").GetBinaryAddress().addr;
    rmtEp.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    EndpointDesc rmtEp2;
    rmtEp2.protocol = COMM_PROTOCOL_UB_MEM;
    rmtEp2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    rmtEp2.commAddr.addr = Hccl::IpAddress("0.0.0.0").GetBinaryAddress().addr;
    rmtEp2.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;

    std::vector<HcclChannelDesc> channelDesc(channelNum);
    for (uint32_t i = 0; i < channelNum; ++i) {
        channelDesc[i].channelProtocol = COMM_PROTOCOL_UB_MEM;
        channelDesc[i].notifyNum = 2;
        channelDesc[i].localEndpoint = localEp;
        // 对前半部分使用 remote rank = 1，后面的使用 rank = 2 来模拟不同远端
        if (i < channelNum / 2) {
            channelDesc[i].remoteRank = 1;
            channelDesc[i].remoteEndpoint = rmtEp;
        } else {
            channelDesc[i].remoteRank = 2;
            channelDesc[i].remoteEndpoint = rmtEp2;
        }
    }

    // 调用 BatchCreateSockets + BatchCreateChannels，模拟内核场景
    std::vector<HcommChannelDesc> hcommDesc(channelNum);
    EXPECT_EQ(myRank->BatchCreateSockets(channelDesc.data(), channelNum, "test", hcommDesc), HCCL_SUCCESS);

    outHostChannelHandles.resize(channelNum);
    ChannelHandle *hostChannelHandleList = outHostChannelHandles.data();
    EXPECT_EQ(myRank->BatchCreateChannels(COMM_ENGINE_AICPU_TS, channelDesc.data(), channelNum, hcommDesc, hostChannelHandleList), HCCL_SUCCESS);

    return myRank;
}

// Tests for SetKfcControlTransfer
TEST_F(MyRankTest, Ut_SetKfcControlTransferWhenCalledExpectNoCrash)
{
    // 初始化 MyRank（无通道）
    std::vector<ChannelHandle> dummy;
    auto myRank = CreateMyRankWithChannels(0, dummy);

    // stub NsRecoveryProcessor::SetKfcControlTransfer 为空行为（void）
    MOCKER_CPP(&hccl::NsRecoveryProcessor::SetKfcControlTransfer).stubs().with(any(), any()).will(returnValue());

    // 调用接口，期望不崩溃
    std::shared_ptr<HDCommunicate> a = std::make_shared<HDCommunicate>();
    std::shared_ptr<HDCommunicate> b = std::make_shared<HDCommunicate>();
    EXPECT_NO_THROW(myRank->SetKfcControlTransfer(a, b));
}

// Tests for StopLaunch
TEST_F(MyRankTest, Ut_StopLaunchWhenNsReturnErrorExpectError)
{
    std::vector<ChannelHandle> dummy;
    auto myRank = CreateMyRankWithChannels(0, dummy);

    MOCKER_CPP(&hccl::NsRecoveryProcessor::StopLaunch).stubs().with(any()).will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = myRank->StopLaunch();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(MyRankTest, Ut_StopLaunchWhenNsReturnSuccessExpectSuccess)
{
    std::vector<ChannelHandle> dummy;
    auto myRank = CreateMyRankWithChannels(0, dummy);

    MOCKER_CPP(&hccl::NsRecoveryProcessor::StopLaunch).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclResult ret = myRank->StopLaunch();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Tests for Clean
TEST_F(MyRankTest, Ut_CleanWhenChannelListEmptyExpectSuccess)
{
    // 不创建通道，GetAllChannelList 应为空
    std::vector<ChannelHandle> dummy;
    auto myRank = CreateMyRankWithChannels(0, dummy);

    // 保证 nsRecoveryProcessor_->Clean 返回成功
    MOCKER_CPP(&hccl::NsRecoveryProcessor::Clean).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclResult ret = myRank->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MyRankTest, Ut_CleanWhenChannelCleanFailsExpectError)
{
    // 创建通道以触发 ChannelClean 调用
    std::vector<ChannelHandle> hostHandles;
    auto myRank = CreateMyRankWithChannels(2, hostHandles);

    // 模拟 ChannelProcess::ChannelClean 失败
    MOCKER_CPP(&hcomm::ChannelProcess::ChannelClean).stubs().with(any(), any()).will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = myRank->Clean();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(MyRankTest, Ut_CleanWhenAllSuccessExpectSuccess)
{
    std::vector<ChannelHandle> hostHandles;
    auto myRank = CreateMyRankWithChannels(2, hostHandles);

    MOCKER_CPP(&hcomm::ChannelProcess::ChannelClean).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::NsRecoveryProcessor::Clean).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    HcclResult ret = myRank->Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Tests for Resume
TEST_F(MyRankTest, Ut_ResumeWhenChannelResumeFailsExpectError)
{
    std::vector<ChannelHandle> hostHandles;
    auto myRank = CreateMyRankWithChannels(2, hostHandles);

    // ChannelResume 失败
    MOCKER_CPP(&hcomm::ChannelProcess::ChannelResume).stubs().with(any(), any()).will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = myRank->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(MyRankTest, Ut_ResumeWhenAllSuccessExpectSuccess)
{
    std::vector<ChannelHandle> hostHandles;
    auto myRank = CreateMyRankWithChannels(2, hostHandles);

    MOCKER_CPP(&hcomm::ChannelProcess::ChannelResume).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::NsRecoveryProcessor::Resume).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));

    HcclResult ret = myRank->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Test for GetAllChannelList basic behavior
TEST_F(MyRankTest, Ut_GetAllChannelListWhenNoChannelsExpectEmpty)
{
    std::vector<ChannelHandle> dummy;
    auto myRank = CreateMyRankWithChannels(0, dummy);

    auto channelList = myRank->GetAllChannelList();
    EXPECT_TRUE(channelList.empty());
} */


