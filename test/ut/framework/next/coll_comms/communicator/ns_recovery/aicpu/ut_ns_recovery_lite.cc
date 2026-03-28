#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "ns_recovery_lite.h"
#include "hdc_pub.h"
#include "kfc.h"

#define private public
using namespace hccl;

class NsRecoveryLiteUnitTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    void SetUp() override {}
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(NsRecoveryLiteUnitTest, Ut_InitAndBackGroundGetCmd_Succeeds) {
    // create host-side shared memory communicators and init host
    auto h2d = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand));
    auto d2h = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus));
    ASSERT_EQ(h2d->InitHost(), HCCL_SUCCESS);
    ASSERT_EQ(d2h->InitHost(), HCCL_SUCCESS);

    NsRecoveryLite ns;
    ns.Init(h2d, d2h);

    // write a command into h2d buffer and read via NsRecoveryLite
    Hccl::KfcCommand cmd = Hccl::KfcCommand::NS_STOP_LAUNCH;
    EXPECT_EQ(h2d->Put(0, sizeof(cmd), reinterpret_cast<uint8_t *>(&cmd)), HCCL_SUCCESS);

    Hccl::KfcCommand got = ns.BackGroundGetCmd();
    EXPECT_EQ(got, Hccl::KfcCommand::NS_STOP_LAUNCH);
}

TEST_F(NsRecoveryLiteUnitTest, Ut_BackGroundGetCmd_When_GetFails_Returns_NONE) {
    // create HDCommunicate but do NOT call InitHost on h2d to simulate Get failure
    auto h2d = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand));
    auto d2h = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus));
    // init only d2h to avoid unrelated failures
    ASSERT_EQ(d2h->InitHost(), HCCL_SUCCESS);

    NsRecoveryLite ns;
    ns.Init(h2d, d2h);

    // since h2d is not initialized, Get should fail and BackGroundGetCmd should return NONE
    Hccl::KfcCommand got = ns.BackGroundGetCmd();
    EXPECT_EQ(got, Hccl::KfcCommand::NONE);
}

TEST_F(NsRecoveryLiteUnitTest, Ut_BackGroundSetStatus_WritesD2HBuffer) {
    auto h2d = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand));
    auto d2h = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus));
    ASSERT_EQ(h2d->InitHost(), HCCL_SUCCESS);
    ASSERT_EQ(d2h->InitHost(), HCCL_SUCCESS);

    NsRecoveryLite ns;
    ns.Init(h2d, d2h);

    // set status via NsRecoveryLite, which should call into handler->SetKfcExecStatus -> d2h.Put
    ns.BackGroundSetStatus(Hccl::KfcStatus::STOP_LAUNCH_DONE, Hccl::KfcErrType::NONE);

    // read back from d2h buffer
    Hccl::KfcExecStatus exec{};
    auto ret = d2h->Get(0, sizeof(exec), reinterpret_cast<uint8_t *>(&exec));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(exec.kfcStatus, Hccl::KfcStatus::STOP_LAUNCH_DONE);
}

TEST_F(NsRecoveryLiteUnitTest, Ut_SetNeedClean_ResetErrorReported) {
    auto h2d = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_H2D, sizeof(Hccl::KfcCommand));
    auto d2h = std::make_shared<HDCommunicate>(0, HCCL_HDC_TYPE_D2H, sizeof(Hccl::KfcExecStatus));
    ASSERT_EQ(h2d->InitHost(), HCCL_SUCCESS);
    ASSERT_EQ(d2h->InitHost(), HCCL_SUCCESS);

    NsRecoveryLite ns;
    ns.Init(h2d, d2h);

    ns.SetNeedClean(true);
    EXPECT_TRUE(ns.IsNeedClean());
    ns.SetNeedClean(false);
    EXPECT_FALSE(ns.IsNeedClean());

    // ResetErrorReported has no observable accessor; just ensure it doesn't crash
    ns.ResetErrorReported();
}
