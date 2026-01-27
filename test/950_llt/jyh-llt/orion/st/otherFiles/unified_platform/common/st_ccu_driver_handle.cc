#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#define private public
#define protected public
#include "ccu_driver_handle.h"
#include "hccp_tlv_hdc_manager.h"
#include "orion_adapter_tsd.h"
#include "orion_adapter_rts.h"
#include "orion_adapter_hccp.h"
#include "ccu_component.h"
#include "ccu_context_mgr_imp.h"
#include "ccu_res_batch_allocator.h"

#undef private
#undef protected

using namespace testing;
using namespace Hccl;
// Test suite class
class CcuDriverHandleStTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "CcuDriverHandleStTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "CcuDriverHandleStTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in CcuDriverHandleStTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in CcuDriverHandleStTest TearDown" << std::endl;
    }
};

TEST_F(CcuDriverHandleStTest, should_successfully_init_CcuDriverHandle) {
    u32 deviceLogicId = 0;
    MOCKER(RaTlvRequest).stubs().will(returnValue(0));
    void* mockTlvHandle = reinterpret_cast<void*>(0x1234);
    MOCKER_CPP(&HccpTlvHdcManager::GetTlvHandle).stubs().will(returnValue(mockTlvHandle));
    MOCKER_CPP(&CcuComponent::Init).stubs();
    MOCKER_CPP(&CcuResBatchAllocator::Init).stubs();
    MOCKER_CPP(&CtxMgrImp::Init).stubs();
    CcuDriverHandle manager(deviceLogicId);
}
