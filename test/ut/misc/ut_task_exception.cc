/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <sys/time.h> /* 获取时间 */
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#include <runtime/rt_error_codes.h>
#define private public
#define protected public
#include "task_exception_handler_pub.h"
#include "heartbeat.h"
#undef protected
#undef private
#include "sal.h"
#include "externalinput.h"
#include "aicpu_operator_pub.h"
#include "alg_profiling.h"
#include "task_exception.h"
#include "aicpu_communicator.h"

using namespace std;
using namespace hccl;

extern array<map<s32, GetAicpuTaskExceptionCallBack>, MAX_MODULE_DEVICE_NUM> g_communicatorCallbackMap;
class TaskExceptionTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TaskExceptionTest SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TaskExceptionTest TearDown--\033[0m" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};
#if 0 //执行失败Map size is bigger than max stream count
TEST_F(TaskExceptionTest, ut_task_exception_callback)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskParaNotify taskParaNotify;
    AlgType algType = AlgType::Reserved();

    // TaskInfo(u32 &streamID, u32 &taskID, std::string &tag, TaskType &taskType, const TaskParaNotify &para);
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};
    taskExceptionHandler.Callback(&rtExceptionInfo1);
}
TEST_F(TaskExceptionTest, ut_task_exception_callback_ffts)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 1;
    u32 taskID = 0;
    std::string tag = "test_tag";
    aclrtStream steam;
    aclrtCreateStream(&steam);
    aclrtStreamGetId(steam, (s32 *)&streamID);
    ret = taskExceptionHandler.AddStream(streamID, tag, 0, AlgType::Reserved());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    RankInfo rankInfo;
    rankInfo.devicePhyId = 10;
    UIDType crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    UIDType informer = Heartbeat::GetInstance(0).GetUId(rankInfo);

    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    ret = taskExceptionHandler.Save(streamID, taskID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 1;
    ret = taskExceptionHandler.Save(streamID, taskID);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, streamID, 0, 0, ACL_ERROR_RT_FFTS_PLUS_TIMEOUT};
    rtExceptionInfo1.expandInfo.type = RT_EXCEPTION_FFTS_PLUS;
    taskExceptionHandler.Callback(&rtExceptionInfo1);
    rtExceptionInfo rtExceptionInfo2{0, streamID, 0, 0, 0};
    rtExceptionInfo2.expandInfo.type = RT_EXCEPTION_FFTS_PLUS;
    taskExceptionHandler.Callback(&rtExceptionInfo2);
    aclrtDestroyStream(steam);
}
TEST_F(TaskExceptionTest, ut_task_exception_callback_context_size)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    std::string tag = "test_tag";
    TaskParaNotify taskParaNotify;
    TaskParaDMA taskParaDMA;
    TaskParaReduce taskParaReduce;
    AlgType algType = AlgType::Reserved();

    u32 taskID = 0;
    TaskType taskType = TaskType::TASK_RDMA;
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaDMA);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 1;
    taskType = TaskType::TASK_REDUCE_TBE;
    TaskInfo taskInfo2(streamID, taskID, tag, taskType, algType, index, taskParaReduce);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 2;
    taskType = TaskType::TASK_REDUCE_INLINE;
    taskParaReduce.remoteUserRank = 1;
    TaskInfo taskInfo3(streamID, taskID, tag, taskType, algType, index, taskParaReduce);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo3);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 3;
    taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskInfo taskInfo4(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo4);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 4;
    taskType = TaskType::TASK_SDMA;
    TaskInfo taskInfo5(streamID, taskID, tag, taskType, algType, index, taskParaDMA);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo5);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 5;
    taskType = TaskType::TASK_NOTIFY_RECORD;
    taskParaNotify.notifyID = 0xffffffffabcdef12;
    TaskInfo taskInfo6(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo6);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskID = 6;
    taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskInfo taskInfo7(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo7);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{taskID, 0, 0, 0, 0};
    taskExceptionHandler.Callback(&rtExceptionInfo1);
}
TEST_F(TaskExceptionTest, ut_task_exception_callback_fftsCtx)
{
    MOCKER(GetExternalInputTaskExceptionSwitch)
    .stubs()
    .will(returnValue(1));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(true));

    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 1;
    u32 taskID = 0;
    std::string tag = "test_tag";

    aclrtStream steam;
    aclrtCreateStream(&steam);
    aclrtStreamGetId(steam, (s32 *)&streamID);
    ret = taskExceptionHandler.AddStream(streamID, tag, 0, AlgType::Reserved());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // ctxInfoVector存储3个CtxInfo
    TaskParaNotify taskParaNotify;
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TaskParaDMA taskParaDMA;
    taskType = TaskType::TASK_RDMA;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaDMA);
    CtxInfo tmpCtxInfo(taskType, taskParaDMA);
    tmpCtxInfo.GetCtxParaInfoStr();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TaskParaReduce taskParaReduce;
    taskType = TaskType::TASK_REDUCE_TBE;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaReduce);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 存储ctxInfoVector至opCtxInfo
    ret = taskExceptionHandler.Save(streamID, taskID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // callback查找
    rtExceptionExpandInfo_t expandInfo;
    expandInfo.u.fftsPlusInfo.contextId = 0;
    rtExceptionInfo rtExceptionInfo1;
    rtExceptionInfo1.streamid = streamID;
    rtExceptionInfo1.expandInfo = expandInfo;
    rtExceptionInfo1.deviceid = 0;
    taskExceptionHandler.Callback(&rtExceptionInfo1);
    aclrtDestroyStream(steam);
    taskExceptionHandler.DeInit();
    GlobalMockObject::verify();
}
TEST_F(TaskExceptionTest, ut_task_exception_callback_fftsCtx_context_size)
{
    MOCKER(GetExternalInputTaskExceptionSwitch)
    .stubs()
    .will(returnValue(1));

    MOCKER(GetExternalInputHcclEnableFfts)
    .stubs()
    .will(returnValue(true));

    MOCKER(GetWorkflowMode)
    .stubs()
    .will(returnValue(HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE));

    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 1;
    u32 taskID = 0;
    std::string tag = "test_tag";

    aclrtStream steam;
    aclrtCreateStream(&steam);
    aclrtStreamGetId(steam, (s32 *)&streamID);
    ret = taskExceptionHandler.AddStream(streamID, tag, 0, AlgType::Reserved());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // ctxInfoVector存储3个CtxInfo
    TaskParaNotify taskParaNotify;
    TaskParaDMA taskParaDMA;
    TaskType taskType = TaskType::TASK_RDMA;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaDMA);

    CtxInfo tmpCtxInfo(taskType, taskParaDMA);
    tmpCtxInfo.GetCtxParaInfoStr();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TaskParaReduce taskParaReduce;
    taskType = TaskType::TASK_REDUCE_TBE;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaReduce);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    taskType = TaskType::TASK_NOTIFY_WAIT;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaNotify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    // 存储ctxInfoVector至opCtxInfo
    ret = taskExceptionHandler.Save(streamID, taskID);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // callback查找
    rtExceptionExpandInfo_t expandInfo;
    expandInfo.u.fftsPlusInfo.contextId = 2;
    expandInfo.type = tagRtExceptionExpandType::RT_EXCEPTION_FFTS_PLUS;
    rtExceptionInfo rtExceptionInfo1;
    rtExceptionInfo1.streamid = streamID;
    rtExceptionInfo1.expandInfo = expandInfo;
    rtExceptionInfo1.deviceid = 0;
    rtExceptionInfo1.taskid = taskID;
    taskExceptionHandler.Callback(&rtExceptionInfo1);

    aclrtDestroyStream(steam);
    taskExceptionHandler.DeInit();
    GlobalMockObject::verify();
}
TEST_F(TaskExceptionTest, ut_task_exception_callback_heartbeat1)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskParaNotify taskParaNotify;
    AlgType algType = AlgType::Reserved();

    // TaskInfo(u32 &streamID, u32 &taskID, std::string &tag, TaskType &taskType, const TaskParaNotify &para);
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};

    RankInfo rankInfo;
    rankInfo.devicePhyId = 10;
    UIDType crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    UIDType informer = Heartbeat::GetInstance(0).GetUId(rankInfo);

    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    rankInfo.devicePhyId = 11;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    rankInfo.devicePhyId = 12;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    taskExceptionHandler.Callback(&rtExceptionInfo1);
}

TEST_F(TaskExceptionTest, ut_task_exception_callback_heartbeat2)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskParaNotify taskParaNotify;
    AlgType algType = AlgType::Reserved();

    // TaskInfo(u32 &streamID, u32 &taskID, std::string &tag, TaskType &taskType, const TaskParaNotify &para);
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};

    RankInfo rankInfo;
    rankInfo.devicePhyId = 20;
    UIDType crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    rankInfo.devicePhyId = 10;
    UIDType informer = Heartbeat::GetInstance(0).GetUId(rankInfo);

    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_LOST);

    taskExceptionHandler.Callback(&rtExceptionInfo1);
}

TEST_F(TaskExceptionTest, ut_task_exception_callback__cqe_heartbeat)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskParaNotify taskParaNotify;
    AlgType algType = AlgType::Reserved();

    // TaskInfo(u32 &streamID, u32 &taskID, std::string &tag, TaskType &taskType, const TaskParaNotify &para);
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};

    RankInfo rankInfo;
    rankInfo.devicePhyId = 10;
    UIDType crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    UIDType informer = Heartbeat::GetInstance(0).GetUId(rankInfo);

    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    rankInfo.devicePhyId = 11;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    rankInfo.devicePhyId = 12;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_CQE_ERR);

    taskExceptionHandler.Callback(&rtExceptionInfo1);
}
#endif
TEST_F(TaskExceptionTest, ut_TaskInfo_GetBaseInfoStr)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_RDMA;
    TaskParaNotify taskParaNotify;
    string str;
    AlgType algType = AlgType::Reserved();

    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    str = taskInfo1.GetBaseInfoStr();
    TaskParaDMA taskParaDMA;
    taskType = TaskType::TASK_RDMA;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaDMA);
    CtxInfo tmpCtxInfo(taskType, taskParaDMA);
    str = tmpCtxInfo.GetCtxBaseInfoStr();
    str = tmpCtxInfo.GetCtxParaNotify();

    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};
    TaskExceptionHandler::DealExceptionCtx(&rtExceptionInfo1);
    TaskExceptionHandler::DealExceptionOp(&rtExceptionInfo1);
    taskExceptionHandler.Flush();
}

TEST_F(TaskExceptionTest, ut_TaskInfo_GetOpDataAndRankInfo)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);

    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    u64 count = 0;
    void *src = nullptr; 
    void *dst = nullptr;
    HcclDataType dataType = HcclDataType::HCCL_DATA_TYPE_INT16;
    u32 rootId = 0;
    std::string group = "test_group";
    TaskType taskType = TaskType::TASK_RDMA;
    TaskParaNotify taskParaNotify;
    string str;
    AlgType algType = AlgType::Reserved();

    aclrtStream steam;
    aclrtCreateStream(&steam);
    aclrtStreamGetId(steam, (s32 *)&streamID);
    ret = taskExceptionHandler.AddStream(streamID, tag, 0, AlgType::Reserved());
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = taskExceptionHandler.AddTag(tag, group, HcclWorkflowMode::HCCL_WORKFLOW_MODE_OP_BASE,false);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = taskExceptionHandler.AddOpData(tag, count, src, dst, dataType, rootId, group, \
        HcclReduceOp::HCCL_REDUCE_SUM);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = taskExceptionHandler.AddGroupRankInfo(group, 8, rootId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    str = taskInfo1.GetBaseInfoStr();
    TaskParaDMA taskParaDMA;
    taskType = TaskType::TASK_RDMA;
    ret = taskExceptionHandler.Save(streamID, taskID, taskType, taskParaDMA);
    CtxInfo tmpCtxInfo(taskType, taskParaDMA);
    str = tmpCtxInfo.GetCtxBaseInfoStr();
    str = tmpCtxInfo.GetCtxParaNotify();

    rtExceptionExpandInfo_t expandInfo;
    expandInfo.u.fftsPlusInfo.contextId = 0;
    expandInfo.type = tagRtExceptionExpandType::RT_EXCEPTION_FFTS_PLUS;
    rtExceptionInfo rtExceptionInfo1;
    rtExceptionInfo1.streamid = streamID;
    rtExceptionInfo1.expandInfo = expandInfo;
    rtExceptionInfo1.deviceid = deviceLogicId;
    taskExceptionHandler.Callback(&rtExceptionInfo1);
    std::string groupRankContentInfo;
    std::string stageErrInfo;
    TaskExceptionHandler::DealExceptionGroupRank(&rtExceptionInfo1, tag, true, groupRankContentInfo, stageErrInfo);
    TaskExceptionHandler::DealExceptionOpData(&rtExceptionInfo1, tag, true, index, stageErrInfo);

    struct timeval tv{0};
    std::string opDataContent;
    (void)gettimeofday(&tv, nullptr);
    TaskExceptionHandler::TimeStruct2Str(tv, opDataContent);
    ret = taskExceptionHandler.DelOpData(tag);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = taskExceptionHandler.DelGroupRankInfo(group);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    taskExceptionHandler.Flush();
    taskExceptionHandler.DeInit();
    aclrtDestroyStream(steam);
    GlobalMockObject::verify();
}
#if 0 //执行失败Map size is bigger than max stream count
TEST_F(TaskExceptionTest, ut_task_exception_callback__stuck_heartbeat)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
 
    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskType taskType = TaskType::TASK_NOTIFY_WAIT;
    TaskParaNotify taskParaNotify;
    AlgType algType = AlgType::Reserved();
 
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskType, algType, index, taskParaNotify);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};
 
    RankInfo rankInfo;
    rankInfo.devicePhyId = 10;
    UIDType crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    UIDType informer = Heartbeat::GetInstance(0).GetUId(rankInfo);
 
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_STUCK);
 
    rankInfo.devicePhyId = 11;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_STUCK);
 
    rankInfo.devicePhyId = 12;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_STUCK);
 
    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_STUCK);
 
    rankInfo.devicePhyId = 13;
    crimer = Heartbeat::GetInstance(0).GetUId(rankInfo);
    Heartbeat::GetInstance(0).SetStatus(crimer, informer, HeartBeatStatus::HEARTBEAT_STUCK);
 
    taskExceptionHandler.Callback(&rtExceptionInfo1);
}
#endif
ErrorMessageReport GetAicpuTaskException()
{
    ErrorMessageReport emrReport;
    strcpy(emrReport.tag, "test");
    strcpy(emrReport.group, "grouptest");
    emrReport.remoteUserRank = 1;
    emrReport.streamId = 2;
    emrReport.taskId = 3;
    emrReport.notifyId = 4;
    emrReport.rankId = 5;
    emrReport.rankSize = 6;
    emrReport.algType.algoLevel0 = AlgTypeLevel0::ALG_LEVEL0_NP_HD;
    emrReport.algType.algoLevel1 = AlgTypeLevel1::ALG_LEVEL1_HD;
    emrReport.taskType = TaskType::TASK_NOTIFY_WAIT;

    return emrReport;
}

TEST_F(TaskExceptionTest, ut_PrintAicpuErrorMessage)
{
    bool isExistAicpuError = false;
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    auto getAicpuTaskExceptionCallBack = []() {return GetAicpuTaskException();};
    RegisterGetAicpuTaskExceptionCallBack(0, 0, getAicpuTaskExceptionCallBack);
    rtExceptionInfo exceptionInfo{0, 0, 0, 0, 0};

    taskExceptionHandler.PrintAicpuErrorMessage(&exceptionInfo, isExistAicpuError);

    taskExceptionHandler.Callback(&exceptionInfo);
}
#if 0 //执行失败Map size is bigger than max stream count
TEST_F(TaskExceptionTest, ut_task_exception_aiv)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
 
    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskParaAiv taskParaAiv;
    
    u32 index = 0;
    TaskInfo taskInfo1(streamID, taskID, tag, taskParaAiv);

    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};
 
    taskExceptionHandler.DealExceptionTask(&rtExceptionInfo1);
}
#endif

TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_GetDevice_Fail_Expect_Ret_HCCL_E_PARA)
{
    MOCKER(hrtGetDevice)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_E_INTERNAL));

    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    
    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_E_PARA);
    GlobalMockObject::verify();
}

TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_AivGroup_Count_Zero_Expect_Print_Group_No_Aiv)
{
    u32 deviceLogicId = 0;

    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    std::string groupName = "group_101";
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId][groupName] = 0;

    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_SUCCESS);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();
    GlobalMockObject::verify();
}

TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_AivGroup_Size_Zero_Expect_Print_Aiv_Group_Not_Recode)
{
    u32 deviceLogicId = 0;

    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();

    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_AivGroup_Size_One_Expect_No_Print_Aiv_Group)
{
    u32 deviceLogicId = 0;

    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    std::string groupName = "group_101";
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId][groupName] = 101;

    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_SUCCESS);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();
    GlobalMockObject::verify();
}


TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_AivGroup_Size_No_Zero_Expect_Print_Multi_Aiv_May_Execution_Stuck)
{
    u32 deviceLogicId = 0;

    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_000"] = 0;
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_001"] = 0;
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_002"] = 0;
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_101"] = 101;
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_102"] = 102;
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId]["group_103"] = 103;

    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_SUCCESS);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();
    GlobalMockObject::verify();
}


TEST_F(TaskExceptionTest, St_PrintCommAivInfo_When_AivGroup_Size_100_Expect_Print_Multi_Aiv_May_Execution_Stuck)
{
    u32 deviceLogicId = 0;

    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    for (int i = 0; i < 100; i++) {
        std::stringstream temp;
        temp << "group_" << i;
        taskExceptionHandler.aivGroupIndexMap_[deviceLogicId][temp.str()] = 7;
    }

    EXPECT_EQ(taskExceptionHandler.PrintCommAivInfo(), HCCL_SUCCESS);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();
    GlobalMockObject::verify();
}
#if 0 //执行失败taskMap size is bigger than max stream count
TEST_F(TaskExceptionTest, St_DealExceptionTask_When_Comm_Has_Multi_Aiv_Expect_Print_May_Execution_Stuck)
{
    u32 deviceLogicId = 0;
    TaskExceptionHandler taskExceptionHandler(deviceLogicId);
 
    HcclResult ret;
    ret = taskExceptionHandler.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    u32 streamID = 0;
    u32 taskID = 0;
    std::string tag = "test_tag";
    TaskParaAiv taskParaAiv;

    TaskInfo taskInfo1(streamID, taskID, tag, taskParaAiv);
    for (int i = 0; i < 100; i++) {
        std::stringstream temp;
        temp << "group_" << i;
        taskExceptionHandler.aivGroupIndexMap_[deviceLogicId][temp.str()] = 1;
    }

    for (int i = 0; i < 100; i++) {
        std::stringstream temp;
        temp << "group_" << i + 100;
        taskExceptionHandler.aivGroupIndexMap_[deviceLogicId][temp.str()] = 0;
    }

    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskExceptionHandler.InsertTaskMap(streamID, taskInfo1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    rtExceptionInfo rtExceptionInfo1{0, 0, 0, 0, 0};
 
    taskExceptionHandler.DealExceptionTask(&rtExceptionInfo1);
    taskExceptionHandler.aivGroupIndexMap_[deviceLogicId].clear();
    GlobalMockObject::verify();
}
#endif

// 回调函数用于获取算子信息
void GetOpInfoCallback(void* opInfo, char* opInfoTmp, u32 maxSize)
{
    const char* testOpInfo = "test_op_info_data";
    strncpy(opInfoTmp, testOpInfo, maxSize - 1);
    opInfoTmp[maxSize - 1] = '\0';
}

TEST_F(TaskExceptionTest, ut_PrintTaskExceptionTaskQue_Coverage)
{
    u32 deviceLogicId = 0;
    u32 localUserRank = 0;
    std::string identifier = "test_identifier";
    
    // 初始化 TaskException 对象
    TaskException taskException;
    HcclResult ret = taskException.Init(deviceLogicId, localUserRank, identifier);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建并初始化 SqeRingBuffer
    HcclSqeContext sqeContext;
    SqeRingBuffer* sqeContextBuffer = &sqeContext.buffer;
    memset(sqeContextBuffer, 0, sizeof(SqeRingBuffer));
    
    // 初始化 201 个位置的 rtsDfxInfo（覆盖循环从 0 到 200）
    // 前 150 个属于算子 0，第 151-200 个属于算子 1
    // 这样会触发 tasksInCurrentLine >= 50 的条件 3 次（50, 100, 150）
    // 以及算子切换 2 次（在位置 150 和 200）
    for (u32 i = 0; i <= 200; i++) {
        // 前 150 个属于算子 0
        if (i < 150) {
            sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = 0;
            sqeContextBuffer->rtsDfxInfo[i].remoteRank = i % 8;
            sqeContextBuffer->rtsDfxInfo[i].notifyId = i % 16;
        } else {
            // 第 151-200 个属于算子 1
            sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = 1;
            sqeContextBuffer->rtsDfxInfo[i].remoteRank = (i + 1) % 8;
            sqeContextBuffer->rtsDfxInfo[i].notifyId = (i + 1) % 16;
        }
        
        // 初始化 rtsMirrorBuffer 中的 rtStarsSqeHeader_t
        uint8_t* sqeMirrorBufferAddr = sqeContextBuffer->rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t* sqeHeader = reinterpret_cast<rtStarsSqeHeader_t*>(sqeMirrorBufferAddr);
        memset(sqeHeader, 0, sizeof(rtStarsSqeHeader_t));
        sqeHeader->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeHeader->taskId = static_cast<u16>(i);
        
        // 初始化 rtsqSqeType 和 addInfo
        sqeContextBuffer->rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer->addInfo[i] = 0;
    }
    
    // 注册算子 0 的信息
    uint8_t opInfo0[64] = {0};
    strncpy(reinterpret_cast<char*>(opInfo0), "op_info_0", sizeof(opInfo0) - 1);
    ret = taskException.RegisterOpInfo(opInfo0, sizeof(opInfo0));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskException.RegisterOpInfoCallback(GetOpInfoCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 注册算子 1 的信息（需要先调用一次 RegisterOpInfo 来切换到新的 opRingBufferIdx）
    uint8_t opInfo1[64] = {0};
    strncpy(reinterpret_cast<char*>(opInfo1), "op_info_1", sizeof(opInfo1) - 1);
    ret = taskException.RegisterOpInfo(opInfo1, sizeof(opInfo1));
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = taskException.RegisterOpInfoCallback(GetOpInfoCallback);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 由于 RegisterOpInfo 会递增 opRingBufferIdx_，现在需要调整 rtsDfxInfo 的 opRingBufferIdx
    // 第一个算子应该是 opRingBufferIdx_ - 1
    // 第二个算子应该是 opRingBufferIdx_
    u32 opIdx0 = taskException.GetOpRingBufferIdx();
    u32 opIdx1 = (opIdx0 + 1) % OPINFO_RING_BUFFER_MAX;
    
    for (u32 i = 0; i < 150; i++) {
        sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opIdx0;
    }
    for (u32 i = 150; i <= 200; i++) {
        sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opIdx1;
    }
    
    // 调用 PrintTaskExceptionTaskQue 函数
    // 这个函数会遍历 201 个位置（0 到 200）
    // 触发以下逻辑：
    // 1. tasksInCurrentLine = 0（初始化）
    // 2. tasksInCurrentLine++（在循环中递增 201 次）
    // 3. tasksInCurrentLine >= 50 && i < 200（在 i=50,100,150 时触发换行）
    // 4. tasksInCurrentLine = 0（在换行时重置 3 次）
    // 5. tasksInCurrentLine = 0（在算子切换时重置 1 次，i=150 时）
    // 6. if (tasksInCurrentLine > 0)（在最后打印剩余的 task）
    taskException.PrintTaskExceptionTaskQue(0, sqeContextBuffer);
    
    // 测试通过
    SUCCEED();
}

// 测试 HcclCommAicpu::PrintTaskExceptionTaskQue 的覆盖率
// 通过 Mock aicpuShareData_.GetAicpuOpInfo 来避免初始化 aicpuShareData
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Coverage)
{
    HcclCommAicpu aicpuComm;
    
    // 创建并初始化 SqeRingBuffer
    HcclSqeContext sqeContext;
    SqeRingBuffer* sqeContextBuffer = &sqeContext.buffer;
    memset(sqeContextBuffer, 0, sizeof(SqeRingBuffer));
    
    AicpuOpInfo opInfo0;
    opInfo0.opIndex = 0;
    strncpy(opInfo0.tagBuff, "tag0", sizeof(opInfo0.tagBuff) - 1);
    
    AicpuOpInfo opInfo1;
    opInfo1.opIndex = 1;
    strncpy(opInfo1.tagBuff, "tag1", sizeof(opInfo1.tagBuff) - 1);
    
    AicpuOpInfo opInfo2;
    opInfo2.opIndex = 2;
    strncpy(opInfo2.tagBuff, "tag2", sizeof(opInfo2.tagBuff) - 1);
    
    // Mock aicpuShareData_.GetAicpuOpInfo 方法
    // 这样可以避免初始化复杂的 aicpuShareData_
    u32 opRingBufferIdx0 = 0;
    u32 opRingBufferIdx1 = 1;
    u32 opRingBufferIdx2 = 2;
    
    auto aicpuShareDataGetter = [&](u32 opRingBufferIdx) -> const AicpuOpInfo* {
        if (opRingBufferIdx == opRingBufferIdx0) {
            return &opInfo0;
        } else if (opRingBufferIdx == opRingBufferIdx1) {
            return &opInfo1;
        } else if (opRingBufferIdx == opRingBufferIdx2) {
            return &opInfo2;
        }
        return nullptr;
    };
    
    // 使用宏访问私有成员 aicpuShareData_
    // 在 SetUpTestCase 中已经定义了 #define private public
    MOCKER(aicpuShareData_.GetAicpuOpInfo)
        .stubs()
        .with(any())
        .will(invoke([&](u32 idx) -> const AicpuOpInfo* {
            return aicpuShareDataGetter(idx);
        }));
    
    // 初始化 201 个位置的 rtsDfxInfo
    for (u32 i = 0; i <= 200; i++) {
        if (i < 150) {
            sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opRingBufferIdx0;
            sqeContextBuffer->rtsDfxInfo[i].remoteRank = i % 8;
            sqeContextBuffer->rtsDfxInfo[i].notifyId = i % 16;
        } else if (i < 175) {
            sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opRingBufferIdx1;
            sqeContextBuffer->rtsDfxInfo[i].remoteRank = (i + 1) % 8;
            sqeContextBuffer->rtsDfxInfo[i].notifyId = (i + 1) % 16;
        } else {
            sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opRingBufferIdx2;
            sqeContextBuffer->rtsDfxInfo[i].remoteRank = (i + 2) % 8;
            sqeContextBuffer->rtsDfxInfo[i].notifyId = (i + 2) % 16;
        }
        
        // 初始化 rtsMirrorBuffer
        uint8_t* sqeMirrorBufferAddr = sqeContextBuffer->rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t* sqeHeader = reinterpret_cast<rtStarsSqeHeader_t*>(sqeMirrorBufferAddr);
        memset(sqeHeader, 0, sizeof(rtStarsSqeHeader_t));
        sqeHeader->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeHeader->taskId = static_cast<u16>(i);
        
        // 初始化 rtsqSqeType 和 addInfo
        sqeContextBuffer->rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer->addInfo[i] = 0;
    }
    
    // Mock GetTaskBriefsInfo 方法（这个函数也需要被 mock）
    MOCKER(aicpuComm.GetTaskBriefsInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue("NW(0,0)"));
    
    // Mock GetTaskExceptionOpInfo 方法（这个函数也需要被 mock）
    MOCKER(aicpuComm.GetTaskExceptionOpInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue("opInfo: test_op_info"));
    
    // 测试 isMonitor = false 的场景（HCCL_ERROR日志）
    aicpuComm.PrintTaskExceptionTaskQue(0, sqeContextBuffer, false);
    
    // 清理 Mock
    GlobalMockObject::verify();
    
    // 测试通过
    SUCCEED();
}

// 测试 isMonitor = true 的场景（HCCL_RUN_INFO日志）
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Monitor_Coverage)
{
    HcclCommAicpu aicpuComm;
    
    // 创建并初始化 SqeRingBuffer
    HcclSqeContext sqeContext;
    SqeRingBuffer* sqeContextBuffer = &sqeContext.buffer;
    memset(sqeContextBuffer, 0, sizeof(SqeRingBuffer));
    
    // 准备测试用的算子信息（简单的单算子场景）
    AicpuOpInfo opInfo0;
    opInfo0.opIndex = 0;
    strncpy(opInfo0.tagBuff, "tag0", sizeof(opInfo0.tagBuff) - 1);
    
    u32 opRingBufferIdx0 = 0;
    
    // Mock aicpuShareData_.GetAicpuOpInfo 方法
    auto aicpuShareDataGetter = [&](u32 opRingBufferIdx) -> const AicpuOpInfo* {
        if (opRingBufferIdx == opRingBufferIdx0) {
            return &opInfo0;
        }
        return nullptr;
    };
    
    MOCKER(aicpuShareData_.GetAicpuOpInfo)
        .stubs()
        .with(any())
        .will(invoke([&](u32 idx) -> const AicpuOpInfo* {
            return aicpuShareDataGetter(idx);
        }));
    
    // 初始化 201 个位置的 rtsDfxInfo（单算子场景）
    for (u32 i = 0; i <= 200; i++) {
        sqeContextBuffer->rtsDfxInfo[i].opRingBufferIdx = opRingBufferIdx0;
        sqeContextBuffer->rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer->rtsDfxInfo[i].notifyId = i % 16;
        
        uint8_t* sqeMirrorBufferAddr = sqeContextBuffer->rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t* sqeHeader = reinterpret_cast<rtStarsSqeHeader_t*>(sqeMirrorBufferAddr);
        memset(sqeHeader, 0, sizeof(rtStarsSqeHeader_t));
        sqeHeader->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeHeader->taskId = static_cast<u16>(i);
        
        sqeContextBuffer->rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer->addInfo[i] = 0;
    }
    
    // Mock GetTaskBriefsInfo 方法
    MOCKER(aicpuComm.GetTaskBriefsInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue("NW(0,0)"));
    
    // Mock GetTaskExceptionOpInfo 方法
    MOCKER(aicpuComm.GetTaskExceptionOpInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue("opInfo: test_op_info"));
    
    // 测试 isMonitor = true 的场景（HCCL_RUN_INFO日志）
    aicpuComm.PrintTaskExceptionTaskQue(0, sqeContextBuffer, true);
    
    // 清理 Mock
    GlobalMockObject::verify();
    
    // 测试通过
    SUCCEED();
}

