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
#include <cstring>

#define private public
#define protected public
#include "roce_mem.h"
#include "urma_mem.h"
#include "ub_mem.h"
#undef protected
#undef private

#include "hcomm_c_adpt.h"
#include "local_rdma_rma_buffer.h"
#include "local_ub_rma_buffer.h"
#include "local_ipc_rma_buffer.h"
#include "hccp.h"

using namespace hcomm;

// ============================================================================
// RoceRegedMemMgr — RegisterMemory / UnregisterMemory / MemoryExport  HcommMemInfo
// ============================================================================

class RoceRegedMemMgrHcommMemInfoTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_ValidInput_Expect_HcommMemInfoReturnedWithCorrectFields)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);  // dummy non-null

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x1000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "test_tag", &handle), HCCL_SUCCESS);
    ASSERT_NE(handle, nullptr);

    auto *memInfo = static_cast<HcommMemInfo *>(handle);
    EXPECT_EQ(memInfo->addr, mem.addr);
    EXPECT_EQ(memInfo->size, mem.size);
    EXPECT_STREQ(memInfo->memTag, "test_tag");
    EXPECT_NE(memInfo->localRmaBuffer, nullptr);

    // cleanup
    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_SameMemoryTwice_Expect_TwoHandlesShareLocalRmaBuffer)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x2000);
    mem.size = 8192;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *h1 = nullptr;
    void *h2 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag1", &h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag2", &h2), HCCL_SUCCESS);

    auto *info1 = static_cast<HcommMemInfo *>(h1);
    auto *info2 = static_cast<HcommMemInfo *>(h2);

    EXPECT_NE(h1, h2);  // different HcommMemInfo objects
    EXPECT_EQ(info1->addr, info2->addr);
    EXPECT_EQ(info1->size, info2->size);
    EXPECT_STREQ(info1->memTag, "tag1");
    EXPECT_STREQ(info2->memTag, "tag2");
    EXPECT_EQ(info1->localRmaBuffer, info2->localRmaBuffer);  // same buffer

    // allRegisteredBuffers_  size should be 1 (not duplicated)
    EXPECT_EQ(mgr.allRegisteredBuffers_.size(), 1U);

    ASSERT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
    // second handle still valid, refcount > 0 during first Unregister
    ASSERT_EQ(mgr.UnregisterMemory(h2), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_NullMemTag_Expect_EmptyMemTagInHcommMemInfo)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x3000);
    mem.size = 1024;
    mem.type = COMM_MEM_TYPE_HOST;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, nullptr, &handle), HCCL_SUCCESS);

    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_EQ(info->memTag[0], '\0');
    EXPECT_NE(info->localRmaBuffer, nullptr);

    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_MemTagExceedsMaxLen_Expect_Truncated)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x4000);
    mem.size = 2048;
    mem.type = COMM_MEM_TYPE_DEVICE;

    std::string longTag(HCOMM_RES_TAG_MAX_LEN + 10, 'A');

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, longTag.c_str(), &handle), HCCL_SUCCESS);

    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_EQ(strlen(info->memTag), HCOMM_RES_TAG_MAX_LEN - 1);
    EXPECT_EQ(info->memTag[HCOMM_RES_TAG_MAX_LEN - 1], '\0');

    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_ZeroSize_Expect_Error)
{
    RoceRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x5000);
    mem.size = 0;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &handle), HCCL_E_PARA);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_NullMemHandle_Expect_Error)
{
    RoceRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x6000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", nullptr), HCCL_E_PTR);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_NullAddr_Expect_Error)
{
    RoceRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = nullptr;
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &handle), HCCL_E_PTR);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_InvalidMemType_Expect_Error)
{
    RoceRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x7000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_INVALID;

    void *handle = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &handle), HCCL_E_PARA);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_Overlapping_Expect_Error)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem1{};
    mem1.addr = reinterpret_cast<void *>(0x8000);
    mem1.size = 4096;
    mem1.type = COMM_MEM_TYPE_DEVICE;

    void *h1 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem1, "tag1", &h1), HCCL_SUCCESS);

    // overlapping: starts inside first region
    HcommMem mem2{};
    mem2.addr = reinterpret_cast<void *>(0x8100);
    mem2.size = 4096;
    mem2.type = COMM_MEM_TYPE_DEVICE;

    void *h2 = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem2, "tag2", &h2), HCCL_E_INTERNAL);

    ASSERT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_UnregisterMemory_When_ValidHandle_Expect_HcommMemInfoFreed)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x9000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag", &handle), HCCL_SUCCESS);

    auto *bufPtr = static_cast<HcommMemInfo *>(handle)->localRmaBuffer;
    ASSERT_NE(bufPtr, nullptr);
    ASSERT_EQ(mgr.allRegisteredBuffers_.size(), 1U);

    EXPECT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
    EXPECT_EQ(mgr.allRegisteredBuffers_.size(), 0U);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_UnregisterMemory_When_RefCountGreaterThanOne_Expect_Reduced)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0xA000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *h1 = nullptr;
    void *h2 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag1", &h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag2", &h2), HCCL_SUCCESS);
    ASSERT_EQ(mgr.allRegisteredBuffers_.size(), 1U);

    // unregister first handle, refcount goes from 2 to 1
    EXPECT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
    EXPECT_EQ(mgr.allRegisteredBuffers_.size(), 1U);

    // unregister second handle, refcount goes from 1 to 0, buffer removed
    EXPECT_EQ(mgr.UnregisterMemory(h2), HCCL_SUCCESS);
    EXPECT_EQ(mgr.allRegisteredBuffers_.size(), 0U);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_MemoryExport_When_ValidHcommMemInfoHandle_Expect_Success)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0xB000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "export_tag", &handle), HCCL_SUCCESS);

    EndpointDesc ep{};
    void *desc = nullptr;
    uint32_t descLen = 0;
    EXPECT_EQ(mgr.MemoryExport(ep, handle, &desc, &descLen), HCCL_SUCCESS);
    EXPECT_NE(desc, nullptr);
    EXPECT_GT(descLen, 0U);

    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_MemoryExport_When_NullHandle_Expect_Error)
{
    RoceRegedMemMgr mgr;
    EndpointDesc ep{};
    void *desc = nullptr;
    uint32_t len = 0;
    EXPECT_EQ(mgr.MemoryExport(ep, nullptr, &desc, &len), HCCL_E_PTR);
}

TEST_F(RoceRegedMemMgrHcommMemInfoTest, Ut_MemoryExport_When_NullDesc_Expect_Error)
{
    RoceRegedMemMgr mgr;
    EndpointDesc ep{};
    void *fake = reinterpret_cast<void *>(0x1);
    EXPECT_EQ(mgr.MemoryExport(ep, fake, nullptr, nullptr), HCCL_E_PTR);
}

// ============================================================================
// UbRegedMemMgr — RegisterMemory / UnregisterMemory  HcommMemInfo
// ============================================================================

class UbRegedMemMgrHcommMemInfoTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(UbRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_ValidInput_Expect_HcommMemInfoReturned)
{
    UbRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x1000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "ub_tag", &handle), HCCL_SUCCESS);
    ASSERT_NE(handle, nullptr);

    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_EQ(info->addr, mem.addr);
    EXPECT_EQ(info->size, mem.size);
    EXPECT_STREQ(info->memTag, "ub_tag");
    EXPECT_NE(info->localRmaBuffer, nullptr);

    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(UbRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_Twice_Expect_SameBufferInTwoHcommMemInfo)
{
    UbRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x2000);
    mem.size = 8192;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *h1 = nullptr;
    void *h2 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "first", &h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.RegisterMemory(mem, "second", &h2), HCCL_SUCCESS);

    auto *info1 = static_cast<HcommMemInfo *>(h1);
    auto *info2 = static_cast<HcommMemInfo *>(h2);

    EXPECT_NE(h1, h2);
    EXPECT_STREQ(info1->memTag, "first");
    EXPECT_STREQ(info2->memTag, "second");
    EXPECT_EQ(info1->localRmaBuffer, info2->localRmaBuffer);

    ASSERT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.UnregisterMemory(h2), HCCL_SUCCESS);
}

// ============================================================================
// UbMemRegedMemMgr — RegisterMemory / UnregisterMemory  HcommMemInfo
// ============================================================================

class UbMemRegedMemMgrHcommMemInfoTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(UbMemRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_ValidInput_Expect_HcommMemInfoReturned)
{
    UbMemRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x1000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "ubmem_tag", &handle), HCCL_SUCCESS);
    ASSERT_NE(handle, nullptr);

    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_EQ(info->addr, mem.addr);
    EXPECT_EQ(info->size, mem.size);
    EXPECT_STREQ(info->memTag, "ubmem_tag");
    EXPECT_NE(info->localRmaBuffer, nullptr);

    ASSERT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}

TEST_F(UbMemRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_Twice_Expect_SameBuffer)
{
    UbMemRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x2000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_HOST;

    void *h1 = nullptr;
    void *h2 = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag_a", &h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.RegisterMemory(mem, "tag_b", &h2), HCCL_SUCCESS);

    auto *info1 = static_cast<HcommMemInfo *>(h1);
    auto *info2 = static_cast<HcommMemInfo *>(h2);

    EXPECT_NE(h1, h2);
    EXPECT_EQ(info1->localRmaBuffer, info2->localRmaBuffer);

    ASSERT_EQ(mgr.UnregisterMemory(h1), HCCL_SUCCESS);
    ASSERT_EQ(mgr.UnregisterMemory(h2), HCCL_SUCCESS);
}

TEST_F(UbMemRegedMemMgrHcommMemInfoTest, Ut_RegisterMemory_When_ZeroSize_Expect_Error)
{
    UbMemRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0x3000);
    mem.size = 0;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    EXPECT_EQ(mgr.RegisterMemory(mem, "tag", &handle), HCCL_E_PARA);
}

// ============================================================================
//  —  RegisterMemory + UnregisterMemory + MemoryExport
// ============================================================================

class FullLifecycleTest : public testing::Test {
protected:
    void TearDown() override { GlobalMockObject::verify(); }
};

TEST_F(FullLifecycleTest, Ut_Roce_RegisterExportUnregister_Expect_NoLeak)
{
    MOCKER_CPP(RaRegisterMr).stubs().will(returnValue(0));
    RoceRegedMemMgr mgr;
    mgr.rdmaHandle_ = reinterpret_cast<void *>(0x1);

    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0xC000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "lifecycle", &handle), HCCL_SUCCESS);

    // verify HcommMemInfo
    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_STREQ(info->memTag, "lifecycle");

    // export
    EndpointDesc ep{};
    void *desc = nullptr;
    uint32_t descLen = 0;
    ASSERT_EQ(mgr.MemoryExport(ep, handle, &desc, &descLen), HCCL_SUCCESS);
    EXPECT_GT(descLen, 0U);

    // unregister
    EXPECT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
    EXPECT_EQ(mgr.allRegisteredBuffers_.size(), 0U);
}

TEST_F(FullLifecycleTest, Ut_UbRegedMemMgr_RegisterExportUnregister_Expect_Success)
{
    UbRegedMemMgr mgr;
    HcommMem mem{};
    mem.addr = reinterpret_cast<void *>(0xD000);
    mem.size = 4096;
    mem.type = COMM_MEM_TYPE_DEVICE;

    void *handle = nullptr;
    ASSERT_EQ(mgr.RegisterMemory(mem, "lifecycle_ub", &handle), HCCL_SUCCESS);

    auto *info = static_cast<HcommMemInfo *>(handle);
    EXPECT_STREQ(info->memTag, "lifecycle_ub");

    EndpointDesc ep{};
    void *desc = nullptr;
    uint32_t descLen = 0;
    ASSERT_EQ(mgr.MemoryExport(ep, handle, &desc, &descLen), HCCL_SUCCESS);

    EXPECT_EQ(mgr.UnregisterMemory(handle), HCCL_SUCCESS);
}
