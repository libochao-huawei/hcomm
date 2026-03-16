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

// ==================== TaskException::PrintTaskExceptionTaskQue 测试用例 ====================

/**
 * @tc.name: ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_LessThan50
 * @tc.desc: 测试单个算子且task数小于50的场景
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_LessThan50)
{
    TaskException taskException;
    HcclResult ret = taskException.Init(0, 0, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // 构造测试数据：注册30个task（单个算子）
    IndOpInfo opInfo;
    opInfo.opIndex = 1;
    opInfo.callback = nullptr;
    for (u32 i = 0; i < 30; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo), sizeof(opInfo));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 构造SqeRingBuffer（使用栈上内存）
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    // 初始化rtsDfxInfo
    for (u32 i = 0; i < 30; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0; // 都指向同一个opInfo
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;
    }

    // 初始化rtsMirrorBuffer
    for (u32 i = 0; i < 30; i++) {
        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }

    // 调用PrintTaskExceptionTaskQue
    taskException.PrintTaskExceptionTaskQue(0, &sqeContextBuffer);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_Equal50
 * @tc.desc: 测试单个算子且task数等于50的场景
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_Equal50)
{
    TaskException taskException;
    HcclResult ret = taskException.Init(0, 0, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // 构造测试数据：注册50个task
    IndOpInfo opInfo;
    opInfo.opIndex = 1;
    opInfo.callback = nullptr;
    for (u32 i = 0; i < 50; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo), sizeof(opInfo));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 构造SqeRingBuffer
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 50; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }

    // 调用PrintTaskExceptionTaskQue
    taskException.PrintTaskExceptionTaskQue(0, &sqeContextBuffer);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_MoreThan50
 * @tc.desc: 测试单个算子且task数大于50的场景（需要切分多行）
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_TaskException_PrintTaskExceptionTaskQue_SingleOp_MoreThan50)
{
    TaskException taskException;
    HcclResult ret = taskException.Init(0, 0, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // 构造测试数据：注册120个task（超过50，需要切分）
    IndOpInfo opInfo;
    opInfo.opIndex = 1;
    opInfo.callback = nullptr;
    for (u32 i = 0; i < 120; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo), sizeof(opInfo));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 构造SqeRingBuffer
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 120; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }

    // 调用PrintTaskExceptionTaskQue
    taskException.PrintTaskExceptionTaskQue(0, &sqeContextBuffer);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_TaskException_PrintTaskExceptionTaskQue_MultipleOps
 * @tc.desc: 测试多个算子的场景（不同opIndex）
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_TaskException_PrintTaskExceptionTaskQue_MultipleOps)
{
    TaskException taskException;
    HcclResult ret = taskException.Init(0, 0, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // 构造测试数据：3个不同算子
    u32 taskCount = 0;
    
    // 第一个算子：30个task
    IndOpInfo opInfo1;
    opInfo1.opIndex = 1;
    opInfo1.callback = nullptr;
    for (u32 i = 0; i < 30; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo1), sizeof(opInfo1));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    
    // 第二个算子：40个task
    IndOpInfo opInfo2;
    opInfo2.opIndex = 2;
    opInfo2.callback = nullptr;
    for (u32 i = 0; i < 40; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo2), sizeof(opInfo2));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    
    // 第三个算子：50个task
    IndOpInfo opInfo3;
    opInfo3.opIndex = 3;
    opInfo3.callback = nullptr;
    for (u32 i = 0; i < 50; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo3), sizeof(opInfo3));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 构造SqeRingBuffer
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    // 第一个算子（opIndex=1）
    for (u32 i = 0; i < 30; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }
    
    // 第二个算子（opIndex=2）
    for (u32 i = 30; i < 70; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 1;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_SDMA;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }
    
    // 第三个算子（opIndex=3）
    for (u32 i = 70; i < 120; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 2;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_RECORD;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }

    // 调用PrintTaskExceptionTaskQue
    taskException.PrintTaskExceptionTaskQue(119, &sqeContextBuffer);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_TaskException_PrintTaskExceptionTaskQue_Boundary_200
 * @tc.desc: 测试边界情况：正好200个task
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_TaskException_PrintTaskExceptionTaskQue_Boundary_200)
{
    TaskException taskException;
    HcclResult ret = taskException.Init(0, 0, "test_group");
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // 构造测试数据：注册200个task（边界值）
    IndOpInfo opInfo;
    opInfo.opIndex = 1;
    opInfo.callback = nullptr;
    for (u32 i = 0; i < 200; i++) {
        ret = taskException.RegisterOpInfo(reinterpret_cast<void*>(&opInfo), sizeof(opInfo));
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 构造SqeRingBuffer
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 200; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;
    }

    // 调用PrintTaskExceptionTaskQue
    taskException.PrintTaskExceptionTaskQue(199, &sqeContextBuffer);

    GlobalMockObject::verify();
}

// ==================== HcclCommAicpu::PrintTaskExceptionTaskQue 测试用例 ====================

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Monitor_SingleOp_LessThan50
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 监控模式，单算子，task数<50
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Monitor_SingleOp_LessThan50)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo
    std::string mockOpInfo = "tag:test_op, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.";
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .will(returnValue(mockOpInfo));

    // 构造SqeRingBuffer：30个task
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 30; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue（isMonitor = true）
    hcclCommAicpu.PrintTaskExceptionTaskQue(0, &sqeContextBuffer, true);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Exception_SingleOp_MoreThan50
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 异常模式，单算子，task数>50（需要切分）
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Exception_SingleOp_MoreThan50)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo
    std::string mockOpInfo = "tag:test_op, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.";
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .will(returnValue(mockOpInfo));

    // 构造SqeRingBuffer：120个task
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 120; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue（isMonitor = false）
    hcclCommAicpu.PrintTaskExceptionTaskQue(0, &sqeContextBuffer, false);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Monitor_MultipleOps_DifferentTags
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 监控模式，多算子，不同tag
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Monitor_MultipleOps_DifferentTags)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo - 返回不同tag的opInfo
    int callCount = 0;
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .with(any(), any())
        .will(repeat()
            .onCall(0)
            .return(std::string("tag:allreduce, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000."))
            .onCall(30)
            .return(std::string("tag:allgather, group:test_group, isCustom:0, opLaunchIdx:2, opExecIdx:2, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000."))
            .onCall(70)
            .return(std::string("tag:broadcast, group:test_group, isCustom:0, opLaunchIdx:3, opExecIdx:3, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.")));

    // 构造SqeRingBuffer：3个不同算子
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    // 第一个算子：30个task
    for (u32 i = 0; i < 30; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }
    
    // 第二个算子：40个task
    for (u32 i = 30; i < 70; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 1;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_SDMA;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_SDMA;
        sqeContextBuffer.addInfo[i] = 0;
    }
    
    // 第三个算子：35个task
    for (u32 i = 70; i < 105; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 2;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_RECORD;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_RECORD;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue（isMonitor = true）
    hcclCommAicpu.PrintTaskExceptionTaskQue(104, &sqeContextBuffer, true);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Exception_MultipleOps_WithSplit
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 异常模式，多算子，单算子需要切分
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Exception_MultipleOps_WithSplit)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .with(any(), any())
        .will(repeat()
            .onCall(0)
            .return(std::string("tag:allreduce_v2, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000."))
            .onCall(80)
            .return(std::string("tag:allreduce_v3, group:test_group, isCustom:0, opLaunchIdx:2, opExecIdx:2, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.")));

    // 构造SqeRingBuffer：2个算子，每个算子需要切分
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    // 第一个算子：80个task（需要切分2行）
    for (u32 i = 0; i < 80; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }
    
    // 第二个算子：90个task（需要切分2行）
    for (u32 i = 80; i < 170; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 1;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_SDMA;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_SDMA;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue（isMonitor = false）
    hcclCommAicpu.PrintTaskExceptionTaskQue(0, &sqeContextBuffer, false);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Boundary_200Tasks
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 边界情况，正好200个task
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_Boundary_200Tasks)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo
    std::string mockOpInfo = "tag:test_op, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.";
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .will(returnValue(mockOpInfo));

    // 构造SqeRingBuffer：200个task（边界值）
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    for (u32 i = 0; i < 200; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue
    hcclCommAicpu.PrintTaskExceptionTaskQue(0, &sqeContextBuffer, false);

    GlobalMockObject::verify();
}

/**
 * @tc.name: ut_HcclCommAicpu_PrintTaskExceptionTaskQue_SameOpIndex_DifferentTags
 * @tc.desc: 测试HcclCommAicpu::PrintTaskExceptionTaskQue - 同一opIndex但不同tag的场景
 * @tc.type: FUNC
 */
TEST_F(TaskExceptionTest, ut_HcclCommAicpu_PrintTaskExceptionTaskQue_SameOpIndex_DifferentTags)
{
    HcclCommAicpu hcclCommAicpu;
    
    // Mock日志函数
    MOCKER(HCCL_ERROR)
        .stubs()
        .will(defaultValue());
    MOCKER(HCCL_RUN_INFO)
        .stubs()
        .will(defaultValue());

    // Mock GetTaskExceptionOpInfo - 相同opIndex但不同tag
    MOCKER(hccl::HcclCommAicpu::GetTaskExceptionOpInfo)
        .stubs()
        .with(any(), any())
        .will(repeat()
            .onCall(0)
            .return(std::string("tag:allreduce_1, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:1, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000."))
            .onCall(30)
            .return(std::string("tag:allreduce_2, group:test_group, isCustom:0, opLaunchIdx:1, opExecIdx:2, count:100, dataType:0, opType:0, rootId:0, dstAddr:0x1000, srcAddr:0x2000.")));

    // 构造SqeRingBuffer：相同opIndex但不同tag
    SqeRingBuffer sqeContextBuffer;
    memset(&sqeContextBuffer, 0, sizeof(sqeContextBuffer));
    
    // 第一个tag：30个task
    for (u32 i = 0; i < 30; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0;
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_NOTIFY_WAIT;
        sqeContextBuffer.addInfo[i] = 0;
    }
    
    // 第二个tag：30个task
    for (u32 i = 30; i < 60; i++) {
        sqeContextBuffer.rtsDfxInfo[i].opRingBufferIdx = 0; // 相同的opRingBufferIdx
        sqeContextBuffer.rtsDfxInfo[i].remoteRank = i % 8;
        sqeContextBuffer.rtsDfxInfo[i].notifyId = i % 16;

        uint8_t *sqeAddr = sqeContextBuffer.rtsMirrorBuffer + i * HCCL_SQE_SIZE;
        rtStarsSqeHeader_t *header = reinterpret_cast<rtStarsSqeHeader_t*>(sqeAddr);
        header->type = RT_STARS_SQE_TYPE_SDMA;
        header->taskId = i;
        header->addr1Low = i;
        header->addr1High = i >> 16;

        sqeContextBuffer.rtsqSqeType[i] = RT_STARS_SQE_TYPE_SDMA;
        sqeContextBuffer.addInfo[i] = 0;
    }

    // 调用PrintTaskExceptionTaskQue
    hcclCommAicpu.PrintTaskExceptionTaskQue(0, &sqeContextBuffer, false);

    GlobalMockObject::verify();
}
