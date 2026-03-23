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
#include "tcp_recv_task.h"
#undef protected
#undef private
#include "llt_hccl_stub_pub.h"
#include "llt_hccl_stub_gdr.h"
#include <sys/mman.h>
#include <fcntl.h>
#include "hccl_comm_pub.h"
#include "rank_consistentcy_checker.h"
#include "transport_heterog_event_tcp_pub.h"

#define SOCK_ESOCKCLOSED   128207   /* ESOCKCLOSED：socket has been closed */
#define SOCK_EAGAIN    128201
constexpr u32 SOCKET_NUM_PER_LINK = 3;

class MPI_RecvTask_Test : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "MPI_RecvTask_Test SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "MPI_RecvTask_Test TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        static s32  call_cnt = 0;
        std::string name =std::to_string(call_cnt++) +"_" + __PRETTY_FUNCTION__;
        ra_set_shm_name(name .c_str());
        ra_set_test_type(1, "st_MPI_TEST");
        s32 portNum = -1;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "MPI_RecvTask_Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "MPI_RecvTask_Test TearDown" << std::endl;
    }
};

TEST_F(MPI_RecvTask_Test, ut_recv_RecvDataCb)
{
    MOCKER_CPP(&TcpRecvTask::RecvData)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    void *fdHandle = nullptr;
    TcpRecvTask::GetRecvTaskInstance()->RecvDataCb(fdHandle);
    u32 fd = 0;
    fdHandle = &fd;
    TcpRecvTask::GetRecvTaskInstance()->RecvDataCb(fdHandle);
    GlobalMockObject::verify();
}

TEST_F(MPI_RecvTask_Test, ut_recv_task_init)
{
    HCCL_INFO("st_recv_task_init");
    MOCKER(hrtSetRecvDataCallback)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    SocketInfoT socketInfo;
    void* transportPtr = nullptr;
    TcpRecvTask::GetRecvTaskInstance()->initCount_ = 0;
    std::unique_lock<std::mutex> lock(TcpRecvTask::GetRecvTaskInstance()->transportMapMutex_);
    TcpRecvTask::GetRecvTaskInstance()->fdTransportMap_.clear();
    lock.unlock();
    HcclResult ret = TcpRecvTask::GetRecvTaskInstance()->Init(socketInfo, transportPtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(MPI_RecvTask_Test, ut_recv_Deinit)
{
    TcpRecvTask::GetRecvTaskInstance()->initCount_ = 1;
    HcclResult ret = TcpRecvTask::GetRecvTaskInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = TcpRecvTask::GetRecvTaskInstance()->Deinit();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(MPI_RecvTask_Test, ut_recv_task_RecvData)
{
    HcclResult ret;
    struct socket_peer_info fdHandle;
    fdHandle.fd = 1;
    fdHandle.phy_id = 1;
    void *fdHandlePtr = &fdHandle;

    HcclIpAddress invalidIp;
    TransportHeterogEventTcp Transport("tag", invalidIp, invalidIp, 18000, 0, 0, TransportResourceInfo());
    void *transportPtr = &Transport;

    s32 returnData = 0;
    MOCKER(hrtRaSocketBlockRecv)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::ReportEnvelpComp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    TcpRecvTask::GetRecvTaskInstance()->fdTransportMap_[fdHandlePtr] = transportPtr;
    TcpRecvTask::GetRecvTaskInstance()->envelopMap_.clear();
    ret = TcpRecvTask::GetRecvTaskInstance()->RecvData(fdHandlePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    u32 recvData = 1;
    TransData transData;
    transData.count = 1;
    transData.dataType = HCCL_DATA_TYPE_INT8;
    transData.srcBuf = reinterpret_cast<u64>(&recvData);
    HcclRequestInfo request;
    request.transportHandle = transportPtr;
    request.transportRequest.transData = transData;
    RecvRecord recvRecord;
    recvRecord.buffer = &recvData;
    recvRecord.size = 1;
    TcpRecvTask::GetRecvTaskInstance()->recvTaskMap_[fdHandlePtr] = std::make_pair(&request, recvRecord);
    TcpRecvTask::GetRecvTaskInstance()->fdTransportMap_[fdHandlePtr] = transportPtr;
    EnvelopStatusFlag statusFlag;
    statusFlag.flag = true;
    TcpRecvTask::GetRecvTaskInstance()->envelopMap_[fdHandlePtr] = statusFlag;

    u64 recvSize = 1;
    returnData = 0;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any(), any(), any(), outBoundP(&recvSize, sizeof(recvSize)))
    .will(returnValue(returnData));
    MOCKER(hrtEpollCtlMod)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&TransportHeterogEventTcp::ReportRecvComp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));

    ret = TcpRecvTask::GetRecvTaskInstance()->RecvData(fdHandlePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    TcpRecvTask::GetRecvTaskInstance()->recvTaskMap_[fdHandlePtr] = std::make_pair(&request, recvRecord);
    statusFlag.flag = true;
    TcpRecvTask::GetRecvTaskInstance()->envelopMap_[fdHandlePtr] = statusFlag;
    recvSize = 0;
    returnData = 0;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any(), any(), any(), outBoundP(&recvSize, sizeof(recvSize)))
    .will(returnValue(returnData));
    ret = TcpRecvTask::GetRecvTaskInstance()->RecvData(fdHandlePtr);
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    GlobalMockObject::verify();

    returnData = SOCK_EAGAIN;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any(), any(), any(), outBoundP(&recvSize, sizeof(recvSize)))
    .will(returnValue(returnData));
    ret = TcpRecvTask::GetRecvTaskInstance()->RecvData(fdHandlePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();

    returnData = SOCK_ENOENT;
    MOCKER(hrtRaSocketRecv)
    .stubs()
    .with(any(), any(), any(), outBoundP(&recvSize, sizeof(recvSize)))
    .will(returnValue(returnData));
    ret = TcpRecvTask::GetRecvTaskInstance()->RecvData(fdHandlePtr);
    EXPECT_EQ(ret, HCCL_E_TCP_TRANSFER);
    GlobalMockObject::verify();
}

TEST_F(MPI_RecvTask_Test, ut_recv_task_SetRecvTask)
{
    struct socket_peer_info fdHandle;
    fdHandle.fd = 1;
    fdHandle.phy_id = 1;
    void *fdHandlePtr = &fdHandle;
    HcclIpAddress invalidIp;
    TransportHeterogEventTcp Transport("tag", invalidIp, invalidIp, 18000, 0, 0, TransportResourceInfo());
    void *transportPtr = &Transport;

    u32 recvData = 1;
    TransData transData;
    transData.count = 1;
    transData.dataType = HCCL_DATA_TYPE_INT8;
    transData.srcBuf = reinterpret_cast<u64>(&recvData);
    HcclRequestInfo request;
    request.transportHandle = transportPtr;
    request.transportRequest.transData = transData;

    MOCKER(hrtEpollCtlMod)
    .stubs()
    .with(any())
    .will(returnValue(0));
    HcclResult ret = TcpRecvTask::GetRecvTaskInstance()->SetRecvTask(fdHandlePtr, &request);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    request.transportRequest.transData.count = 0;
    MOCKER_CPP(&TransportHeterogEventTcp::ReportRecvComp)
    .stubs()
    .with(any())
    .will(returnValue(HCCL_SUCCESS));
    ret = TcpRecvTask::GetRecvTaskInstance()->SetRecvTask(fdHandlePtr, &request);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    GlobalMockObject::verify();
}