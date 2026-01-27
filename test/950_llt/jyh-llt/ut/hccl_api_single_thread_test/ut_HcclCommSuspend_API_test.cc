#include "hccl_api_base_test.h"

class HcclCommSuspendTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommSuspendTest, Ut_HcclCommSuspend_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR) {
    Ut_Device_Set(0);
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommSuspend(comm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommSuspendTest, Ut_HcclCommSuspend_When_CommIsOk_Expect_ReturnIsHCCL_SUCCESS) {
    UT_COMM_CREATE_DEFAULT(comm);

    HcclResult ret = HcclCommSuspend(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_Destroy(comm);
}