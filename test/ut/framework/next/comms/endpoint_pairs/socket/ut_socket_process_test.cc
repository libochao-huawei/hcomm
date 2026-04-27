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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>

#include "../../../ut_hcomm_base.h"
#define private public
#include "socket_process.h"
#undef private
#include "log.h"
#include "hccl_comm_socket_c_adpt.h"
#include "sal_pub.h"
#include "hccl_types.h"

using namespace hcomm;

class TestSocketProcess : public TestHcommCAdptBase {
public:
    TestSocketProcess() {
        InitSocketDesc();
    }
    ~TestSocketProcess() {
        if (socketHandle != nullptr) {
            hcomm::SocketProcess::GetInstance(0).DestroySocketHandle(socketHandle);
            socketHandle = nullptr;
        }
    }
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
        
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
        GlobalMockObject::verify();
    }

    void InitSocketDesc() {
        // 初始化socketDesc
        string testTag = "socket_test_tag";
        struct in_addr localAddr;
        struct in_addr remoteAddr;
        inet_pton(AF_INET, "127.0.0.1", &localAddr);
        TestSocketProcess::socketDesc.localEndpoint.commAddr.addr = localAddr;
        inet_pton(AF_INET, "192.186.0.1", &remoteAddr);
        TestSocketProcess::socketDesc.remoteEndpoint.commAddr.addr = remoteAddr;
        s32 ret = memcpy_s(TestSocketProcess::socketDesc.tag, HCCL_SOCKET_TAG_LEN, testTag.c_str(), testTag.length() + 1);
        EXPECT_EQ(ret, EOK);
        TestSocketProcess::socketDesc.role = HCOMM_SOCKET_ROLE_CLIENT;
        TestSocketProcess::socketDesc.listenPort = 8080;
    }

    static HcclCommSocketHandle socketHandle;
    static HcclCommSocketDesc socketDesc;
};

HcclCommSocketHandle TestSocketProcess::socketHandle = nullptr;
HcclCommSocketDesc TestSocketProcess::socketDesc{};

TEST_F(TestSocketProcess, Ut_TestGetSocket_When_Return_ERROR)
{
    HcclCommSocketDesc *tempSocketDesc = nullptr;
    HcclCommSocketHandle tempSocketHandle = nullptr;
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).GetSocket(tempSocketDesc, tempSocketHandle);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestSocketProcess, Ut_TestGetSocket_When_Return_Success)
{
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).GetSocket(&TestSocketProcess::socketDesc, TestSocketProcess::socketHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestSocketProcess, Ut_TestGetStatus_When_Return_ERROR)
{
    HcclCommSocketHandle tempSocketHandle = nullptr;
    HcclCommSocketStatus socketStatus;
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).GetStatus(tempSocketHandle, socketStatus);
    EXPECT_EQ(ret, HCCL_E_PARA);

    tempSocketHandle = reinterpret_cast<HcclCommSocketHandle>(0x1);
    ret = hcomm::SocketProcess::GetInstance(0).GetStatus(tempSocketHandle, socketStatus);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TestSocketProcess, Ut_TestGetStatus_When_Return_Success)
{
    HcclResult ret;
    HcclCommSocketStatus socketStatus;
    if (TestSocketProcess::socketHandle == nullptr) {
        ret = hcomm::SocketProcess::GetInstance(0).GetSocket(&TestSocketProcess::socketDesc, TestSocketProcess::socketHandle);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }
    while (TestSocketProcess::socketHandle != nullptr && socketStatus != HcclCommSocketStatus::SOCKET_OK) {
        HcclResult ret = hcomm::SocketProcess::GetInstance(0).GetStatus(TestSocketProcess::socketHandle, socketStatus);
        EXPECT_EQ(ret, HCCL_SUCCESS);
        if (socketStatus == HcclCommSocketStatus::SOCKET_TIMEOUT) {
            EXPECT_EQ(socketStatus, HcclCommSocketStatus::SOCKET_OK);
            break;
        } 
    }
}

TEST_F(TestSocketProcess, Ut_TestSendNoBlock_When_Return_ERROR)
{
    HcclCommSocketHandle tempSocketHandle = nullptr;
    u64 sendbuffer = 123;
    u64 sendSize = sizeof(sendbuffer);
    u64 *sentSize = nullptr;
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).SendNoBlock(tempSocketHandle, &sendbuffer, sendSize, sentSize);
    EXPECT_EQ(ret, HCCL_E_PARA);

    tempSocketHandle = reinterpret_cast<HcclCommSocketHandle>(0x1);
    ret = hcomm::SocketProcess::GetInstance(0).SendNoBlock(tempSocketHandle, &sendbuffer, sendSize, sentSize);
    EXPECT_EQ(ret, HCCL_E_PARA);

    void *errorBuffer = nullptr;
    ret = hcomm::SocketProcess::GetInstance(0).SendNoBlock(TestSocketProcess::socketHandle, &errorBuffer, sendSize, sentSize);
    EXPECT_EQ(ret, HCCL_E_PARA);    
}

TEST_F(TestSocketProcess, Ut_TestSendNoBlock_When_Return_SUCCESS)
{
    HcclResult ret;
    if (TestSocketProcess::socketHandle == nullptr) {
        ret = hcomm::SocketProcess::GetInstance(0).GetSocket(&TestSocketProcess::socketDesc, TestSocketProcess::socketHandle);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    u64 sendbuffer = 123;
    u64 sendSize = sizeof(sendbuffer);
    u64 sentSize = 0;
    u64 *sentSizePtr = &sentSize;
    ret = hcomm::SocketProcess::GetInstance(0).SendNoBlock(TestSocketProcess::socketHandle, &sendbuffer, sendSize, sentSizePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(sendSize, *sentSizePtr);
}

TEST_F(TestSocketProcess, Ut_TestRecvNoBlock_When_Return_ERROR)
{
    HcclCommSocketHandle tempSocketHandle = nullptr;
    u64 recvbuffer = 0;
    u64 recvSize = sizeof(recvbuffer);
    u64 recvedSize = 0;
    u64 *recvedSizePtr = &recvedSize;
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).RecvNoBlock(tempSocketHandle, &recvbuffer, recvSize, recvedSizePtr);
    EXPECT_EQ(ret, HCCL_E_PARA);

    tempSocketHandle = reinterpret_cast<HcclCommSocketHandle>(0x1);
    ret = hcomm::SocketProcess::GetInstance(0).RecvNoBlock(tempSocketHandle, &recvbuffer, recvSize, recvedSizePtr);
    EXPECT_EQ(ret, HCCL_E_PARA);

    void *errorBuffer = nullptr;
    ret = hcomm::SocketProcess::GetInstance(0).RecvNoBlock(TestSocketProcess::socketHandle, &errorBuffer, recvSize, recvedSizePtr);
    EXPECT_EQ(ret, HCCL_E_PARA);   
}

TEST_F(TestSocketProcess, Ut_TestRecvNoBlock_When_Return_HCCL_SUCCESS)
{
    HcclResult ret;
    if (TestSocketProcess::socketHandle == nullptr) {
        ret = hcomm::SocketProcess::GetInstance(0).GetSocket(&TestSocketProcess::socketDesc, TestSocketProcess::socketHandle);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    u64 recvbuffer = 0;
    u64 recvSize = sizeof(recvbuffer);
    u64 recvedSize = 0;
    u64 *recvedSizePtr = &recvedSize;
    ret = hcomm::SocketProcess::GetInstance(0).RecvNoBlock(TestSocketProcess::socketHandle, &recvbuffer, recvSize, recvedSizePtr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(recvSize, *recvedSizePtr);
}

TEST_F(TestSocketProcess, Ut_TestConvertToHcclSocketRole_W_Role)
{
    Hccl::SocketRole role;
    HcommSocketRole hcommRole;
    hcommRole = HCOMM_SOCKET_ROLE_CLIENT;
    role = hcomm::SocketProcess::GetInstance(0).ConvertToHcclSocketRole(hcommRole);
    EXPECT_EQ(role, Hccl::SocketRole::CLIENT);

    hcommRole = HCOMM_SOCKET_ROLE_SERVER;
    role = hcomm::SocketProcess::GetInstance(0).ConvertToHcclSocketRole(hcommRole);
    EXPECT_EQ(role, Hccl::SocketRole::SERVER);

    hcommRole = HCOMM_SOCKET_ROLE_RESERVED;
    role = hcomm::SocketProcess::GetInstance(0).ConvertToHcclSocketRole(hcommRole);
    EXPECT_EQ(role, Hccl::SocketRole::CLIENT);
}

TEST_F(TestSocketProcess, Ut_TestGetInstance_When_DeviceLogicIdValid_Return_SocketProcessRef)
{
    s32 deviceLogicId = 0;
    hcomm::SocketProcess &process1 = hcomm::SocketProcess::GetInstance(deviceLogicId);
    hcomm::SocketProcess &process2 = hcomm::SocketProcess::GetInstance(deviceLogicId);
    EXPECT_EQ(&process1, &process2);
}

TEST_F(TestSocketProcess, Ut_TestGetInstance_When_DeviceLogicIdInvalid_Return_FirstInstance)
{
    s32 invalidDeviceLogicId = 999;
    hcomm::SocketProcess &process = hcomm::SocketProcess::GetInstance(invalidDeviceLogicId);
    hcomm::SocketProcess &expectedProcess = hcomm::SocketProcess::GetInstance(0);
    EXPECT_EQ(&process, &expectedProcess);
}

TEST_F(TestSocketProcess, Ut_TestDestroySocketHandle_When_Return_ERROR)
{
    HcclCommSocketHandle tempSocketHandle = nullptr;
    HcclResult ret = hcomm::SocketProcess::GetInstance(0).DestroySocketHandle(tempSocketHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);

    tempSocketHandle = reinterpret_cast<HcclCommSocketHandle>(0x1);
    ret = hcomm::SocketProcess::GetInstance(0).DestroySocketHandle(tempSocketHandle);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(TestSocketProcess, Ut_TestDestroySocketHandle_When_Return_SUCCESS)
{
    HcclResult ret;
    if (TestSocketProcess::socketHandle == nullptr) {
        ret = hcomm::SocketProcess::GetInstance(0).GetSocket(&TestSocketProcess::socketDesc, TestSocketProcess::socketHandle);
        EXPECT_EQ(ret, HCCL_SUCCESS);
    }

    ret = hcomm::SocketProcess::GetInstance(0).DestroySocketHandle(TestSocketProcess::socketHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    TestSocketProcess::socketHandle = nullptr;
}