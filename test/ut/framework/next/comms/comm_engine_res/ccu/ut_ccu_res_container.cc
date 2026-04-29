#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "ccu_res_container.h"
#include "ccu_dev_mgr_pub.h"
#define private public
using namespace hcomm;

class CcuResContainerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CcuResContainerTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CcuResContainerTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CcuResContainerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuResContainerTest TearDown" << std::endl;
    }
};

TEST_F(CcuResContainerTest, Ut_ChangeMode_When_Normal_Expect_SUCCESS)
{
    MOCKER_CPP(&hcomm::CcuInitFeature).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuResPack::Init).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
    CcuResContainer ccuResContainer;
    EXPECT_EQ(ccuResContainer.ChangeMode(0), HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuResContainerTest, Ut_ChangeMode_When_CcuInitFeature_EAGAIN_Expect_EAGAIN)
{
    MOCKER_CPP(&hcomm::CcuInitFeature).stubs().will(returnValue(HcclResult::HCCL_E_AGAIN));
    MOCKER_CPP(&hcomm::CcuResPack::Init).stubs().will(returnValue(HcclResult::HCCL_E_UNAVAIL));
    CcuResContainer ccuResContainer;
    EXPECT_EQ(ccuResContainer.ChangeMode(0), HcclResult::HCCL_E_AGAIN);
}

TEST_F(CcuResContainerTest, Ut_ChangeMode_When_CcuResPack_Init_UNAVAIL_Expect_UNAVAIL)
{
    MOCKER_CPP(&hcomm::CcuInitFeature).stubs().will(returnValue(HcclResult::HCCL_SUCCESS));
    MOCKER_CPP(&hcomm::CcuResPack::Init).stubs().will(returnValue(HcclResult::HCCL_E_UNAVAIL));
    CcuResContainer ccuResContainer;
    EXPECT_EQ(ccuResContainer.ChangeMode(0), HcclResult::HCCL_E_UNAVAIL);
}

