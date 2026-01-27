#include "hccl_api_base_test.h"

class HcclCommResumeTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
        HcclCommunicator commun_mock;
        MOCKER_CPP_VIRTUAL(commun_mock, &HcclCommunicator::Resume)
            .stubs()
            .with(any())
            .will(returnValue(HCCL_SUCCESS));
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommResumeTest, Ut_HcclCommResume_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR) {
    Ut_Device_Set(0);
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommResume(comm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCommResumeTest, Ut_HcclCommResume_When_CommIsOk_Expect_ReturnIsHCCL_SUCCESS) {
    UT_COMM_CREATE_DEFAULT(comm);

    HcclResult ret = HcclCommResume(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_Destroy(comm);
}