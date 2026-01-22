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

void get_ranks_1server_2dev(std::vector<RankInfo>& rank_vector)
{
    RankInfo tmp_para_0;

    tmp_para_0.userRank = 0;
    tmp_para_0.devicePhyId = 0;
    tmp_para_0.deviceType = DevType::DEV_TYPE_910;
    tmp_para_0.serverIdx = 0;
    tmp_para_0.serverId = "10.0.0.10";
    tmp_para_0.nicIp.push_back(HcclIpAddress("192.168.0.11"));
    tmp_para_0.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    RankInfo tmp_para_1;

    tmp_para_1.userRank = 1;
    tmp_para_1.devicePhyId = 1;
    tmp_para_1.deviceType = DevType::DEV_TYPE_910;
    tmp_para_1.serverIdx = 0;
    tmp_para_1.serverId = "10.0.0.10";
    tmp_para_1.nicIp.push_back(HcclIpAddress("192.168.0.12"));
    tmp_para_1.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    rank_vector.push_back(tmp_para_0);
    rank_vector.push_back(tmp_para_1);
    return;
}

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
    AllGatherManager allGatherManager(socketManager, 0, 0, localIPs, rank_vector, 0, true,"AllGatherManagerTest");
    EXPECT_EQ(allGatherManager.Init(), HCCL_SUCCESS);

    EXPECT_EQ(ret, HCCL_SUCCESS);
}