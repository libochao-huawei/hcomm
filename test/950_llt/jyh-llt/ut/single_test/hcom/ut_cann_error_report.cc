#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

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

class CannErrorReporter_UT : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CannErrorReporter_UT SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "CannErrorReporter_UT TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        MockHrtDlopen();
        std::cout << "CannErrorReporter_UT Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "CannErrorReporter_UT Test TearDown" << std::endl;
    }
};

TEST_F(CannErrorReporter_UT, Register_Update_DeRegister_success)
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

TEST_F(CannErrorReporter_UT, Update_fail_and_Clear)
{
    uint32_t devId = 0;
    HcclResult ret = dfx::CannErrorReporter::GetInstance().Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().UpdateSensorNode(devId, dfx::ReportStatus::kDefault);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    ret = dfx::CannErrorReporter::GetInstance().Clear();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CannErrorReporter_UT, DeRegister_direct)
{
    uint32_t devId = 0;
    HcclResult ret = dfx::CannErrorReporter::GetInstance().Clear();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(CannErrorReporter_UT, Register_double)
{
    uint32_t devId = 0;
    HcclResult ret = dfx::CannErrorReporter::GetInstance().Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = dfx::CannErrorReporter::GetInstance().Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}