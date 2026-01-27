#include "hccl_api_base_test.h"

class HcclGetRankSizeTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_1SERVER_1RANK_AS_DEFAULT;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclGetRankSizeTest, Ut_HcclGetRankSize_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    uint32_t rankSize = 0;

    HcclResult ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetRankSizeTest, Ut_HcclGetRankSize_When_RankSizeIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_COMM_CREATE_DEFAULT(comm);
    uint32_t *pRankSize = nullptr;

    HcclResult ret = HcclGetRankSize(comm, pRankSize);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclGetRankSizeTest, Ut_HcclGetRankSize_When_1Server2Rank_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    uint32_t rankSize = 0;

    HcclResult ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankSize, 2);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclGetRankSizeTest, Ut_HcclGetRankSize_When_2Server4Rank_Expect_ReturnHCCL_SUCCESS)
{
    UT_COMM_CREATE_DEFAULT(comm);
    uint32_t rankSize = 0;

    HcclResult ret = HcclGetRankSize(comm, &rankSize);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rankSize, 8);

    Ut_Comm_Destroy(comm);
}