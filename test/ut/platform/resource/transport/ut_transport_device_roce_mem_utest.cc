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

#ifndef private
#define private public
#define protected public
#endif

#include "transport_device_roce_mem.h"

#undef private
#undef protected

using namespace std;
using namespace hccl;

class TransportDeviceRoceMem_UT : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--TransportDeviceRoceMem_UT SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--TransportDeviceRoceMem_UT TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TransportDeviceRoceMem_UT, ExchangeMemDesc_NotSupported)
{
    TransportMem::RmaMemDescs localMemDescs;
    TransportMem::RmaMemDescs remoteMemDescs;
    u32 actualNumOfRemote = 0;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.ExchangeMemDesc(localMemDescs, remoteMemDescs, actualNumOfRemote), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, EnableMemAccess_NotSupported)
{
    TransportMem::RmaMemDesc remoteMemDesc;
    TransportMem::RmaMem remoteMem;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.EnableMemAccess(remoteMemDesc, remoteMem), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, DisableMemAccess_NotSupported)
{
    TransportMem::RmaMemDesc remoteMemDesc;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.DisableMemAccess(remoteMemDesc), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, SetSocket_NotSupported)
{
    std::shared_ptr<HcclSocket> socket;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.SetSocket(socket), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, Connect_NotSupported)
{
    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.Connect(0), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, Write_HcclBuf_NotSupported)
{
    HcclBuf remoteMem;
    HcclBuf localMem;
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.Write(remoteMem, localMem, stream), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, Read_HcclBuf_NotSupported)
{
    HcclBuf localMem;
    HcclBuf remoteMem;
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.Read(localMem, remoteMem, stream), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, Write_RmaOpMem_NotSupported)
{
    TransportMem::RmaOpMem remoteMem;
    TransportMem::RmaOpMem localMem;
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.Write(remoteMem, localMem, stream), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, Read_RmaOpMem_NotSupported)
{
    TransportMem::RmaOpMem localMem;
    TransportMem::RmaOpMem remoteMem;
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.Read(localMem, remoteMem, stream), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, AddOpFence_Stream_NotSupported)
{
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.AddOpFence(stream), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, GetTransInfo_NotSupported)
{
    HcclQpInfoV2 qpInfo;
    u32 lkey = 0;
    u32 rkey = 0;
    HcclBuf localMem;
    HcclBuf remoteMem;
    u32 num = 0;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfoForCtor;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfoForCtor);

    EXPECT_EQ(transportMem.GetTransInfo(qpInfo, &lkey, &rkey, &localMem, &remoteMem, num), HCCL_E_NOT_SUPPORT);
}

TEST_F(TransportDeviceRoceMem_UT, WaitOpFence_NotSupported)
{
    rtStream_t stream = nullptr;

    std::unique_ptr<NotifyPool> notifyPool;
    HcclNetDevCtx netDevCtx;
    HcclDispatcher dispatcher;
    TransportMem::AttrInfo attrInfo;
    bool aicpuUnfoldMode = false;
    HcclQpInfoV2 qpInfo;

    TransportDeviceRoceMem transportMem(notifyPool, netDevCtx, dispatcher, attrInfo, aicpuUnfoldMode, qpInfo);

    EXPECT_EQ(transportMem.WaitOpFence(stream), HCCL_E_NOT_SUPPORT);
}