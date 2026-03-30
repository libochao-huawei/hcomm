/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "next/comms/endpoint_pairs/channels/aicpu/aicpu_ts_roce_channel.h"
#include "next/comms/endpoint_pairs/channels/aicpu/dev_rdma_connection.h"
#include "socket.h"
#include "orion_adapter_hccp.h"
#include "hccp_common.h"
#include "dev_capability.h"
#define private public

using namespace hcomm;

class AicpuTsRoceChannelTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AicpuTsRoceChannelTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AicpuTsRoceChannelTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AicpuTsRoceChannelTest SetUP" << std::endl;
        
        // 初始化测试参数
        localEp_ = {};
        localEp_.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
        localEp_.loc.device.superPodIdx = 0;
        localEp_.loc.device.serverIdx = 0;
        localEp_.loc.device.superDevId = 0;
        localEp_.loc.device.devPhyId = 0;
        localEp_.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        localEp_.commAddr.addr = 0x0100007f; // 127.0.0.1
        
        remoteEp_ = {};
        remoteEp_.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
        remoteEp_.loc.device.superPodIdx = 0;
        remoteEp_.loc.device.serverIdx = 0;
        remoteEp_.loc.device.superDevId = 0;
        remoteEp_.loc.device.devPhyId = 1;
        remoteEp_.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        remoteEp_.commAddr.addr = 0x0100007f; // 127.0.0.1
        
        channelDesc_ = {};
        channelDesc_.remoteEndpoint = remoteEp_;
        channelDesc_.notifyNum = RDMA_NOTIFY_NUM;
        channelDesc_.memHandleNum = 0;
        channelDesc_.memHandles = nullptr;
        
        // 创建模拟socket
        fakeSocket = new Hccl::Socket(nullptr, localIp_, listenPort_, remoteIp_, tag_, Hccl::SocketRole::SERVER, 
                                      Hccl::NicType::HOST_NIC_TYPE);
        
        // Mock依赖项
        MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue(Hccl::SocketStatus::OK));
        MOCKER_CPP(&Hccl::Socket::Listen).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::Socket::Send).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&Hccl::Socket::Recv).stubs().will(returnValue(HCCL_SUCCESS));
        
        MOCKER(Hccl::HrtMemcpy).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER(Hccl::HostSocketHandleManager::GetInstance).stubs().will(returnValue(Hccl::HostSocketHandleManager::GetInstance()));
        MOCKER(Hccl::HostSocketHandleManager::Create).stubs().with(any(), any()).will(returnValue((Hccl::SocketHandle)0x1000));
        
        // Mock DevCapability
        MOCKER(Hccl::DevCapability::GetInstance).stubs().will(returnValue(Hccl::DevCapability::GetInstance()));
        MOCKER_CPP(&Hccl::DevCapability::GetNotifySize).stubs().will(returnValue(8));
        
        // Mock DevRdmaConnection
        MOCKER_CPP(&DevRdmaConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&DevRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&DevRdmaConnection::GetExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&DevRdmaConnection::ParseRmtExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&DevRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in AicpuTsRoceChannelTest TearDown" << std::endl;
    }
    
    // 测试参数
    EndpointDesc localEp_;
    EndpointDesc remoteEp_;
    HcommChannelDesc channelDesc_;
    
    // 模拟对象
    Hccl::Socket* fakeSocket;
    Hccl::IpAddress localIp_;
    Hccl::IpAddress remoteIp_;
    u32 listenPort_ = 60001;
    std::string tag_ = "test_socket";
};

TEST_F(AicpuTsRoceChannelTest, Ut_When_Normal_Init_Expect_Success)
{
    std::cout << "Start Ut_When_Normal_Init_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.socket_ = std::shared_ptr<Hccl::Socket>(fakeSocket);
    aicpuTsRoceChannel.localEp_ = localEp_;
    aicpuTsRoceChannel.remoteEp_ = remoteEp_;
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    
    // 测试初始化
    HcclResult ret = aicpuTsRoceChannel.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 验证状态
    EXPECT_EQ(aicpuTsRoceChannel.notifyNum_, RDMA_NOTIFY_NUM);
    EXPECT_EQ(aicpuTsRoceChannel.localNotifies_.size(), RDMA_NOTIFY_NUM);
    EXPECT_EQ(aicpuTsRoceChannel.connections_.size(), 1);
    EXPECT_NE(aicpuTsRoceChannel.notifyValueMem_, nullptr);
    EXPECT_NE(aicpuTsRoceChannel.notifyValueBuffer_, nullptr);
    
    std::cout << "End Ut_When_Normal_Init_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetStatus_Expect_Ready)
{
    std::cout << "Start Ut_When_GetStatus_Expect_Ready" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.socket_ = std::shared_ptr<Hccl::Socket>(fakeSocket);
    aicpuTsRoceChannel.localEp_ = localEp_;
    aicpuTsRoceChannel.remoteEp_ = remoteEp_;
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    aicpuTsRoceChannel.rdmaStatus_ = AicpuTsRoceChannel::RdmaStatus::DATA_EXCHANGE;
    
    // 测试获取状态
    ChannelStatus status;
    HcclResult ret = aicpuTsRoceChannel.GetStatus(status);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(status, ChannelStatus::READY);
    
    std::cout << "End Ut_When_GetStatus_Expect_Ready" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_CompareEndpointDeviceInfo_Expect_Correct)
{
    std::cout << "Start Ut_When_CompareEndpointDeviceInfo_Expect_Correct" << std::endl;
    
    EndpointDeviceLoc localDevice = {0, 0, 0, 0};
    EndpointDeviceLoc remoteDevice = {0, 0, 0, 1};
    
    // 测试本地小于远程
    int result = AicpuTsRoceChannel::CompareEndpointDeviceInfo(localDevice, remoteDevice);
    EXPECT_EQ(result, -1);
    
    // 测试本地大于远程
    result = AicpuTsRoceChannel::CompareEndpointDeviceInfo(remoteDevice, localDevice);
    EXPECT_EQ(result, 1);
    
    // 测试两者相等
    result = AicpuTsRoceChannel::CompareEndpointDeviceInfo(localDevice, localDevice);
    EXPECT_EQ(result, 0);
    
    std::cout << "End Ut_When_CompareEndpointDeviceInfo_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildNotify_Expect_Success)
{
    std::cout << "Start Ut_When_BuildNotify_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.localEp_ = localEp_;
    aicpuTsRoceChannel.remoteEp_ = remoteEp_;
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    
    // 测试构建通知
    HcclResult ret = aicpuTsRoceChannel.BuildNotify();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(aicpuTsRoceChannel.notifyNum_, RDMA_NOTIFY_NUM);
    EXPECT_EQ(aicpuTsRoceChannel.localNotifies_.size(), RDMA_NOTIFY_NUM);
    
    std::cout << "End Ut_When_BuildNotify_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildBuffer_Expect_Success)
{
    std::cout << "Start Ut_When_BuildBuffer_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    
    // 设置memHandleNum和memHandles
    HcommChannelDesc desc = channelDesc_;
    desc.memHandleNum = 0;
    desc.memHandles = nullptr;
    
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, desc, COMM_ENGINE_DSM);
    
    // 测试构建缓冲区
    HcclResult ret = aicpuTsRoceChannel.BuildBuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(aicpuTsRoceChannel.bufferNum_, 0);
    EXPECT_EQ(aicpuTsRoceChannel.localRmaBuffers_.size(), 0);
    
    std::cout << "End Ut_When_BuildBuffer_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildNotifyValueBuffer_Expect_Success)
{
    std::cout << "Start Ut_When_BuildNotifyValueBuffer_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    
    // 测试构建通知值缓冲区
    HcclResult ret = aicpuTsRoceChannel.BuildNotifyValueBuffer();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuTsRoceChannel.notifyValueMem_, nullptr);
    EXPECT_NE(aicpuTsRoceChannel.notifyValueBuffer_, nullptr);
    
    std::cout << "End Ut_When_BuildNotifyValueBuffer_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetNotifyNum_Expect_Correct)
{
    std::cout << "Start Ut_When_GetNotifyNum_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置通知数量
    aicpuTsRoceChannel.notifyNum_ = 3;
    
    // 测试获取通知数量
    uint32_t notifyNum = 0;
    HcclResult ret = aicpuTsRoceChannel.GetNotifyNum(&notifyNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyNum, 3);
    
    std::cout << "End Ut_When_GetNotifyNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetBufferNum_Expect_Correct)
{
    std::cout << "Start Ut_When_GetBufferNum_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置缓冲区数量
    aicpuTsRoceChannel.bufferNum_ = 2;
    
    // 测试获取缓冲区数量
    uint32_t bufferNum = 0;
    HcclResult ret = aicpuTsRoceChannel.GetBufferNum(&bufferNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(bufferNum, 2);
    
    std::cout << "End Ut_When_GetBufferNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetQpNum_Expect_Correct)
{
    std::cout << "Start Ut_When_GetQpNum_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置连接数量
    aicpuTsRoceChannel.connNum_ = 2;
    
    // 测试获取QP数量
    uint32_t qpNum = 0;
    HcclResult ret = aicpuTsRoceChannel.GetQpNum(&qpNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(qpNum, 2);
    
    std::cout << "End Ut_When_GetQpNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetCommEngine_Expect_Correct)
{
    std::cout << "Start Ut_When_GetCommEngine_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 测试获取通信引擎
    CommEngine engine = aicpuTsRoceChannel.GetCommEngine();
    EXPECT_EQ(engine, COMM_ENGINE_DSM);
    
    std::cout << "End Ut_When_GetCommEngine_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetCommProtocol_Expect_Correct)
{
    std::cout << "Start Ut_When_GetCommProtocol_Expect_Correct" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 测试获取通信协议
    CommProtocol protocol = aicpuTsRoceChannel.GetCommProtocol();
    EXPECT_EQ(protocol, channelDesc_.remoteEndpoint.protocol);
    
    std::cout << "End Ut_When_GetCommProtocol_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildConnection_Expect_Success)
{
    std::cout << "Start Ut_When_BuildConnection_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.socket_ = fakeSocket;
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    
    // Mock DevRdmaConnection
    MOCKER_CPP(&DevRdmaConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::GetExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::ParseRmtExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    
    // 测试构建连接
    HcclResult ret = aicpuTsRoceChannel.BuildConnection();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(aicpuTsRoceChannel.connections_.size(), 1);
    
    std::cout << "End Ut_When_BuildConnection_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildSocket_Expect_Success)
{
    std::cout << "Start Ut_When_BuildSocket_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.localEp_ = localEp_;
    aicpuTsRoceChannel.remoteEp_ = remoteEp_;
    
    // Mock SocketMgr
    MOCKER_CPP(&SocketMgr::GetSocket).stubs().with(any()).will(returnValue(fakeSocket));
    
    // 测试构建Socket
    HcclResult ret = aicpuTsRoceChannel.BuildSocket();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(aicpuTsRoceChannel.socket_, nullptr);
    
    std::cout << "End Ut_When_BuildSocket_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_Clean_Expect_Success)
{
    std::cout << "Start Ut_When_Clean_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.connections_.push_back(std::make_unique<DevRdmaConnection>(fakeSocket, (RdmaHandle)0x1000));
    aicpuTsRoceChannel.localRmaBuffers_.push_back(nullptr);
    aicpuTsRoceChannel.localNotifies_.push_back(nullptr);
    aicpuTsRoceChannel.notifyValueMem_ = std::make_shared<Hccl::DevBuffer>();
    aicpuTsRoceChannel.notifyValueBuffer_ = std::make_unique<Hccl::LocalRdmaRmaBuffer>();
    aicpuTsRoceChannel.remoteNotifies_.push_back(nullptr);
    aicpuTsRoceChannel.rmtRmaBuffers_.push_back(nullptr);
    aicpuTsRoceChannel.remoteMems.push_back(nullptr);
    
    // 测试Clean方法
    HcclResult ret = aicpuTsRoceChannel.Clean();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(aicpuTsRoceChannel.connections_.size(), 0);
    EXPECT_EQ(aicpuTsRoceChannel.localRmaBuffers_.size(), 0);
    EXPECT_EQ(aicpuTsRoceChannel.localNotifies_.size(), 0);
    EXPECT_EQ(aicpuTsRoceChannel.notifyValueMem_, nullptr);
    EXPECT_EQ(aicpuTsRoceChannel.notifyValueBuffer_, nullptr);
    EXPECT_EQ(aicpuTsRoceChannel.remoteNotifies_.size(), 0);
    EXPECT_EQ(aicpuTsRoceChannel.rmtRmaBuffers_.size(), 0);
    EXPECT_EQ(aicpuTsRoceChannel.remoteMems.size(), 0);
    
    std::cout << "End Ut_When_Clean_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_Resume_Expect_Success)
{
    std::cout << "Start Ut_When_Resume_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置内部状态
    aicpuTsRoceChannel.socket_ = fakeSocket;
    aicpuTsRoceChannel.rdmaHandle_ = (RdmaHandle)0x1000;
    
    // Mock DevRdmaConnection
    MOCKER_CPP(&DevRdmaConnection::Init).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::GetExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::ParseRmtExchangeDto).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&DevRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    
    // 测试Resume方法
    HcclResult ret = aicpuTsRoceChannel.Resume();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_Resume_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 测试Describe方法
    std::string desc = aicpuTsRoceChannel.Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetLocNotifyInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetLocNotifyInfo_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置本地通知
    aicpuTsRoceChannel.localNotifies_.push_back(std::make_unique<Hccl::RdmaLocalNotify>());
    
    // 测试BuildAndGetLocNotifyInfo
    Notify* notify = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetLocNotifyInfo(&notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(notify, nullptr);
    
    std::cout << "End Ut_When_BuildAndGetLocNotifyInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置远程通知
    aicpuTsRoceChannel.remoteNotifies_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>());
    
    // 测试BuildAndGetRmtNotifyInfo
    Notify* notify = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetRmtNotifyInfo(&notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(notify, nullptr);
    
    std::cout << "End Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetLocBufInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetLocBufInfo_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置本地缓冲区
    aicpuTsRoceChannel.localRmaBuffers_.push_back(nullptr);
    
    // 测试BuildAndGetLocBufInfo
    ProtectionInfo* protectionInfo = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetLocBufInfo(&protectionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetLocBufInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetRmtBufInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetRmtBufInfo_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置远程缓冲区
    aicpuTsRoceChannel.rmtRmaBuffers_.push_back(std::make_unique<Hccl::RemoteRdmaRmaBuffer>());
    
    // 测试BuildAndGetRmtBufInfo
    ProtectionInfo* protectionInfo = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetRmtBufInfo(&protectionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetRmtBufInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetSqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetSqContext_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置连接
    aicpuTsRoceChannel.connections_.push_back(std::make_unique<DevRdmaConnection>(fakeSocket, (RdmaHandle)0x1000));
    
    // Mock BuildSqContext
    MOCKER_CPP(&DevRdmaConnection::BuildSqContext).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    
    // 测试BuildAndGetSqContext
    SqContext* sqContext = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetSqContext(&sqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetSqContext_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetCqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetCqContext_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置连接
    aicpuTsRoceChannel.connections_.push_back(std::make_unique<DevRdmaConnection>(fakeSocket, (RdmaHandle)0x1000));
    
    // Mock BuildCqContext
    MOCKER_CPP(&DevRdmaConnection::BuildCqContext).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
    
    // 测试BuildAndGetCqContext
    CqContext* cqContext = nullptr;
    HcclResult ret = aicpuTsRoceChannel.BuildAndGetCqContext(&cqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetCqContext_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetRemoteMem_Expect_Success)
{
    std::cout << "Start Ut_When_GetRemoteMem_Expect_Success" << std::endl;
    
    // 创建AicpuTsRoceChannel实例
    void* endpointHandle = reinterpret_cast<void*>(&localEp_);
    AicpuTsRoceChannel aicpuTsRoceChannel(endpointHandle, channelDesc_, COMM_ENGINE_DSM);
    
    // 设置远程内存
    aicpuTsRoceChannel.remoteMems.push_back(std::make_unique<HcclMem>());
    
    // 测试GetRemoteMem
    HcclMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTags = nullptr;
    HcclResult ret = aicpuTsRoceChannel.GetRemoteMem(&remoteMem, &memNum, &memTags);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(memNum, 1);
    
    std::cout << "End Ut_When_GetRemoteMem_Expect_Success" << std::endl;
}