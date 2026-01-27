#include "hccl_api_base_test.h"

/*
    
*/

class HcclCommInitClusterInfoTest : public BaseInit {
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

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_RankIdInRankTable_Expect_ReturnIsHCCL_SUCCESS) {
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 0;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_RankIdNotInRankTable_Expect_ReturnIsHCCL_E_PARA) {
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 1;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_clusterInfoFileNotExist_Expect_ReturnIsHCCL_E_INTERNAL) {
    Ut_Device_Set(0);
    const char* rankTableFile = "fake.json";
    u32 rankId = 0;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_clusterInfoFileContentInvaild_Expect_ReturnIsHCCL_E_INTERNAL) {
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    std::ofstream badFile(rankTableFile, std::ios::trunc);
    badFile << "111";
    badFile.close();
    u32 rankId = 0;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_clusterInfoIsNull_Expect_ReturnIsHCCL_E_PTR) {
    Ut_Device_Set(0);
    const char* rankTableFile = nullptr;
    u32 rankId = 0;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_E_PTR);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_GetDeviceError_Expect_ReturnIsHCCL_E_UNAVAIL) {
    MOCKER(aclrtGetDevice)
        .stubs()
        .will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 0;
    HcclComm comm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, &comm);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);

    Ut_Comm_CheckRet_Then_Destroy(ret,comm);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_commIsNull_Expect_ReturnIsHCCL_E_PTR) {
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 0;
    void **pComm = nullptr;

    HcclResult ret = HcclCommInitClusterInfo(rankTableFile, rankId, pComm);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommInitClusterInfoTest, Ut_HcclCommInitClusterInfo_When_MultiInit_Expect_ReturnIsHCCL_E_UNAVAIL) {
    Ut_Device_Set(0);
    const char* rankTableFile = rankTableFileName;
    u32 rankId = 0;
    HcclComm comm1;
    HcclComm comm2;

    HcclResult ret1 = HcclCommInitClusterInfo(rankTableFile, rankId, &comm1);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    HcclResult ret2 = HcclCommInitClusterInfo(rankTableFile, rankId, &comm2);
    EXPECT_EQ(ret2, HCCL_E_UNAVAIL);

    Ut_Comm_CheckRet_Then_Destroy(ret1,comm1);
    Ut_Comm_CheckRet_Then_Destroy(ret2,comm2);
}