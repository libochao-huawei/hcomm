#define private public
#include "coll_comm_aicpu.h"
#include "ns_recovery/aicpu/ns_recovery_lite.h"
#include "hccl_aicpu_hdc_handler.h"
#undef private

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#include "ns_recovery_func_lite.h"

using namespace hccl;

class NsRecoveryFuncLiteTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

// Helper mocks for halTsdrvCtl
extern "C" drvError_t halTsdrvCtl_ack_mismatch(uint32_t devId, int cmd, void *param, size_t paramSize, void *out, size_t *outSize) {
    // set outSize to an unexpected value
    if (outSize != nullptr) {
        *outSize = 0;
    }
    return DRV_ERROR_NONE;
}

extern "C" drvError_t halTsdrvCtl_set_status(uint32_t devId, int cmd, void *param, size_t paramSize, void *out, size_t *outSize) {
    if (out != nullptr) {
        ts_ctrl_msg_body_t *out_msg = reinterpret_cast<ts_ctrl_msg_body_t *>(out);
        out_msg->u.query_task_ack_info.status = static_cast<uint32_t>(APP_ABORT_STAUTS::APP_ABORT_KILL_FINISH);
    }
    if (outSize != nullptr) {
        *outSize = sizeof(ts_ctrl_msg_body_t);
    }
    return DRV_ERROR_NONE;
}

// Helper for mocking HcclAicpuHdcHandler::GetKfcCommand
HcclResult HdcGetCmdMockStop(HcclAicpuHdcHandler* thiz, Hccl::KfcCommand &cmd) {
    (void)thiz;
    cmd = Hccl::KfcCommand::NS_STOP_LAUNCH;
    return HcclResult::HCCL_SUCCESS;
}
HcclResult HdcGetCmdMockClean(HcclAicpuHdcHandler* thiz, Hccl::KfcCommand &cmd) {
    (void)thiz;
    cmd = Hccl::KfcCommand::NS_CLEAN;
    return HcclResult::HCCL_SUCCESS;
}

TEST_F(NsRecoveryFuncLiteTest, Ut_DeviceQueryWhenDrvAckCountMismatchExpectEDRV) {
    MOCKER(halTsdrvCtl).stubs().with(any()).will(invoke(halTsdrvCtl_ack_mismatch));
    auto ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, 0, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_E_DRV);
}

TEST_F(NsRecoveryFuncLiteTest, Ut_DeviceQueryWhenStatusReachedExpectSuccess) {
    MOCKER(halTsdrvCtl).stubs().with(any()).will(invoke(halTsdrvCtl_set_status));
    auto ret = NsRecoveryFuncLite::GetInstance().DeviceQuery(0, APP_ABORT_STAUTS::APP_ABORT_KILL_FINISH, 0);
    EXPECT_EQ(ret, HcclResult::HCCL_SUCCESS);
}

TEST_F(NsRecoveryFuncLiteTest, Ut_HandleStopLaunchWhenCmdIsStopLaunchExpectSuspending) {
    // prepare coll and nsRecovery
    CollCommAicpu coll;
    coll.SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_READY);
    coll.nsRecoveryLitePtr_ = std::make_shared<NsRecoveryLite>();
    // ensure internal hdc handler exists so BackGroundGetCmd will call it
    coll.nsRecoveryLitePtr_->hdcHandler_ = std::make_unique<HcclAicpuHdcHandler>(nullptr, nullptr);

    // mock HdcHandler GetKfcCommand to return STOP_LAUNCH
    MOCKER_CPP(&HcclAicpuHdcHandler::GetKfcCommand, HcclResult(HcclAicpuHdcHandler::*)(Hccl::KfcCommand&))
        .stubs().with(any()).will(invoke(HdcGetCmdMockStop));

    NsRecoveryFuncLite::GetInstance().HandleStopLaunch(&coll);
    EXPECT_EQ(coll.GetCommmStatus(), HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
    EXPECT_TRUE(coll.GetNsRecoveryLitePtr()->IsNeedClean());
}

TEST_F(NsRecoveryFuncLiteTest, Ut_HandleStopLaunchWhenAlreadySuspendingExpectNoChange) {
    CollCommAicpu coll;
    coll.SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
    coll.nsRecoveryLitePtr_ = std::make_shared<NsRecoveryLite>();
    coll.nsRecoveryLitePtr_->hdcHandler_ = std::make_unique<HcclAicpuHdcHandler>(nullptr, nullptr);

    MOCKER_CPP(&HcclAicpuHdcHandler::GetKfcCommand, HcclResult(HcclAicpuHdcHandler::*)(Hccl::KfcCommand&))
        .stubs().with(any()).will(invoke(HdcGetCmdMockStop));

    // record initial flags
    bool beforeNeedClean = coll.GetNsRecoveryLitePtr()->IsNeedClean();
    NsRecoveryFuncLite::GetInstance().HandleStopLaunch(&coll);
    EXPECT_EQ(coll.GetCommmStatus(), HcclCommStatus::HCCL_COMM_STATUS_SUSPENDING);
    EXPECT_EQ(coll.GetNsRecoveryLitePtr()->IsNeedClean(), beforeNeedClean);
}

TEST_F(NsRecoveryFuncLiteTest, Ut_HandleCleanWhenNeedCleanTrueAndCmdCleanExpectReset) {
    CollCommAicpu coll;
    coll.SetCommmStatus(HcclCommStatus::HCCL_COMM_STATUS_READY);
    coll.nsRecoveryLitePtr_ = std::make_shared<NsRecoveryLite>();
    coll.nsRecoveryLitePtr_->hdcHandler_ = std::make_unique<HcclAicpuHdcHandler>(nullptr, nullptr);

    // set need clean
    coll.nsRecoveryLitePtr_->SetNeedClean(true);

    // mock HdcHandler GetKfcCommand to return CLEAN
    MOCKER_CPP(&HcclAicpuHdcHandler::GetKfcCommand, HcclResult(HcclAicpuHdcHandler::*)(Hccl::KfcCommand&))
        .stubs().with(any()).will(invoke(HdcGetCmdMockClean));

    // mock halTsdrvCtl to make DeviceQuery return drv error so StreamClean will set error but not crash
    MOCKER(halTsdrvCtl).stubs().with(any()).will(returnValue(DRV_ERROR_NOT_SUPPORT));

    NsRecoveryFuncLite::GetInstance().HandleClean(&coll);
    EXPECT_FALSE(coll.GetNsRecoveryLitePtr()->IsNeedClean());
}