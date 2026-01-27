#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#define __HCCL_SAL_GLOBAL_RES_INCLUDE__

#include <sal.h>
#include <hccl/base.h>
#include <hccl/hccl_types.h>

#include "sal.h"
#include "hccl/pub_inc/log.h"
#include "slog.h"
#include "slog_api.h"
#include "dltrace_function.h"
#include "hccl_trace_info.h"

using namespace std;
using namespace hccl;
class SalTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "SalTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "SalTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

TEST_F(SalTest, st_ip_addr_check)
{
    MOCKER(inet_ntop)
    .stubs()
    .will(returnValue((char const*)NULL));

    HcclInAddr ipv4;
    ipv4.addr.s_addr = 666;

    HcclInAddr ipv6;
    ipv6.addr6.s6_addr32[0] = 6;
    ipv6.addr6.s6_addr32[1] = 6;
    ipv6.addr6.s6_addr32[2] = 6;
    ipv6.addr6.s6_addr32[3] = 6;

    HcclIpAddress ip_addr(AF_INET, ipv4);
    EXPECT_EQ(ip_addr.IsInvalid(), false);
    HcclIpAddress ipv6_addr(AF_INET6, ipv6);
    EXPECT_EQ(ipv6_addr.IsInvalid(), false);

    string ipStr = "::::";
    HcclIpAddress ip;
    HcclResult ret = ip.SetReadableAddress(ipStr);
    EXPECT_EQ(ret, HCCL_E_PARA);
    GlobalMockObject::verify();
}

TEST_F(SalTest, st_SalStrToLonglong_error)
{
    HcclResult ret = HCCL_SUCCESS;
    std::string str = "";
    s64 val = 0;
    ret = SalStrToLonglong(str, 10, val);
    EXPECT_EQ(ret, HCCL_E_PARA);
    str = "023";
    ret = SalStrToLonglong(str, 10, val);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    str = "9223372036854775808";
    ret = SalStrToLonglong(str, 10, val);
    EXPECT_EQ(ret, HCCL_E_PARA);
    str = "Something went wrong.";
    ret = SalStrToLonglong(str.c_str(), 10, val);
    EXPECT_EQ(ret, HCCL_E_PARA);
    str = "123";
    ret = SalStrToLonglong(str, 10, val);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(SalTest, st_atrace_error_test)
{
    DlTraceFunction::GetInstance().DlTraceFunctionInit();
    MOCKER(AtraceCreateWithAttr)
    .stubs()
    .will(returnValue(TRACE_INVALID_HANDLE));

    HcclTraceInfo atrace;
    std::string hccl = "HCCL";
    HcclResult ret = atrace.Init(hccl);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string log = "TEST LOG";
    ret = atrace.SaveTraceInfo(log, AtraceOption::Opbasekey);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}