/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <string>

#include "hccl_network.h"
#include "local_ipc_rma_buffer.h"

#define private public
#include "hccs_reged_mem_mgr.h"
#undef private

using namespace hcomm;

class HccsRegedMemMgrTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

// 父+子集注册 → 先解注册子(alias) → 再解注册父
TEST_F(HccsRegedMemMgrTest, ut_HccsRegedMemMgr_When_ParentChild_Expect_UnregisterChildFirst)
{
    MOCKER_CPP(&hccl::LocalIpcRmaBuffer::Init).stubs().will(returnValue(HCCL_SUCCESS));

    hccl::HcclIpAddress localIp;
    ASSERT_EQ(localIp.SetReadableAddress("127.0.0.1"), HCCL_SUCCESS);
    hccl::NetDevContext netCtx;
    ASSERT_EQ(netCtx.Init(NicType::DEVICE_NIC_TYPE, 0, 0, localIp), HCCL_SUCCESS);

    HccsRegedMemMgr mgr(reinterpret_cast<HcclNetDevCtx>(&netCtx));

    HcommMem mem0;
    mem0.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem0.addr = (void*)0x3000;
    mem0.size = 8192;
    void *h0 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem0, "parent", &h0), HCCL_SUCCESS);
    ASSERT_NE(h0, nullptr);

    // 子集注册（应创建alias buffer，共享父IPC资源）
    HcommMem mem1;
    mem1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem1.addr = (void*)0x3000;
    mem1.size = 1024;
    void *h1 = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem1, "child", &h1), HCCL_SUCCESS);
    EXPECT_NE(h1, h0); // alias有独立的handle

    // 先解注册子buffer（IsAlias → Find(ownKey)找父 → Del父key ref 2→1）
    EXPECT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);

    // 再解注册父buffer（自己的key在tree中 → Del ref 1→0）
    EXPECT_EQ(mgr.UnregisterMemory(h0), HCCL_SUCCESS);
}

// 多次注册同一范围，各自解注册
TEST_F(HccsRegedMemMgrTest, ut_HccsRegedMemMgr_When_MultipleSameRange_Expect_AllSuccess)
{
    MOCKER_CPP(&hccl::LocalIpcRmaBuffer::Init).stubs().will(returnValue(HCCL_SUCCESS));

    hccl::HcclIpAddress localIp;
    ASSERT_EQ(localIp.SetReadableAddress("127.0.0.1"), HCCL_SUCCESS);
    hccl::NetDevContext netCtx;
    ASSERT_EQ(netCtx.Init(NicType::DEVICE_NIC_TYPE, 0, 0, localIp), HCCL_SUCCESS);

    HccsRegedMemMgr mgr(reinterpret_cast<HcclNetDevCtx>(&netCtx));

    HcommMem mem;
    mem.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem.addr = (void*)0x8000;
    mem.size = 4096;

    void *h1 = nullptr, *h2 = nullptr, *h3 = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "t1", &h1), HCCL_SUCCESS);
    EXPECT_EQ(mgr.RegisterMemory(mem, "t2", &h2), HCCL_SUCCESS);
    EXPECT_EQ(mgr.RegisterMemory(mem, "t3", &h3), HCCL_SUCCESS);

    EXPECT_EQ(mgr.UnregisterMemory(h3), HCCL_SUCCESS);
    EXPECT_EQ(mgr.UnregisterMemory(h2), HCCL_SUCCESS);
    EXPECT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
}
