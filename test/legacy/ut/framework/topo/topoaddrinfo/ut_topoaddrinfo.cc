#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <cstdlib>
#include <string>
#include "topo_addr_info.h"
#include "hal.h"

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

TEST_F(TopoAddrInfoTest, Ut_get_mainbaord_id)
{
    unsigned int mainboard_id = 0;
    int ret = hal_get_mainboard_id(0, &mainboard_id);
    // check
    EXPECT_EQ(mainboard_id, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(TopoAddrInfoTest, Ut_get_mainbaord_id)
{
    MOCKER(hal_get_mainboard_id).stubs().will(returnValue(devType));
    unsigned int mainboard_id = 0;
    int ret = hal_get_mainboard_id(0, &mainboard_id);
    // check
    EXPECT_EQ(mainboard_id, 0);
    EXPECT_EQ(ret, -1);
}