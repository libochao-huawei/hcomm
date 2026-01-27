#include "hccl_api_base_test.h"

class HcclCommInitRootInfoTest : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_nRanksIsZero_Expect_ReturnIsHCCL_E_PARA)
{
    HcclRootInfo id;
    HcclResult ret = hrtSetDevice(0);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm comm;
    ret = HcclCommInitRootInfo(0, &id, 0, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_GetDeviceError_Expect_ReturnIsHCCL_E_RUNTIME)
{
    HcclRootInfo id;
    HcclResult ret;
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    MOCKER(aclrtGetDevice)
        .stubs()
        .will(returnValue(ACL_ERROR_RT_CONTEXT_NULL));
    HcclComm comm;
    ret = HcclCommInitRootInfo(1, &id, 0, &comm);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_RootInfoIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclResult ret = hrtSetDevice(0);
    HcclComm comm;
    ret = HcclCommInitRootInfo(1, nullptr, 0, &comm);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_RankGreaterThannRanks_Expect_ReturnIsHCCL_E_PARA)
{
    HcclRootInfo id;
    HcclResult ret = hrtSetDevice(0);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm comm;
    ret = HcclCommInitRootInfo(10, &id, 10, &comm);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_RankGreaterThannRanks_Expect_ReturnIsHCCL_E_PTR)
{
    HcclRootInfo id;
    HcclResult ret = hrtSetDevice(0);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    HcclComm comm;
    ret = HcclCommInitRootInfo(1, &id, 0, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_nRanksIsLarge_Expect_ReturnIsHCCL_E_INTERNAL)
{
    HcclRootInfo id;
    HcclResult ret = hrtSetDevice(0);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm comm;
    ret = HcclCommInitRootInfo(128*1024, &id, 0, &comm);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(HcclCommInitRootInfoTest, Ut_HcclCommInitRootInfoTest_When_nRanksIsOne_Expect_ReturnIsHCCL_SUCCESS)
{
    HcclRootInfo id;
    HcclResult ret = hrtSetDevice(0);
    ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    HcclComm comm;
    ret = HcclCommInitRootInfo(1, &id, 0, &comm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
