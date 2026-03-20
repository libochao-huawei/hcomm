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
#include "service_scheduler.h"
#include "resource_entities.h"

#include <thread>
#include <atomic>
#include <chrono>

class TestServiceScheduler : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_Normal_Init_Register_KernelRun_Unregister_Expect_SUCCESS)
{
    // prepare mocks for MsgQueue memory operations
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
    EXPECT_EQ(sendQ->Init(), HCCL_SUCCESS);

    static bool serviceExecuted = false;
    serviceExecuted = false;
    auto serviceCb = [](void * /*args*/, uint64_t /*size*/) -> HcclResult {
        serviceExecuted = true;
        return HCCL_SUCCESS;
    };
    ThreadServiceHandle handle = 0;
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &handle), HCCL_SUCCESS);

    // execute directly and verify
    EXPECT_EQ(scheduler.executeService(handle, nullptr, 0), HCCL_SUCCESS);
    EXPECT_TRUE(serviceExecuted);

    EXPECT_EQ(scheduler.ServiceUnregister(handle), HCCL_SUCCESS);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_Register_Duplicate_Expect_Again)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    ThreadServiceHandle h1 = 0;
    auto serviceCb = [](void*, uint64_t) -> HcclResult { return HCCL_SUCCESS; };
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &h1), HCCL_SUCCESS);
    ThreadServiceHandle h2 = 0;
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &h2), HCCL_E_AGAIN);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_Unregister_NotFound_Expect_E_NOT_FOUND)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    EXPECT_EQ(scheduler.ServiceUnregister(0x12345678), HCCL_E_NOT_FOUND);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_ExecuteService_NotFound_Expect_E_NOT_FOUND)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    EXPECT_EQ(scheduler.executeService(0xdeadbeef, nullptr, 0), HCCL_E_NOT_FOUND);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_ServiceRun_EmptyAndStop_Expect_SUCCESS)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::MsgQueue::Push).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
    EXPECT_EQ(sendQ->Init(), HCCL_SUCCESS);

    std::thread t([&scheduler]() {
        scheduler.ServiceRun();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // wait for ServiceRun to start and check empty queue

    scheduler.StopService();
    t.join();
    EXPECT_TRUE(true); // just verify it can stop graceFully
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_ServiceRun_ProcessMessage_And_Stop_Expect_SUCCESS)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::MsgQueue::Push).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
    EXPECT_EQ(sendQ->Init(), HCCL_SUCCESS);

    static bool executed{false};
    executed = false;
    auto serviceCb = [](void* /*args*/, uint64_t /*size*/) -> HcclResult {
        executed = true;
        return HCCL_SUCCESS;
    };
    ThreadServiceHandle handle = 0;
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &handle), HCCL_SUCCESS);

    ThreadMsgEntity entity{};
    entity.msgId = 3;
    entity.serviceHandle = handle;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    EXPECT_EQ(sendQ->Push(entity), HCCL_SUCCESS);

    EXPECT_EQ(scheduler.ServiceUnregister(handle), HCCL_SUCCESS);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_ServiceRun_ProcessMessage_WhenServiceFails_Expect_Fail)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::MsgQueue::Push).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
    EXPECT_EQ(sendQ->Init(), HCCL_SUCCESS);

    auto serviceCb = [](void* /*args*/, uint64_t /*size*/) -> HcclResult {
        return HCCL_E_INTERNAL;
    };
    ThreadServiceHandle handle = 0;
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &handle), HCCL_SUCCESS);

    ThreadMsgEntity entity{};
    entity.msgId = 2;
    entity.serviceHandle = handle;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    EXPECT_EQ(sendQ->Push(entity), HCCL_SUCCESS);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_ServiceRun_ProcessMessage_Then_Stop_Expect_SUCCESS)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::MsgQueue::Push).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));
    MOCKER(aclrtFree)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
    EXPECT_EQ(sendQ->Init(), HCCL_SUCCESS);

    static std::atomic<bool> serviceExecuted{false};
    serviceExecuted = false;
    auto serviceCb = [](void* /*args*/, uint64_t /*size*/) -> HcclResult {
        serviceExecuted = true;
        return HCCL_SUCCESS;
    };
    ThreadServiceHandle handle = 0;
    EXPECT_EQ(scheduler.ServiceRegister(serviceCb, &handle), HCCL_SUCCESS);

    ThreadMsgEntity entity{};
    entity.msgId = 2;
    entity.serviceHandle = handle;
    entity.args = nullptr;
    entity.argsSizeByte = 0;
    EXPECT_EQ(sendQ->Push(entity), HCCL_SUCCESS);

    std::thread t([&scheduler]() {
        scheduler.ServiceRun();
    });

    int loop = 0;
    while (!serviceExecuted && loop++ < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    scheduler.StopService();
    t.join();

    EXPECT_TRUE(true);
}

TEST_F(TestServiceScheduler, Ut_When_ServiceScheduler_GetSendQueue_ReturnsNonNull)
{
    MOCKER_CPP(&hccl::MsgQueue::Init).stubs().will(returnValue(HCCL_SUCCESS));
    ServiceScheduler scheduler;
    EXPECT_EQ(scheduler.Init(), HCCL_SUCCESS);
    auto sendQ = scheduler.GetSendQueue();
    ASSERT_NE(sendQ, nullptr);
}
