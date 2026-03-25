#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <cstdlib>
#include <string>



class TopoAddrInfoTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TopoAddrInfo tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TopoAddrInfo tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TopoAddrInfoTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TopoAddrInfoTest TearDown" << std::endl;
    }
};

TEST_F(TopoAddrInfoTest, Ut_load_dcmi)
{
    // when
    MOCKER(dlopen).stubs().with(any()).will(returnValue(1));
    MOCKER(dlsym).stubs().with(any()).will(returnValue(0));

    // check
    EXPECT_EQ(load_dcmi(), -1);
}