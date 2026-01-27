#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "notify_fixed_value.h"
#include "dev_capability.h"
#include "orion_adapter_rts.h"

using namespace Hccl;

class NotifyFixedValueTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "NotifyFixedValueTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "NotifyFixedValueTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in NotifyFixedValueTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in NotifyFixedValueTest TearDown" << std::endl;
    }
};

TEST_F(NotifyFixedValueTest, notify_fixed_value_get_addr_and_size)
{
    // Given
    MOCKER(HrtGetDeviceType).
        stubs().
        will(returnValue((DevType)DevType::DEV_TYPE_910_95));
 
    void* fakeAddr = nullptr;
    MOCKER(HrtMalloc)
        .stubs()
        .with(any(), any())
        .will(returnValue(fakeAddr));
 
    NotifyFixedValue notifyFixedValue;
    // when
    u64 addrRes = notifyFixedValue.GetAddr();
    u32 sizeRes = notifyFixedValue.GetSize();

    // then
    EXPECT_EQ(0, addrRes);
    EXPECT_EQ(8, sizeRes);
}