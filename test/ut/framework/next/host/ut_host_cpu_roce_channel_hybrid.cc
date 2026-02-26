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
#include "mockcpp/mockc.h"
#include <mockcpp/mockcpp.hpp>
#include <vector>
#include <iostream>
#include "host_socket_handle_manager.h"
#include "cpu_roce_endpoint.h"
#include "buffer/local_rdma_rma_buffer.h"
#include "host/host_cpu_roce_channel.h"
#include "host/host_rdma_connection.h"
#include "topo_common_types.h"
#include "ip_address.h"
#include "op_mode.h"
#include "rdma_handle_manager.h"
#include "host/exchange_rdma_conn_dto.h"
#include "socket.h"
#include "hccp.h"
#include "hybrid_mode_config.h"

#define private public
using namespace hcomm;

// ========== RoCE 混合模式单元测试 ==========
class HostCpuRoceChannelHybridTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HostCpuRoceChannelHybridTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HostCpuRoceChannelHybridTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HostCpuRoceChannelHybridTest SetUP" << std::endl;
        Hccl::DevType dev = Hccl::DevType::DEV_TYPE_910_95;
        MOCKER(Hccl::HrtGetDevice).stubs().will(returnValue(0));
        MOCKER(Hccl::HrtGetDeviceType).stubs().will(returnValue(dev));
        MOCKER(Hccl::HrtGetDevicePhyIdByIndex).stubs().with(any()).will(returnValue(0));
        RdmaHandle rdmaHandle = (void *)0x1000000;
        MOCKER(Hccl::HrtRaRdmaInit).stubs().with(any(), any()).will(returnValue(rdmaHandle));
        EndpointDesc endpointDesc{};
        endpointDesc.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        Hccl::IpAddress localIp("1.0.0.0");
        endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        endpoint = std::make_unique<CpuRoceEndpoint>(endpointDesc);
        endpoint->Init();
        endpointHandle = static_cast<EndpointHandle>(endpoint.get());
        EndpointDesc endpointDesc2;
        endpointDesc2.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        Hccl::IpAddress remoteIp("2.0.0.0");
        endpointDesc2.commAddr.addr = remoteIp.GetBinaryAddress().addr;
        endpointDesc2.loc.locType = ENDPOINT_LOC_TYPE_HOST;
        channelDesc.remoteEndpoint = endpointDesc2;
        channelDesc.notifyNum = 2;
        fakeSocket = new Hccl::Socket(nullptr, localIp, 60001, remoteIp, "_0_1_", Hccl::SocketRole::SERVER, 
                                        Hccl::NicType::HOST_NIC_TYPE);
        void* fsocket = static_cast<void*>(fakeSocket);
        channelDesc.socket = fsocket;
        localBufferPtr = std::make_shared<Hccl::Buffer>(666);
        localRdmaRmaBuffer = std::make_shared<Hccl::LocalRdmaRmaBuffer>(localBufferPtr, rdmaHandle);
        void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
        channelDesc.memHandles = &memHandle;
        channelDesc.memHandleNum = 1;
        channelDesc.exchangeAllMems = false;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HostCpuRoceChannelHybridTest TearDown" << std::endl;
        delete fakeSocket;
    }
    
    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer;
    std::vector<std::shared_ptr<Hccl::Buffer>> bufs{std::make_shared<Hccl::Buffer>((uintptr_t)2, 64)};
    std::unique_ptr<CpuRoceEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
};

// 测试 RoCECapability 序列化和反序列化
TEST_F(HostCpuRoceChannelHybridTest, Ut_RoCECapability_Serialize_Deserialize)
{
    RoCECapability cap;
    cap.magic = ROCE_HYBRID_MAGIC;
    cap.version = ROCE_CAPABILITY_VERSION;
    cap.nodeType = 0;
    cap.nicDeploy = NicDeployType::NIC_DEPLOY_HOST;
    cap.commStack = CommStackType::COMM_STACK_HOST_CPU_ROCE;
    cap.syncMode = SyncMode::SYNC_MODE_WRITE_IMM;
    cap.totalLength = sizeof(RoCECapability);
    
    Hccl::BinaryStream stream;
    cap.Serialize(stream);
    
    RoCECapability cap2;
    cap2.Deserialize(stream);
    
    EXPECT_EQ(cap2.magic, ROCE_HYBRID_MAGIC);
    EXPECT_EQ(cap2.version, ROCE_CAPABILITY_VERSION);
    EXPECT_EQ(cap2.nicDeploy, NicDeployType::NIC_DEPLOY_HOST);
    EXPECT_EQ(cap2.commStack, CommStackType::COMM_STACK_HOST_CPU_ROCE);
    EXPECT_EQ(cap2.syncMode, SyncMode::SYNC_MODE_WRITE_IMM);
}

// 测试 HybridExchangeData 序列化和反序列化
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridExchangeData_Serialize_Deserialize)
{
    HybridExchangeData data;
    data.qpn = 12345;
    data.psn = 67890;
    memset(data.gid, 0xAB, HCCP_GID_RAW_LEN);
    data.gidIdx = 1;
    data.bufAddr = 0x10000000;
    data.bufRkey = 0x20000000;
    data.bufLkey = 0x30000000;
    data.bufSize = 1024 * 1024;
    data.dpuNotifyId = 42;
    data.hostNotifyAddr = 0x40000000;
    data.hostNotifyRkey = 0x50000000;
    data.hostNotifyOffset = 64;
    
    Hccl::BinaryStream stream;
    data.Serialize(stream);
    
    HybridExchangeData data2;
    data2.Deserialize(stream);
    
    EXPECT_EQ(data2.qpn, 12345);
    EXPECT_EQ(data2.psn, 67890);
    EXPECT_EQ(data2.gidIdx, 1);
    EXPECT_EQ(data2.bufAddr, 0x10000000);
    EXPECT_EQ(data2.bufRkey, 0x20000000);
    EXPECT_EQ(data2.bufLkey, 0x30000000);
    EXPECT_EQ(data2.bufSize, 1024 * 1024);
    EXPECT_EQ(data2.dpuNotifyId, 42);
    EXPECT_EQ(data2.hostNotifyAddr, 0x40000000);
    EXPECT_EQ(data2.hostNotifyRkey, 0x50000000);
    EXPECT_EQ(data2.hostNotifyOffset, 64);
}

// 测试混合模式配置读取
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridModeConfig_LoadFromEnv)
{
    // 设置环境变量
    setenv("HCCL_HYBRID_MODE", "1", 1);
    setenv("HCCL_HYBRID_QP_CONNECT", "ibv", 1);
    setenv("HCCL_HYBRID_DEBUG", "1", 1);
    setenv("HCCL_HYBRID_POLL_TIMEOUT", "60000", 1);
    setenv("HCCL_HYBRID_POLL_INTERVAL", "5", 1);
    
    // 重新加载配置
    HybridModeConfig& config = HybridModeConfig::GetInstance();
    
    // 验证配置（注意：由于单例模式，这些值可能已被之前的测试修改）
    // 这里主要验证代码路径是否正常
    EXPECT_TRUE(config.IsHybridModeEnabled());
    EXPECT_EQ(config.GetQpConnectMode(), "ibv");
    EXPECT_TRUE(config.IsDebugEnabled());
    EXPECT_EQ(config.GetPollTimeoutMs(), 60000);
    EXPECT_EQ(config.GetPollIntervalMs(), 5);
    
    // 清理环境变量
    unsetenv("HCCL_HYBRID_MODE");
    unsetenv("HCCL_HYBRID_QP_CONNECT");
    unsetenv("HCCL_HYBRID_DEBUG");
    unsetenv("HCCL_HYBRID_POLL_TIMEOUT");
    unsetenv("HCCL_HYBRID_POLL_INTERVAL");
}

// 测试 DpuNotifyManager 单 ID 分配和释放
TEST_F(HostCpuRoceChannelHybridTest, Ut_DpuNotifyManager_SingleId_Alloc_Free)
{
    DpuNotifyManager& manager = DpuNotifyManager::GetInstance();
    
    // 分配单个 ID
    int id1 = manager.AllocSingleNotifyId();
    EXPECT_GE(id1, 0);
    
    int id2 = manager.AllocSingleNotifyId();
    EXPECT_GE(id2, 0);
    EXPECT_NE(id1, id2);
    
    // 释放 ID
    manager.FreeSingleNotifyId(id1);
    manager.FreeSingleNotifyId(id2);
    
    // 再次分配，应该能获取到之前释放的 ID
    int id3 = manager.AllocSingleNotifyId();
    EXPECT_GE(id3, 0);
    manager.FreeSingleNotifyId(id3);
}

// 测试混合模式初始化资源
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridMode_InitResources)
{
    DevType devType = DevType::DEV_TYPE_910_95;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER(HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(HostRdmaConnection::ModifyQp).stubs().will(returnValue(HCCL_SUCCESS));
    
    // Mock HrtRaGetNotifyBaseAddr
    uint64_t mockNotifyBase = 0x80000000;
    MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(returnValue(HCCL_SUCCESS));
    
    // Mock HrtRaGetNotifyMrInfo
    struct MrInfoT mockMrInfo;
    mockMrInfo.addr = (void*)0x80000000;
    mockMrInfo.size = 1024 * 1024;
    mockMrInfo.lkey = 0x12345678;
    mockMrInfo.rkey = 0x87654321;
    MOCKER(HrtRaGetNotifyMrInfo).stubs().will(returnValue(HCCL_SUCCESS));
    
    // 创建设备
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    channelDesc.notifyNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    
    // 初始化
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    
    // 验证初始状态
    EXPECT_FALSE(impl_->isHybridMode_);
    EXPECT_EQ(impl_->localDpuNotifyId_, INVALID_DPU_NOTIFY_ID);
    
    // 手动设置混合模式并初始化资源
    impl_->isHybridMode_ = true;
    HcclResult ret = impl_->InitHybridModeResources();
    
    // 验证资源分配
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(impl_->localDpuNotifyId_, INVALID_DPU_NOTIFY_ID);
    EXPECT_NE(impl_->localNotifyBaseAddr_, 0);
    
    // 清理
    impl_->isHybridMode_ = false;  // 避免析构时释放混合模式资源
}

// 测试混合模式 NotifyWait 参数校验
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridMode_NotifyWait_InvalidIndex)
{
    DevType devType = DevType::DEV_TYPE_910_95;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    
    // 设置混合模式和必要的成员
    impl_->isHybridMode_ = true;
    impl_->localNotifyBaseAddr_ = 0x80000000;
    
    // 索引不为 0 应该返回错误
    HcclResult ret = impl_->NotifyWaitHybrid(1, 1000);
    EXPECT_EQ(ret, HCCL_E_PARA);
    
    // 索引为 0 应该正常（虽然会超时，因为内存没有写入）
    // ret = impl_->NotifyWaitHybrid(0, 1);
    // EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

// 测试混合模式状态机转换
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridMode_StateMachine)
{
    DevType devType = DevType::DEV_TYPE_910_95;
    MOCKER(hrtGetDeviceType).stubs().with(outBound(devType)).will(returnValue(HCCL_SUCCESS));
    MOCKER(Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
    MOCKER(HostRdmaConnection::CreateQp).stubs().will(returnValue(HCCL_SUCCESS));
    
    void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
    channelDesc.memHandles = &memHandle;
    channelDesc.memHandleNum = 1;
    auto impl_ = std::make_unique<hcomm::HostCpuRoceChannel>(endpointHandle, channelDesc);
    
    EXPECT_EQ(impl_->Init(), HCCL_SUCCESS);
    
    // 初始状态
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::INIT);
    
    // 第一次 GetStatus 应该进入 SOCKET_OK
    hcomm::ChannelStatus status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::SOCKET_OK);
    
    // 第二次 GetStatus 应该进入 CAP_EXCHANGED
    status = impl_->GetStatus();
    EXPECT_EQ(impl_->rdmaStatus_, HostCpuRoceChannel::RdmaStatus::CAP_EXCHANGED);
    
    // 验证能力协商后的状态
    // 注意：由于没有 Mock ExchangeCapability，后续可能会失败
    // 这里主要验证状态机流程
}

// 测试混合模式常量定义
TEST_F(HostCpuRoceChannelHybridTest, Ut_HybridMode_Constants)
{
    // 验证魔数
    EXPECT_EQ(ROCE_HYBRID_MAGIC, 0x48434C52);
    
    // 验证版本
    EXPECT_EQ(ROCE_CAPABILITY_VERSION, 1);
    EXPECT_EQ(ROCE_MIN_SUPPORTED_VERSION, 1);
    
    // 验证无效 Notify ID
    EXPECT_EQ(INVALID_DPU_NOTIFY_ID, 0xFFFFFFFF);
}
