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

#include "aicpu_ts_roce_mem.h"

using namespace hcomm;

class AicpuTsRoceRegedMemMgrTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_RegisterMemory_When_NetDevNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x1000U);
    mem.size = 4096U;
    mem.type = COMM_MEM_TYPE_DEVICE;
    void *handle = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "t", &handle), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_MemoryExport_When_MemHandleNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    EndpointDesc ep{};
    void *desc = nullptr;
    uint32_t len = 0;
    EXPECT_EQ(mgr.MemoryExport(ep, nullptr, &desc, &len), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_MemoryExport_When_MemDescOutNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    EndpointDesc ep{};
    void *fake = reinterpret_cast<void *>(0x1);
    uint32_t len = 0;
    EXPECT_EQ(mgr.MemoryExport(ep, fake, nullptr, &len), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_MemoryImport_When_DescTooShort_Returns_E_PARA)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    char buf[4] = {};
    HcommMem out{};
    EXPECT_EQ(mgr.MemoryImport(buf, sizeof(buf), &out), HCCL_E_PARA);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_MemoryImport_When_OutMemNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    char buf[sizeof(EndpointDesc)] = {};
    EXPECT_EQ(mgr.MemoryImport(buf, sizeof(buf), nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_MemoryUnimport_When_MemDescNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    EXPECT_EQ(mgr.MemoryUnimport(nullptr, sizeof(EndpointDesc)), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_GetAllMemHandles_When_CountOutNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    void *handles = nullptr;
    EXPECT_EQ(mgr.GetAllMemHandles(&handles, nullptr), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_GetAllMemDetails_When_NetDevNull_Returns_E_PTR)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    std::vector<RoceMemDetails> local;
    std::vector<RoceMemDetails> remote;
    EXPECT_EQ(mgr.GetAllMemDetails(local, remote), HCCL_E_PTR);
}

TEST_F(AicpuTsRoceRegedMemMgrTest, Ut_GetAllMemHandles_When_NoRecords_Returns_SUCCESS)
{
    AicpuTsRoceRegedMemMgr mgr(nullptr);
    void *handles = reinterpret_cast<void *>(0xdeadbeefULL);
    uint32_t n = 99U;
    ASSERT_EQ(mgr.GetAllMemHandles(&handles, &n), HCCL_SUCCESS);
    EXPECT_EQ(n, 0U);
    EXPECT_EQ(handles, nullptr);
}
