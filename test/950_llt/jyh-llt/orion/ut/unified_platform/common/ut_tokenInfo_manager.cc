#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "tokenInfo_manager.h"
#include "orion_adapter_hccp.h"

using namespace Hccl;

// Test fixture for HcclNetDev tests
class HcclTokenInfoManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Test case for GetTokenInfo method
TEST_F(HcclTokenInfoManagerTest, GetTokenInfo) {
    RdmaHandle rdmahandle;
    TokenInfoManager tokenInfoManager(0, rdmahandle);
    std::pair<TokenIdHandle, uint32_t> tokenInfo{1, 1};
    MOCKER(RaUbAllocTokenIdHandle).stubs().will(returnValue(tokenInfo));
    MOCKER(RaUbFreeTokenIdHandle).stubs();

    std::pair<TokenIdHandle, uint32_t> outTokenInfo = tokenInfoManager.GetTokenInfo(BufferKey<uintptr_t, u64>{0,0});
    EXPECT_EQ(tokenInfo, outTokenInfo);
    EXPECT_EQ(tokenInfo, outTokenInfo);

    EXPECT_NO_THROW(tokenInfoManager.Destroy());
}
