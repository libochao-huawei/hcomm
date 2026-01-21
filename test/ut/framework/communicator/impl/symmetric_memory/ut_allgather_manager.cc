#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <iostream>
#include <vector>
#include <algorithm>

// 假设SimpleVaAllocator在这些头文件中
#define private public
#define protected public
#include "symmetric_memory.h"  // 替换为实际的头文件
#undef private
#undef protected

using namespace std;
using namespace hccl;

class AllGatherManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AllGatherManagerTest Testcase SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "AllGatherManagerTest Testcase TearDown" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A AllGatherManagerTest SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A AllGatherManagerTest TearDown" << std::endl;
    }
};

TEST_F(AllGatherManagerTest, ut_Init_When_Normal_Expect_ReturnHCCL_SUCCESS)
{
    HcclResult ret = ;
    std::unique_ptr<HcclSocketManager> socketManager;
    socketManager.reset(new (std::nothrow) HcclSocketManager(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, 0));
    std::string commTag = "SocketManagerTest";
    bool isInterLink = false;
    u32 socketsPerLink = 1;
    NicType socketType = NicType::VNIC_TYPE;
    HcclSocketRole localRole = HcclSocketRole::SOCKET_ROLE_SERVER;
    HcclIpAddress localIPs(0x01);
    ret = HcclNetInit(NICDeployment::NIC_DEPLOYMENT_DEVICE, 0, 0, false);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::vector<RankInfo> rank_vector;
    get_ranks_1server_2dev(rank_vector);
    ZeroCopyMemoryAgent ZeroCopyMemoryAgent(socketManager, 0, 0, localIPs, rank_vector, 0, true,"ZeroCopyMemoryAgentTest");
    EXPECT_EQ(ZeroCopyMemoryAgent.Init(), HCCL_SUCCESS);

    std::vector<RankInfo> rankInfoList(2);
    HcclIpAddress localVnicIp;
    std::shared_ptr<AllGatherManager> allGatherManager_ = std::make_shared<AllGatherManager>(nullptr, 0,
        0, localVnicIp, rankInfoList, 0, true, "1");
    MOCKER_CPP(&AllGatherManager::Init)
        .stubs()
        .with()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&AllGatherManager::AllGather)
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));
    
    SymmetricMemory symmetricMemory(0, 2, 2097152, allGatherManager_);
    HcclResult ret = symmetricMemory.Init(); // 1MB
    EXPECT_EQ(ret, HCCL_SUCCESS);
}