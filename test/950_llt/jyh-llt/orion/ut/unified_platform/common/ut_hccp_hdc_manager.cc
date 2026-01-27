#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "hccp_hdc_manager.h"
#include "orion_adapter_rts.h"
using namespace Hccl;

class HccpHdcManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpHdcManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HccpHdcManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HccpHdcManagerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HccpHdcManagerTest TearDown" << std::endl;
    }
};

TEST_F(HccpHdcManagerTest, hccp_hdc_manager_getInstance)
{
    // Given
    s32 fakedevPhyId  = 3;
    s32 fakedevPhyId1  = 4;
    MOCKER(HrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(fakedevPhyId))
        .then(returnValue(fakedevPhyId1));
    // when
    s32 deviceLogicId = 0;
    HccpHdcManager::GetInstance().Init(deviceLogicId);
    s32 deviceLogicId1 = 1;
    HccpHdcManager::GetInstance().Init(deviceLogicId1);
    auto res = HccpHdcManager::GetInstance().GetSet();

    // then
    EXPECT_EQ(2, res.size());
}

TEST_F(HccpHdcManagerTest, hccp_hdc_manager_init)
{
    // Given
    s32 deviceLogicId = 0;
    s32 deviceLogicId1 = 1;
    s32 deviceLogicId2 = 2;
    s32 fakedevPhyId   = 3;
    MOCKER(HrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(fakedevPhyId));
    // when
    HccpHdcManager::GetInstance().Init(deviceLogicId);
    auto res1 = HccpHdcManager::GetInstance().GetSet();
    HccpHdcManager::GetInstance().Init(deviceLogicId);
    auto res2 = HccpHdcManager::GetInstance().GetSet();

    // then
    EXPECT_EQ(res1, res2);

    // when
    HccpHdcManager::GetInstance().Init(deviceLogicId);
    HccpHdcManager::GetInstance().Init(deviceLogicId);
    HccpHdcManager::GetInstance().Init(deviceLogicId1);
    HccpHdcManager::GetInstance().Init(deviceLogicId1);
    HccpHdcManager::GetInstance().Init(deviceLogicId2);
    HccpHdcManager::GetInstance().Init(deviceLogicId2);
    auto res = HccpHdcManager::GetInstance().GetSet();

    // then
    EXPECT_EQ(3, res.size());
}