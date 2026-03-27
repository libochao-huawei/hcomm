#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "ns_recovery_func_lite.h"
#include "aicpu_indop_process.h"

#define private public
using namespace hccl;

class NsRecoveryFuncLiteTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        // noop
    }
    static void TearDownTestCase() {
        // noop
    }
    void SetUp() override {}
    void TearDown() override {
        GlobalMockObject::verify();
    }
};

// Case: AicpuGetCommAll returns error -> Call should exit gracefully (no crash)
TEST_F(NsRecoveryFuncLiteTest, Ut_AicpuGetCommAll_Error_NoCrash) {
    // Mock AicpuGetCommAll to return an error
    MOCKER_CPP(&AicpuIndopProcess::AicpuGetCommAll, HcclResult(std::vector<std::pair<std::string, CollCommAicpuMgr*>>&))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_E_INTERNAL));

    // Should not throw / crash
    NsRecoveryFuncLite::GetInstance().Call();
}

// Case: AicpuGetCommAll returns success but empty list -> Call should iterate zero times and exit
TEST_F(NsRecoveryFuncLiteTest, Ut_AicpuGetCommAll_EmptyList_Noop) {
    std::vector<std::pair<std::string, CollCommAicpuMgr*>> emptyList;
    // Set out param to empty vector and return success
    MOCKER_CPP(&AicpuIndopProcess::AicpuGetCommAll, HcclResult(std::vector<std::pair<std::string, CollCommAicpuMgr*>>&))
        .stubs()
        .with(outBoundP(&emptyList))
        .will(returnValue(HCCL_SUCCESS));

    NsRecoveryFuncLite::GetInstance().Call();
}

// Additional smoke test: Call twice to ensure reentrancy/no state leak
TEST_F(NsRecoveryFuncLiteTest, Ut_Call_Twice_NoCrash) {
    // On first call, make AicpuGetCommAll return error; on second call return empty list
    std::vector<std::pair<std::string, CollCommAicpuMgr*>> emptyList;
    MOCKER_CPP(&AicpuIndopProcess::AicpuGetCommAll, HcclResult(std::vector<std::pair<std::string, CollCommAicpuMgr*>>&))
        .stubs()
        .with(any())
        .will(returnValue(HCCL_E_INTERNAL));

    NsRecoveryFuncLite::GetInstance().Call();

    MOCKER_CPP(&AicpuIndopProcess::AicpuGetCommAll, HcclResult(std::vector<std::pair<std::string, CollCommAicpuMgr*>>&))
        .stubs()
        .with(outBoundP(&emptyList))
        .will(returnValue(HCCL_SUCCESS));

    NsRecoveryFuncLite::GetInstance().Call();
}
