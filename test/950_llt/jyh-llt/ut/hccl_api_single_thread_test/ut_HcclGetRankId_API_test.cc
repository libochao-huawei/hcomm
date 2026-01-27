#include "hccl_api_base_test.h"

class HcclGetRankIdTest : public BaseInit {
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

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_WhenCommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    Ut_Device_Set(0);
    HcclComm comm = nullptr;
    uint32_t rankId = 0;

    HcclResult ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_WhenRankIdIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    int devId = 0;
    int rankId = 0;
    HcclComm comm = nullptr;
    Ut_Comm_Create(comm , devId, rankTableFileName, rankId);
    uint32_t *prankId = nullptr;

    HcclResult ret = HcclGetRankId(comm, prankId);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclGetRankIdTest, Ut_HcclGetRankId_When_1Server2Rank_Expect_ReturnHCCL_SUCCESS)
{
    UT_USE_RANK_TABLE_910_1SERVER_2RANK;
    int devId = 0;
    int _rankId = 0;
    HcclComm comm = nullptr;
    Ut_Comm_Create(comm , devId, rankTableFileName, _rankId);
    uint32_t rankId = 0;

    HcclResult ret = HcclGetRankId(comm, &rankId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(_rankId, rankId);

    Ut_Comm_Destroy(comm);
}