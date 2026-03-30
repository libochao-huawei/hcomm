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
#include "framework/next/comms/endpoint_pairs/channels/aicpu/dev_rdma_connection.h"
#include "socket.h"
#include "orion_adapter_hccp.h"
#include "hccp_common.h"
#define private public

using namespace hcomm;

class NdaRdmaConnectionTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "NdaRdmaConnectionTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "NdaRdmaConnectionTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in NdaRdmaConnectionTest SetUP" << std::endl;
        
        // 创建模拟socket
        fakeSocket = new Hccl::Socket(nullptr, localIp_, listenPort_, remoteIp_, tag_, Hccl::SocketRole::SERVER, 
                                      Hccl::NicType::HOST_NIC_TYPE);
        
        // Mock依赖项
        MOCKER_CPP(&Hccl::Socket::GetStatus).stubs().will(returnValue((Hccl::SocketStatus)Hccl::SocketStatus::OK));
        MOCKER(Hccl::HrtRaQpCreate).stubs().with(any(), any(), any()).will(returnValue((Hccl::QpHandle)0x1000000));
        MOCKER(Hccl::HrtRaDestroyQpWithCq).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));
        MOCKER(RaCreateCompChannel).stubs().with(any(), any()).will(returnValue(0));
        MOCKER(RaDestroyCompChannel).stubs().with(any(), any()).will(returnValue(0));
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in NdaRdmaConnectionTest TearDown" << std::endl;
    }
    
    // 模拟对象
    Hccl::Socket* fakeSocket;
    Hccl::IpAddress localIp_;
    Hccl::IpAddress remoteIp_;
    u32 listenPort_ = 60001;
    std::string tag_ = "test_socket";
};

TEST_F(NdaRdmaConnectionTest, Ut_When_Normal_Call_Expect_Status_Consistent)
{
    std::cout << "Start Ut_When_Normal_Call_Expect_Status_Consistent" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 验证初始状态
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::CLOSED);
    
    // 测试Init
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::INIT);
    
    // 测试CreateQp
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::QP_CREATED);
    
    // 测试GetExchangeDto和ParseRmtExchangeDto
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(locDto, nullptr);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试ModifyQp
    ret = rdmaConnection.ModifyQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::QP_MODIFIED);
    
    // 测试DestroyQp
    rdmaConnection.DestroyQp();
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::CLOSED);
    
    std::cout << "End Ut_When_Normal_Call_Expect_Status_Consistent" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_Init_Failed_Expect_ERROR)
{
    std::cout << "Start Ut_When_Init_Failed_Expect_ERROR" << std::endl;
    
    // Mock NdaGetOps失败
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(-1));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 测试Init
    HcclResult ret = rdmaConnection.Init();
    EXPECT_NE(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::CLOSED);
    
    std::cout << "End Ut_When_Init_Failed_Expect_ERROR" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_CreateQp_Failed_Expect_ERROR)
{
    std::cout << "Start Ut_When_CreateQp_Failed_Expect_ERROR" << std::endl;
    
    // Mock NdaOps相关函数，其中NdaQpCreate失败
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(-1));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试CreateQp
    ret = rdmaConnection.CreateQp();
    EXPECT_NE(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::INIT);
    
    std::cout << "End Ut_When_CreateQp_Failed_Expect_ERROR" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_CqCreate_Failed_Expect_ERROR)
{
    std::cout << "Start Ut_When_CqCreate_Failed_Expect_ERROR" << std::endl;
    
    // Mock NdaOps相关函数，其中NdaCqCreate失败
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(-1));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试CreateQp
    ret = rdmaConnection.CreateQp();
    EXPECT_NE(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::INIT);
    
    std::cout << "End Ut_When_CqCreate_Failed_Expect_ERROR" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_ModifyQp_Failed_Expect_ERROR)
{
    std::cout << "Start Ut_When_ModifyQp_Failed_Expect_ERROR" << std::endl;
    
    // Mock NdaOps相关函数，其中NdaModifyQpToRtr失败
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(-1));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建交换DTO并解析
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试ModifyQp
    ret = rdmaConnection.ModifyQp();
    EXPECT_NE(ret, HCCL_SUCCESS);
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::QP_CREATED);
    
    std::cout << "End Ut_When_ModifyQp_Failed_Expect_ERROR" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_QpAttrDto_IsValid_Expect_Correct)
{
    std::cout << "Start Ut_When_QpAttrDto_IsValid_Expect_Correct" << std::endl;
    
    // 创建QpAttrDto实例并测试IsValid方法
    DevRdmaConnection::QpAttrDto validDto;
    validDto.qpn = 123;
    validDto.psn = 456;
    EXPECT_TRUE(validDto.IsValid());
    
    DevRdmaConnection::QpAttrDto invalidDto1;
    invalidDto1.qpn = UINT32_MAX;
    invalidDto1.psn = 456;
    EXPECT_FALSE(invalidDto1.IsValid());
    
    DevRdmaConnection::QpAttrDto invalidDto2;
    invalidDto2.qpn = 123;
    invalidDto2.psn = UINT32_MAX;
    EXPECT_FALSE(invalidDto2.IsValid());
    
    DevRdmaConnection::QpAttrDto invalidDto3;
    invalidDto3.qpn = UINT32_MAX;
    invalidDto3.psn = UINT32_MAX;
    EXPECT_FALSE(invalidDto3.IsValid());
    
    std::cout << "End Ut_When_QpAttrDto_IsValid_Expect_Correct" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_Describe_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_Describe_Expect_NotEmpty" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试Describe方法
    std::string desc = rdmaConnection.Describe();
    EXPECT_FALSE(desc.empty());
    
    std::cout << "End Ut_When_Describe_Expect_NotEmpty" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_BuildSqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildSqContext_Expect_Success" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建交换DTO并解析
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ModifyQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试BuildSqContext
    SqContext sqContext;
    ret = rdmaConnection.BuildSqContext(&sqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildSqContext_Expect_Success" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_BuildCqContext_Expect_Success)
{
    std::cout << "Start Ut_When_BuildCqContext_Expect_Success" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建交换DTO并解析
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ModifyQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试BuildCqContext
    CqContext cqContext;
    ret = rdmaConnection.BuildCqContext(&cqContext);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    std::cout << "End Ut_When_BuildCqContext_Expect_Success" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_GetUniqueId_Expect_NotEmpty)
{
    std::cout << "Start Ut_When_GetUniqueId_Expect_NotEmpty" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建交换DTO并解析
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ModifyQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试GetUniqueId
    std::vector<char> uniqueId = rdmaConnection.GetUniqueId();
    EXPECT_FALSE(uniqueId.empty());
    
    std::cout << "End Ut_When_GetUniqueId_Expect_NotEmpty" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_DestroyQp_Expect_Success)
{
    std::cout << "Start Ut_When_DestroyQp_Expect_Success" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToInit).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRtr).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaModifyQpToRts).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 创建交换DTO并解析
    std::unique_ptr<Hccl::Serializable> locDto;
    ret = rdmaConnection.GetExchangeDto(locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ParseRmtExchangeDto(*locDto);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.ModifyQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 测试DestroyQp
    rdmaConnection.DestroyQp();
    EXPECT_EQ(rdmaConnection.rdmaConnStatus_, DevRdmaConnection::RdmaConnStatus::CLOSED);
    
    std::cout << "End Ut_When_DestroyQp_Expect_Success" << std::endl;
}

TEST_F(NdaRdmaConnectionTest, Ut_When_GetExchangeDto_Twice_Expect_Success)
{
    std::cout << "Start Ut_When_GetExchangeDto_Twice_Expect_Success" << std::endl;
    
    // Mock NdaOps相关函数
    MOCKER(NdaGetOps).stubs().with(any()).will(returnValue(0));
    MOCKER(NdaQpCreate).stubs().with(any(), any()).will(returnValue(0));
    MOCKER(NdaCqCreate).stubs().with(any(), any()).will(returnValue(0));
    
    // 创建DevRdmaConnection实例
    RdmaHandle rdmaHandle = (RdmaHandle)0x1000000;
    DevRdmaConnection rdmaConnection(fakeSocket, rdmaHandle);
    
    // 初始化和创建QP
    HcclResult ret = rdmaConnection.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    ret = rdmaConnection.CreateQp();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 第一次获取ExchangeDto
    std::unique_ptr<Hccl::Serializable> locDto1;
    ret = rdmaConnection.GetExchangeDto(locDto1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(locDto1, nullptr);
    
    // 第二次获取ExchangeDto
    std::unique_ptr<Hccl::Serializable> locDto2;
    ret = rdmaConnection.GetExchangeDto(locDto2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(locDto2, nullptr);
    
    std::cout << "End Ut_When_GetExchangeDto_Twice_Expect_Success" << std::endl;
}