#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>

#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>

#include "event_pub.h"
#include "sal.h"


using namespace std;
using namespace hccl;

class EventTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--EventTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--EventTest TearDown--\033[0m" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(EventTest, constructor_00)
{
    aclrtEvent rtevent;
    aclrtCreateEvent(&rtevent);
    Event event = Event::create(rtevent) ;

    EXPECT_EQ(event.ptr(), rtevent);
    aclrtDestroyEvent(rtevent);
}


TEST_F(EventTest, constructor_01)
{
    Event event = Event::alloc();
    EXPECT_NE(event.ptr(), nullptr);
}

TEST_F(EventTest, constructor_02)
{
    Event event_1 = Event::alloc();
    Event event_2 (event_1);

    EXPECT_EQ(event_1.ptr(), event_2.ptr());
}

TEST_F(EventTest, event_alloc_fail)
{
    /*¹¹ÔìrtEventCreateÒì³£*/
    MOCKER(aclrtCreateEvent)
    .expects(atMost(1))
    .will(returnValue(1));

    Event event = Event::alloc();

    GlobalMockObject::verify();

    EXPECT_EQ(event.ptr(), nullptr);
}


