#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "../../../framework/communicator/hccl_api_base_test.h"
#include "next/coll_comms/communicator/aicpu/aicpu_indop_process.h"
#include "next/coll_comms/communicator/aicpu/coll_comm_aicpu_mgr.h"

using namespace hccl;

class HcclCommDeviceTest : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

// Device-targeted tests: these live in test/ut/device so they are built against device implementation
// Resume when communicator is V2 -> should not call communicator_->Resume and return SUCCESS
TEST_F(HcclCommDeviceTest, Ut_ResumeWhenIsCommunicatorV2ExpectSuccess)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_950; // mark as communicator V2 (device behavior)

    HcclResult ret = hcclCommPtr->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Resume when communicator is V1 and communicator_->Resume succeeds
TEST_F(HcclCommDeviceTest, Ut_ResumeWhenIsCommunicatorV1AndCommResumeSucceedsExpectSuccess)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_910_93; // V1

    // ensure communicator_ exists and mock Resume
    hcclCommPtr->communicator_.reset(new HcclCommunicator());
    MOCKER_CPP(&HcclCommunicator::Resume)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = hcclCommPtr->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Resume when communicator is V1 and communicator_->Resume fails
TEST_F(HcclCommDeviceTest, Ut_ResumeWhenIsCommunicatorV1AndCommResumeFailsExpectError)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_910_93; // V1

    hcclCommPtr->communicator_.reset(new HcclCommunicator());
    MOCKER_CPP(&HcclCommunicator::Resume)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    HcclResult ret = hcclCommPtr->Resume();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// GetCommStatus when communicator is V1 -> should return READY
TEST_F(HcclCommDeviceTest, Ut_GetCommStatusWhenIsCommunicatorV1ExpectReady)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_910_93; // V1

    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    HcclResult ret = hcclCommPtr->GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
}

/* // GetCommStatus when communicator is V2 but no matching aicpu entry -> should remain READY
TEST_F(HcclCommDeviceTest, Ut_GetCommStatusWhenIsCommunicatorV2AndNoMatchExpectReady)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_950; // V2

    // create an aicpu comm mgr with a different group name
    CollCommAicpuMgr *mgr = nullptr;
    std::string otherGroup = "other_group_ut_device";
    EXPECT_EQ(AicpuIndopProcess::AcquireAicpuCommMgr(otherGroup, &mgr), HCCL_SUCCESS);

    // set identifier to something that does not match the created group
    hcclCommPtr->identifier_ = "non_matching_identifier";

    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    HcclResult ret = hcclCommPtr->GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
}

// GetCommStatus when communicator is V2 and matching aicpu entry exists -> should return that status
TEST_F(HcclCommDeviceTest, Ut_GetCommStatusWhenIsCommunicatorV2AndMatchExpectCollStatus)
{
    std::shared_ptr<hccl::hcclComm> hcclCommPtr = std::make_shared<hccl::hcclComm>();
    hcclCommPtr->devType_ = DevType::DEV_TYPE_950; // V2

    // create an aicpu comm mgr with a specific group name
    CollCommAicpuMgr *mgr = nullptr;
    std::string group = "matching_group_ut_device";
    EXPECT_EQ(AicpuIndopProcess::AcquireAicpuCommMgr(group, &mgr), HCCL_SUCCESS);

    // set coll comm status to SUSPENDING
    CollCommAicpu* aicpuComm = mgr->GetCollCommAicpu();
    ASSERT_NE(aicpuComm, nullptr);
    aicpuComm->SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

    // set hccl identifier to match
    hcclCommPtr->identifier_ = group;

    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    HcclResult ret = hcclCommPtr->GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
} */
