/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define private public
#define protected public
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "dev_ub_connection.h"
#include "rma_conn_manager.h"
#include "socket.h"
#include "orion_adapter_rts.h"
#include "not_support_exception.h"
#include "rma_conn_exception.h"
#undef private

using namespace Hccl;

namespace Hccl {
uint32_t TaHwValueToMs(uint8_t hwValue);
uint8_t FindMinTaHwValueGreaterThan(uint32_t tpTotalTimeoutMs);
}

class DevUbConnectionTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "DevUbConnection tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "DevUbConnection tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in DevUbConnection SetUP" << std::endl;
        fakeSocket = new Socket(nullptr, localIp, listenPort, remoteIp, tag, SocketRole::SERVER, NicType::DEVICE_NIC_TYPE);

    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in DevUbConnection TearDown" << std::endl;
    }
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    std::string tag = "test";
};

TEST(DevUbConnectionTest, rma_ub_connection_prepare_inline_write_tasks_with_writeval_send)
{
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;

    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
    MemoryBuffer remoteMemBuffer(2000, 1000, 0);
    SqeConfig config{};
    config.wqeMode = WqeMode::WRITE_VALUE;
    
    auto task = devUbConnection.PrepareInlineWrite(remoteMemBuffer, 1, config);
    EXPECT_NE(task, nullptr);
    EXPECT_EQ(TaskType::WRITE_VALUE, task->GetType());
}

TEST(DevUbConnectionTest, rma_net_connection_prepare_write_tasks_in_offload_mode_with_db_send)
{
    GlobalMockObject::verify();

    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    std::string tag = "SENDRECV";
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::OK));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    MOCKER(HrtRaQpCreate).stubs().with(any(), any(), any()).will(returnValue(fakeQpHandle));
    HrtRaUbSendWrRespParam postSendRes1;
    postSendRes1.dwqeSize = 64;
    HrtRaUbSendWrRespParam postSendRes2;
    postSendRes2.dwqeSize = 128;
    postSendRes2.piVal = 3;
    HrtRaUbSendWrRespParam postSendRes3;
    postSendRes3.dwqeSize = 100;
    MOCKER(HrtRaUbPostSend)
        .stubs()
        .with(any(), any())
        .will(returnValue(postSendRes1))
        .then(returnValue(postSendRes2))
        .then(returnValue(postSendRes3));
 
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OFFLOAD);
    
 
    // Given
    MemoryBuffer localMemBuffer1(0, 0, 0);
    MemoryBuffer remoteMemBuffer1(2000, 0, 0);
    SqeConfig config{};
    config.wqeMode = WqeMode::DB_SEND;
    // When
    auto result1 = devUbConnection.PrepareWriteReduce(remoteMemBuffer1, localMemBuffer1, DataType::INT8, ReduceOp::SUM, config);
    // Then
    EXPECT_EQ(nullptr,result1);
 
    MemoryBuffer localMemBuffer2(0, 100, 0);
    MemoryBuffer remoteMemBuffer2(2000, 100, 0);
    auto result2 = devUbConnection.PrepareWriteReduce(remoteMemBuffer2, localMemBuffer2, DataType::INT8, ReduceOp::SUM, config);
    EXPECT_NE(nullptr,result2);
    
    auto result3 = devUbConnection.PrepareWriteReduce(remoteMemBuffer2, localMemBuffer2, DataType::INT8, ReduceOp::SUM, config);
    EXPECT_NE(nullptr,result3);
    
    EXPECT_THROW(devUbConnection.PrepareWriteReduce(remoteMemBuffer2, localMemBuffer2, DataType::INT8, ReduceOp::SUM, config),
                 InvalidParamsException);
 
    MemoryBuffer localMemBuffer10(0, 0, 0);
    MemoryBuffer remoteMemBuffer10(2000, 10, 0);
    EXPECT_THROW(devUbConnection.PrepareWriteReduce(remoteMemBuffer10, localMemBuffer10, DataType::INT8, ReduceOp::SUM, config), InvalidParamsException);
 
    MOCKER(HrtRaUbPostNops).stubs().with(any(), any(), any());
    MOCKER(HrtUbDbSend).stubs().with(any(), any());
    void* ptr = nullptr;
    MOCKER(HrtStreamCreateWithFlags).stubs().with(any(), any()).will(returnValue(ptr));
    MOCKER(HrtGetStreamId).stubs().with(any()).will(returnValue(0));
    MOCKER(HrtGetDevice).stubs().will(returnValue(0));
    MOCKER(HrtGetDevicePhyIdByIndex).stubs().will(returnValue(static_cast<DevId>(1)));
    Stream stream;
    devUbConnection.AddNop(stream);
 
    devUbConnection.piVal = 9999;
    EXPECT_THROW(devUbConnection.AddNop(stream), InvalidParamsException);
 
    DevUbConnection devUbConnection1(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    EXPECT_NO_THROW(devUbConnection1.AddNop(stream));
}

TEST(DevUbConnectionTest, rma_ub_connection_get_pi_ci_sqDepth_ok)
{
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
 
    // Then
    EXPECT_EQ(devUbConnection.GetPiVal(), 0);
    EXPECT_EQ(devUbConnection.GetCiVal(), 0);
    EXPECT_EQ(devUbConnection.GetSqDepth(), 8192);
}
 
TEST(DevUbConnectionTest, rma_ub_connection_get_jfcMode_and_jettyHandle_ok)
{
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
    JettyHandle jettyHandle = 100;
    devUbConnection.jettyHandle = jettyHandle;
 
    // Then
    EXPECT_EQ(devUbConnection.GetUbJfcMode(), HrtUbJfcMode::STARS_POLL);
    EXPECT_EQ(devUbConnection.GetJettyHandle(), jettyHandle);
}
 
TEST(DevUbConnectionTest, GetStarsPollUbConns_ok)
{
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
    std::vector<RmaConnection *> conns;
    conns.push_back(&devUbConnection);
    std::vector<DevUbConnection *> devUbConns = GetStarsPollUbConns(conns);
 
    // Then
    EXPECT_EQ(devUbConns.size(), 1);
}
 
TEST(DevUbConnectionTest, IfNeedUpdatingUbCi_false)
{
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
    devUbConnection.piVal = 10;
    devUbConnection.ciVal = 0;
    std::vector<DevUbConnection *> conns;
    conns.push_back(&devUbConnection);
    bool ret = IfNeedUpdatingUbCi(conns);
 
    // Then
    EXPECT_EQ(ret, false);
}
 
TEST(DevUbConnectionTest, IfNeedUpdatingUbCi_true)
{
    // Given: socket time out
    Socket *fakeSocket;
    IpAddress localIp;
    IpAddress remoteIp;
    u32 listenPort = 100;
    MOCKER_CPP(&Socket::GetStatus).stubs().will(returnValue((SocketStatus)SocketStatus::TIMEOUT));
    RdmaHandle rdmaHandle = (void *)0x1000000;
    string tag = "SENDRECV";
    QpHandle fakeQpHandle = (void *)0x1000000;
 
    BasePortType basePortType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 0, 1, 0, 1);
 
    // When
    DevUbConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    
    devUbConnection.piVal = 4000;
    devUbConnection.ciVal = 0;
    std::vector<DevUbConnection *> conns;
    conns.push_back(&devUbConnection);
    bool ret = IfNeedUpdatingUbCi(conns);
 
    // Then
    EXPECT_EQ(ret, false);

    GlobalMockObject::verify();
}

TEST(DevUbConnectionTest, tp_import_test)
{
    MOCKER(HrtGetDevicePhyIdByIndex).defaults().will(returnValue(static_cast<DevId>(0)));
    // construct DevUbConnection
    RdmaHandle rdmaHandle = (void *)0x1000000;

    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 10, 11, 10, 11);
    linkData.localAddr_ = IpAddress("10.0.0.1");
    linkData.remoteAddr_ = IpAddress("10.0.0.2");
    linkData.linkProtocol_ = LinkProtocol::UB_TP;
    std::string tag = "test";
    DevUbTpConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::INIT); // TP_INFO_GETTING
    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::EXCHANGEABLE);

    // Then
    auto rmtDto = devUbConnection.GetExchangeDto();
    devUbConnection.ParseRmtExchangeDto(*rmtDto);
    devUbConnection.ImportRmtDto();
    EXPECT_EQ(devUbConnection.status, RmaConnStatus::EXCHANGEABLE);
    EXPECT_EQ(DevUbConnection::UbConnStatus::JETTY_IMPORTING, devUbConnection.ubConnStatus);

    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::READY);
    EXPECT_EQ(DevUbConnection::UbConnStatus::READY, devUbConnection.ubConnStatus);
}

TEST(DevUbConnectionTest, ctp_import_test)
{
    // construct DevUbConnection
    RdmaHandle rdmaHandle = (void *)0x1000000;

    BasePortType portType(PortDeploymentType::DEV_NET, ConnectProtoType::UB);
    LinkData     linkData(portType, 10, 11, 10, 11);
    linkData.localAddr_ = IpAddress("11.0.0.1");
    linkData.remoteAddr_ = IpAddress("11.0.0.2");
    linkData.linkProtocol_ = LinkProtocol::UB_CTP;
    std::string tag = "test";
    DevUbCtpConnection devUbConnection(rdmaHandle, linkData.GetLocalAddr(), linkData.GetRemoteAddr(), OpMode::OPBASE);
    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::INIT); // TP_INFO_GETTING
    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::EXCHANGEABLE);

    // Then
    auto rmtDto = devUbConnection.GetExchangeDto();
    devUbConnection.ParseRmtExchangeDto(*rmtDto);
    devUbConnection.ImportRmtDto();
    EXPECT_EQ(devUbConnection.status, RmaConnStatus::EXCHANGEABLE);
    EXPECT_EQ(DevUbConnection::UbConnStatus::JETTY_IMPORTING, devUbConnection.ubConnStatus);

    EXPECT_EQ(devUbConnection.GetStatus(), RmaConnStatus::READY);
    EXPECT_EQ(DevUbConnection::UbConnStatus::READY, devUbConnection.ubConnStatus);
}

TEST(DevUbConnectionTest, Ut_TaHwValueToMs_When_InputGear0_Expect_Return512ms)
{
    uint32_t timeoutMs = TaHwValueToMs(0);
    EXPECT_EQ(timeoutMs, 512u);
    timeoutMs = TaHwValueToMs(7);
    EXPECT_EQ(timeoutMs, 512u);
}

TEST(DevUbConnectionTest, Ut_TaHwValueToMs_When_InputGear1_Expect_Return1000ms)
{
    uint32_t timeoutMs = TaHwValueToMs(8);
    EXPECT_EQ(timeoutMs, 1000u);
    timeoutMs = TaHwValueToMs(15);
    EXPECT_EQ(timeoutMs, 1000u);
}

TEST(DevUbConnectionTest, Ut_TaHwValueToMs_When_InputGear2_Expect_Return8000ms)
{
    uint32_t timeoutMs = TaHwValueToMs(16);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = TaHwValueToMs(23);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST(DevUbConnectionTest, Ut_TaHwValueToMs_When_InputGear3_Expect_Return32000ms)
{
    uint32_t timeoutMs = TaHwValueToMs(24);
    EXPECT_EQ(timeoutMs, 32000u);
    timeoutMs = TaHwValueToMs(31);
    EXPECT_EQ(timeoutMs, 32000u);
}

TEST(DevUbConnectionTest, Ut_TaHwValueToMs_When_InputInvalid_Expect_ReturnDefault8000ms)
{
    uint32_t timeoutMs = TaHwValueToMs(32);
    EXPECT_EQ(timeoutMs, 8000u);
    timeoutMs = TaHwValueToMs(100);
    EXPECT_EQ(timeoutMs, 8000u);
}

TEST(DevUbConnectionTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan512ms_Expect_Return0)
{
    uint8_t hwValue = FindMinTaHwValueGreaterThan(100);
    EXPECT_EQ(hwValue, 0u);
    hwValue = FindMinTaHwValueGreaterThan(511);
    EXPECT_EQ(hwValue, 0u);
}

TEST(DevUbConnectionTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan1000ms_Expect_Return8)
{
    uint8_t hwValue = FindMinTaHwValueGreaterThan(512);
    EXPECT_EQ(hwValue, 8u);
    hwValue = FindMinTaHwValueGreaterThan(999);
    EXPECT_EQ(hwValue, 8u);
}

TEST(DevUbConnectionTest, Ut_FindMinTaHwValueGreaterThan_When_LessThan8000ms_Expect_Return16)
{
    uint8_t hwValue = FindMinTaHwValueGreaterThan(1000);
    EXPECT_EQ(hwValue, 16u);
    hwValue = FindMinTaHwValueGreaterThan(7999);
    EXPECT_EQ(hwValue, 16u);
}

TEST(DevUbConnectionTest, Ut_FindMinTaHwValueGreaterThan_When_GreaterOrEqual8000ms_Expect_Return24)
{
    uint8_t hwValue = FindMinTaHwValueGreaterThan(8000);
    EXPECT_EQ(hwValue, 24u);
    hwValue = FindMinTaHwValueGreaterThan(10000);
    EXPECT_EQ(hwValue, 24u);
}
