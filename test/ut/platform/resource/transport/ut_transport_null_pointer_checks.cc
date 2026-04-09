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
#include "transport_p2p.h"
#include "transport_ibverbs.h"
#include "transport_direct_npu.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "dispatcher_aicpu_pub.h"

using namespace std;
using namespace hccl;

// 测试TransportP2p的TxAsync空指针检查
TEST(TransportNullPointerCheckTest, Ut_TransportP2p_TxAsync_When_GetRemoteMemReturnsNull_Expect_HCCL_E_PTR)
{
    MachinePara machinePara;
    machinePara.linkAttribute = 0; // 不支持目的端发起
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportP2p transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetRemoteMem返回空指针
    void* nullMemPtr = nullptr;
    MOCKER(&TransportP2p::GetRemoteMem).stubs().with(any(), outBound(&nullMemPtr)).will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    UserMemType dstMemType = USER_MEM_TYPE_OUTPUT;
    u64 dstOffset = 0;
    const void* src = (void*)0x1000;
    u64 len = 1024;
    Stream stream;
    
    // 调用TxAsync，应该返回HCCL_E_PTR
    HcclResult ret = transport.TxAsync(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TransportP2p的TxAsync(vector)空指针检查
TEST(TransportNullPointerCheckTest, Ut_TransportP2p_TxAsyncVector_When_GetRemoteMemReturnsNull_Expect_HCCL_E_PTR)
{
    MachinePara machinePara;
    machinePara.linkAttribute = 0;
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportP2p transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetRemoteMem返回空指针
    void* nullMemPtr = nullptr;
    MOCKER(&TransportP2p::GetRemoteMem).stubs().with(any(), outBound(&nullMemPtr)).will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    std::vector<TxMemoryInfo> txMems;
    TxMemoryInfo memInfo;
    memInfo.src = (void*)0x1000;
    memInfo.dstMemType = USER_MEM_TYPE_OUTPUT;
    memInfo.dstOffset = 0;
    memInfo.len = 1024;
    txMems.push_back(memInfo);
    
    Stream stream;
    
    // 调用TxAsync，应该返回HCCL_E_PTR
    HcclResult ret = transport.TxAsync(txMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TransportP2p的RxAsync空指针检查
TEST(TransportNullPointerCheckTest, Ut_TransportP2p_RxAsync_When_GetRemoteMemReturnsNull_Expect_HCCL_E_PTR)
{
    MachinePara machinePara;
    machinePara.linkAttribute = 2; // 支持目的端发起
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportP2p transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetRemoteMem返回空指针
    void* nullMemPtr = nullptr;
    MOCKER(&TransportP2p::GetRemoteMem).stubs().with(any(), outBound(&nullMemPtr)).will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    UserMemType srcMemType = USER_MEM_TYPE_INPUT;
    u64 srcOffset = 0;
    void* dst = (void*)0x2000;
    u64 len = 1024;
    Stream stream;
    
    // 调用RxAsync，应该返回HCCL_E_PTR
    HcclResult ret = transport.RxAsync(srcMemType, srcOffset, dst, len, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TransportP2p的RxAsync(vector)空指针检查
TEST(TransportNullPointerCheckTest, Ut_TransportP2p_RxAsyncVector_When_GetRemoteMemReturnsNull_Expect_HCCL_E_PTR)
{
    MachinePara machinePara;
    machinePara.linkAttribute = 0;
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportP2p transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetRemoteMem返回空指针
    void* nullMemPtr = nullptr;
    MOCKER(&TransportP2p::GetRemoteMem).stubs().with(any(), outBound(&nullMemPtr)).will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    std::vector<RxMemoryInfo> rxMems;
    RxMemoryInfo memInfo;
    memInfo.dst = (void*)0x2000;
    memInfo.srcMemType = USER_MEM_TYPE_INPUT;
    memInfo.srcOffset = 0;
    memInfo.len = 1024;
    rxMems.push_back(memInfo);
    
    Stream stream;
    
    // 调用RxAsync，应该返回HCCL_E_PTR
    HcclResult ret = transport.RxAsync(rxMems, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TransportDirectNpu的边界检查
TEST(TransportNullPointerCheckTest, Ut_TransportDirectNpu_TxData_BoundaryCheck_Expect_Success)
{
    MachinePara machinePara;
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportDirectNpu transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // 测试边界情况：src地址正好等于outputMemDetails.addr + outputMemDetails.size
    UserMemType dstMemType = USER_MEM_TYPE_OUTPUT;
    u64 dstOffset = 0;
    u64 len = 1024;
    const void* src = (void*)0x3000;
    Stream stream;
    
    // Mock GetMemInfo返回有效的内存信息
    void* validMemPtr = (void*)0x4000;
    u64 validMemSize = 4096;
    MOCKER(&TransportDirectNpu::GetMemInfo).stubs().with(any(), outBound(&validMemPtr)).will(returnValue(HCCL_SUCCESS));
    
    // Mock其他必要的方法
    MOCKER(&TransportDirectNpu::TxWqeList).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&TransportDirectNpu::RdmaSendAsync).stubs().will(returnValue(HCCL_SUCCESS));
    
    // 调用TxData
    HcclResult ret = transport.TxData(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs的TxPayLoad空指针检查
TEST(TransportNullPointerCheckTest, Ut_TransportIbverbs_TxPayLoad_When_GetMemInfoReturnsNull_Expect_HCCL_E_PTR)
{
    MachinePara machinePara;
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportIbverbs transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetMemInfo返回空指针
    void* nullMemPtr = nullptr;
    u64 memSize = 0;
    MOCKER(&TransportIbverbs::GetMemInfo).stubs().with(any(), outBound(&nullMemPtr)).will(returnValue(HCCL_SUCCESS));
    MOCKER(&TransportIbverbs::GetMemInfo).stubs().with(any(), any(), outBound(&memSize)).will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    UserMemType dstMemType = USER_MEM_TYPE_OUTPUT;
    u64 dstOffset = 0;
    const void* src = (void*)0x5000;
    u64 len = 1024;
    Stream stream;
    
    // 调用TxPayLoad，应该返回HCCL_E_PTR
    HcclResult ret = transport.TxPayLoad(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// 测试TransportIbverbs的TxPayLoad正常情况
TEST(TransportNullPointerCheckTest, Ut_TransportIbverbs_TxPayLoad_When_MemValid_Expect_Success)
{
    MachinePara machinePara;
    machinePara.deviceLogicId = 0;
    
    std::unique_ptr<DispatcherAiCpu> dispatcher(new (std::nothrow) DispatcherAiCpu(0));
    std::unique_ptr<NotifyPool> notifyPool(new (std::nothrow) NotifyPool());
    std::chrono::milliseconds timeout;
    
    TransportIbverbs transport(dispatcher.get(), notifyPool.get(), machinePara, timeout);
    
    // Mock GetMemInfo返回有效的内存指针
    void* validMemPtr = (void*)0x6000;
    u64 validMemSize = 4096;
    MOCKER(&TransportIbverbs::GetMemInfo).stubs().with(any(), outBound(&validMemPtr)).will(returnValue(HCCL_SUCCESS));
    MOCKER(&TransportIbverbs::GetMemInfo).stubs().with(any(), any(), outBound(&validMemSize)).will(returnValue(HCCL_SUCCESS));
    
    // Mock其他必要的方法
    MOCKER(&TransportIbverbs::TxSendWqe).stubs().will(returnValue(HCCL_SUCCESS));
    MOCKER(&TransportIbverbs::RdmaSendAsync).stubs().will(returnValue(HCCL_SUCCESS));
    
    // 准备测试数据
    UserMemType dstMemType = USER_MEM_TYPE_OUTPUT;
    u64 dstOffset = 0;
    const void* src = (void*)0x7000;
    u64 len = 1024;
    Stream stream;
    
    // 调用TxPayLoad，应该返回HCCL_SUCCESS
    HcclResult ret = transport.TxPayLoad(dstMemType, dstOffset, src, len, stream);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}