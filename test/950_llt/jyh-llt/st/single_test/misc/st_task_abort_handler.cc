#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <mutex>
#include "task_abort_handler_pub.h"
#include "../inc/hccl/base.h"
#include <hccl/hccl_types.h>
#include <stdio.h>
#define private public
#include "hccl_communicator.h"
#include "notify_pool.h"
#include "comm_base_pub.h"
#include "hccl_impl.h"
#undef private
#include "hccl_common.h"
#include "hccl_comm_pub.h"
#include "adapter_rts.h"
#include "adapter_rts_common.h"
#include "base.h"
#include "task_abort_handler.h"


using namespace std;
using namespace hccl;


class TaskAbortHandlerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TaskAbortHandlerTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TaskAbortHandlerTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TaskAbortHandlerTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TaskAbortHandlerTest TearDown" << std::endl;
    }
    
};


TEST_F(TaskAbortHandlerTest, st_task_abort_handle_init_test)
{
    HcclCommunicator *communicator = nullptr;
    
    TaskAbortHandler taskAbortHandler;
    MOCKER(hrtTaskAbortHandleCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = taskAbortHandler.Init(communicator);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TaskAbortHandlerTest, st_task_abort_handle_deinit_test)
{
    HcclCommunicator *communicator = nullptr;
 
    TaskAbortHandler taskAbortHandler;
    MOCKER(hrtTaskAbortHandleCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = taskAbortHandler.DeInit(communicator);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

//这个就是进行回调函数里面的suspend的相关内容
TEST_F(TaskAbortHandlerTest, st_ProcessTaskAbortHandleCallback_stage0_success)
{
    uint32_t deviceLogicId = 0;
    rtDeviceTaskAbortStage stage = RT_DEVICE_TASK_ABORT_PRE;
    int a = 8;
    void* args = reinterpret_cast<void*>(&a);
    uint32_t time = 30U;
    //构造一个commVector变量
    hccl:: HcclCommunicator* communicator = new  hccl::HcclCommunicator;
    TaskAbortHandler taskAbortHandler;
    taskAbortHandler.Init(communicator);
    
    MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
 
    time = 0U;
    MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
    taskAbortHandler.DeInit(communicator); 
    delete communicator;
}
 
TEST_F(TaskAbortHandlerTest, st_ProcessTaskAbortHandleCallback_stage0_false)
{
    uint32_t deviceLogicId = 0;
    rtDeviceTaskAbortStage stage = RT_DEVICE_TASK_ABORT_PRE;
    int a = 8;
    void* args = reinterpret_cast<void*>(&a);
    uint32_t time = 30U;
    //构造一个commVector变量
    hccl:: HcclCommunicator* communicator = new  hccl::HcclCommunicator;
    TaskAbortHandler taskAbortHandler;
    taskAbortHandler.Init(communicator);
 
    MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify();
 
    deviceLogicId = 1;
    time = 0U;
    MOCKER_CPP(&HcclCommunicator::Suspend, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .with(any())
    .will(returnValue(HCCL_E_INTERNAL));
    ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify(); 
    taskAbortHandler.DeInit(communicator);   
    delete communicator;
}
 
TEST_F(TaskAbortHandlerTest, st_ProcessTaskAbortHandleCallback_stage1_success)
{
    uint32_t deviceLogicId = 0;
    rtDeviceTaskAbortStage stage= RT_DEVICE_TASK_ABORT_POST;
    int a = 8;
    void* args = reinterpret_cast<void*>(&a);
    uint32_t time = 30U;
    //构造一个commVector变量
    hccl:: HcclCommunicator* communicator = new  hccl::HcclCommunicator;
    TaskAbortHandler taskAbortHandler;
    taskAbortHandler.Init(communicator);
    
    HcclCommunicator communicatorMock;
    MOCKER_CPP_VIRTUAL(communicatorMock,&HcclCommunicator::StopExec, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
 
    time = 0U;
    MOCKER_CPP_VIRTUAL(communicatorMock,&HcclCommunicator::StopExec, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .will(returnValue(HCCL_SUCCESS));
    ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 0);
    taskAbortHandler.DeInit(communicator); 
    GlobalMockObject::verify();
 
 
    delete communicator;
}
 
TEST_F(TaskAbortHandlerTest, st_ProcessTaskAbortHandleCallback_stage1_false)
{
    uint32_t deviceLogicId = 0;
    rtDeviceTaskAbortStage stage= RT_DEVICE_TASK_ABORT_POST;
    int a = 8;
    void* args = reinterpret_cast<void*>(&a);
    uint32_t time = 30U;
    //构造一个commVector变量
    hccl:: HcclCommunicator* communicator = new  hccl::HcclCommunicator;
    TaskAbortHandler taskAbortHandler;
    taskAbortHandler.Init(communicator);
 
    HcclCommunicator communicatorMock;
    MOCKER_CPP_VIRTUAL(communicatorMock,&HcclCommunicator::StopExec, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .will(returnValue(HCCL_E_INTERNAL));
    auto ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 1);
    GlobalMockObject::verify();
 
    time = 0U;
    MOCKER_CPP_VIRTUAL(communicatorMock,&HcclCommunicator::StopExec, HcclResult(HcclCommunicator:: *)())
    .stubs()
    .will(returnValue(HCCL_E_INTERNAL));
    ret = ProcessTaskAbortHandleCallback(deviceLogicId,stage,time,args);
    EXPECT_EQ(ret, 1);
    taskAbortHandler.DeInit(communicator);
    GlobalMockObject::verify();
    delete communicator;
}
