#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#ifndef private
#define private public
#define protected public
#endif
#include "zero_copy_address_mgr.h"
#undef private
#undef protected

using namespace std;
using namespace hccl;

class Zero_Copy_Device_ST : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "Zero_Copy_Device_ST SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "Zero_Copy_Device_ST TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(Zero_Copy_Device_ST, ZeroCopyDeviceTest) {
    ZeroCopyAddressMgr addressMrg;
    addressMrg.InitRingBuffer();
    ZeroCopyRingBufferItem item;
    addressMrg.PushOne(item);
}
