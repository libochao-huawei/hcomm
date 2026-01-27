#include "hccl_api_base_test.h"

class HcclGetConfigTest : public BaseInit {
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

TEST_F(HcclGetConfigTest, Ut_HcclGetConfig_WhenConfigValueIsNull_Expect_ReturnIsHCCL_E_PTR)
{
    HcclResult ret = HcclGetConfig(HCCL_DETERMINISTIC, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcclGetConfigTest, Ut_HcclGetConfig_When_Normal_Expect_ReturnValueIsVaild)
{
    union HcclConfigValue hcclConfigValue;
    hcclConfigValue.value = 1;
    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    union HcclConfigValue hcclConfigValueRet;

    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclConfigValue.value, hcclConfigValueRet.value);

    hcclConfigValue.value = 0;
    ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclConfigValue.value, hcclConfigValueRet.value);
}

TEST_F(HcclGetConfigTest, Ut_HcclGetConfig_When_SetFailed_Expect_ReturnValueIsDETERMINISTIC_DISABLE)
{
    union HcclConfigValue hcclConfigValue;
    union HcclConfigValue hcclConfigValueRet;
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(deviceType)).will(returnValue(HCCL_SUCCESS));
    hcclConfigValue.value = 2;
    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    
    ret = HcclGetConfig(HCCL_DETERMINISTIC, &hcclConfigValueRet);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(hcclConfigValueRet.value, DETERMINISTIC_DISABLE);
}