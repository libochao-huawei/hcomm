#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mockcpp/mokc.h>

#include "mc2_global_mirror_tasks.h"
#include "internal_exception.h"

using namespace std;
using namespace Hccl;

class MC2GlobalMirrorTasksTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MC2GlobalMirrorTasksTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "MC2GlobalMirrorTasksTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in MC2GlobalMirrorTasksTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        MC2GlobalMirrorTasks::GetInstance().Clear();
        std::cout << "A Test case in MC2GlobalMirrorTasksTest TearDown" << std::endl;
    }

    shared_ptr<TaskInfo> InitTaskInfo(u32 streamId = 0, u32 taskId = 0, u32 remoteRank = 0)
    {
        TaskParam taskParam{};
        shared_ptr<DfxOpInfo> dfxOpInfo = make_shared<DfxOpInfo>();
        return make_shared<TaskInfo>(streamId, taskId, remoteRank, taskParam, dfxOpInfo);
    }
};

TEST_F(MC2GlobalMirrorTasksTest, test_AddTaskInfo)
{
    shared_ptr<TaskInfo> taskInfo = InitTaskInfo();
    EXPECT_THROW(MC2GlobalMirrorTasks::GetInstance().AddTaskInfo(100, taskInfo), InternalException);

    EXPECT_THROW(MC2GlobalMirrorTasks::GetInstance().AddTaskInfo(1, nullptr), InternalException);

    EXPECT_NO_THROW(MC2GlobalMirrorTasks::GetInstance().AddTaskInfo(1, taskInfo));
}

TEST_F(MC2GlobalMirrorTasksTest, test_GetTaskInfo)
{
    EXPECT_EQ(MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(100, 0, 0, 0), nullptr);

    EXPECT_EQ(MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(0, 0, 0, 0), nullptr);

    shared_ptr<TaskInfo> taskInfo = InitTaskInfo();
    taskInfo->taskParam_.taskType = TaskParamType::TASK_CCU;
    taskInfo->taskParam_.taskPara.Ccu.dieId = 1;
    taskInfo->taskParam_.taskPara.Ccu.missionId = 2;
    taskInfo->taskParam_.taskPara.Ccu.instrId = 3;
    taskInfo->taskParam_.taskPara.Ccu.executeId = 4;
    MC2GlobalMirrorTasks::GetInstance().AddTaskInfo(10, taskInfo);
    shared_ptr<TaskInfo> ret = MC2GlobalMirrorTasks::GetInstance().GetTaskInfo(10, 1, 2, 3);
    EXPECT_NE(ret, nullptr);
    EXPECT_EQ(taskInfo->taskParam_.taskPara.Ccu.executeId, 4);
}