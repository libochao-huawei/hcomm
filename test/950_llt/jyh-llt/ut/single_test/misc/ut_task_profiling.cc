#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#define __HCCL_SAL_GLOBAL_RES_INCLUDE__

#include "sal.h"     //FIXME!! why include private .h files //FIXME!! why include private .h files
#include "llt_hccl_stub_pub.h"
#include "task_profiling.h"
#include "command_handle.h"
#include "profiling_manager.h"

#include <semaphore.h>
#include <sys/time.h> /* 获取时间 */
#include <sys/mman.h>
#include <fcntl.h>
#include <securec.h>
#include <dirent.h>

using namespace std;
using namespace hccl;

class TaskProfilingTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TaskProfilingTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TaskProfilingTest TearDown" << std::endl;
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

TEST_F(TaskProfilingTest, ut_host_nic_bandWidth)
{
    TaskProfiling taskProfilingTest(0, 0);
    std::chrono::microseconds duration(1000);
    std::string tag = "test_tag";
    TaskParaHost tmp(1, 1, 100, duration, tag);
    taskProfilingTest.SaveToLog(tmp);
    u32 streamID=1;
    u32 taskID = 1;
    taskProfilingTest.Save(streamID, taskID);
}

TEST_F(TaskProfilingTest, ut_test_aiv_profiling)
{
    TaskProfiling taskProfilingTest(0, 0);
    TaskParaAiv paraAiv;
    u32 streamID=1;
    u32 taskID = 1;
    taskProfilingTest.Save(streamID, taskID, paraAiv);
}