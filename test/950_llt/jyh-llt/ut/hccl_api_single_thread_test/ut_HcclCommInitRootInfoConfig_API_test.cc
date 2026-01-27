#include "hccl_api_base_test.h"

class HcclCommInitRootInfoConfigTest : public BaseInit {
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

TEST_F(HcclCommInitRootInfoConfigTest, ut_HcclCommInitRootInfoConfig_default)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(deviceType))
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 200 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 200 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 0);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommInitRootInfoConfigTest, ut_HcclCommInitRootInfoConfig_user_config)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(deviceType))
        .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1);
    EXPECT_EQ(pComm->communicator_->SetDeterministicConfig(config.hcclDeterministic), HCCL_SUCCESS);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclCommInitRootInfoConfigTest, ut_HcclCommInitRootInfoConfig_user_config_world_group)
{
    HcclRootInfo id;

    DevType deviceType = DevType::DEV_TYPE_910B;
    MOCKER(hrtGetDeviceType)
    .stubs()
    .with(outBound(deviceType))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = HcclGetRootInfo(&id);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HcclCommConfig config;
    HcclCommConfigInit(&config);

    config.hcclBufferSize = 300;
    config.hcclDeterministic = 1;

    strcpy_s(config.hcclCommName, 128, HCCL_WORLD_GROUP);

    HcclComm newcomm;
    ret = HcclCommInitRootInfoConfig(1, &id, 0, &config, &newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclComm *pComm = static_cast<hcclComm *>(newcomm);
    EXPECT_EQ(pComm->GetConfigInCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->GetConfigOutCCLbufferSize(), 300 * 1024 * 1024);
    EXPECT_EQ(pComm->communicator_->GetDeterministicConfig(), 1); 
    EXPECT_EQ(pComm->GetIdentifier(), HCCL_WORLD_GROUP);

    ret = HcclCommDestroy(newcomm);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}