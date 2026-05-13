#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>


#include "../../../../../src/framework/next/comms/ccu/ccu_device/ccu_res_batch_allocator.h"
#include "ccu_dev_mgr_pub.h"
#include "ccu_comp.h"
#include "hccl_common.h"

using namespace hcomm;

class AppendToContextTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    static void TearDownTestCase() {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void SetUp() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }
    void TearDown() override {
        mockcpp::GlobalMockObject::verify();
        mockcpp::GlobalMockObject::reset();
    }

};
