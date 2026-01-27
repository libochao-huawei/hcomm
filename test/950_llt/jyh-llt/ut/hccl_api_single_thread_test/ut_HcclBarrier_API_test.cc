#include "hccl_api_base_test.h"

class HcclBarrierTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclBarrierTest, Ut_HcclBarrier_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    UT_STREAM_CREATE_DEFAULT(stream);

    HcclResult ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclBarrierTest, Ut_HcclBarrier_When_RankSizeIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    UT_COMM_CREATE_DEFAULT(comm);
    rtStream_t stream = nullptr;

    HcclResult ret = HcclBarrier(comm, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}