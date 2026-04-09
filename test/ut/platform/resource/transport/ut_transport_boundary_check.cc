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
#include <vector>

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "transport_ibverbs_pub.h"
#include "transport_direct_npu_pub.h"
#include "transport_roce_pub.h"
#include "transport_tcp_pub.h"
#include "transport_shm_event_pub.h"
#include "mem_host_pub.h"
#include "mem_device_pub.h"
#include "sal.h"
#include "adapter_rts.h"
#include "ascend_hal.h"
#include "dispatcher_aicpu_pub.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportBoundaryCheck_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportBoundaryCheck_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportBoundaryCheck_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        dispatcher = new (std::nothrow) DispatcherAiCpu(0);
        s32 portNum = 7;
        MOCKER(hrtGetHccsPortNum)
            .stubs()
            .with(any(), outBound(portNum))
            .will(returnValue(HCCL_SUCCESS));
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        delete dispatcher;
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }

    DispatcherPub *dispatcher;
    std::unique_ptr<NotifyPool> notifyPool;
    MachinePara machinePara;
    std::chrono::milliseconds timeout;
};

// ========== TransportIbverbs测试 ==========

// 测试TransportIbverbs::CreateOneQp的错误处理和资源回滚
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_create_one_qp_rollback)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock SetQpAttrQos返回失败
    MOCKER(TransportIbverbs::SetQpAttrQos)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    s32 qpMode = 0;
    u32 qpsPerConnection = 1;
    u32 udpSport = 0;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo;

    HcclResult ret = transIbverbs->CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, true, udpSport);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
    
    // 预期qpHandle为nullptr（资源被正确回滚）
    EXPECT_EQ(qpHandle, nullptr);
}

// 测试TransportIbverbs::CreateOneQp中hrtRaGetQpAttr失败时的资源回滚
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_create_one_qp_get_attr_fail)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock前几个步骤成功，但hrtRaGetQpAttr失败
    MOCKER(TransportIbverbs::SetQpAttrQos)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(TransportIbverbs::SetQpAttrTimeOut)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(TransportIbverbs::SetQpAttrRetryCnt)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_E_INTERNAL));

    s32 qpMode = 0;
    u32 qpsPerConnection = 1;
    u32 udpSport = 0;
    QpHandle qpHandle = nullptr;
    AiQpInfo aiQpInfo;

    HcclResult ret = transIbverbs->CreateOneQp(qpMode, qpsPerConnection, qpHandle, aiQpInfo, true, udpSport);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs::CreateMultiQp的资源回滚
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_create_multi_qp_rollback)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;
    machinePara.srcPorts = {61000, 61001, 61002};

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock第一个成功，第二个失败
    int createCallCount = 0;
    MOCKER(TransportIbverbs::SetQpAttrQos)
        .stubs()
        .will(invoke([&createCallCount]() -> HcclResult {
            createCallCount++;
            if (createCallCount <= 1) {
                return HCCL_SUCCESS;  // 第一个成功
            }
            return HCCL_E_INTERNAL;  // 第二个失败
        }));
    
    MOCKER(TransportIbverbs::SetQpAttrTimeOut)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(TransportIbverbs::SetQpAttrRetryCnt)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    
    MOCKER(hrtRaGetQpAttr)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    s32 qpMode = 0;
    u32 qpsPerConnection = 1;

    HcclResult ret = transIbverbs->CreateMultiQp(qpMode, qpsPerConnection);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
    
    // 预期multiCombineQpHandles_为空（资源被正确回滚）
    EXPECT_EQ(transIbverbs->multiCombineQpHandles_.size(), 0);
}

// 测试TransportIbverbs::TxPayLoad的边界检查 - dstMemPtr为空
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_tx_pay_load_null_ptr)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock GetMemInfo返回nullptr
    MOCKER(TransportIbverbs::GetMemInfo)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));

    transIbverbs->remoteOutputMemPtr_ = nullptr;

    Stream stream;
    std::vector<WqeInfo> wqeInfoVec;
    u64 dstOffset = 0;
    const void* src = &dstOffset;
    u64 len = 100;
    WqeType wqeType = WqeType::RDMA_WRITE;
    TransportAuxInfo aux;

    HcclResult ret = transIbverbs->TxPayLoad(UserMemType::OUTPUT_MEM, dstOffset, src, len, 
                                              wqeType, aux, wqeInfoVec, stream);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs::TxPayLoad的边界检查 - dstOffset越界
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_tx_pay_load_dst_offset_overflow)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock GetMemInfo
    MOCKER(TransportIbverbs::GetMemInfo)
        .stubs()
        .with(any(), any())
        .will(invoke([](UserMemType memType, void** memPtr) -> HcclResult {
            *memPtr = reinterpret_cast<void*>(0x1000);
            return HCCL_SUCCESS;
        }));

    // 设置远程内存大小
    transIbverbs->remoteOutputSize_ = 0x800;

    Stream stream;
    std::vector<WqeInfo> wqeInfoVec;
    u64 dstOffset = 0x1000;  // 超过remoteOutputSize
    const void* src = &dstOffset;
    u64 len = 100;
    WqeType wqeType = WqeType::RDMA_WRITE;
    TransportAuxInfo aux;

    HcclResult ret = transIbverbs->TxPayLoad(UserMemType::OUTPUT_MEM, dstOffset, src, len, 
                                              wqeType, aux, wqeInfoVec, stream);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs::TxPayLoad的边界检查 - len越界
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_tx_pay_load_len_overflow)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // Mock GetMemInfo
    MOCKER(TransportIbverbs::GetMemInfo)
        .stubs()
        .with(any(), any())
        .will(invoke([](UserMemType memType, void** memPtr) -> HcclResult {
            *memPtr = reinterpret_cast<void*>(0x1000);
            return HCCL_SUCCESS;
        }));

    // 设置远程内存大小
    transIbverbs->remoteOutputSize_ = 0x1000;

    Stream stream;
    std::vector<WqeInfo> wqeInfoVec;
    u64 dstOffset = 0x400;
    const void* src = &dstOffset;
    u64 len = 0xC00;  // len > (0x1000 - 0x400) = 0x600
    WqeType wqeType = WqeType::RDMA_WRITE;
    TransportAuxInfo aux;

    HcclResult ret = transIbverbs->TxPayLoad(UserMemType::OUTPUT_MEM, dstOffset, src, len, 
                                              wqeType, aux, wqeInfoVec, stream);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs::GetIndOpRemoteAddr的空间检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_get_ind_op_remote_addr_insufficient_space)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // 测试exchangeDataBlankSize不足
    u8 exchangeData[4];
    u8* exchangeDataPtr = exchangeData;
    u64 exchangeDataBlankSize = 3;  // 小于sizeof(u32)=4

    HcclResult ret = transIbverbs->GetIndOpRemoteAddr(exchangeDataPtr, exchangeDataBlankSize);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportIbverbs::GetRemoteNotifyAddr的空间检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_ibverbs_get_remote_notify_addr_insufficient_space)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.isAicpuModeEn = true;

    std::shared_ptr<TransportIbverbs> transIbverbs;
    transIbverbs.reset(new TransportIbverbs(dispatcher, notifyPool, machinePara, timeout));

    // 测试exchangeDataBlankSize不足
    u8 exchangeData[20];
    u8* exchangeDataPtr = exchangeData;
    u64 exchangeDataBlankSize = sizeof(MemMsg) - 1;

    MemMsg memMsg;
    HcclResult ret = transIbverbs->GetRemoteNotifyAddr(exchangeDataPtr, exchangeDataBlankSize, memMsg);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// ========== TransportRoce测试 ==========

// 测试TransportRoce::GetSocketInfo的vector空检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_roce_get_socket_info_empty)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;

    std::shared_ptr<TransportRoce> transRoce;
    transRoce.reset(new TransportRoce(dispatcher, notifyPool, machinePara, timeout));

    // Mock GetSocketInfos返回包含空vector的结果
    std::vector<std::vector<SocketInfo>> socketsInfo(1);
    socketsInfo[0].clear();  // 空vector
    
    MOCKER(TransportRoce::GetSocketInfos)
        .stubs()
        .with(any())
        .will(invoke([&socketsInfo](std::vector<std::vector<SocketInfo>>& info) -> HcclResult {
            info = socketsInfo;
            return HCCL_SUCCESS;
        }));

    HcclResult ret = transRoce->GetSocketInfo();
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// ========== TransportTcp测试 ==========

// 测试TransportTcp::RxAsync的len最大值检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_tcp_rx_async_len_overflow)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.nicDeploy = NICDeployment::NIC_DEPLOYMENT_DEVICE;

    std::shared_ptr<TransportTcp> transTcp;
    transTcp.reset(new TransportTcp(dispatcher, notifyPool, machinePara, timeout));

    // Mock初始化
    MOCKER(TransportTcp::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    // Mock接收buffer
    transTcp->deviceRecvBuffer_ = DeviceMem::alloc(0x1000);

    Stream stream;
    u64 srcOffset = 0;
    void* dst = &srcOffset;
    // 设置len超过最大值 (UINT64_MAX - TCP_BUFFER_SIZE)
    u64 len = UINT64_MAX;

    HcclResult ret = transTcp->RxAsync(UserMemType::INPUT_MEM, srcOffset, dst, len, stream);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// ========== TransportShmEvent测试 ==========

// 测试TransportShmEvent::WaitPeerMemConfig的offset边界检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_shm_event_wait_peer_mem_config_offset_overflow)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;

    std::shared_ptr<TransportShmEvent> transShmEvent;
    transShmEvent.reset(new TransportShmEvent(dispatcher, notifyPool, machinePara, timeout));

    // 测试offset >= size的情况
    void* memPtr = nullptr;
    u8 memName[] = "test_mem";
    u64 size = 0x1000;
    u64 offset = 0x1000;  // offset == size

    HcclResult ret = transShmEvent->WaitPeerMemConfig(&memPtr, memName, size, offset);
    
    // 预期返回错误
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// ========== TransportDirectNpu测试 ==========

// 测试TransportDirectNpu::TxData的地址范围边界检查
TEST_F(TransportBoundaryCheck_UT, ut_transport_direct_npu_tx_data_address_range)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.inputMem = DeviceMem::alloc(0x1000);
    machinePara.outputMem = DeviceMem::alloc(0x1000);

    std::shared_ptr<TransportDirectNpu> transDirectNpu;
    transDirectNpu.reset(new TransportDirectNpu(dispatcher, notifyPool, machinePara, timeout));

    // Mock初始化
    MOCKER(TransportDirectNpu::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    Stream stream;
    u64 dstOffset = 0;
    const void* src = reinterpret_cast<void*>(0x2000);  // 超过outputMem的地址范围
    u64 len = 100;

    HcclResult ret = transDirectNpu->TxData(UserMemType::OUTPUT_MEM, dstOffset, src, len, stream);
    
    // 预期返回错误（地址超出范围）
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// 测试TransportDirectNpu::TxData的地址边界条件 - 边界值
TEST_F(TransportBoundaryCheck_UT, ut_transport_direct_npu_tx_data_address_boundary)
{
    notifyPool.reset(new (std::nothrow) NotifyPool());
    machinePara.localDeviceId = 0;
    machinePara.inputMem = DeviceMem::alloc(0x1000);
    machinePara.outputMem = DeviceMem::alloc(0x1000);

    std::shared_ptr<TransportDirectNpu> transDirectNpu;
    transDirectNpu.reset(new TransportDirectNpu(dispatcher, notifyPool, machinePara, timeout));

    // Mock初始化
    MOCKER(TransportDirectNpu::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    u64 inputMemAddr = reinterpret_cast<u64>(machinePara.inputMem.ptr());
    u64 outputMemAddr = reinterpret_cast<u64>(machinePara.outputMem.ptr());

    Stream stream;
    u64 dstOffset = 0;
    u64 len = 100;

    // 测试inputMem的边界条件
    const void* src1 = reinterpret_cast<void*>(inputMemAddr + 0x1000 - 1);  // 边界内
    HcclResult ret1 = transDirectNpu->TxData(UserMemType::INPUT_MEM, dstOffset, src1, len, stream);
    // 边界内应该成功或返回其他错误（不是地址越界错误）
    
    // 测试outputMem的边界条件（修复后，<=改为<）
    const void* src2 = reinterpret_cast<void*>(outputMemAddr + 0x1000 - 1);  // 边界内
    HcclResult ret2 = transDirectNpu->TxData(UserMemType::OUTPUT_MEM, dstOffset, src2, len, stream);
    // 边界内应该成功或返回其他错误（不是地址越界错误）
    
    const void* src3 = reinterpret_cast<void*>(outputMemAddr + 0x1000);  // 边界外
    HcclResult ret3 = transDirectNpu->TxData(UserMemType::OUTPUT_MEM, dstOffset, src3, len, stream);
    // 边界外应该返回地址越界错误
    EXPECT_NE(ret3, HCCL_SUCCESS);
}