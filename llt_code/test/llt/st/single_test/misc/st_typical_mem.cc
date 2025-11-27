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
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>

#define private public
#define protected public
#include "adapter_rts_common.h"
#include "adapter_hccp_common.h"
#include "network_manager_pub.h"
#include "adapter_rts.h"
#include "typical_window_mem.h"
#include "typical_sync_mem.h"
#include "typical_mr_manager.h"
#include "rdma_resource_manager.h"
#undef private
#undef protected

#include "dlra_function.h"

using namespace hccl;

HcclResult stub_HrtRaGetNotifyBaseAddr(RdmaHandle handle, u64 *va, u64 *size)
{
    *va = 0x20000000;
    *size = 4;
    return HCCL_SUCCESS;
}

class TypicalWindowMemTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalWindowMemTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalWindowMemTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

class TypicalSyncMemTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalSyncMemTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalSyncMemTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        DlRaFunction::GetInstance().DlRaFunctionInit();

        hrtSetDevice(0);
        MOCKER(hrtGetNotifySize)
        .stubs()
        .with(any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(hrtMemSyncCopy)
        .stubs()
        .with(any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(HrtRaMrReg)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));

        MOCKER(HrtRaMrDereg)
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_SUCCESS));

        TypicalSyncMem::GetInstance().rdmaHandle_ = (void *)0x1000000;
        TypicalSyncMem::GetInstance().notifySrcMrInfo_.addr = (void *)0x2000000;

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        hrtResetDevice(0);
        GlobalMockObject::verify();
        std::cout << "A Test TearDown" << std::endl;
    }
};

class TypicalMrManagerTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "TypicalMrManagerTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "TypicalMrManagerTest TearDown" << std::endl;
    }
    // Some expensive resource shared by all tests.
    virtual void SetUp()
    {
        MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(invoke(stub_HrtRaGetNotifyBaseAddr));
 
        DlRaFunction::GetInstance().DlRaFunctionInit();
        hrtSetDevice(0);
        RdmaResourceManager::GetInstance().Init();

        TypicalMrManager::GetInstance().rdmaHandle_ = (void *)0x1000000;

        ptr = nullptr;
        size_t size = 32;
        TypicalWindowMem::GetInstance().AllocWindowMem(&ptr, size);
        fakeMrInfo.addr = ptr;
        fakeMrInfo.size = size;
        fakeMrInfo.lkey = DEFAULT_MR_KEY;

        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        hrtResetDevice(0);
        GlobalMockObject::verify();
        TypicalWindowMem::GetInstance().FreeWindowMem(ptr);
        // TypicalWindowMem::GetInstance().FreeWindowMem(ptr2);
        std::cout << "A Test TearDown" << std::endl;
    }

public:
    void *ptr;
    struct mr_info fakeMrInfo;
};

TEST_F(TypicalWindowMemTest, st_window_mem_alloc_free_test)
{
    s32 ret = HCCL_SUCCESS;

    void *mem_ptr = nullptr;
    size_t mem_size = 16;
    ret = TypicalWindowMem::GetInstance().AllocWindowMem(&mem_ptr, mem_size);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(mem_ptr != nullptr);
    EXPECT_EQ(TypicalWindowMem::GetInstance().windowMemMap_.size(), 1);

    void *mem_ptr_zero = nullptr;
    size_t mem_size_zero = 0;
    ret = TypicalWindowMem::GetInstance().AllocWindowMem(&mem_ptr_zero, mem_size_zero);
    EXPECT_EQ(ret, HCCL_E_PARA);
    EXPECT_TRUE(mem_ptr_zero == nullptr);
    EXPECT_EQ(TypicalWindowMem::GetInstance().windowMemMap_.size(), 1);

    ret = TypicalWindowMem::GetInstance().FreeWindowMem(mem_ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalWindowMem::GetInstance().windowMemMap_.size(), 0);
}
 
TEST_F(TypicalWindowMemTest, st_free_all_window_mem_test)
{
    s32 ret = HCCL_SUCCESS;

    void *mem_ptr1 = nullptr;
    size_t mem_size1 = 32;
    ret = TypicalWindowMem::GetInstance().AllocWindowMem(&mem_ptr1, mem_size1);

    ASSERT_EQ(ret, HCCL_SUCCESS);
    void *mem_ptr2 = nullptr;
    size_t mem_size2 = 16;
    ret = TypicalWindowMem::GetInstance().AllocWindowMem(&mem_ptr2, mem_size2);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    
    EXPECT_EQ(TypicalWindowMem::GetInstance().windowMemMap_.size(), 2);

    ret = TypicalWindowMem::GetInstance().FreeAllWinowMem();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalWindowMem::GetInstance().windowMemMap_.size(), 0);
}
 

TEST_F(TypicalSyncMemTest, st_sync_mem_alloc_free_test)
{
    MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(invoke(stub_HrtRaGetNotifyBaseAddr));
    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    s32 device_id = 0;
    HcclResult ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = RdmaResourceManager::GetInstance().Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);


    int32_t *sync_ptr = nullptr;
    ret = TypicalSyncMem::GetInstance().AllocSyncMem(&sync_ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(sync_ptr != nullptr);
    EXPECT_EQ(TypicalSyncMem::GetInstance().syncMemMap_.size(), 1);

    ret = TypicalSyncMem::GetInstance().FreeSyncMem(sync_ptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalSyncMem::GetInstance().syncMemMap_.size(), 0);

    ret = hrtResetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalSyncMemTest, st_free_all_sync_mem_test)
{
    MOCKER(HrtRaGetNotifyBaseAddr).stubs().will(invoke(stub_HrtRaGetNotifyBaseAddr));
    MOCKER_CPP(&TypicalSyncMem::InitNotifySrcMem).stubs().with(any()).will(returnValue(HCCL_SUCCESS));

    s32 device_id = 0;
    HcclResult ret = hrtSetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
 
    ret = RdmaResourceManager::GetInstance().Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    int32_t *sync_ptr = nullptr;
    ret = TypicalSyncMem::GetInstance().AllocSyncMem(&sync_ptr);
    ASSERT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(sync_ptr != nullptr);
    EXPECT_EQ(TypicalSyncMem::GetInstance().syncMemMap_.size(), 1);

    ret = TypicalSyncMem::GetInstance().FreeAllSyncMem();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalSyncMem::GetInstance().syncMemMap_.size(), 0);

    ret = hrtResetDevice(device_id);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
 
TEST_F(TypicalSyncMemTest, st_get_noitfy_handle_test)
{
    s32 ret = HCCL_SUCCESS;

    u64 fakeNotifyVa = 0x12345678;
    HcclRtSignal fakeNotify = (void *)0x30000000;
    TypicalSyncMem::GetInstance().syncMemMap_[fakeNotifyVa] = fakeNotify;

    HcclRtSignal notifyOut = nullptr;
    ret = TypicalSyncMem::GetInstance().GetNotifyHandle(fakeNotifyVa, notifyOut);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_TRUE(notifyOut == fakeNotify);
    TypicalSyncMem::GetInstance().syncMemMap_.clear();
}
 
TEST_F(TypicalSyncMemTest, st_get_noitfy_src_mem)
{
    s32 ret = HCCL_SUCCESS;

    mr_info srcMrInfoOut = {};
    ret = TypicalSyncMem::GetInstance().GetNotifySrcMem(srcMrInfoOut);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(srcMrInfoOut.addr, TypicalSyncMem::GetInstance().notifySrcMrInfo_.addr);
    EXPECT_EQ(srcMrInfoOut.size, TypicalSyncMem::GetInstance().notifySrcMrInfo_.size);
    EXPECT_EQ(srcMrInfoOut.access, TypicalSyncMem::GetInstance().notifySrcMrInfo_.access);
    EXPECT_EQ(srcMrInfoOut.lkey, TypicalSyncMem::GetInstance().notifySrcMrInfo_.lkey);
}
 
HcclResult stub_hrtRaRegGlobalMr(const RdmaHandle rdmaHandle, struct mr_info &mrInfo, MrHandle &mrHandle)
{
    MrHandle fakeMrHandle = (void *)0x1000000;
    uint32_t fakeKey = 2;

    mrHandle = fakeMrHandle;
    mrInfo.lkey = fakeKey;
    fakeKey++;
    return HCCL_SUCCESS;
}
 
TEST_F(TypicalMrManagerTest, st_mr_manager_register_deregister_test)
{
    MOCKER(hrtRaRegGlobalMr)
    .stubs()
    .will(invoke(stub_hrtRaRegGlobalMr));

    MOCKER(hrtRaDeRegGlobalMr)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));

    s32 ret = HCCL_SUCCESS;

    ret = TypicalMrManager::GetInstance().RegisterMem(fakeMrInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalMrManager::GetInstance().regedMrMap_.size(), 1);

    ret = TypicalMrManager::GetInstance().DeRegisterMem(fakeMrInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalMrManager::GetInstance().regedMrMap_.size(), 0);
    fakeMrInfo.lkey = DEFAULT_MR_KEY;
}

TEST_F(TypicalMrManagerTest, st_mr_manager_release_mr_resource_test)
{
    MOCKER(hrtRaRegGlobalMr)
    .stubs()
    .will(invoke(stub_hrtRaRegGlobalMr));

    MOCKER(hrtRaDeRegGlobalMr)
    .stubs()
    .with(any(), any())
    .will(returnValue(HCCL_SUCCESS));

    s32 ret = HCCL_SUCCESS;

    ret = TypicalMrManager::GetInstance().RegisterMem(fakeMrInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalMrManager::GetInstance().regedMrMap_.size(), 1);

    ret = TypicalMrManager::GetInstance().ReleaseMrResource();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalMrManager::GetInstance().regedMrMap_.size(), 0);
    fakeMrInfo.lkey = DEFAULT_MR_KEY;
}