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
#include "local_notify_impl.h"
#include "llt_hccl_stub_rank_graph.h"

#include <thread>
#include <atomic>
#include <chrono>

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

TEST_F(TestMsgQueue, Ut_MsgQueue_Init_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Push_And_Pop_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));
    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity poppedEntity;
    ret = msgQueue.pop(poppedEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(poppedEntity.msgId, entity.msgId);
    EXPECT_EQ(poppedEntity.serviceHandle, entity.serviceHandle);
    EXPECT_EQ(poppedEntity.args, entity.args);
    EXPECT_EQ(poppedEntity.argsSizeByte, entity.argsSizeByte);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Pop_While_Empty_Expect_Return_ERROR)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    ret = msgQueue.pop(entity);
    EXPECT_EQ(ret, HCCL_E_AGAIN);  // 队列为空
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Push_While_Full_Expect_Return_ERROR)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1);  // 小容量以便快速填满
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    // 填满队列
    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 再次push应该失败
    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_E_AGAIN);  // 队列为满
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Init_Failed_On_aclrtMallocFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Push_Uninitialized_Expect_Return_HCCL_E_PARA)
{
    MsgQueue msgQueue;
    ThreadMsgEntity entity;
    HcclResult ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Pop_Uninitialized_Expect_Return_HCCL_E_PARA)
{
    MsgQueue msgQueue;
    ThreadMsgEntity entity;
    HcclResult ret = msgQueue.pop(entity);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Push_MemcpyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS))
        .then(returnValue(ACL_ERROR_INVALID_PARAM));  // 第一次成功，第二次失败
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Pop_MemcpyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            if (count == sizeof(uint64_t) && kind == ACL_MEMCPY_DEVICE_TO_HOST) {
                return ACL_ERROR_INVALID_PARAM;  // 模拟memcpy失败
            }
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity poppedEntity;
    ret = msgQueue.pop(poppedEntity);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Empty_On_Normal_Expect_Correct)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isEmpty = false;
    ret = msgQueue.empty(isEmpty);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(isEmpty);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = msgQueue.empty(isEmpty);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(isEmpty);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Full_On_Normal_Expect_Correct)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1);  // 容量1
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isFull = false;
    ret = msgQueue.full(isFull);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_FALSE(isFull);

    ThreadMsgEntity entity;
    entity.msgId = 1;
    entity.serviceHandle = 2;
    entity.args = nullptr;
    entity.argsSizeByte = 0;

    ret = msgQueue.push(entity);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = msgQueue.full(isFull);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(isFull);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Empty_MemcpyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isEmpty = false;
    ret = msgQueue.empty(isEmpty);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    EXPECT_TRUE(isEmpty);  // 失败时设为true
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Full_MemcpyFailed_Expect_Return_HCCL_E_RUNTIME)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool isFull = false;
    ret = msgQueue.full(isFull);
    EXPECT_EQ(ret, HCCL_E_RUNTIME);
    EXPECT_TRUE(isFull);  // 失败时设为true
}

TEST_F(TestMsgQueue, Ut_MsgQueue_GetQueueInfo_On_Normal_Expect_Correct)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    QueueInfo info = msgQueue.GetQueueInfo();
    EXPECT_EQ(info.capacity, 1024);
    EXPECT_EQ(info.msgSize, sizeof(ThreadMsgEntity));
    EXPECT_NE(info.addr, 0);
    EXPECT_NE(info.headIdxAddr, 0);
    EXPECT_NE(info.tailIdxAddr, 0);
}

TEST_F(TestMsgQueue, Ut_MsgQueue_Circular_Buffer_Expect_Correct)
{
    MOCKER(aclrtMalloc)
        .stubs()
        .will(Invoke([](void **devPtr, size_t size, uint32_t flag) {
            *devPtr = malloc(size);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(Invoke([](void *dst, size_t destMax, const void *src, size_t count, aclrtMemcpyKind kind) {
            memcpy(dst, src, count);
            return ACL_SUCCESS;
        }));
    MOCKER(aclrtFree)
        .stubs()
        .will(Invoke([](void *devPtr) {
            free(devPtr);
            return ACL_SUCCESS;
        }));

    MsgQueue msgQueue;
    HcclResult ret = msgQueue.Init(2);  // 小容量测试循环
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ThreadMsgEntity entity1;
    entity1.msgId = 1;
    entity1.serviceHandle = 2;
    entity1.args = nullptr;
    entity1.argsSizeByte = 0;

    ThreadMsgEntity entity2;
    entity2.msgId = 2;
    entity2.serviceHandle = 3;
    entity2.args = nullptr;
    entity2.argsSizeByte = 0;

    // push两个
    ret = msgQueue.push(entity1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = msgQueue.push(entity2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // pop一个
    ThreadMsgEntity popped;
    ret = msgQueue.pop(popped);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(popped.msgId, 1);

    // push第三个，测试循环
    ThreadMsgEntity entity3;
    entity3.msgId = 3;
    entity3.serviceHandle = 4;
    entity3.args = nullptr;
    entity3.argsSizeByte = 0;

    ret = msgQueue.push(entity3);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // pop第二个
    ret = msgQueue.pop(popped);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(popped.msgId, 2);

    // pop第三个
    ret = msgQueue.pop(popped);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(popped.msgId, 3);
}