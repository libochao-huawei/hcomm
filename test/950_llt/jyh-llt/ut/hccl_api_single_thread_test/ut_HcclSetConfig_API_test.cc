#include "hccl_api_base_test.h"

class HcclSetConfigTest : public BaseInit {
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

TEST_F(HcclSetConfigTest, Ut_HcclSetConfig_When_ConfigValueIsNotInDeterministicEnableLevel_Expect_ReturnIsHCCL_E_PARA)
{
    HcclConfigValue value;
    value.value = 3;

    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC,value);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcclSetConfigTest, Ut_HcclSetConfig_When_ConfigValueIsSTRICTButDevTypeIsNot910B_Expect_ReturnIsHCCL_E_NOT_SUPPORT)
{
    HcclConfigValue value;
    value.value = DETERMINISTIC_STRICT;
    DevType deviceType = DevType::DEV_TYPE_910_93;
    MOCKER(hrtGetDeviceType)
        .stubs()
        .with(outBound(deviceType))
        .will(returnValue(HCCL_SUCCESS));
    
    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, value);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcclSetConfigTest, Ut_HcclSetConfigTest_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    union HcclConfigValue hcclConfigValue;
    hcclConfigValue.value = 1;

    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hcclConfigValue.value = 0;

    ret = HcclSetConfig(HCCL_DETERMINISTIC, hcclConfigValue);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcclSetConfigTest, Ut_HcclSetConfig_When_SetEnvHCCL_DETERMINISTIC_Expect_ReturnIsHCCL_SUCCESS)
{
    setenv("HCCL_DETERMINISTIC", "true", 1);
    HcclConfigValue value;
    value.value = 3;

    HcclResult ret = HcclSetConfig(HCCL_DETERMINISTIC, value);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    unsetenv("HCCL_DETERMINISTIC");
}
