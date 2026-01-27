#include "hccl_api_base_test.h"

class HcclGetCommConfigCapabilityTest : public testing::Test {
public:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HcclGetCommConfigCapabilityTest, Ut_HcclGetCommConfigCapability_When_Normal_Expect_ReturnIsNormal)
{
    uint32_t ret = HcclGetCommConfigCapability();
    EXPECT_EQ(ret, HCCL_COMM_CONFIG_RESERVED);
}
