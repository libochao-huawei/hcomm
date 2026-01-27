#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "env_config.h"

#ifndef private
#define private public
#define protected public
#endif

#include "cann_error_reporter.h"

#include "llt_hccl_stub.h"
#undef private
#undef protected

using namespace std;

namespace {

HcclResult hrtHalSensorNodeRegisterStub(uint32_t devId, uint64_t *handle)
{
    *handle = 0x1111;
    return HCCL_SUCCESS;
}

HcclResult hrtHalSensorNodeUnregisterStub(uint32_t devId, uint64_t handle)
{
    return HCCL_SUCCESS;
}

HcclResult hrtHalSensorNodeUpdateStateStub(uint32_t devId, uint64_t handle, int val, HcclGeneralEventType assertion)
{
    return HCCL_SUCCESS;
}

void MockHrtDlopen()
{
    MOCKER(hrtHalSensorNodeRegister).stubs().will(invoke(hrtHalSensorNodeRegisterStub));
    MOCKER(hrtHalSensorNodeUnregister).stubs().will(invoke(hrtHalSensorNodeUnregisterStub));
    MOCKER(hrtHalSensorNodeUpdateState).stubs().will(invoke(hrtHalSensorNodeUpdateStateStub));
}
}

class CannErrorReporter_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CannErrorReporter_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "CannErrorReporter_ST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MockHrtDlopen();
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        setenv("HCCL_DFS_CONFIG", "connection_fault_detection_time:0", 1);
        InitEnvParam();
        std::cout << "CannErrorReporter_ST Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "CannErrorReporter_ST Test TearDown" << std::endl;
    }
};

TEST_F(CannErrorReporter_ST, Register_Update_DeRegister_success)
{
    uint32_t devId = 0;
    HcclResult ret = dfx::CannErrorReporter::GetInstance().Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().UpdateSensorNode(devId, dfx::ReportStatus::kRetrySuccess);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().UpdateSensorNode(devId, dfx::ReportStatus::kRetryWithBackupLink);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().UpdateSensorNode(devId, dfx::ReportStatus::kRetryFail);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().Clear();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

