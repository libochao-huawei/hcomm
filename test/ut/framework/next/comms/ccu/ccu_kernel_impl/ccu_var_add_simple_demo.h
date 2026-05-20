/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "adapter_hccp.h"
#include "ccu_primitives.hpp"
#include "ccu_types.h"
#include "ccu_log.h" // demo演示使用，hccl仓需要另外实现
#include <vector>

namespace ccu = ::AscendC::ccu;

struct CcuVarAddKernelArg {
    uint64_t numA{0xffffffff};
    uint32_t numB{0};
    ChannelHandle channelHandle{0};
};

// CcuResult CcuVarAddDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable result{}, numA{}, numB{};
//     CCU_CHK_RET(ccu::VariableCreate(&result));
//     CCU_CHK_RET(ccu::VariableCreate(&numA));
//     CCU_CHK_RET(ccu::VariableCreate(&numB));

//     CcuLoad(args->numA, numA, 1);

//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);
//     evt.setMask(0x03);
//     CcuRecordEvent(evt);
//     CcuWaitEvent(evt);

//     numB = args->numB;
//     result = numA + numB;
//     return CcuResult::CCU_SUCCESS;
// }
void test_method(std::vector<ccu::RemoteAddr> &loopsrc){
    for (int i = 0; i < 1; i++) {
        ccu::LocalAddr localAddr1;
        loopsrc.emplace_back(*reinterpret_cast<ccu::RemoteAddr*>(&localAddr1));
    }
}
CcuResult CcuAssignDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr remoteAddr;
    remoteAddr.addr = 0x10000000;
    remoteAddr.token = 0x20000000;
    std::vector<ccu::RemoteAddr> loopsrc;
    test_method(loopsrc);
    loopsrc[0].addr = remoteAddr.addr;
    loopsrc[0].token = remoteAddr.token;
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuAllocDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::Variable varA, varB, result;
    varA  = 1024;
    varB = 2048;
    result=varA + varB;
    ccu::LoadArg(varA,0);
    ccu::LoadArg(varB,1);

    ccu::Variable varC = ccu::GetResByChannel<ccu::Variable>(args->channelHandle, 0);
    varA = varC;

    ccu::Address addrA, addrB, addrResult;
    addrA = 0x80000000;
    addrB = 0x90000000;
    addrResult = addrA + addrB;

    ccu::Event evt;
    ccu::EventRecord(evt);
    ccu::EventWait(evt);

    ccu::Array<ccu::Event> evt2(2);
    ccu::EventRecord(evt2[0]);
    ccu::EventWait(evt2[0]);
    ccu::EventRecord(evt2[1]);
    ccu::EventWait(evt2[1]);

    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalAddrDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::LocalAddr localAddr;
    localAddr.addr = 0x10000000;
    localAddr.token = 0x20000000;

    ccu::LocalAddr localAddr2;
    localAddr2 = localAddr;
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuRemoteAddrDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr remoteAddr;
    remoteAddr.addr = 0x10000000;
    remoteAddr.token = 0x20000000;

    ccu::RemoteAddr remoteAddr2;
    remoteAddr2 = remoteAddr;
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLoadStoreDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::Variable varA, varB, result;
    ccu::Variable srcAddr, dstAddr;
    srcAddr= 0x30000000;
    dstAddr= 0x40000000;
    ccu::Load(0x10000000, varA);
    ccu::Store(0x20000000, varB);
    ccu::Load(srcAddr, varA);
    ccu::Store(dstAddr, varB);
    ccu::Array<ccu::Variable> varArr(2);
    ccu::Array<ccu::Variable> varArr2(2);
    ccu::Load(0x10000000, varArr,2);
    ccu::Store(0x20000000, varArr2,2);
    ccu::Load(srcAddr, varArr,2);
    ccu::Store(dstAddr, varArr2,2);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuNotifyDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    // mask 已与 Event 解耦，统一作为 API 参数显式传入。
    ccu::Array<ccu::Event> evt(3);
    ccu::EventRecord(evt[0], 0x12);
    ccu::EventWait(evt[0], 0x12);
    ccu::EventRecord(evt[1], 0x13);
    ccu::EventWait(evt[1], 0x13);
    ccu::EventRecord(evt[2], 0x14);
    ccu::EventWait(evt[2], 0x14);

    ccu::NotifyRecord(args->channelHandle, 0, 0x12);
    ccu::NotifyWait(args->channelHandle, 0, 0x12);
    ccu::Variable varA;
    varA = 1024;
    ccu::WriteVariableWithNotify(args->channelHandle, varA, 0, 0, 0x12);
    ccu::NotifyWait(args->channelHandle, 0, 0x12);
    return CcuResult::CCU_SUCCESS;
}
// 演示同 device 内跨 core 通知同步：用字符串 notifyTag 作为生产者/消费者的配对标识，
// 同一 tag 必须先 Record 后 Wait；mask 与 tag 一同决定语义，默认 mask=1。
// 与 NotifyRecord/Wait（用 ChannelHandle 标识跨 rank 通道）形成层次对偶。
CcuResult CcuLocalNotifyDemoKernel(CcuKernelArg arg)
{
    (void)arg;
    // 1) 默认 mask = 1：C++ wrapper EventRecord/EventWait(const char*) 重载
    ccu::EventRecord("local_notify_tag_default");
    ccu::EventWait("local_notify_tag_default");

    // 2) 显式 mask：演示同一 tag 上多组掩码的同步配对
    ccu::EventRecord("local_notify_tag_a", 0x12);
    ccu::EventWait("local_notify_tag_a", 0x12);

    // 3) 直接调用 C API CcuLocalNotifyRecord/Wait（C 风格调用点）
    CcuLocalNotifyRecord("local_notify_tag_b", 0x34);
    CcuLocalNotifyWait("local_notify_tag_b", 0x34);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalCopyKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::LocalAddr src, dst;
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Event evt;
    ccu::Array<ccu::CcuBuffer> buf(1);
    ccu::Variable len;
    len = 1024;
    ccu::LocalCopy(buf[0], src, len, evt);
    ccu::EventWait(evt);
    ccu::LocalCopy(dst, buf[0], len, evt);
    ccu::EventWait(evt);
    ccu::LocalCopy(dst, src, len, evt);
    ccu::EventWait(evt);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalReduceKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::LocalAddr src, dst;
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Variable len;
    len = 1024;
    ccu::Event evt;
    ccu::LocalReduce(dst, src, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::EventWait(evt);
    ccu::Array<ccu::CcuBuffer> buf(2);
    ccu::LocalReduce(buf.data(), 2, HCCL_DATA_TYPE_FP16, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, len, evt);
    ccu::EventWait(evt);

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuRemoteReadKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr dst;
    ccu::LocalAddr src;
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Variable len;
    len = 1024;
    ccu::Event evt;
    ccu::Read(args->channelHandle, src, dst, len, evt);
    ccu::EventWait(evt);
    ccu::Array<ccu::CcuBuffer> buf(1);
    ccu::Read(args->channelHandle, buf[0], dst, len, evt);
    ccu::EventWait(evt);
    ccu::ReadReduce(args->channelHandle, src, dst, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::EventWait(evt);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr dst;
    ccu::LocalAddr src;
    ccu::Array<ccu::CcuBuffer> buf(1);
    ccu::Variable len;
    len = 1024;
    ccu::Event evt;
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;

    ccu::Write(args->channelHandle, dst, src, len, evt);
    ccu::EventWait(evt);
    ccu::Write(args->channelHandle, dst, buf[0], len, evt);
    ccu::EventWait(evt);
    ccu::WriteReduce(args->channelHandle, dst, src, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::EventWait(evt);
    return CcuResult::CCU_SUCCESS;
}
// CcuResult CcuAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);

//     ccu::Variable offset, len;
//     ccu::VariableCreate(&offset);
//     ccu::VariableCreate(&len);
//     offset = 64;
//     len = 1024;

//     ccu::Address baseAddr, dstAddr;
//     ccu::AddressCreate(&baseAddr);
//     ccu::AddressCreate(&dstAddr);

//     // 立即数赋值
//     baseAddr = 0x80000000;

//     // 变量赋值
//     baseAddr = offset;

//      // 立即数赋值
//      baseAddr = 0x80000000;

//     // 地址 + 变量偏移
//     dstAddr = baseAddr + offset;   // C++ 重载 

//     // 就地偏移（addr += variable）
//     baseAddr += offset;            // C++ 重载 

//     // // 就地偏移（addr += addr）
//     // baseAddr += dstAddr;           // C++ 重载 → addr = addr + otherAddr → CcuRepAdd

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult ccu::LocalAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable token;
//     ccu::Create(&token);
//     token = 1024;
//     ccu::LocalAddr localAddr;
//     ccu::Create(&localAddr);
//     localAddr.addr = 0x80000000;
//     localAddr.token = token;
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult ccu::LocalAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable token;
//     ccu::VariableCreate(&token);
//     token = 1024;

//     ccu::LocalAddr localAddr{};
//     ccu::LocalAddrCreate(&localAddr);
//     localAddr.addr = 0x80000000;      // 通过 ccu::Address 重载设置地址
//     localAddr.token = token;    // 通过 ccu::Variable 重载设置 token
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalCopyKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 1024;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     ccu::LocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     ccu::LocalAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Copy(dst, src, len, evt);
//     ccu::Wait(evt);
//     ccu::CcuBuffer buf;
//     ccu::BlockCreate(&buf, 1);
//     ccu::Copy(buf, src, len, evt);
//     ccu::Wait(evt);
//     ccu::Copy(dst, buf, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalCopyKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 1024;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);

//     ccu::LocalAddr src, dst;
//     ccu::LocalAddrCreate(&src);
//     ccu::LocalAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuLocalCopyHBMToHBM(dst, src, len, evt);           // LocalAddr → LocalAddr
//     CcuWaitEvent(evt);
    
//     ccu::CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuLocalCopyHBMToBuffer(buf, src, len, evt);   // LocalAddr → CcuBuffer
//     CcuWaitEvent(evt);
//     CcuLocalCopyBufferToHBM(dst, buf, len, evt); // CcuBuffer → LocalAddr
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult ccu::LocalAddrReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 1024;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     ccu::LocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     ccu::LocalAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Reduce(dst, src, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult ccu::LocalAddrReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 1024;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);
//     ccu::LocalAddr src, dst;
//     ccu::LocalAddrCreate(&src);
//     ccu::LocalAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuLocalHBMReduce(dst, src, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     CcuWaitEvent(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalBufferReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 4096;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     ccu::LocalAddr src0, src1;
//     ccu::Create(&src0);
//     ccu::Create(&src1);
//     src0.addr = 0x10000000;
//     src0.token = 0x20000000;
//     src1.addr = 0x30000000;
//     src1.token = 0x40000000;
//     ccu::CcuBuffer bufs[2];
//     ccu::BlockCreate(bufs, 2);
//     ccu::Copy(bufs[0], src0, len, evt);
//     ccu::Wait(evt);
//     ccu::Copy(bufs[1], src1, len, evt);
//     ccu::Wait(evt);
//     ccu::Reduce(bufs, 2,
//         HCCL_DATA_TYPE_FP16, HCCL_DATA_TYPE_FP16,
//         HCCL_REDUCE_SUM, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalBufferReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 512;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);
//     ccu::LocalAddr src0, src1;
//     ccu::LocalAddrCreate(&src0);
//     ccu::LocalAddrCreate(&src1);
//     src0.addr = 0x10000000;
//     src0.token = 0x20000000;
//     src1.addr = 0x30000000;
//     src1.token = 0x40000000;
//     constexpr uint32_t bufCount = 2;
//     ccu::CcuBuffer bufs[bufCount];
//     CcuBlockBufferCreate(bufs, bufCount);
//     CcuLocalCopyHBMToBuffer(bufs[0], src0, len, evt);
//     CcuWaitEvent(evt);
//     CcuLocalCopyHBMToBuffer(bufs[1], src1, len, evt);
//     CcuWaitEvent(evt);
//     CcuLocalBufferReduce(bufs, bufCount,
//         HCCL_DATA_TYPE_FP16, HCCL_DATA_TYPE_FP16,
//         HCCL_REDUCE_SUM, len, evt);
//     CcuWaitEvent(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuRemoteReadKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 1024;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     ccu::LocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     ccu::RemoteAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Read(args->channelHandle, src, dst, len, evt);
//     ccu::Wait(evt);
//     ccu::CcuBuffer buf[2];
//     ccu::BlockCreate(buf, 2);
//     ccu::Read(args->channelHandle, buf[1], dst, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;

// }
// CcuResult CcuRemoteReadKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 1024;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);

//     ccu::LocalAddr src;
//     ccu::RemoteAddr dst;
//     ccu::LocalAddrCreate(&src);
//     ccu::RemoteAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuReadHBMToHBM(args->channelHandle, src, dst, len, evt);
//     CcuWaitEvent(evt);
//     ccu::CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuReadHBMToBuffer(args->channelHandle, buf, dst, len, evt);
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 1024;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);

//     ccu::LocalAddr src;
//     ccu::RemoteAddr dst;
//     ccu::LocalAddrCreate(&src);
//     ccu::RemoteAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuWriteHBMToHBM(args->channelHandle, dst, src, len, evt);
//     CcuWaitEvent(evt);
//     ccu::CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuWriteBufferToHBM(args->channelHandle, dst, buf, len, evt);
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 1024;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     ccu::LocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     ccu::RemoteAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Write(args->channelHandle, dst, src, len, evt);
//     ccu::Wait(evt);
//     ccu::CcuBuffer buf;
//     ccu::BlockCreate(&buf, 1);
//     ccu::Write(args->channelHandle, dst, buf, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuReadWriteReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Variable len;
//     ccu::VariableCreate(&len);
//     len = 1024;
//     ccu::Event evt;
//     CcuCompletedEventCreate(&evt);
//     ccu::LocalAddr src;
//     ccu::RemoteAddr dst;
//     ccu::LocalAddrCreate(&src);
//     ccu::RemoteAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuReadHBMToHBMReduce(args->channelHandle, src, dst, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     CcuWaitEvent(evt);
    
//     CcuWriteHBMToHBMReduce(args->channelHandle, dst, src, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     CcuWaitEvent(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuReadWriteReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);

//     ccu::Variable len;
//     ccu::Create(&len);
//     len = 4096;
//     ccu::Event evt;
//     ccu::Create(&evt);
//     evt.setMask(0x12);
//     ccu::LocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     ccu::RemoteAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::ReadReduce(args->channelHandle, src, dst, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     ccu::Wait(evt);
//     ccu::WriteReduce(args->channelHandle, dst, src, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuSyncKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);

    
//     ccu::Event evt;
//     ccu::Create(&evt);
//     uint32_t mask = 0x12;
//     evt.setMask(mask);
    
//     ccu::Variable var;
//     ccu::Create(&var);
//     var = 1024;
//     ccu::WriteVariableWithNotify(args->channelHandle, var, 0, 0, mask);
//     ccu::NotifyWait(args->channelHandle, 0, mask);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult ccu::EventKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     ccu::Event evt[2];
//     ccu::BlockCreate(evt, 2);
//     ccu::Record(evt[0]);
//     ccu::Wait(evt[0]);
//     ccu::Record(evt[1]);
//     ccu::Wait(evt[1]);
//     ccu::Event single_evt;
//     ccu::Create(&single_evt);
//     ccu::Record(single_evt);
//     ccu::Wait(single_evt);
//     ccu::WriteNotify(args->channelHandle, 0, 0x12);
//     ccu::NotifyWait(args->channelHandle, 0, 0x12);

//     return CcuResult::CCU_SUCCESS;
// }

// CcuResult CcuCreateFromChannelKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     uint32_t varIndex = 0;
//     ccu::Variable tmp;
//     ccu::Create(&tmp);
//     ccu::WriteVariableWithNotify(args->channelHandle,tmp, 0, 0, 0);
//     ccu::Variable var;
//     ccu::CreateFromChannel(args->channelHandle, 0, &var);
//     var = 1024;
//     return CcuResult::CCU_SUCCESS;
// }
