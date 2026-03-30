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
#include <vector>
#include <iostream>
#include "aicpu_ts_roce_channel.h"
#include "next/comms/endpoint_pairs/channels/aicpu/dev_rdma_connection.h"
#include "socket.h"
#include "orion_adapter_hccp.h"
#include "hccp.h"
#include "hccp_common.h"
#include "hcomm_c_adpt.h"
#include "buffer/local_rdma_rma_buffer.h"
#include "rdma_handle_manager.h"
#include "cpu_roce_endpoint.h"

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
        
        Hccl::IpAddress localIp("1.0.0.0");
        Hccl::IpAddress remoteIp("2.0.0.0");
        fakeSocket = new Hccl::Socket(nullptr, localIp, 60001, remoteIp, "_0_1_", 
                                      Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
        
        MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
        MOCKER(Hccl::HrtRaNdaQpCreate).stubs().with(any(), any(), any(), any(), any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(Hccl::HrtRaNdaCqCreate).stubs().with(any(), any(), any(), any(), any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(Hccl::HrtRaNdaCqDestroy).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(Hccl::HrtRaQpDestroy).stubs().with(any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(RaNdaGetDirectFlag).stubs().with(any(), any()).will(returnValue(0));
        MOCKER(RaGetQpAttr).stubs().with(any(), any()).will(returnValue(0));
        
        EndpointDesc endpointDesc{};
        endpointDesc.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        endpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
        endpoint = std::make_unique<CpuRoceEndpoint>(endpointDesc);
        endpoint->Init();
        endpointHandle = static_cast<EndpointHandle>(endpoint.get());
        
        EndpointDesc endpointDesc2;
        endpointDesc2.protocol = COMM_PROTOCOL_ROCE;
        endpointDesc2.commAddr.type = COMM_ADDR_TYPE_IP_V4;
        endpointDesc2.commAddr.addr = remoteIp.GetBinaryAddress().addr;
        endpointDesc2.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
        channelDesc.remoteEndpoint = endpointDesc2;
        channelDesc.notifyNum = 2;
        channelDesc.port = 60001;
        void* fsocket = static_cast<void*>(fakeSocket);
        channelDesc.socket = fsocket;
        
        localBufferPtr = std::make_shared<Hccl::Buffer>(666);
        RdmaHandle rdmaHandle = (void *)0x1000000;
        localRdmaRmaBuffer = std::make_shared<Hccl::LocalRdmaRmaBuffer>(localBufferPtr, rdmaHandle);
        void* memHandle = static_cast<void*>(localRdmaRmaBuffer.get());
        channelDesc.memHandles = &memHandle;
        channelDesc.memHandleNum = 1;
        channelDesc.exchangeAllMems = false;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AicpuTsRoceChannelTest TearDown" << std::endl;
        delete fakeSocket;
    }
    
    std::shared_ptr<Hccl::Buffer> localBufferPtr;
    std::shared_ptr<Hccl::LocalRdmaRmaBuffer> localRdmaRmaBuffer;
    std::unique_ptr<CpuRoceEndpoint> endpoint;
    EndpointHandle endpointHandle{};
    HcommChannelDesc channelDesc{};
    Hccl::Socket* fakeSocket;
};

TEST_F(AicpuTsRoceChannelTest, Ut_When_Normal_Init_Expect_HCCL_SUCCESS)
{
    std::cout << "Start Ut_When_Normal_Init_Expect_HCCL_SUCCESS" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    
    HcclResult ret = channel->Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_Normal_Init_Expect_HCCL_SUCCESS" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetNotifyNum_Expect_Correct)
{
    std::cout << "Start Ut_When_GetNotifyNum_Expect_Correct" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    uint32_t notifyNum = 0;
    HcclResult ret = channel->GetNotifyNum(&notifyNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(notifyNum, channelDesc.notifyNum);
    
    std::cout << "End Ut_When_GetNotifyNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetBufferNum_Expect_Correct)
{
    std::cout << "Start Ut_When_GetBufferNum_Expect_Correct" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    uint32_t bufferNum = 0;
    HcclResult ret = channel->GetBufferNum(&bufferNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(bufferNum, channelDesc.memHandleNum);
    
    std::cout << "End Ut_When_GetBufferNum_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetStatus_Expect_Correct)
{
    std::cout << "Start Ut_When_GetStatus_Expect_Correct" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    ChannelStatus status = channel->GetStatus();
    EXPECT_EQ(status, ChannelStatus::INIT);
    
    std::cout << "End Ut_When_GetStatus_Expect_Correct" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    std::string desc = channel->Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}


TEST_F(AicpuTsRoceChannelTest, Ut_When_GetRemoteMem_Expect_Success)
{
    std::cout << "Start Ut_When_GetRemoteMem_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    HcclMem *remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTagsArray[10] = {nullptr};
    HcclResult ret = channel->GetRemoteMem(&remoteMem, &memNum, memTagsArray);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_GetRemoteMem_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_GetQpNum_Expect_Success)
{
    std::cout << "Start Ut_When_GetQpNum_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    uint32_t qpNum = 0;
    HcclResult ret = channel->GetQpNum(&qpNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_GetQpNum_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetLocNotifyInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetLocNotifyInfo_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    Notify* notify = nullptr;
    HcclResult ret = channel->BuildAndGetLocNotifyInfo(&notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetLocNotifyInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    Notify* notify = nullptr;
    HcclResult ret = channel->BuildAndGetRmtNotifyInfo(&notify);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetRmtNotifyInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetLocBufInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetLocBufInfo_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    ProtectionInfo* protectionInfo = nullptr;
    HcclResult ret = channel->BuildAndGetLocBufInfo(&protectionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetLocBufInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetRmtBufInfo_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetRmtBufInfo_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    ProtectionInfo* protectionInfo = nullptr;
    HcclResult ret = channel->BuildAndGetRmtBufInfo(&protectionInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetRmtBufInfo_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetSqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetSqContext_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    SqContext* sqContext = nullptr;
    HcclResult ret = channel->BuildAndGetSqContext(&sqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetSqContext_Expect_Success" << std::endl;
}

TEST_F(AicpuTsRoceChannelTest, Ut_When_BuildAndGetCqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildAndGetCqContext_Expect_Success" << std::endl;
    
    auto channel = std::make_unique<AicpuTsRoceChannel>(endpointHandle, channelDesc, CommEngine::COMM_ENGINE_AICPU);
    channel->Init();
    
    CqContext* cqContext = nullptr;
    HcclResult ret = channel->BuildAndGetCqContext(&cqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildAndGetCqContext_Expect_Success" << std::endl;
}
