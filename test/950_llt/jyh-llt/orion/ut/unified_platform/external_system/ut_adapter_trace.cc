#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "orion_adapter_trace.h"
#include "atrace_api.h"

using namespace Hccl;

class AdapterTraceTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AdapterTraceTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AdapterTraceTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AdapterTraceTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AdapterTraceTest TearDown" << std::endl;
    }
};

TEST_F(AdapterTraceTest, trace_create_test)
{
    MOCKER(AtraceCreateWithAttr).stubs().will(returnValue(0));
    TraHandle handle;
    TraceCreate("HCCL");

}

TEST_F(AdapterTraceTest, Ut_TraceCreate_Fail_When_AtraceCreateWithAttr_Return_TRACE_INVALID_HANDLE)
{
    MOCKER(AtraceCreateWithAttr).stubs().will(returnValue(TRACE_INVALID_HANDLE));
    TraHandle handle;
    EXPECT_EQ(TraceCreate("HCCL"), -1);
}

TEST_F(AdapterTraceTest, traceSubmit_success_test)
{
    MOCKER(AtraceSubmit).stubs().will(returnValue(0));
    TraHandle handle = 0;
    std::string buffer = "HCCL_TEST";
    u32 size = sizeof(buffer);
    const unsigned char *startPos = (const unsigned char *)(buffer.c_str());
    TraceSubmit(handle, startPos, size);
}

TEST_F(AdapterTraceTest, traceDestroy_test)
{
    MOCKER(AtraceDestroy).stubs();
}