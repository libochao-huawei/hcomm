#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "ccu_dev_mgr_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"

#include "unified_platform/ccu/ccu_device/ccu_component/ccu_component.h"

using namespace hcomm;

class CcuCompPubTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    static void TearDownTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void SetUp() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void TearDown() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }

    // Helpers to stub CcuComponent member functions
    void StubSetTaskKill(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::SetTaskKill).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::SetTaskKill).stubs().will(returnValue(ret));
    }
    void StubSetTaskKillDone(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::SetTaskKillDone).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::SetTaskKillDone).stubs().will(returnValue(ret));
    }
    void StubCleanTaskKillState(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::CleanTaskKillState).stubs().will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::CleanTaskKillState).stubs().will(returnValue(ret));
    }
    void StubCleanDieCkes(const HcclResult ret) {
        MOCKER_CPP(&CcuComponent::CleanDieCkes).stubs().with(any()).will(returnValue(ret));
        MOCKER_CPP(&Hccl::CcuComponent::CleanDieCkes).stubs().with(any()).will(returnValue(ret));
    }
};

// CcuSetTaskKill: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenDeviceIdInvalidExpectHcclEPara) {
    const int32_t badId = static_cast<int32_t>(MAX_MODULE_DEVICE_NUM); // out of range
    auto ret = CcuSetTaskKill(-1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuSetTaskKill(badId);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuSetTaskKill: valid deviceLogicId forwards to CcuComponent::SetTaskKill
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenUnderlyingSucceedsExpectSuccess) {
    StubSetTaskKill(HcclResult::HCCL_SUCCESS);
    auto ret = CcuSetTaskKill(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillWhenUnderlyingFailsExpectFailure) {
    StubSetTaskKill(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuSetTaskKill(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuSetTaskKillDone: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuSetTaskKillDone(-2);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuSetTaskKillDone(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM));
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuSetTaskKillDone: forwards to CcuComponent::SetTaskKillDone
TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenUnderlyingSucceedsExpectSuccess) {
    StubSetTaskKillDone(HcclResult::HCCL_SUCCESS);
    auto ret = CcuSetTaskKillDone(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuSetTaskKillDoneWhenUnderlyingFailsExpectFailure) {
    StubSetTaskKillDone(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuSetTaskKillDone(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuCleanTaskKillState: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuCleanTaskKillState(-5);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuCleanTaskKillState(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM));
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuCleanTaskKillState: forwards to CcuComponent::CleanTaskKillState
TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenUnderlyingSucceedsExpectSuccess) {
    StubCleanTaskKillState(HcclResult::HCCL_SUCCESS);
    auto ret = CcuCleanTaskKillState(0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuCleanTaskKillStateWhenUnderlyingFailsExpectFailure) {
    StubCleanTaskKillState(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuCleanTaskKillState(0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}

// CcuCleanDieCkes: invalid deviceLogicId -> HCCL_E_PARA
TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenDeviceIdInvalidExpectHcclEPara) {
    auto ret = CcuCleanDieCkes(-1, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
    ret = CcuCleanDieCkes(static_cast<int32_t>(MAX_MODULE_DEVICE_NUM), 1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_PARA);
}

// CcuCleanDieCkes: forwards to CcuComponent::CleanDieCkes
TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenUnderlyingSucceedsExpectSuccess) {
    StubCleanDieCkes(HcclResult::HCCL_SUCCESS);
    auto ret = CcuCleanDieCkes(0, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(CcuCompPubTest, Ut_CcuCleanDieCkesWhenUnderlyingFailsExpectFailure) {
    StubCleanDieCkes(HcclResult::HCCL_E_INTERNAL);
    auto ret = CcuCleanDieCkes(0, 1);
    EXPECT_EQ(ret, HcclResult::HCCL_E_INTERNAL);
}