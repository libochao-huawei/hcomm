#include "hccl_api_base_test.h"

class HcclCommDestroyTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommDestroyTest, Ut_HcclCommDestroy_When_CommIsNull_Expect_ReturnIsHCCL_SUCCESS) {
    Ut_Device_Set(0);
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommDestroyTest, Ut_HcclCommDestroy_When_GetDeviceError_Expect_ReturnIsHCCL_E_INTERNAL) {
    MOCKER(aclrtGetDevice)
        .stubs()
        .will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(HcclCommDestroyTest, Ut_HcclCommDestroy_When_InitSuccess_Expect_ReturnIsHCCL_SUCCESS) {
    UT_COMM_CREATE_DEFAULT(comm);

    HcclResult ret = HcclCommDestroy(comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}