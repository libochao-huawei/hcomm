#define private public
#define protected public
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <stdexcept>
#include <string>
#include "profiling_reporter_lite.h"
#include "communicator_impl_lite.h"
#undef private
#undef protected

using namespace Hccl;


class ProfilingReporterLiteTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "ProfilingReporterLiteTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "ProfilingReporterLiteTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in ProfilingReporterLiteTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in ProfilingReporterLiteTest TearDown" << std::endl;
    }
};


// 测试ProfilingReporterLite类接口
TEST_F(ProfilingReporterLiteTest, Call_profilingReporterLite_api_test)
{
    GlobalMirrorTasks &globalMirrorTasks = GlobalMirrorTasks::Instance();
    MirrorTaskManager mirrorTaskManager(0, &globalMirrorTasks, 0);
     // 初始化TaskParam
    TaskParam taskParam = {.taskType = TaskParamType::TASK_NOTIFY_RECORD,
        .beginTime = 0,
        .endTime = 0,
        .taskPara = {.Notify = {.notifyID = 123, .value = 456}}};
    // 初始化dfxOpInfo
    std::shared_ptr<DfxOpInfo> dfxOpInfo = std::make_shared<DfxOpInfo>();
    CollOperator op;
    op.opType = OpType::ALLREDUCE;
    op.staticAddr = false;
    dfxOpInfo->op_ = op;
    CommunicatorImplLite* comm = new CommunicatorImplLite(0);
    dfxOpInfo->comm_ = comm;
    mirrorTaskManager.SetCurrDfxOpInfo(dfxOpInfo);
    std::shared_ptr<TaskInfo> taskInfo1 = std::make_shared<TaskInfo>(3, 0, 0, taskParam, dfxOpInfo);
    std::shared_ptr<TaskInfo> taskInfo2 = std::make_shared<TaskInfo>(0, 1, 1, taskParam, dfxOpInfo);
    mirrorTaskManager.AddTaskInfo(taskInfo1);
    mirrorTaskManager.AddTaskInfo(taskInfo2);

    ProfilingReporterLite profilingReporter(&mirrorTaskManager, &ProfilingHandlerLite::GetInstance());
    profilingReporter.Init();
    profilingReporter.ReportAllTasks();
    profilingReporter.UpdateProfStat();
    delete comm;
}