#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "dispatcher_pub.h"
#include "dispatcher_graph_pub.h"
#include "dispatcher_virtural_pub.h"
#include "dispatcher_pub.h"
#include "profiler_manager.h"
#include "alg_profiling.h"

using namespace std;
using namespace hccl;

class DispatcherCommonTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DispatcherCommonTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "DispatcherCommonTest TearDown" << std::endl;
    }
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

TEST_F(DispatcherCommonTest, ut_profiler_manager_task_aiv_profiler) {
    hccl::ProfilerManager profilerManager(0, 0, 0);
    TaskParaAiv taskParaAiv;
    TaskParaGeneral taskPara;
    taskPara.aiv = taskParaAiv;
 
    MOCKER(GetIfProfile)
    .stubs()
    .will(returnValue(true));
 
    profilerManager.TaskAivProfilerHandle(&taskPara, sizeof(struct TaskParaGeneral));
}