#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "hccp_tlv_hdc_manager.h"
#include "orion_adapter_tsd.h"
#include "orion_adapter_rts.h"
#include "socket_handle_manager.h"
#include "rdma_handle_manager.h"
#include "hccp_tlv.h"

using namespace testing;
using namespace Hccl;
// Test suite class
class HccpTlvHdcManagerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpTlvHdcManagerTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HccpTlvHdcManagerTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HccpTlvHdcManagerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HccpTlvHdcManagerTest TearDown" << std::endl;
    }
};

TEST_F(HccpTlvHdcManagerTest, should_successfully_init_HccpTlvHdcManager) {
    u32 deviceLogicId = 0;
    MOCKER(HrtGetDevicePhyIdByIndex)
        .stubs()
        .with(any())
        .will(returnValue(0));
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    HccpTlvHdcManager::GetInstance().Init(deviceLogicId);
}