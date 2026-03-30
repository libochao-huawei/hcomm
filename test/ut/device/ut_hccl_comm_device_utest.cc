#include "hccl_api_base_test.h"
#include "hccl_comm_pub.h"
#include "hccl_communicator.h"
#include "aicpu_indop_process.h"

using namespace hccl;

class HcclCommDeviceUtest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommDeviceUtest, Ut_hcclCommDevice_Resume_When_CommunicatorResumeCalled_ReturnsSuccess) {
    UT_COMM_CREATE_DEFAULT(comm);
    hccl::hcclComm *pComm = static_cast<hccl::hcclComm *>(comm);

    // Force non-V2 path so communicator_->Resume() is called
    pComm->devType_ = DevType::DEV_TYPE_NOSOC;

    // Attach a real communicator instance and mock its Resume()
    pComm->communicator_ = std::make_unique<HcclCommunicator>();
    MOCKER_CPP_VIRTUAL(*pComm->communicator_, &HcclCommunicator::Resume)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = pComm->Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_Destroy(comm);
}

// Globals for Aicpu stub
// static CollCommAicpuMgr g_collCommAicpuMgr_for_test;
// static std::string g_collcomm_identifier_for_test;

// HcclResult AicpuGetCommAll_stub(std::vector<std::pair<std::string, CollCommAicpuMgr *>> &aicpuCommInfo) {
//     g_collCommAicpuMgr_for_test.AcquireCollCommAicpu();
//     aicpuCommInfo.clear();
//     aicpuCommInfo.push_back(std::make_pair(g_collcomm_identifier_for_test, &g_collCommAicpuMgr_for_test));
//     return HCCL_SUCCESS;
// }

// TEST_F(HcclCommDeviceUtest, Ut_hcclCommDevice_GetCommStatus_When_DeviceCommFound_ReturnsDeviceStatus) {
//     UT_COMM_CREATE_DEFAULT(comm);
//     hccl::hcclComm *pComm = static_cast<hccl::hcclComm *>(comm);

//     // Force V2 path
//     pComm->devType_ = DevType::DEV_TYPE_950;

//     // Ensure identifier matches what the stub will return
//     pComm->identifier_ = "ut_group_for_status";
//     g_collcomm_identifier_for_test = pComm->identifier_;

//     // Prepare CollCommAicpu inside our manager and set its status
//     g_collCommAicpuMgr_for_test.AcquireCollCommAicpu();
//     CollCommAicpu *coll = g_collCommAicpuMgr_for_test.GetCollCommAicpu();
//     ASSERT_NE(coll, nullptr);
//     coll->SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

//     // Mock the static AicpuIndopProcess::AicpuGetCommAll to return our single entry
//     MOCKER(AicpuIndopProcess::AicpuGetCommAll)
//         .stubs()
//         .will(invoke(AicpuGetCommAll_stub));

//     HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
//     HcclResult ret = pComm->GetCommStatus(status);
//     EXPECT_EQ(ret, HCCL_SUCCESS);
//     EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

//     Ut_Comm_Destroy(comm);
// }
