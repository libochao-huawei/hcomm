#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "rank_graph_interface.h"
#include "rank_graph_v2.h"
#include "hcomm_c_adpt.h"
#include "my_rank.h"
#include "channel_process.h"
#include "base_config.h"
#define private public
using namespace hccl;

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
    setenv("HCCL_DFS_CONFIG", "task_exception:on", 1);
    uint32_t devPort = 60001;
    MOCKER_CPP(&Hccl::IRankGraph::GetDevicePort).stubs().with(any(), outBoundP(&devPort)).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&Hccl::SocketManager::GetConnectedSocket).stubs().with(any()).will(returnValue((Hccl::Socket*)0xab));
    MOCKER_CPP(&hccl::CommMems::GetTagMemoryHandles).stubs().with(any()).will(returnValue(static_cast<int>(HCCL_SUCCESS)));
    MOCKER_CPP(&hcomm::EndpointMgr::RegisterMemory).stubs().with(any()).will(returnValue(static_cast<int>(HCCL_SUCCESS)));
    MOCKER_CPP(&hccl::CommMems::SetMemHandles).stubs().with(any()).will(returnValue(static_cast<int>(HCCL_SUCCESS)));
    MOCKER_CPP(&hcomm::CcuResContainer::Init).stubs().with(any()).will(returnValue(static_cast<int>(HCCL_SUCCESS)));
    ChannelHandle channelHandle = 0xab;
    MOCKER(hcomm::ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
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

    MOCKER_CPP(&hcomm::ChannelProcess::ChannelGetStatus).stubs().with(any()).will(returnValue(HCCL_E_AGAIN));
    MOCKER_CPP(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().with(any()).will(returnValue((s32)(1)));
    EXPECT_EQ(myRank.BatchConnectChannels(channelDesc, hostChannelHandleList, 3), HCCL_E_TIMEOUT);
    MOCKER_CPP(&hcomm::ChannelProcess::ChannelGetStatus).stubs().with(any()).will(returnValue(HCCL_E_TIMEOUT));
    EXPECT_EQ(myRank.BatchConnectChannels(channelDesc, hostChannelHandleList, 3), HCCL_E_TIMEOUT);
    unsetenv("HCCL_DFS_CONFIG");
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_Normal_Expect_SUCCESS)
{
    MOCKER(hcomm::ChannelProcess::ChannelGetUserRemoteMem)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());

    ChannelHandle channel = 0x12345;
    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = myRank.ChannelGetRemoteMem(channel, &remoteMem, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_RemoteMemNull_Expect_E_PTR)
{
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());

    ChannelHandle channel = 0x12345;
    char** memTag = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = myRank.ChannelGetRemoteMem(channel, nullptr, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_MemTagNull_Expect_E_PTR)
{
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());

    ChannelHandle channel = 0x12345;
    CommMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = myRank.ChannelGetRemoteMem(channel, &remoteMem, nullptr, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, Ut_When_ChannelGetRemoteMem_MemNumNull_Expect_E_PTR)
{
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());

    ChannelHandle channel = 0x12345;
    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    HcclResult ret = myRank.ChannelGetRemoteMem(channel, &remoteMem, &memTag, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(MyRankTest, ut_SetMemHandles_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    aclrtBinHandle binHandle;
    CommConfig config;
    ManagerCallbacks callbacks;
    void* rankGraphPtr = (void*)0x114514;
    std::shared_ptr<RankGraph> rankGraph = std::make_shared<RankGraphV2>(rankGraphPtr);
    MyRank myRank(binHandle, 0, config, callbacks, rankGraph.get());
    myRank.commMems_ = std::make_unique<CommMems>((uint64_t)0x100);

    CommMemInfo memInfo = CommMemInfo{};
    std::vector<CommMemInfo*> mems{};
    mems.push_back(&memInfo);
    void **memHandles = reinterpret_cast<void**>(mems.data());
    std::vector<MemHandle> memHandleVec{};
    memHandleVec.emplace_back((void*)0x100);
    memHandleVec.emplace_back((void*)0x101);

    std::vector<MemHandle> commMemHandleVec{};
    HcclResult ret = myRank.commMems_->SetMemHandles(memHandles, memHandleVec, commMemHandleVec);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    CommMemInfo** handles = reinterpret_cast<CommMemInfo**>(memHandles);
    EXPECT_EQ(handles[0]->bufferHandle, (void*)0x101);
    auto memInfo0 = static_cast<CommMemInfo*>(commMemHandleVec[0]);
    auto memInfo1 = static_cast<CommMemInfo*>(commMemHandleVec[1]);
    EXPECT_EQ(memInfo0->bufferHandle, (void*)0x100);
    EXPECT_EQ(memInfo1->bufferHandle, (void*)0x101);
}
