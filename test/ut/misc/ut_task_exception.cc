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
        // 创建测试对象
        taskException_ = std::make_unique<TaskException>();
        
        // 初始化Ring Buffer
        InitSqeRingBuffer();
        
        // 设置Mock期望
        SetupDefaultExpectations();
    }
    
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
        taskException_.reset();
        GlobalMockObject::verify();
    }
    
    // 辅助函数：初始化Ring Buffer
    void InitSqeRingBuffer()
    {
        for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
            sqeContextBuffer_.rtsDfxInfo[i].opRingBufferIdx = i;
        }
    }
    
    // 辅助函数：设置默认期望
    void SetupDefaultExpectations()
    {
        // 默认情况下，GetTaskBriefsInfo返回简单信息
        EXPECT_CALL(*taskException_, GetTaskBriefsInfo(_, _))
            .WillRepeatedly(Invoke([this](u32 sqIdx, SqeRingBuffer* buffer) {
                return "Task" + std::to_string(sqIdx);
            }));
    }
    
    // 辅助函数：设置IndOpInfo
    void SetIndOpInfo(u32 ringBufferIdx, u32 opIndex)
    {
        IndOpInfo opInfo;
        opInfo.opIndex = opIndex;
        opInfo.tagBuff = "Op" + std::to_string(opIndex);
        taskException_->indOpInfos_[ringBufferIdx] = opInfo;
    }
    
    // 辅助函数：设置连续的IndOpInfo
    void SetContinuousOpInfos(u32 startIdx, u32 count, u32 opIndex)
    {
        for (u32 i = 0; i < count; i++) {
            u32 idx = (startIdx + i) % HCCL_SQE_MAX_CNT;
            SetIndOpInfo(idx, opIndex);
        }
    }
    
    std::unique_ptr<TaskException> taskException_;
    SqeRingBuffer sqeContextBuffer_;
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

// 测试用例1：正常场景 - 不同算子的任务
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_MultiOpsInQueue_Expect_PrintCorrectly)
{
    // Arrange
    u32 sqIdx = 50;  // 从中间开始
    
    // 设置前100个任务为Op1，后100个为Op2
    SetContinuousOpInfos(0, 100, 1);   // Op1
    SetContinuousOpInfos(100, 100, 2); // Op2
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 验证打印了正确的算子信息和任务序列
    // Google Mock会自动验证GetTaskBriefsInfo的调用
}

// 测试用例2：边界场景 - 200个任务都属于同一个算子
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_SingleOpWith200Tasks_Expect_Print4Lines)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 所有200个任务都属于Op1
    for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
        SetIndOpInfo(i, 1);  // 全部是Op1
    }
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 预期打印行数：200/50 = 4行，每行都是Op1的任务
    // 验证GetTaskBriefsInfo被调用了200次
}

// 测试用例3：边界场景 - 在行末切换算子
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_OpChangeAtLineEnd_Expect_CorrectLineSplit)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 前50个任务是Op1（正好一行）
    SetContinuousOpInfos(0, 50, 1);
    // 第51个任务是Op2
    SetIndOpInfo(50, 2);
    // 后面149个任务是Op2
    SetContinuousOpInfos(51, 149, 2);
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 预期：第一行打印Op1的50个任务，第二行开始打印Op2的任务
}

// 测试用例4：边界场景 - 在行中切换算子
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_OpChangeInMiddleOfLine_Expect_ImmediateLineSplit)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 前30个任务是Op1
    SetContinuousOpInfos(0, 30, 1);
    // 第31个任务切换到Op2
    SetIndOpInfo(30, 2);
    // 后面都是Op2
    SetContinuousOpInfos(31, 169, 2);
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 预期：第一行打印Op1的30个任务，第二行开始打印Op2的任务
    // 不会等到50个才换行
}

// 测试用例5：边界场景 - 算子频繁切换
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_OpsFrequentlyChange_Expect_PrintManyLines)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 每个任务都切换算子
    for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
        SetIndOpInfo(i, i % 10);  // 10个算子循环
    }
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 预期：每个任务都会触发一次打印，因为算子一直在变
    // 总共会打印200行，每行只有一个任务
}

// 测试用例6：异常场景 - 无效的sqIdx
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_InvalidSqIdx_Expect_HandleGracefully)
{
    // Arrange
    u32 sqIdx = HCCL_SQE_MAX_CNT + 10;  // 无效索引
    
    // 设置一些基本数据
    SetIndOpInfo(0, 1);
    
    // Act & Assert
    EXPECT_NO_THROW({
        taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    });
}

// 测试用例7：异常场景 - 空指针
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_NullBuffer_Expect_NoCrash)
{
    // Arrange
    u32 sqIdx = 0;
    
    // Act & Assert
    EXPECT_NO_THROW({
        taskException_->PrintTaskExceptionTaskQue(sqIdx, nullptr);
    });
}

// 测试用例8：边界场景 - 从不同起始位置遍历
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_StartFromDifferentPosition_Expect_CorrectCircularTraversal)
{
    // 测试从环形缓冲区的不同位置开始遍历
    
    std::vector<u32> startPositions = {0, 50, 100, 150, 199};
    
    for (u32 startIdx : startPositions) {
        // Arrange
        // 清空并重新设置数据
        taskException_->indOpInfos_.clear();
        
        // 设置简单的算子模式：前一半Op1，后一半Op2
        SetContinuousOpInfos(0, 100, 1);
        SetContinuousOpInfos(100, 100, 2);
        
        // Act
        taskException_->PrintTaskExceptionTaskQue(startIdx, &sqeContextBuffer_);
        
        // Assert
        // 验证从不同起始位置都能正确遍历200个任务
    }
}

// 测试用例9：验证GetTaskBriefsInfo调用次数
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_Normal_Expect_GetTaskBriefsInfoCalled200Times)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 设置200个任务都是Op1
    SetContinuousOpInfos(0, HCCL_SQE_MAX_CNT, 1);
    
    // 设置期望：GetTaskBriefsInfo应该被调用200次
    EXPECT_CALL(*taskException_, GetTaskBriefsInfo(_, &sqeContextBuffer_))
        .Times(HCCL_SQE_MAX_CNT)
        .WillRepeatedly(Return("MockTask"));
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
}

// 测试用例10：性能测试
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_LargeData_Expect_GoodPerformance)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 设置复杂的数据模式
    for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
        SetIndOpInfo(i, i / 20);  // 每20个任务一个算子
    }
    
    // Act
    auto start = std::chrono::high_resolution_clock::now();
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Assert
    EXPECT_LT(duration.count(), 100);  // 应该在100ms内完成
}

// 测试用例11：边界场景 - 只有少量任务
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_FewTasks_Expect_PrintCorrectly)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 只设置前10个任务，其他都是默认值
    for (u32 i = 0; i < 10; i++) {
        SetIndOpInfo(i, 1);
    }
    
    // Act
    taskException_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
    
    // Assert
    // 应该只打印包含10个任务的一行
}

// 测试用例12：验证PrintTaskExceptionOpInfo调用
TEST_F(TaskExceptionTest, PrintTaskExceptionTaskQue_When_OpChanges_Expect_PrintOpInfo)
{
    // Arrange
    u32 sqIdx = 0;
    
    // 创建Mock对象来验证PrintTaskExceptionOpInfo的调用
    MockTaskException mockTaskException;
    
    // 设置数据：两个不同的算子
    SetIndOpInfo(0, 1);
    SetIndOpInfo(1, 2);
    
    // 期望PrintTaskExceptionOpInfo被调用两次（每个算子一次）
    EXPECT_CALL(mockTaskException, PrintTaskExceptionOpInfo(_))
        .Times(2);
    
    // Act
    mockTaskException.PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_);
}

// 测试类定义
class HcclCommAicpuTest : public testing::Test {
protected:
    void SetUp() override {
        // 创建mock对象
        deviceLogicId_ = 0;
        hcclCommAicpu_ = std::make_unique<HcclCommAicpu>(deviceLogicId_);
        
        // 初始化共享数据
        InitAicpuShareData();
    }
    
    void TearDown() override {
        hcclCommAicpu_.reset();
        GlobalMockObject::verify();
    }
    
    // 辅助函数：初始化Aicpu共享数据
    void InitAicpuShareData() {
        // 初始化ring buffer
        for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
            sqeContextBuffer_.rtsDfxInfo[i].opRingBufferIdx = i;
            sqeContextBuffer_.rtsDfxInfo[i].sqHead = i;
            sqeContextBuffer_.rtsDfxInfo[i].taskType = 0;
        }
        
        // 设置aicpu共享数据
        hcclCommAicpu_->SetAicpuShareData(&aicpuShareData_);
    }
    
    // 辅助函数：设置OpInfo
    void SetupOpInfo(u32 ringBufferIdx, u32 opIndex, const std::string& tag) {
        AicpuOpInfo opInfo;
        opInfo.opIndex = opIndex;
        strncpy_s(opInfo.tagBuff, tag.c_str(), sizeof(opInfo.tagBuff) - 1);
        EXPECT_CALL(aicpuShareData_, GetAicpuOpInfo(ringBufferIdx))
            .WillRepeatedly(Return(&opInfo));
    }
    
    // 辅助函数：设置GetTaskBriefsInfo返回值
    void SetupTaskBriefInfo(u32 sqIdx, const std::string& briefInfo) {
        EXPECT_CALL(*hcclCommAicpu_, GetTaskBriefsInfo(sqIdx, &sqeContextBuffer_))
            .WillRepeatedly(Return(briefInfo));
    }
    
    u32 deviceLogicId_;
    std::unique_ptr<HcclCommAicpu> hcclCommAicpu_;
    SqeRingBuffer sqeContextBuffer_;
    MockAicpuShareData aicpuShareData_; // 需要创建mock类
};

// 测试用例1：正常场景 - 不同算子的任务
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_MultiOpsInQueue_Expect_PrintAllTasks)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = false;
    
    // 设置前100个任务属于Op1，后100个属于Op2
    for (u32 i = 0; i < 100; i++) {
        SetupOpInfo(i, 1, "Op1");
        SetupTaskBriefInfo(i, "Task" + std::to_string(i));
    }
    for (u32 i = 100; i < 200; i++) {
        SetupOpInfo(i, 2, "Op2");
        SetupTaskBriefInfo(i, "Task" + std::to_string(i));
    }
    
    // 设置最后一个op信息
    SetupOpInfo(sqeContextBuffer_.rtsDfxInfo[sqIdx].opRingBufferIdx, 2, "Op2");
    
    // Act
    hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    
    // Assert
    // 验证打印函数被调用
    GlobalMockObject::verify();
}

// 测试用例2：边界场景 - 队列填满且都是同一个算子的任务
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_SingleOpWith200Tasks_Expect_PrintMultipleLines)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = false;
    
    // 设置200个任务都属于同一个Op
    for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
        SetupOpInfo(i, 1, "SingleOp");
        SetupTaskBriefInfo(i, "Task" + std::to_string(i));
    }
    
    // 设置最后一个op信息
    SetupOpInfo(sqeContextBuffer_.rtsDfxInfo[sqIdx].opRingBufferIdx, 1, "SingleOp");
    
    // Act
    hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    
    // Assert
    // 预期会打印4行（200/50=4），每行有OP信息+50个task
    GlobalMockObject::verify();
}

// 测试用例3：监控模式测试
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_IsMonitorTrue_Expect_UseRunInfoInsteadOfError)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = true; // 监控模式
    
    // 设置简单的测试数据
    SetupOpInfo(0, 1, "MonitorOp");
    SetupTaskBriefInfo(0, "MonitorTask");
    SetupOpInfo(1, 1, "MonitorOp");
    SetupTaskBriefInfo(1, "MonitorTask");
    
    SetupOpInfo(sqeContextBuffer_.rtsDfxInfo[sqIdx].opRingBufferIdx, 1, "MonitorOp");
    
    // Act
    hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    
    // Assert
    // 验证使用的是RUN_INFO级别而不是ERROR级别
    GlobalMockObject::verify();
}

// 测试用例4：异常场景 - 空指针检查
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_OpInfoIsNull_Expect_ReturnWithoutCrash)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = false;
    
    // 设置GetAicpuOpInfo返回空指针
    EXPECT_CALL(aicpuShareData_, GetAicpuOpInfo(_))
        .WillRepeatedly(Return(nullptr));
    
    // Act & Assert
    EXPECT_NO_THROW({
        hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    });
}

// 测试用例5：边界场景 - 算子切换边界测试
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_OpChangesAtLineBoundary_Expect_CorrectLineSplit)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = false;
    
    // 设置前50个任务属于Op1，第51个任务切换到Op2
    for (u32 i = 0; i < 50; i++) {
        SetupOpInfo(i, 1, "Op1");
        SetupTaskBriefInfo(i, "Task" + std::to_string(i));
    }
    
    SetupOpInfo(50, 2, "Op2");
    SetupTaskBriefInfo(50, "Task50");
    
    SetupOpInfo(sqeContextBuffer_.rtsDfxInfo[sqIdx].opRingBufferIdx, 2, "Op2");
    
    // Act
    hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    
    // Assert
    // 预期第一行打印Op1的50个任务，第二行打印Op2的任务
    GlobalMockObject::verify();
}

// 测试用例6：性能测试 - 大量数据时的执行时间
TEST_F(HcclCommAicpuTest, PrintTaskExceptionTaskQue_When_LargeAmountOfData_Expect_NotStuck)
{
    // Arrange
    u32 sqIdx = 0;
    bool isMonitor = false;
    
    // 设置所有200个任务
    for (u32 i = 0; i < HCCL_SQE_MAX_CNT; i++) {
        SetupOpInfo(i, i % 10, "Op" + std::to_string(i % 10));
        SetupTaskBriefInfo(i, "Task" + std::to_string(i));
    }
    
    SetupOpInfo(sqeContextBuffer_.rtsDfxInfo[sqIdx].opRingBufferIdx, 9, "Op9");
    
    // Act
    auto start = std::chrono::high_resolution_clock::now();
    hcclCommAicpu_->PrintTaskExceptionTaskQue(sqIdx, &sqeContextBuffer_, isMonitor);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Assert
    // 预期执行时间不超过100ms
    EXPECT_LT(duration.count(), 100);
    GlobalMockObject::verify();
}