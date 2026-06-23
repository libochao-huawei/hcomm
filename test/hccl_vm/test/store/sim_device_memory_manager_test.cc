/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "store_sim_device_memory_manager.h"
#include "store_sim_memory_manager.h"

class DeviceMemoryManagerTest : public testing::Test {
protected:
    void TearDown() override {
        sim::MemoryManager::GetInstance().FreeMemByName("dev_test_phy");
    }
};

TEST_F(DeviceMemoryManagerTest, GetInstance_Singleton_SameInstance) {
    sim::DeviceMemoryManager& inst1 = sim::DeviceMemoryManager::GetInstance();
    sim::DeviceMemoryManager& inst2 = sim::DeviceMemoryManager::GetInstance();
    EXPECT_EQ(&inst1, &inst2);
}

TEST_F(DeviceMemoryManagerTest, AllocVirMem_Normal_Success) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocVirMem(0, 1024);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(DeviceMemoryManagerTest, AllocVirMem_DifferentDevices_DifferentAddr) {
    void* ptr0 = sim::DeviceMemoryManager::GetInstance().AllocVirMem(0, 1024);
    void* ptr1 = sim::DeviceMemoryManager::GetInstance().AllocVirMem(1, 1024);
    EXPECT_NE(ptr0, nullptr);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr0, ptr1);
}

TEST_F(DeviceMemoryManagerTest, AllocVirMem_SameDevice_Sequential_Success) {
    void* ptr1 = sim::DeviceMemoryManager::GetInstance().AllocVirMem(0, 1024);
    void* ptr2 = sim::DeviceMemoryManager::GetInstance().AllocVirMem(0, 2048);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
}

TEST_F(DeviceMemoryManagerTest, FreeVirMem_Normal_DoNothing) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocVirMem(0, 1024);
    EXPECT_NE(ptr, nullptr);
    EXPECT_NO_THROW(sim::DeviceMemoryManager::GetInstance().FreeVirMem(0, ptr));
}

TEST_F(DeviceMemoryManagerTest, AllocPhyMem_Normal_Success) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem("dev_test_phy", 0, 1024);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(DeviceMemoryManagerTest, AllocPhyMem_NullName_Fail) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem(nullptr, 0, 1024);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DeviceMemoryManagerTest, FreePhyMem_Normal_Success) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem("dev_test_phy_free", 0, 1024);
    EXPECT_NE(ptr, nullptr);
    EXPECT_NO_THROW(sim::DeviceMemoryManager::GetInstance().FreePhyMem("dev_test_phy_free", 0));
    sim::MemoryManager::GetInstance().FreeMemByName("dev_test_phy_free");
}

TEST_F(DeviceMemoryManagerTest, FreePhyMem_NullName_DoNothing) {
    EXPECT_NO_THROW(sim::DeviceMemoryManager::GetInstance().FreePhyMem(nullptr, 0));
}

TEST_F(DeviceMemoryManagerTest, AcquirePhyMem_NotExist_Fail) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AcquirePhyMem("not_exist_phy", 0, 1024);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DeviceMemoryManagerTest, AcquirePhyMem_AfterAlloc_Success) {
    void* ptr1 = sim::DeviceMemoryManager::GetInstance().AllocPhyMem("dev_test_acq", 0, 1024);
    EXPECT_NE(ptr1, nullptr);
    void* ptr2 = sim::DeviceMemoryManager::GetInstance().AcquirePhyMem("dev_test_acq", 0, 1024);
    EXPECT_NE(ptr2, nullptr);
    sim::DeviceMemoryManager::GetInstance().FreePhyMem("dev_test_acq", 0);
    sim::MemoryManager::GetInstance().FreeMemByName("dev_test_acq");
}

TEST_F(DeviceMemoryManagerTest, ReleasePhyMem_Normal_Success) {
    void* ptr = sim::DeviceMemoryManager::GetInstance().AllocPhyMem("dev_test_rel", 0, 1024);
    EXPECT_NE(ptr, nullptr);
    int ret = sim::DeviceMemoryManager::GetInstance().ReleasePhyMem("dev_test_rel", 0);
    EXPECT_EQ(ret, 0);
    sim::MemoryManager::GetInstance().FreeMemByName("dev_test_rel");
}

TEST_F(DeviceMemoryManagerTest, ReleasePhyMem_NullName_Fail) {
    int ret = sim::DeviceMemoryManager::GetInstance().ReleasePhyMem(nullptr, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(DeviceMemoryManagerTest, MapDevPtrHostPtr_Normal_Success) {
    void* devPtr = (void*)0x1000;
    void* hostPtr = (void*)0x2000;
    EXPECT_NO_THROW(sim::DeviceMemoryManager::GetInstance().MapDevPtrHostPtr(devPtr, hostPtr));
    void* result = sim::DeviceMemoryManager::GetInstance().GetHostPtrByDevPtr(devPtr);
    EXPECT_EQ(result, hostPtr);
    sim::DeviceMemoryManager::GetInstance().UnmapDevPtrHostPtr(devPtr);
}

TEST_F(DeviceMemoryManagerTest, UnmapDevPtrHostPtr_AfterMap_Success) {
    void* devPtr = (void*)0x1000;
    void* hostPtr = (void*)0x2000;
    sim::DeviceMemoryManager::GetInstance().MapDevPtrHostPtr(devPtr, hostPtr);
    EXPECT_NO_THROW(sim::DeviceMemoryManager::GetInstance().UnmapDevPtrHostPtr(devPtr));
    void* result = sim::DeviceMemoryManager::GetInstance().GetHostPtrByDevPtr(devPtr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(DeviceMemoryManagerTest, GetHostPtrByDevPtr_NotExist_ReturnsNull) {
    void* devPtr = (void*)0x9999;
    void* result = sim::DeviceMemoryManager::GetInstance().GetHostPtrByDevPtr(devPtr);
    EXPECT_EQ(result, nullptr);
}
