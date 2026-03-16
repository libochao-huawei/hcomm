/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../hccl_api_base_test.h"
#include "msg_queue.h"
#include "resource_entities.h"
#include "acl/acl_rt.h"

class TestMsgQueue : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestMsgQueue, Ut_When_MsgQueue_Push_And_Pop_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    ret = msgQueue.Push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity poppedEntity;
    ret = msgQueue.Pop(poppedEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestMsgQueue, Ut_When_MsgQueue_Pop_While_Empty_Expect_Return_ERROR)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&hccl::MsgQueue::Empty).stubs().with(outBound(true)).will(returnValue(HCCL_SUCCESS));
    ThreadMsgEntity entity;
    ret = msgQueue.Pop(entity);
    EXPECT_EQ(ret, HCCL_E_AGAIN);  // 队列为空
}

TEST_F(TestMsgQueue, Ut_When_MsgQueue_Push_While_Full_Expect_Return_ERROR)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER_CPP(&hccl::MsgQueue::Full).stubs().with(outBound(true)).will(returnValue(HCCL_SUCCESS));
    ThreadMsgEntity entity;
    ret = msgQueue.Push(entity);
    EXPECT_EQ(ret, HCCL_E_AGAIN);  // 队列已满
}

TEST_F(TestMsgQueue, Ut_When_MsgQueue_Push_Or_Pop_Uninitialized_Expect_Return_HCCL_E_PARA)
{
    MOCKER_CPP(aclrtMalloc).stubs().will(returnValue(ACL_SUCCESS));
    MOCKER_CPP(aclrtMemcpy).stubs().will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue;
    ThreadMsgEntity entity;
    HcclResult ret = msgQueue.Push(entity);
    EXPECT_EQ(ret, HCCL_E_PARA);
    ret = msgQueue.Pop(entity);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestMsgQueue, Ut_When_MsgQueue_Empty_On_Normal_Expect_Correct)
{
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.Push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    bool isEmpty = false;
    ret = msgQueue.Empty(isEmpty);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(isEmpty);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Full_On_Normal_Expect_Correct)
{
    MsgQueue msgQueue(2, sizeof(ThreadMsgEntity)); // 容量2, 实际可用容量为1
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isFull = false;
    ret = msgQueue.Full(isFull);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(isFull);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Push_MemcpyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_ERROR_INVALID_PARAM));  // 第一次成功，第二次失败
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue(1024, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.Push(entity);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_GetQueueInfo_On_Normal_Expect_Correct)
{
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    hccl::QueueInfo info = msgQueue.GetQueueInfo();
    EXPECT_EQ(info.capacity, 256);
    EXPECT_EQ(info.msgSize, sizeof(ThreadMsgEntity));
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Multiple_Push_And_Pop_Expect_Correct)
{
    MsgQueue msgQueue(256, sizeof(ThreadMsgEntity));
    HcclResult ret = msgQueue.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 连续 Push 10 条消息
    for (uint32_t i = 0; i < 10; ++i) {
        ThreadMsgEntity entity;
        entity.msgId = i;
        entity.serviceHandle = i * 100;
        entity.args = nullptr;
        entity.argsSizeByte = 0;

        ret = msgQueue.Push(entity);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    // 连续 Pop 10 条消息
    for (uint32_t i = 0; i < 10; ++i) {
        ThreadMsgEntity entity;
        ret = msgQueue.Pop(entity);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
}