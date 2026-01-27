#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#define private public
#include "hccl/inc/p2p_mgmt_pub.h"
#undef private


using namespace std;
using namespace hccl;
class P2PMgmtTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--P2PMgmtTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--P2PMgmtTest TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(P2PMgmtTest, ut_BatchEnable_DisableP2P)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(P2PMgmtTest, ut_BatchEnable_DisableP2P_1)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    P2PMgmt::Instance().deviceType_ = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(P2PMgmtTest, ut_BatchEnable_DisableP2P_NoWait)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(P2PMgmtTest, ut_BatchEnable_DestroyP2P)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableAllP2P();
    EXPECT_EQ(ret, HCCL_SUCCESS);

}

TEST_F(P2PMgmtTest, ut_BatchEnable_DestroyP2P_NoWait)
{

    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableAllP2P();
    EXPECT_EQ(ret, HCCL_SUCCESS);

}

TEST_F(P2PMgmtTest, ut_RepeatedlyBatchEnable_DisableP2P)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

        ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(P2PMgmtTest, ut_RepeatedlyBatchEnable_DisableP2P_NoWait)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(P2PMgmtTest, ut_RepeatedlyBatchEnable_DisableP2P_Cross)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(P2PMgmtTest, ut_RepeatedlyBatchEnable_DisableP2P_Cross_NoWait)
{
    HcclResult ret = hrtSetDevice(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    std::vector<uint32_t> remoteDevices = {0,1,2,3,4,5,6,7};

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = P2PMgmt::Instance().EnableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().WaitP2PEnabled(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = P2PMgmt::Instance().DisableP2P(remoteDevices);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}