#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "trace.h"

using namespace Hccl;

class TraceTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TraceTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TraceTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TraceTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TraceTest TearDown" << std::endl;
    }
};

TEST_F(TraceTest, save_test)
{
    std::string buffer = "HCCL_TEST";
    auto size = sizeof(buffer);
    MOCKER(TraceSubmit)
        .stubs()
        .with(any())
        .will(returnValue(true));
    Trace trace;
    trace.Save(buffer);
    trace.traceHandle = TRACE_INVALID_HANDLE;
    trace.Save(buffer);
}