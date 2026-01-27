#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#define __HCCL_SAL_GLOBAL_RES_INCLUDE__

#include "sal.h" //FIXME!! why include private .h files //FIXME!! why include private .h files
#include "llt_hccl_stub_pub.h"
#include "hccl/pub_inc/log.h"
#include "slog.h"
#include "slog_api.h"
#include <semaphore.h>
#include <sys/time.h>  /* 获取时间 */
#include <sys/mman.h>
#include <fcntl.h>
#include <securec.h>
#include <dirent.h>
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
    //    (void)rt_stop_sequence_thread();
    }
    static void TearDownTestCase()
    {
     //  (void)rt_start_sequence_thread();
        std::cout << "SalTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

/* SAL 异常对象测试用例 */
// TEST_F(SalTest, ut_sal_inet_pton)
// {
//     s32 ret = HCCL_SUCCESS;

//     MOCKER(inet_pton)
//     .expects(atMost(1))
//     .will(returnValue(-1));

//     ret = SalInetPton("123456",  NULL);
//     EXPECT_EQ(ret, HCCL_E_PTR);

//     GlobalMockObject::verify();
// }

/* 内存UT测试用例 */
TEST_F(SalTest, ut_sal_mem_test)
{
    char* src_ptr = NULL;
    char* dst_ptr = NULL;
    u32   src_size    = 100;
    char  src_value   = 1;
    u32   dst_size    = 100;
    char  dst_value   = 2;


    s32 ret = SAL_OK;

    /* 遍历源内存空间 小于/等于/大于 目的内存空间的场景 */
    for (src_size = (dst_size - 1); src_size <= (dst_size + 1); src_size ++)
    {
        HCCL_DEBUG("ut_sal_mem_test: src_size[%d], src_value[%d], dst_size[%d], dst_value[%d]",
                  src_size, src_value, dst_size, dst_value);

        /* 申请源内存空间 */
        src_ptr = (char*)sal_malloc(src_size);
        EXPECT_NE((long)src_ptr, NULL);

        if (NULL == src_ptr)
        { return; }

        /* 申请目的内存空间 */
        dst_ptr = (char*)sal_malloc(dst_size);
        EXPECT_NE((long)dst_ptr, NULL);

        if (NULL == dst_ptr)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 源内存初始化 */
        ret = sal_memset(src_ptr, src_size, src_value, src_size);
        EXPECT_EQ(ret, SAL_OK);

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 校验源内存初始化结果 */
        EXPECT_EQ(*src_ptr, src_value);
        EXPECT_EQ(*(src_ptr + src_size - 1), src_value);

        /* 目的内存初始化 */
        ret = sal_memset(dst_ptr, dst_size, dst_value, dst_size);
        EXPECT_EQ(ret, SAL_OK);

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 校验目的内存初始化结果 */
        EXPECT_EQ(*dst_ptr, dst_value);
        EXPECT_EQ(*(dst_ptr + dst_size - 1), dst_value);

        /* 源内存拷贝到目的内存 */
        ret = sal_memcpy(dst_ptr, dst_size, src_ptr, src_size);

        /* 源内存大于目的内存时预期会返回错误 */
        if (src_size <= dst_size )
        { EXPECT_EQ(ret, SAL_OK); }
        else
        { EXPECT_NE(ret, SAL_OK); }

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 校验内存拷贝结果 */
        EXPECT_EQ(*dst_ptr, src_value);
        EXPECT_EQ(*(dst_ptr + (src_size >= dst_size ? dst_size : src_size) - 1), src_value);

        HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
        HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);

        /* 替换源内存内容 */
        src_value += 2;
    }

}
/* 字符串对比测试用例 */
TEST_F(SalTest, ut_sal_str_cmp_test)
{

    char  str_a[]   = "aaaa";
    char  str_b[]   = "bbbb";
    char  str_aa[]   = "aaaaa";
    s32 ret = SAL_OK;

    /* 比较字符串内容 */
    ret = strcmp(str_a, str_b);
    EXPECT_LT(ret, 0);
    HCCL_DEBUG("[%s] vs [%s]: ret[%d]", str_a, str_b, ret);

    ret = strcmp(str_b, str_a);
    EXPECT_GT(ret, 0);
    HCCL_DEBUG("[%s] vs [%s]: ret[%d]", str_b, str_a, ret);

    ret = strcmp(str_a, str_aa);
    EXPECT_LT(ret, 0);
    HCCL_DEBUG("[%s] vs [%s]: ret[%d]", str_a, str_aa, ret);

    ret = strcmp(str_aa, str_a);
    EXPECT_GT(ret, 0);
    HCCL_DEBUG("[%s] vs [%s]: ret[%d]", str_aa, str_a, ret);

    ret = strcmp(str_aa, str_aa);
    EXPECT_EQ(ret, 0);
    HCCL_DEBUG("[%s] vs [%s]: ret[%d]", str_aa, str_aa, ret);

}

/* 路径信息测试用例 */
TEST_F(SalTest, ut_sal_is_dir_exit_test)
{
    std::string nunFile;
    s32 status = -1;
    HcclResult ret = SalIsDirExist(nunFile, status);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

/* 字符串综合测试用例 */
TEST_F(SalTest, ut_sal_str_test)
{
    char* src_ptr = NULL;
    char* dst_ptr = NULL;
    u32   src_size    = 10;
    char  src_value   = 'A';
    u32   dst_size    = 10;
    char  dst_value   = 'B';

    u32   str_len     = 0;



    s32 ret = SAL_OK;

    for (src_size = (dst_size - 1); src_size <= (dst_size + 1); src_size ++)
    {
        HCCL_DEBUG("ut_sal_str_test: src_size[%d], src_value[\'%c\'], dst_size[%d], dst_value[\'%c\']",
                  src_size, src_value, dst_size, dst_value);

        /* 申请源字符串 */
        src_ptr = (char*)sal_malloc(src_size);
        EXPECT_NE((long)src_ptr, NULL);

        if (NULL == src_ptr)
        { return; }

        /* 申请目的字符串 */
        dst_ptr = (char*)sal_malloc(dst_size);
        EXPECT_NE((long)dst_ptr, NULL);

        if (NULL == dst_ptr)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 源字符串初始化 */
        ret = sal_memset(src_ptr, src_size, src_value, src_size);
        EXPECT_EQ(ret, SAL_OK);

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 添加字符串结束符 */
        src_ptr[src_size - 1] = '\0';

        /* 校验字符串初始化结果 */
        EXPECT_EQ(*src_ptr, src_value);
        EXPECT_EQ(*(src_ptr + src_size - 2), src_value);

        /* 校验源字符串长度 */
        str_len = SalStrLen(src_ptr);
        EXPECT_EQ(str_len, src_size - 1);

        /* 目的字符串初始化 */
        ret = sal_memset(dst_ptr, dst_size, dst_value, dst_size);
        EXPECT_EQ(ret, SAL_OK);

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        /* 添加字符串结束符 */
        dst_ptr[dst_size - 1] = '\0';

        /* 校验字符串初始化结果 */
        EXPECT_EQ(*dst_ptr, dst_value);
        EXPECT_EQ(*(dst_ptr + dst_size - 2), dst_value);

        /* 校验目的字符串长度 */
        str_len = SalStrLen(dst_ptr);
        EXPECT_EQ(str_len, dst_size - 1);

        /* 比较字符串内容 */
        ret = strcmp(src_ptr, dst_ptr);
        EXPECT_NE(ret, 0);

        /* 字符串拷贝 */
        ret = sal_strncpy(dst_ptr, dst_size, src_ptr, src_size);

        if (dst_size >= src_size)
        { EXPECT_EQ(ret, SAL_OK); }
        else
        { EXPECT_NE(ret, SAL_OK); }

        if (SAL_OK != ret)
        {
            HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
            HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);
            return;
        }

        if (dst_size == src_size)
        {
            /* 比较字符串内容 */
            ret = strcmp(src_ptr, dst_ptr);
            EXPECT_EQ(ret, 0);
        }

        /* 校验字符串拷贝结果 */
        EXPECT_EQ(*dst_ptr, src_value);
        EXPECT_EQ(*(dst_ptr + (src_size >= dst_size ? dst_size : src_size) - 2), src_value);

        HCCL_RELEASE_PTR_AND_SET_NULL(src_ptr);
        HCCL_RELEASE_PTR_AND_SET_NULL(dst_ptr);

        /* 替换源字符串内容 */
        src_value += 2;
    }

}

#if 1
int test_mock_clock_gettime(clockid_t clk_id, struct timespec* tp)
{
    tp->tv_nsec = 0xFFFFFFFF;
    tp->tv_sec = 1;
    return SAL_OK;
}

TEST_F(SalTest, ut_sal_compute_timeout_error_0)
{
    HcclResult ret = HCCL_SUCCESS;
    struct timespec ts;

    MOCKER(clock_gettime)
    .expects(once())
    .will(invoke(test_mock_clock_gettime));

    ret = sal_compute_timeout(&ts, 999);
    EXPECT_EQ(HCCL_SUCCESS, ret);

    GlobalMockObject::verify();
}
#endif

TEST_F(SalTest, ut_sal_sleep_error)
{
    s32 ret = 0;
    MOCKER(sleep)
    .expects(once())
    .will(returnValue((s32)1));
    SalSleep(1);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(SalTest, ut_sal_usleep_error)
{
    s32 ret = 0;
    /* mock usleep失败 */
    MOCKER(usleep)
    .expects(once())
    .will(returnValue((s32)1));
    SaluSleep(SAL_MILLISECOND_USEC * 20);
    GlobalMockObject::verify();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

//TEST_F(SalTest, ut_sal_getenv)
//{
//    std::string str;
//    str = SalGetEnv(NULL);
//    EXPECT_EQ(str, "EmptyString");
//}

/*sal_malloc异常用例*/
TEST_F(SalTest, sal_malloc_error)
{
    void* ret;

    ret = sal_malloc(0);
    EXPECT_EQ((long)ret, NULL);
}

// 代码覆盖率sal_get_unique_id函数异常测试
TEST_F(SalTest, ut_sal_get_unique_id_error)
{
    s32 ret = SAL_OK;
    char rootInfo [SAL_UNIQUE_ID_BYTES];

    ret = SalGetUniqueId(NULL);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(SalTest, ut_SalStrToLonglong_error)
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

#if 1
TEST_F(SalTest, ut_sal_log_printf_error_0)
{
    HCCL_LOG_PRINT(6 | RUN_LOG_MASK, DLOG_INFO, "test \\n info \n");
    HCCL_ERROR_LOG_PRINT("test");
    HCCL_RUN_LOG_PRINT("test");
}
#endif

TEST_F(SalTest, ut_atrace_error_test)
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