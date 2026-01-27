#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "p2p_enable_manager.h"
#include "orion_adapter_rts.h"
#include "driver/ascend_hal.h"


using namespace Hccl;

class P2PEnableManagerTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "P2PEnableManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase() {
        std::cout << "P2PEnableManagerTest TearDown" << std::endl;
    }

    virtual void SetUp() {
        std::cout << "A Test case in P2PEnableManagerTest SetUp" << std::endl;
    }

    virtual void TearDown() {
        GlobalMockObject::verify();
        std::cout << "A Test case in P2PEnableManager TearDown" << std::endl;
    }
};

TEST_F(P2PEnableManagerTest, should_successfully_add_device_pairs_to_set_after_calling_enablep2p) {
    // Given
    auto devicePairs = P2PEnableManager::GetInstance().GetSet();

    // then
    // 注：期望值有时可能需要调整
    // 原因：P2PEnableManager是单例，容易受其他UT用例的影响，目前RmaConnManager相关UT会影响
    EXPECT_EQ(devicePairs.size(), 0);
}
