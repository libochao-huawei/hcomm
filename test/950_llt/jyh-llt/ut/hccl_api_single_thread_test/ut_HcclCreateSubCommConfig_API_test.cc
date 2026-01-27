#include "hccl_api_base_test.h"

class HcclCreateSubCommConfigTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
        setenv("HCCL_DEBUG_CONFIG", "alg", 1);
        UT_USE_RANK_TABLE_910_1SERVER_1RANK;
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
        remove(rankTableFileName);
    }
};

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_RankNumIsZero_Expect_ReturnIsHCCL_E_PARA) {
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 0;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 2;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_PARA);
    
    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_RankIdsIsNull_Expect_ReturnIsHCCL_E_PARA) {
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t* rankIds = nullptr;
    u32 subCommId = 2;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_subCommIdIsInvaild_Expect_ReturnIsHCCL_E_PARA) {
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 0xFFFFFFFF;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_RankIdsIsNullAndSubCommIdIsInvaild_Expect_ReturnIsHCCL_SUCCESS) {
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t *rankIds = nullptr;
    u32 subCommId = 0xFFFFFFFF;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_CommConfigIsNull_Expect_ReturnIsHCCL_E_PTR) {
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 0;
    u32 subCommRankId = 0;
    HcclCommConfig *pCommConfig = nullptr;
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, pCommConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_SubCommRankId_GE_RankNum_Expect_ReturnIsHCCL_E_PARA)
{
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[2] = {0, 1};
    u32 subCommId = 2;
    u32 subCommRankId = 2;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}


TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_CommIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 2;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void **pSubComm = nullptr;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, pSubComm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_Destroy(comm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_GetDeviceError_Expect_ReturnIsHCCL_E_RUNTIME)
{
    UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 2;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;
    MOCKER(aclrtGetDevice)
        .stubs()
        .will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));
    
    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);

    GlobalMockObject::verify();
    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}

TEST_F(HcclCreateSubCommConfigTest, Ut_HcclCreateSubCommConfig_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
        UT_COMM_CREATE_DEFAULT(comm);
    int rankNum = 1;
    uint32_t rankIds[1] = {0};
    u32 subCommId = 2;
    u32 subCommRankId = 0;
    HcclCommConfig commConfig;
    HcclCommConfigInit(&commConfig);
    void *subComm;

    HcclResult ret = HcclCreateSubCommConfig(&comm, rankNum, rankIds, subCommId, subCommRankId, &commConfig, &subComm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_Destroy(comm);
    Ut_Comm_CheckRet_Then_Destroy(ret,subComm);
}