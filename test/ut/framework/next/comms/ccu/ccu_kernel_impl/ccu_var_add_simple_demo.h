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
#include "ccu_api.hpp"
<<<<<<< HEAD
#include "ccu_local_addr.hpp"
#include "ccu_log.h" // demo演示使用，hccl仓需要另外实现
#include "ccu_remote_addr.hpp"
#include <vector>
=======
#include "ccu_log.h" // demo演示使用，hccl仓需要另外实现
>>>>>>> origin/ccu_c

struct CcuVarAddKernelArg {
    uint64_t numA{0xffffffff};
    uint32_t numB{0};
    ChannelHandle channelHandle{0};
};

struct CcuVarAddTaskArg {
};
// CcuResult CcuVarAddDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable result{}, numA{}, numB{};
//     CCU_CHK_RET(CcuVariableCreate(&result));
//     CCU_CHK_RET(CcuVariableCreate(&numA));
//     CCU_CHK_RET(CcuVariableCreate(&numB));

//     CcuLoad(args->numA, numA, 1);

//     CcuEvent evt;
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
        ccu::Alloc(&localAddr1);
        loopsrc.emplace_back(*reinterpret_cast<ccu::RemoteAddr*>(&localAddr1));
    }
}
CcuResult CcuAssignDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr remoteAddr;
    ccu::Alloc(&remoteAddr);
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
    ccu::Alloc(&varA);
    ccu::Alloc(&varB);
    ccu::Alloc(&result);
    ccu::LoadArg(varA);
    ccu::LoadArg(varB);
    result=varA + varB;

    ccu::Variable varC;
    ccu::CreateByChannel(args->channelHandle, 0, &varC);
    varA = varC;

    ccu::Address addrA, addrB, addrResult;
    ccu::Alloc(&addrA);
    ccu::Alloc(&addrB);
    ccu::Alloc(&addrResult);
    addrA = 0x80000000;
    addrB = 0x90000000;
    addrResult = addrA + addrB;

    ccu::Event evt;
    ccu::Alloc(&evt);
    ccu::RecordEvent(evt);
    ccu::WaitEvent(evt);

    ccu::Event evt2[2];
    ccu::BlockAlloc(evt2, 2);
    ccu::RecordEvent(evt2[0]);
    ccu::WaitEvent(evt2[0]);
    ccu::RecordEvent(evt2[1]);
    ccu::WaitEvent(evt2[1]);

    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuNotifyDemoKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::Event evt[3];
    ccu::BlockAlloc(evt, 3);
    ccu::SetMask(evt[0], 0x12);
    ccu::RecordEvent(evt[0]);
    ccu::WaitEvent(evt[0]);
    evt[1].setMask(0x13);
    ccu::RecordEvent(evt[1]);
    ccu::WaitEvent(evt[1]);
    evt[2].mask = 0x14;
    ccu::RecordEvent(evt[2]);
    ccu::WaitEvent(evt[2]);

    ccu::NotifyRecord(args->channelHandle, 0, 0x12);
    ccu::NotifyWait(args->channelHandle, 0, 0x12);
    ccu::Variable varA;
    ccu::Alloc(&varA);
    varA = 1024;
    ccu::WriteVariableWithNotify(args->channelHandle, varA, 0, 0, 0x12);
    ccu::NotifyWait(args->channelHandle, 0, 0x12);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalCopyKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::LocalAddr src, dst;
    ccu::Alloc(&src);
    ccu::Alloc(&dst);
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Event evt;
    ccu::Alloc(&evt);
    ccu::Buffer buf;
    ccu::BlockAlloc(&buf, 1);
    ccu::Variable len;
    ccu::Alloc(&len);
    len = 1024;
    ccu::LocalCopyNb(buf, src, len, evt);
    ccu::WaitEvent(evt);
    ccu::LocalCopyNb(dst, buf, len, evt);
    ccu::WaitEvent(evt);
    ccu::LocalCopyNb(dst, src, len, evt);
    ccu::WaitEvent(evt);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuLocalReduceKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::LocalAddr src, dst;
    ccu::Alloc(&src);
    ccu::Alloc(&dst);
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Variable len;
    ccu::Alloc(&len);
    len = 1024;
    ccu::Event evt;
    ccu::Alloc(&evt);
    ccu::LocalReduceNb(dst, src, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::WaitEvent(evt);
    ccu::Buffer buf[2];
    ccu::BlockAlloc(buf, 2);
    ccu::LocalReduceNb(buf, 2, HCCL_DATA_TYPE_FP16, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, len, evt);
    ccu::WaitEvent(evt);

    return CcuResult::CCU_SUCCESS;
}

CcuResult CcuRemoteReadKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr dst;
    ccu::LocalAddr src;
    ccu::Alloc(&src);
    ccu::Alloc(&dst);
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;
    ccu::Variable len;
    ccu::Alloc(&len);
    len = 1024;
    ccu::Event evt;
    ccu::Alloc(&evt);
    ccu::ReadNb(args->channelHandle, src, dst, len, evt);
    ccu::WaitEvent(evt);
    ccu::Buffer buf;
    ccu::BlockAlloc(&buf, 1);
    ccu::ReadNb(args->channelHandle, buf, dst, len, evt);
    ccu::WaitEvent(evt);
    ccu::ReadReduceNb(args->channelHandle, src, dst, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::WaitEvent(evt);
    return CcuResult::CCU_SUCCESS;
}
CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
{
    auto *args = static_cast<CcuVarAddKernelArg *>(arg);
    ccu::RemoteAddr dst;
    ccu::LocalAddr src;
    ccu::Buffer buf;
    ccu::Alloc(&src);
    ccu::Alloc(&dst);
    ccu::BlockAlloc(&buf, 1);
    ccu::Variable len;
    ccu::Alloc(&len);
    len = 1024;
    ccu::Event evt;
    ccu::Alloc(&evt);
    src.addr = 0x10000000;
    src.token = 0x20000000;
    dst.addr = 0x30000000;
    dst.token = 0x40000000;

    ccu::WriteNb(args->channelHandle, dst, src, len, evt);
    ccu::WaitEvent(evt);
    ccu::WriteNb(args->channelHandle, dst, buf, len, evt);
    ccu::WaitEvent(evt);
    ccu::WriteReduceNb(args->channelHandle, dst, src, len, HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
    ccu::WaitEvent(evt);
    return CcuResult::CCU_SUCCESS;
}
// CcuResult CcuAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);

//     CcuVariable offset, len;
//     CcuVariableCreate(&offset);
//     CcuVariableCreate(&len);
//     offset = 64;
//     len = 1024;

//     CcuAddress baseAddr, dstAddr;
//     CcuAddressCreate(&baseAddr);
//     CcuAddressCreate(&dstAddr);

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
// CcuResult CcuLocalAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable token;
//     ccu::Create(&token);
//     token = 1024;
//     CcuLocalAddr localAddr;
//     ccu::Create(&localAddr);
//     localAddr.addr = 0x80000000;
//     localAddr.token = token;
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalAddrDemoKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable token;
//     CcuVariableCreate(&token);
//     token = 1024;

//     CcuLocalAddr localAddr{};
//     CcuLocalAddrCreate(&localAddr);
//     localAddr.addr = 0x80000000;      // 通过 CcuAddress 重载设置地址
//     localAddr.token = token;    // 通过 CcuVariable 重载设置 token
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalCopyKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     ccu::Create(&len);
//     len = 1024;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     CcuLocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     CcuLocalAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Copy(dst, src, len, evt);
//     ccu::Wait(evt);
//     CcuBuffer buf;
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
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 1024;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);

//     CcuLocalAddr src, dst;
//     CcuLocalAddrCreate(&src);
//     CcuLocalAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuLocalCopyHBMToHBM(dst, src, len, evt);           // LocalAddr → LocalAddr
//     CcuWaitEvent(evt);
    
//     CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuLocalCopyHBMToBuffer(buf, src, len, evt);   // LocalAddr → Buffer
//     CcuWaitEvent(evt);
//     CcuLocalCopyBufferToHBM(dst, buf, len, evt); // Buffer → LocalAddr
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalAddrReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     ccu::Create(&len);
//     len = 1024;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     CcuLocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     CcuLocalAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Reduce(dst, src, len,
//         HCCL_DATA_TYPE_FP16, HCCL_REDUCE_SUM, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuLocalAddrReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 1024;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);
//     CcuLocalAddr src, dst;
//     CcuLocalAddrCreate(&src);
//     CcuLocalAddrCreate(&dst);
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
//     CcuVariable len;
//     ccu::Create(&len);
//     len = 4096;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     CcuLocalAddr src0, src1;
//     ccu::Create(&src0);
//     ccu::Create(&src1);
//     src0.addr = 0x10000000;
//     src0.token = 0x20000000;
//     src1.addr = 0x30000000;
//     src1.token = 0x40000000;
//     CcuBuffer bufs[2];
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
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 512;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);
//     CcuLocalAddr src0, src1;
//     CcuLocalAddrCreate(&src0);
//     CcuLocalAddrCreate(&src1);
//     src0.addr = 0x10000000;
//     src0.token = 0x20000000;
//     src1.addr = 0x30000000;
//     src1.token = 0x40000000;
//     constexpr uint32_t bufCount = 2;
//     CcuBuffer bufs[bufCount];
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
//     CcuVariable len;
//     ccu::Create(&len);
//     len = 1024;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     CcuLocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     CcuRemoteAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Read(args->channelHandle, src, dst, len, evt);
//     ccu::Wait(evt);
//     CcuBuffer buf[2];
//     ccu::BlockCreate(buf, 2);
//     ccu::Read(args->channelHandle, buf[1], dst, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;

// }
// CcuResult CcuRemoteReadKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 1024;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);

//     CcuLocalAddr src;
//     CcuRemoteAddr dst;
//     CcuLocalAddrCreate(&src);
//     CcuRemoteAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuReadHBMToHBM(args->channelHandle, src, dst, len, evt);
//     CcuWaitEvent(evt);
//     CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuReadHBMToBuffer(args->channelHandle, buf, dst, len, evt);
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 1024;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);

//     CcuLocalAddr src;
//     CcuRemoteAddr dst;
//     CcuLocalAddrCreate(&src);
//     CcuRemoteAddrCreate(&dst);
//     src.addr = 0x10000000;
//     src.token = 0x20000000; 
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     CcuWriteHBMToHBM(args->channelHandle, dst, src, len, evt);
//     CcuWaitEvent(evt);
//     CcuBuffer buf;
//     CcuBlockBufferCreate(&buf,1);
//     CcuWriteBufferToHBM(args->channelHandle, dst, buf, len, evt);
//     CcuWaitEvent(evt);

//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuRemoteWriteKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     ccu::Create(&len);
//     len = 1024;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     CcuLocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     CcuRemoteAddr dst;
//     ccu::Create(&dst);
//     dst.addr = 0x30000000;
//     dst.token = 0x40000000;
//     ccu::Write(args->channelHandle, dst, src, len, evt);
//     ccu::Wait(evt);
//     CcuBuffer buf;
//     ccu::BlockCreate(&buf, 1);
//     ccu::Write(args->channelHandle, dst, buf, len, evt);
//     ccu::Wait(evt);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuReadWriteReduceKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuVariable len;
//     CcuVariableCreate(&len);
//     len = 1024;
//     CcuEvent evt;
//     CcuCompletedEventCreate(&evt);
//     CcuLocalAddr src;
//     CcuRemoteAddr dst;
//     CcuLocalAddrCreate(&src);
//     CcuRemoteAddrCreate(&dst);
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

//     CcuVariable len;
//     ccu::Create(&len);
//     len = 4096;
//     CcuEvent evt;
//     ccu::Create(&evt);
//     evt.setMask(0x12);
//     CcuLocalAddr src;
//     ccu::Create(&src);
//     src.addr = 0x10000000;
//     src.token = 0x20000000;
//     CcuRemoteAddr dst;
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

    
//     CcuEvent evt;
//     ccu::Create(&evt);
//     uint32_t mask = 0x12;
//     evt.setMask(mask);
    
//     CcuVariable var;
//     ccu::Create(&var);
//     var = 1024;
//     ccu::WriteVariableWithNotify(args->channelHandle, var, 0, 0, mask);
//     ccu::NotifyWait(args->channelHandle, 0, mask);
//     return CcuResult::CCU_SUCCESS;
// }
// CcuResult CcuEventKernel(CcuKernelArg arg)
// {
//     auto *args = static_cast<CcuVarAddKernelArg *>(arg);
//     CcuEvent evt[2];
//     ccu::BlockCreate(evt, 2);
//     ccu::Record(evt[0]);
//     ccu::Wait(evt[0]);
//     ccu::Record(evt[1]);
//     ccu::Wait(evt[1]);
//     CcuEvent single_evt;
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
//     CcuVariable tmp;
//     ccu::Create(&tmp);
//     ccu::WriteVariableWithNotify(args->channelHandle,tmp, 0, 0, 0);
//     CcuVariable var;
//     ccu::CreateFromChannel(args->channelHandle, 0, &var);
//     var = 1024;
//     return CcuResult::CCU_SUCCESS;
// }
