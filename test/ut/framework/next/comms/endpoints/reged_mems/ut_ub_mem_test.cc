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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "hcomm_c_adpt.h"
#include "hcomm_res_defs.h"

#define private public
#define protected public

#include "ub_mem.h"

#undef protected
#undef private

using namespace hcomm;

class UbMemRegedMemMgrTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "UbMemRegedMemMgrTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "UbMemRegedMemMgrTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in UbMemRegedMemMgrTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in UbMemRegedMemMgrTest TearDown" << std::endl;
    }
};

// 父+子集注册 → 先解注册子(alias) → 再解注册父
TEST_F(UbMemRegedMemMgrTest, ut_UbMemRegedMemMgr_When_ParentChild_Expect_UnregisterChildFirst)
{
    UbMemRegedMemMgr ubMemRegedMemMgr{};
    HcommMem mem0;
    mem0.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem0.addr = (void*)0x2000;
    mem0.size = 8192;
    std::string memTag0 = "parent";
    void *memHandle0 = nullptr;
    HcclResult ret = ubMemRegedMemMgr.RegisterMemory(mem0, memTag0.c_str(), &memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 子集注册（应创建alias buffer，共享父IPC资源）
    HcommMem mem1;
    mem1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem1.addr = (void*)0x2000;
    mem1.size = 512;
    std::string memTag1 = "child";
    void *memHandle1 = nullptr;
    ret = ubMemRegedMemMgr.RegisterMemory(mem1, memTag1.c_str(), &memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 先解注册子buffer（IsAlias → GetIpcPtr匹配父 → Del父key ref 2→1）
    ret = ubMemRegedMemMgr.UnregisterMemory(memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 再解注册父buffer（自己的key在tree中 → Del ref 1→0）
    ret = ubMemRegedMemMgr.UnregisterMemory(memHandle0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 多次注册同一范围，各自解注册
TEST_F(UbMemRegedMemMgrTest, ut_UbMemRegedMemMgr_When_MultipleSameRange_Expect_AllSuccess)
{
    UbMemRegedMemMgr ubMemRegedMemMgr{};
    HcommMem mem;
    mem.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem.addr = (void*)0x4000;
    mem.size = 4096;

    void *h1 = nullptr, *h2 = nullptr, *h3 = nullptr;
    EXPECT_EQ(ubMemRegedMemMgr.RegisterMemory(mem, "t1", &h1), HCCL_SUCCESS);
    EXPECT_EQ(ubMemRegedMemMgr.RegisterMemory(mem, "t2", &h2), HCCL_SUCCESS);
    EXPECT_EQ(ubMemRegedMemMgr.RegisterMemory(mem, "t3", &h3), HCCL_SUCCESS);

    EXPECT_EQ(ubMemRegedMemMgr.UnregisterMemory(h3), HCCL_SUCCESS);
    EXPECT_EQ(ubMemRegedMemMgr.UnregisterMemory(h2), HCCL_SUCCESS);
    EXPECT_EQ(ubMemRegedMemMgr.UnregisterMemory(h1), HCCL_SUCCESS);
}

// 注册重叠但非包含关系 → 应创建两个独立父buffer
TEST_F(UbMemRegedMemMgrTest, ut_UbMemRegedMemMgr_When_NonOverlap_Expect_TwoParents)
{
    UbMemRegedMemMgr ubMemRegedMemMgr{};
    HcommMem mem0;
    mem0.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem0.addr = (void*)0x1000;
    mem0.size = 100;
    void *h0 = nullptr;
    EXPECT_EQ(ubMemRegedMemMgr.RegisterMemory(mem0, "t0", &h0), HCCL_SUCCESS);

    // 不重叠的新范围
    HcommMem mem1;
    mem1.type = CommMemType::COMM_MEM_TYPE_DEVICE;
    mem1.addr = (void*)0x2000;
    mem1.size = 100;
    void *h1 = nullptr;
    EXPECT_EQ(ubMemRegedMemMgr.RegisterMemory(mem1, "t1", &h1), HCCL_SUCCESS);

    EXPECT_NE(h0, h1); // 两个不同的handle

    EXPECT_EQ(ubMemRegedMemMgr.UnregisterMemory(h0), HCCL_SUCCESS);
    EXPECT_EQ(ubMemRegedMemMgr.UnregisterMemory(h1), HCCL_SUCCESS);
}
