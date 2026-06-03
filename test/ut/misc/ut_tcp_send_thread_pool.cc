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
#include <stdlib.h>
#include <pthread.h>

#include <assert.h>
#include <semaphore.h>
#include <signal.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <securec.h>

#include <sys/types.h>
#include <stddef.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <driver/ascend_hal.h>

#include "dlra_function.h"
#include "hccl/base.h"
#include <hccl/hccl_types.h>
#define private public
#define protected public
#include "tcp_send_thread_pool.h"
#include "transport_heterog_event_tcp_pub.h"
#undef protected
#undef private
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "rank_consistentcy_checker.h"
#include "transport_heterog_event_tcp_pub.h"

class MPI_SendThreadPool_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_SendThreadPool_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_SendThreadPool_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        std::string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "UT_MPI_TEST");

        std::cout << "MPI_SendThreadPool_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "MPI_SendThreadPool_Test TearDown" << std::endl;
    }
};

HcclRequestInfo request;
TcpSendThreadPool::TagTaskQueue *currentThreadTaskQueue = nullptr;
TcpSendThreadPool::TagTaskQueue queue1;
TcpSendThreadPool::TagTaskQueue queue2;
std::pair<TcpSendThreadPool::TagTaskQueue *, TcpSendThreadPool::TagTaskQueue *> taskQueues(&queue1, &queue2);

TEST_F(MPI_SendThreadPool_Test, ut_SendThreadPool_Init_Bind_CPU)
{
    GlobalMockObject::verify();
    DlHalFunction::GetInstance().DlHalFunctionInit();
    MOCKER_CPP(&TcpSendThreadPool::RunTask)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    bool envCompleted = true;
    bool tranCompleted = true;
    MOCKER_CPP(&TransportHeterogEventTcp::SendNoBlock)
    .stubs()
    .with(any(), any(), any(), any(), outBound(envCompleted), outBound(tranCompleted))
    .will(returnValue(HCCL_SUCCESS));

    u32 devId = 0;
    HcclResult ret = TcpSendThreadPool::GetSendPoolInstance()->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    SaluSleep(500);
    ret = TcpSendThreadPool::GetSendPoolInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_SendThreadPool_Test, ut_GetThreadNum)
{

    u32 output = 0;
    MOCKER_CPP(&TcpSendThreadPool::RunTask)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtDrvGetPlatformInfo)
    .stubs()
    .with(outBoundP(&output, sizeof(output)))
    .will(returnValue(HCCL_SUCCESS));

    TcpSendThreadPool::GetSendPoolInstance()->threadNum_ = 0;
    u32 devId = 0;
    HcclResult ret = TcpSendThreadPool::GetSendPoolInstance()->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TcpSendThreadPool::GetSendPoolInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_SendThreadPool_Test, ut_ThreadTaskQueueAddTask)
{
    u32 devId = 0;
    u32 tag = 0;
    u32 threadSerial = 0;
    HcclResult ret = TcpSendThreadPool::GetSendPoolInstance()->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    HCCL_ERROR("TcpSendThreadPool::GetSendPoolInstance() threadNum[%u]", TcpSendThreadPool::GetSendPoolInstance()->threadNum_);

    TcpSendThreadPool::GetSendPoolInstance()->TaskQueueManager_[threadSerial].threadTaskQueuePtr = taskQueues.first;
    TcpSendThreadPool::TagTaskQueue *ptr = TcpSendThreadPool::GetSendPoolInstance()->TaskQueueManager_[threadSerial].threadTaskQueuePtr;
    auto &&iter = ptr->emplace(tag, std::queue<HcclRequestInfo *>());
    iter.first->second.push(&request);
    bool queueEmpty = TcpSendThreadPool::GetSendPoolInstance()->ThreadTaskQueueAddTask(threadSerial, currentThreadTaskQueue);
    EXPECT_EQ(queueEmpty, false);

    ret = TcpSendThreadPool::GetSendPoolInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_SendThreadPool_Test, ut_LoadBalancing)
{
    u32 devId = 0;
    HcclResult ret = TcpSendThreadPool::GetSendPoolInstance()->Init(devId);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    bool envCompleted = true;
    bool tranCompleted = true;
    MOCKER_CPP(&TransportHeterogEventTcp::SendNoBlock)
    .stubs()
    .with(any(), any(), any(), any(), outBound(envCompleted), outBound(tranCompleted))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&TransportHeterogEventTcp::ReportSendComp)
    .stubs()
    .with()
    .will(returnValue(HCCL_SUCCESS));

    ret = TcpSendThreadPool::GetSendPoolInstance()->LoadBalancing(currentThreadTaskQueue);
    ret = TcpSendThreadPool::GetSendPoolInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}