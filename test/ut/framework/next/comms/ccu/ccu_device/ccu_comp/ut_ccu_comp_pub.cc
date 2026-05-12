#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "ccu_dev_mgr_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"

#include "unified_platform/ccu/ccu_device/ccu_component/ccu_component.h"

using namespace hcomm;

namespace hcomm {
uint32_t CcuCompTaHwValueToMs(uint8_t hwValue);
uint8_t CcuCompFindMinTaHwValueGreaterThan(uint32_t tpTotalTimeoutMs);
}

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

TEST_F(CcuCompPubTest, Ut_CcuCompTaHwValueToMs_When_InputGear0_Expect_Return512ms)
{
    uint32_t timeoutMs = CcuCompTaHwValueToMs(0);
    EXPECT_EQ(timeoutMs, 512u);
    timeoutMs = CcuCompTaHwValueToMs(7);
    EXPECT_EQ(timeoutMs, 512u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompTaHwValueToMs_When_InputGear1_Expect_Return1000ms)
{
    uint32_t timeoutMs = CcuCompTaHwValueToMs(8);
    EXPECT_EQ(timeoutMs, 1000u);
    timeoutMs = CcuCompTaHwValueToMs(15);
    EXPECT_EQ(timeoutMs, 1000u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompTaHwValueToMs_When_InputGear2_Expect_Return8000ms)
{
    uint32_t timeoutMs = CcuCompTaHwValueToMs(16);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = CcuCompTaHwValueToMs(23);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompTaHwValueToMs_When_InputGear3_Expect_Return32000ms)
{
    uint32_t timeoutMs = CcuCompTaHwValueToMs(24);
    EXPECT_EQ(timeoutMs, 32000u);
    timeoutMs = CcuCompTaHwValueToMs(31);
    EXPECT_EQ(timeoutMs, 32000u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompTaHwValueToMs_When_InputInvalid_Expect_ReturnDefault8000ms)
{
    uint32_t timeoutMs = CcuCompTaHwValueToMs(32);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = CcuCompTaHwValueToMs(100);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompFindMinTaHwValueGreaterThan_When_LessThan512ms_Expect_Return0)
{
    uint8_t hwValue = CcuCompFindMinTaHwValueGreaterThan(100);
    EXPECT_EQ(hwValue, 0u);
    hwValue = CcuCompFindMinTaHwValueGreaterThan(511);
    EXPECT_EQ(hwValue, 0u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompFindMinTaHwValueGreaterThan_When_LessThan1000ms_Expect_Return8)
{
    uint8_t hwValue = CcuCompFindMinTaHwValueGreaterThan(512);
    EXPECT_EQ(hwValue, 8u);
    hwValue = CcuCompFindMinTaHwValueGreaterThan(999);
    EXPECT_EQ(hwValue, 8u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompFindMinTaHwValueGreaterThan_When_LessThan8000ms_Expect_Return16)
{
    uint8_t hwValue = CcuCompFindMinTaHwValueGreaterThan(1000);
    EXPECT_EQ(hwValue, 16u);
    hwValue = CcuCompFindMinTaHwValueGreaterThan(7999);
    EXPECT_EQ(hwValue, 16u);
}

TEST_F(CcuCompPubTest, Ut_CcuCompFindMinTaHwValueGreaterThan_When_GreaterOrEqual8000ms_Expect_Return24)
{
    uint8_t hwValue = CcuCompFindMinTaHwValueGreaterThan(8000);
    EXPECT_EQ(hwValue, 24u);
    hwValue = CcuCompFindMinTaHwValueGreaterThan(10000);
    EXPECT_EQ(hwValue, 24u);
}