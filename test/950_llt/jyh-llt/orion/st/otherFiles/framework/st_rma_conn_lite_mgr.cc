#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "null_ptr_exception.h"
#define private public
#define protected public
#include "rma_conn_manager.h"
#include "dev_ub_connection.h"
#include "communicator_impl.h"
#undef private
#undef protected
#include "internal_exception.h"
using namespace Hccl;

class RmaConnLiteMgrTest : public testing::Test {
protected:
    static void SetUpTestCase() {
        std::cout << "RmaConnLiteMgrTest SetUP" << std::endl;
    }
 
    static void TearDownTestCase() {
        std::cout << "RmaConnLiteMgrTest TearDown" << std::endl;
    }
 
    virtual void SetUp() {
        MOCKER(HrtGetDevice).stubs().will(returnValue(0));
        std::cout << "A Test case in RmaConnLiteMgrTest SetUP" << std::endl;
    }
 
    virtual void TearDown () {
        GlobalMockObject::verify();
        std::cout << "A Test case in RmaConnLiteMgrTest TearDown" << std::endl;
    }
};
