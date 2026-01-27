#define private public
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "hccp_peer_manager.h"
#include "orion_adapter_rts.h"
#include "orion_adapter_hccp.h"
#undef private
using namespace Hccl;

class HccpPeerManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpPeerManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HccpPeerManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HccpPeerManagerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HccpPeerManagerTest TearDown" << std::endl;
    }
};

TEST_F(HccpPeerManagerTest, hccp_peer_manager_getInstance)
{
    // Given
    s32 fakedevPhyId  = 3;
    s32 fakedevPhyId1  = 4;
    MOCKER(HrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(fakedevPhyId))
        .then(returnValue(fakedevPhyId1));
    MOCKER(HrtRaInit).stubs().with();
    MOCKER(HrtRaDeInit).stubs().with();
    // when
    s32 deviceLogicId = 0;
    HccpPeerManager::GetInstance().Init(deviceLogicId);
    s32 deviceLogicId1 = 1;
    HccpPeerManager::GetInstance().Init(deviceLogicId1);
    auto res = HccpPeerManager::GetInstance().instances_;

    // then
    EXPECT_EQ(2, res.size());
}

TEST_F(HccpPeerManagerTest, hccp_peer_manager_init)
{
    HccpPeerManager::GetInstance().instances_.clear();
    // Given
    s32 deviceLogicId = 0;
    s32 deviceLogicId1 = 1;
    s32 deviceLogicId2 = 2;
    s32 fakedevPhyId   = 3;
    MOCKER(HrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(fakedevPhyId));
    MOCKER(HrtRaDeInit).stubs().with();
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(1));

    // when
    HccpPeerManager::GetInstance().Init(deviceLogicId);
    auto res1 = HccpPeerManager::GetInstance().instances_;
    HccpPeerManager::GetInstance().Init(deviceLogicId);
    auto res2 = HccpPeerManager::GetInstance().instances_;

    // then
    EXPECT_EQ(res1[deviceLogicId].Count() + 1, res2[deviceLogicId].Count());

    // when
    HccpPeerManager::GetInstance().Init(deviceLogicId);
    HccpPeerManager::GetInstance().Init(deviceLogicId);
    HccpPeerManager::GetInstance().Init(deviceLogicId1);
    HccpPeerManager::GetInstance().Init(deviceLogicId1);
    HccpPeerManager::GetInstance().Init(deviceLogicId2);
    HccpPeerManager::GetInstance().Init(deviceLogicId2);
    auto res = HccpPeerManager::GetInstance().instances_;

    // then
    EXPECT_EQ(3, res.size());

    EXPECT_NO_THROW(HccpPeerManager::GetInstance().DeInit(deviceLogicId1));
    EXPECT_NO_THROW(HccpPeerManager::GetInstance().DeInit(deviceLogicId1));
    EXPECT_NO_THROW(HccpPeerManager::GetInstance().DeInitAll());
}