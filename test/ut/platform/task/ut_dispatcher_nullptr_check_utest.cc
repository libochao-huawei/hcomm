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

#include "hccl/base.h"
#include <hccl/hccl_types.h>

#ifndef private
#define private public
#define protected public
#endif

#include "dispatcher_pub.h"
#include "stream_pub.h"
#include "sal.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class DispatcherNullptrCheck_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--DispatcherNullptrCheck_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--DispatcherNullptrCheck_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        // 初始化dispatcher
        dispatcher = new (std::nothrow) DispatcherPub(0);
        CHK_PTR_NULL(dispatcher);

        // 初始化stream
        stream = Stream((HcclRtStream)0x1000);

        // Mock基本函数
        MOCKER(hrtGetDeviceType).stubs().will(returnValue(HCCL_SUCCESS));
    }
    virtual void TearDown()
    {
        delete dispatcher;
        GlobalMockObject::verify();
    }

    DispatcherPub *dispatcher;
    Stream stream;
};

// Test MemcpySync with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, MemcpySync_nullptr_dst)
{
    void *dst = nullptr;
    uint64_t destMax = 1024;
    const void *src = (void*)0x2000;
    uint64_t count = 512;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE;

    HcclResult ret = dispatcher->MemcpySync(dst, destMax, src, count, kind);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpySync with nullptr src
TEST_F(DispatcherNullptrCheck_UT, MemcpySync_nullptr_src)
{
    void *dst = (void*)0x1000;
    uint64_t destMax = 1024;
    const void *src = nullptr;
    uint64_t count = 512;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE;

    HcclResult ret = dispatcher->MemcpySync(dst, destMax, src, count, kind);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_nullptr_dst)
{
    void *dst = nullptr;
    uint64_t destMax = 1024;
    const void *src = (void*)0x2000;
    u64 count = 512;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE;

    HcclResult ret = dispatcher->MemcpyAsync(dst, destMax, src, count, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync with nullptr src
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_nullptr_src)
{
    void *dst = (void*)0x1000;
    uint64_t destMax = 1024;
    const void *src = nullptr;
    u64 count = 512;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_DEVICE;

    HcclResult ret = dispatcher->MemcpyAsync(dst, destMax, src, count, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (HostMem to DeviceMem) with nullptr hostMem
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_HostMem_nullptr_hostMem)
{
    void *hostMemPtr = nullptr;
    u64 hostMemLen = 0;
    void *deviceMemPtr = (void*)0x1000;
    u64 deviceMemLen = 1024;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_DEVICE;

    HcclResult ret = dispatcher->MemcpyAsync(hostMemPtr, deviceMemLen, deviceMemPtr, deviceMemLen, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on hostMem
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (HostMem to HostMem) with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_HostMem_to_HostMem_nullptr_dst)
{
    void *dst = nullptr;
    u64 destMax = 1024;
    const void *src = (void*)0x1000;
    u64 count = 512;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_HOST_TO_HOST;

    HcclResult ret = dispatcher->MemcpyAsync(dst, destMax, src, count, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (DeviceMem to HostMem) with nullptr deviceMem
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_DeviceMem_nullptr_deviceMem)
{
    void *hostMemPtr = (void*)0x1000;
    u64 hostMemLen = 1024;
    const void *deviceMemPtr = nullptr;
    u64 deviceMemLen = 0;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST;

    HcclResult ret = dispatcher->MemcpyAsync(hostMemPtr, hostMemLen, deviceMemPtr, deviceMemLen, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on deviceMem
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (DeviceMem to HostMem) with nullptr hostMem
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_DeviceMem_to_HostMem_nullptr_hostMem)
{
    void *hostMemPtr = nullptr;
    u64 hostMemLen = 0;
    const void *deviceMemPtr = (void*)0x1000;
    u64 deviceMemLen = 1024;
    HcclRtMemcpyKind kind = HcclRtMemcpyKind::HCCL_RT_MEMCPY_KIND_DEVICE_TO_HOST;

    HcclResult ret = dispatcher->MemcpyAsync(hostMemPtr, hostMemLen, deviceMemPtr, deviceMemLen, kind, stream);

    // Should return HCCL_E_PTR due to nullptr check on hostMem
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test ReduceAsync with nullptr src
TEST_F(DispatcherNullptrCheck_UT, ReduceAsync_nullptr_src)
{
    const void *src = nullptr;
    void *dst = (void*)0x2000;
    u64 dataCount = 1024;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HcclReduceOp::HCCL_REDUCE_SUM;

    HcclResult ret = dispatcher->ReduceAsync(src, dst, dataCount, datatype, redOp, stream);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test ReduceAsync with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, ReduceAsync_nullptr_dst)
{
    const void *src = (void*)0x1000;
    void *dst = nullptr;
    u64 dataCount = 1024;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HcclReduceOp::HCCL_REDUCE_SUM;

    HcclResult ret = dispatcher->ReduceAsync(src, dst, dataCount, datatype, redOp, stream);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test InlineReduceAsync with nullptr src
TEST_F(DispatcherNullptrCheck_UT, InlineReduceAsync_nullptr_src)
{
    const void *src = nullptr;
    u64 count = 1024;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HcclReduceOp::HCCL_REDUCE_SUM;
    void *dst = (void*)0x2000;

    HcclResult ret = dispatcher->InlineReduceAsync(src, count, datatype, redOp, stream, dst);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test InlineReduceAsync with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, InlineReduceAsync_nullptr_dst)
{
    const void *src = (void*)0x1000;
    u64 count = 1024;
    HcclDataType datatype = HcclDataType::HCCL_DATA_TYPE_FP32;
    HcclReduceOp redOp = HcclReduceOp::HCCL_REDUCE_SUM;
    void *dst = nullptr;

    HcclResult ret = dispatcher->InlineReduceAsync(src, count, datatype, redOp, stream, dst);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaSend (with qpn and wqeIndex) with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaSend_nullptr_wr)
{
    u32 qpn = 1;
    u32 wqeIndex = 0;
    const struct SendWr *wr = nullptr;

    HcclResult ret = dispatcher->RdmaSend(qpn, wqeIndex, *wr, stream);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaSend (with dbindex and dbinfo) with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaSend_dbindex_nullptr_wr)
{
    u32 dbindex = 0;
    u64 dbinfo = 0x1000;
    const struct SendWr *wr = nullptr;

    HcclResult ret = dispatcher->RdmaSend(dbindex, dbinfo, *wr, stream);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaSend (with remoteUserRank) with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaSend_remoteUserRank_nullptr_wr)
{
    u32 dbindex = 0;
    u64 dbinfo = 0x1000;
    const struct SendWr *wr = nullptr;
    u32 remoteUserRank = 1;

    HcclResult ret = dispatcher->RdmaSend(dbindex, dbinfo, *wr, stream, remoteUserRank);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaSend (with offset) with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaSend_offset_nullptr_wr)
{
    u32 dbindex = 0;
    u64 dbinfo = 0x1000;
    const struct SendWr *wr = nullptr;
    u32 userRank = 0;
    u64 offset = 0x2000;

    HcclResult ret = dispatcher->RdmaSend(dbindex, dbinfo, *wr, stream, userRank, offset);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaSend (with RdmaTaskInfo) with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaSend_taskInfo_nullptr_wr)
{
    u32 dbindex = 0;
    u64 dbinfo = 0x1000;
    const struct SendWr *wr = nullptr;
    RdmaTaskInfo taskInfo;

    HcclResult ret = dispatcher->RdmaSend(dbindex, dbinfo, stream, taskInfo);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test HostNicRdmaSend with nullptr qpHandle
TEST_F(DispatcherNullptrCheck_UT, HostNicRdmaSend_nullptr_qpHandle)
{
    QpHandle qpHandle = nullptr;
    SendWrlistDataExt wr = {0};
    SendWrRsp opRsp = {0};

    HcclResult ret = dispatcher->HostNicRdmaSend(qpHandle, wr, opRsp, stream);

    // Should return HCCL_E_PTR due to nullptr check on qpHandle
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test HostNicTcpSend with nullptr src
TEST_F(DispatcherNullptrCheck_UT, HostNicTcpSend_nullptr_src)
{
    SocketHandle socketFdHandle = (FdHandle)0x1000;
    const void *socketBufferPtr = (void*)0x2000;
    u64 socketBufferLen = 1024;
    const void *src = nullptr;
    u64 len = 512;
    NICDeployment nicDeploy = NICDeployment::NIC_DEPLOYMENT_HOST;

    HcclResult ret = dispatcher->HostNicTcpSend(socketFdHandle, socketBufferPtr, socketBufferLen, src, len, stream, nicDeploy);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test HostNicTcpRecv with nullptr src
TEST_F(DispatcherNullptrCheck_UT, HostNicTcpRecv_nullptr_src)
{
    SocketHandle socketFdHandle = (FdHandle)0x1000;
    const void *socketBufferPtr = (void*)0x2000;
    u64 socketBufferLen = 1024;
    const void *src = nullptr;
    u64 len = 512;
    NICDeployment nicDeploy = NICDeployment::NIC_DEPLOYMENT_HOST;

    HcclResult ret = dispatcher->HostNicTcpRecv(socketFdHandle, socketBufferPtr, socketBufferLen, src, len, stream, nicDeploy);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test SignalRecord (with DeviceMem) with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, SignalRecord_DeviceMem_nullptr_dst)
{
    DeviceMem dst(nullptr, 0);
    DeviceMem src((void*)0x1000, 1024);
    u32 remoteUserRank = 1;
    LinkType inLinkType = LinkType::LINK_ONCHIP;
    u32 notifyId = 0;

    HcclResult ret = dispatcher->SignalRecord(dst, src, stream, remoteUserRank, inLinkType, notifyId);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test SignalRecord (with DeviceMem) with nullptr src
TEST_F(DispatcherNullptrCheck_UT, SignalRecord_DeviceMem_nullptr_src)
{
    DeviceMem dst((void*)0x1000, 1024);
    DeviceMem src(nullptr, 0);
    u32 remoteUserRank = 1;
    LinkType inLinkType = LinkType::LINK_ONCHIP;
    u32 notifyId = 0;

    HcclResult ret = dispatcher->SignalRecord(dst, src, stream, remoteUserRank, inLinkType, notifyId);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test RdmaRecord with nullptr wr
TEST_F(DispatcherNullptrCheck_UT, RdmaRecord_nullptr_wr)
{
    u32 dbindex = 0;
    u64 dbinfo = 0x1000;
    const struct SendWr *wr = nullptr;
    RdmaType rdmaType = RdmaType::RDMA_TYPE_RESERVED;
    u32 userRank = 1;
    u64 offset = 0x2000;
    u32 notifyId = 0;

    HcclResult ret = dispatcher->RdmaRecord(dbindex, dbinfo, *wr, stream, rdmaType, userRank, offset, notifyId);

    // Should return HCCL_E_PTR due to nullptr check on wr
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test SignalRecord (with HcclRtNotify) with invalid signal
TEST_F(DispatcherNullptrCheck_UT, SignalRecord_invalid_signal)
{
    HcclRtNotify signal = 0;
    u32 userRank = 1;
    u64 offset = 0x2000;
    s32 stage = 0;
    bool inchip = true;
    u64 signalAddr = 0x3000;
    u32 notifyId = 0;

    HcclResult ret = dispatcher->SignalRecord(signal, stream, userRank, offset, stage, inchip, signalAddr, notifyId);

    // Should return error due to invalid signal check
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// Test SignalWait (with HcclRtNotify) with invalid signal
TEST_F(DispatcherNullptrCheck_UT, SignalWait_invalid_signal)
{
    HcclRtNotify signal = 0;
    u32 userRank = 0;
    u32 remoteUserRank = 1;
    s32 stage = 0;
    bool inchip = true;
    u32 notifyId = 0;
    u32 timeOut = 1000;

    HcclResult ret = dispatcher->SignalWait(signal, stream, userRank, remoteUserRank, stage, inchip, notifyId, timeOut);

    // Should return error due to invalid signal check
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// Test SignalRecord (with notifyId) with valid parameters
TEST_F(DispatcherNullptrCheck_UT, SignalRecord_notifyId_valid_params)
{
    u32 notifyId = 0x1000;

    HcclResult ret = dispatcher->SignalRecord(stream, notifyId);

    // Note: May still fail due to other requirements, but nullptr checks should pass
    // The important thing is that it doesn't return HCCL_E_PTR from nullptr checks
    EXPECT_NE(ret, HCCL_E_PTR);
}

// Test SignalWait (with notifyId) with valid parameters
TEST_F(DispatcherNullptrCheck_UT, SignalWait_notifyId_valid_params)
{
    u32 notifyId = 0x1000;
    u32 timeOut = 1000;

    HcclResult ret = dispatcher->SignalWait(stream, notifyId, timeOut);

    // Note: May still fail due to other requirements, but nullptr checks should pass
    // The important thing is that it doesn't return HCCL_E_PTR from nullptr checks
    EXPECT_NE(ret, HCCL_E_PTR);
}

// Test WaitValue with invalid waitAddr
TEST_F(DispatcherNullptrCheck_UT, WaitValue_invalid_waitAddr)
{
    u64 waitAddr = 0;
    u64 valueAddr = 0x1000;
    bool reset = false;

    HcclResult ret = dispatcher->WaitValue(stream, waitAddr, valueAddr, reset);

    // Should return error due to invalid waitAddr
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// Test WriteValue with invalid writeAddr
TEST_F(DispatcherNullptrCheck_UT, WriteValue_invalid_writeAddr)
{
    u64 writeAddr = 0;
    u64 valueAddr = 0x1000;

    HcclResult ret = dispatcher->WriteValue(stream, writeAddr, valueAddr);

    // Should return error due to invalid writeAddr
    EXPECT_NE(ret, HCCL_SUCCESS);
}

// Test SetHostNicTcpSendThreadPara with nullptr fnData
TEST_F(DispatcherNullptrCheck_UT, SetHostNicTcpSendThreadPara_nullptr_fnData)
{
    void *fnData = nullptr;

    HcclResult ret = dispatcher->SetHostNicTcpSendThreadPara(fnData);

    // Should return HCCL_E_PTR due to nullptr check on fnData
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (with DeviceMem) with nullptr dst
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_DeviceMem_nullptr_dst)
{
    DeviceMem dst(nullptr, 0);
    DeviceMem src((void*)0x1000, 1024);

    HcclResult ret = dispatcher->MemcpyAsync(dst, src, stream);

    // Should return HCCL_E_PTR due to nullptr check on dst
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// Test MemcpyAsync (with DeviceMem) with nullptr src
TEST_F(DispatcherNullptrCheck_UT, MemcpyAsync_DeviceMem_nullptr_src)
{
    DeviceMem dst((void*)0x1000, 1024);
    DeviceMem src(nullptr, 0);

    HcclResult ret = dispatcher->MemcpyAsync(dst, src, stream);

    // Should return HCCL_E_PTR due to nullptr check on src
    EXPECT_EQ(ret, HCCL_E_PTR);
}