#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#define private public
#include "hccl_comm_pub.h"
#undef private
#include "aicpu_indop_process.h"
#include "coll_comm_aicpu_mgr.h"
#include "coll_comm_aicpu.h"

using namespace hccl;

class HcclCommDeviceTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }

    // helper to build a map entry for AicpuGetCommAll
    static std::pair<std::string, CollCommAicpuMgr*> MakeCommEntry(const std::string &id, CollCommAicpuMgr* mgr) {
        return std::make_pair(id, mgr);
    }
};

TEST_F(HcclCommDeviceTest, Ut_GetCommStatusWhenNotV2ExpectReady) {
    // construct with empty identifier
    hcclComm comm(0, 0, "", "");
    // ensure devType_ not 950 so IsCommunicatorV2 returns false
    comm.devType_ = DevType::DEV_TYPE_910; // access internal for test fixture
    HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
    auto ret = comm.GetCommStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_READY);
}

// global entries used by the stub
static std::vector<std::pair<std::string, CollCommAicpuMgr*>> g_aicpu_entries;
static HcclResult AicpuGetCommAll_stub(std::vector<std::pair<std::string, CollCommAicpuMgr*>> &out) {
    out = g_aicpu_entries;
    return HCCL_SUCCESS;
}

// TEST_F(HcclCommDeviceTest, Ut_GetCommStatusWhenV2AndIdentifierMatchExpectDeviceStatus) {
//     // construct with identifier set via ctor because identifier_ is const
//     hcclComm comm(0, 0, "test_id", "");
//     // mark as V2
//     comm.devType_ = DevType::DEV_TYPE_950;

//     // create and register a "other" group
//     CollCommAicpuMgr *otherMgr = nullptr;
//     ASSERT_EQ(AicpuIndopProcess::AcquireAicpuCommMgr(std::string("other"), &otherMgr), HCCL_SUCCESS);
//     // create and register the target group
//     CollCommAicpuMgr *mgr = nullptr;
//     ASSERT_EQ(AicpuIndopProcess::AcquireAicpuCommMgr(std::string("test_id"), &mgr), HCCL_SUCCESS);

//     // ensure internal CollCommAicpu exists
//     ASSERT_EQ(mgr->AcquireCollCommAicpu(), HCCL_SUCCESS);
//     CollCommAicpu* aicpu = mgr->GetCollCommAicpu();
//     ASSERT_NE(aicpu, nullptr);
//     aicpu->SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

//     // sanity: AicpuGetCommAll should return our registered entries
//     std::vector<std::pair<std::string, CollCommAicpuMgr*>> sanityEntries;
//     ASSERT_EQ(AicpuIndopProcess::AicpuGetCommAll(sanityEntries), HCCL_SUCCESS);
//     bool foundTestId = false;
//     for (const auto &e : sanityEntries) {
//         if (e.first == "test_id") {
//             foundTestId = true;
//             break;
//         }
//     }
//     ASSERT_TRUE(foundTestId);

//     HcclCommStatus status = HcclCommStatus::HCCL_COMM_STATUS_INVALID;
//     auto ret = comm.GetCommStatus(status);
//     EXPECT_EQ(ret, HCCL_SUCCESS);
//     EXPECT_EQ(status, HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);

//     // cleanup: mark as destroyed
//     AicpuIndopProcess::AicpuDestroyCommbyGroup(std::string("other"));
//     AicpuIndopProcess::AicpuDestroyCommbyGroup(std::string("test_id"));
// }