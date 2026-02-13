#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "rank_graph_interface.h"
#include "rank_graph_v2.h"
#include "hcomm_c_adpt.h"
#include "my_rank.h"
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