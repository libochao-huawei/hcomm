/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../hccl_api_base_test.h"
#include "cpu_thread.h"
#include "local_notify_impl.h"
#include "llt_hccl_stub_rank_graph.h"

#include <thread>
#include <atomic>
#include <chrono>

class TestCpuThread : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestCpuThread, Ut_MemNotify_When_Normal_Alloc_Wait_Expect_Success)
{
    MemNotify memNotify;

    // Mock for Alloc (non-PCIe path)
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(0LL))
        .will(returnValue(HCCL_SUCCESS));

    void* mockHostVa = reinterpret_cast<void*>(0x1234);
    MOCKER(malloc)
        .stubs()
        .with(sizeof(uint8_t))
        .will(returnValue(mockHostVa));

    void* mockDeviceVa = reinterpret_cast<void*>(0x5678);
    MOCKER(aclrtHostRegister)
        .stubs()
        .with(mockHostVa, sizeof(uint8_t), ACL_HOST_REGISTER_MAPPED, outBoundPointee(mockDeviceVa))
        .will(returnValue(ACL_SUCCESS));

    HcclResult ret = memNotify.Alloc();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock for Wait - simulate flag already set
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(0LL))
        .will(returnValue(HCCL_SUCCESS));

    // Set the flag manually before calling Wait
    *reinterpret_cast<uint8_t*>(mockHostVa) = 1;

    ret = memNotify.Wait();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestCpuThread, Ut_MemNotify_When_Wait_Timeout_Expect_Fail)
{
    MemNotify memNotify;

    // Mock for Alloc (non-PCIe path)
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(0LL))
        .will(returnValue(HCCL_SUCCESS));

    void* mockHostVa = reinterpret_cast<void*>(0x5678);
    MOCKER(malloc)
        .stubs()
        .with(sizeof(uint8_t))
        .will(returnValue(mockHostVa));

    void* mockDeviceVa = reinterpret_cast<void*>(0x9ABC);
    MOCKER(aclrtHostRegister)
        .stubs()
        .with(mockHostVa, sizeof(uint8_t), ACL_HOST_REGISTER_MAPPED, outBoundPointee(mockDeviceVa))
        .will(returnValue(ACL_SUCCESS));

    HcclResult ret = memNotify.Alloc();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock for Wait - flag not set, should timeout
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(0LL))
        .will(returnValue(HCCL_SUCCESS));

    // Ensure flag is 0
    *reinterpret_cast<uint8_t*>(mockHostVa) = 0;

    ret = memNotify.Wait();
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(TestCpuThread, Ut_MemNotify_When_PCIE_Alloc_Wait_Expect_Success)
{
    MemNotify memNotify;

    // Mock for Alloc (PCIe path)
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(1LL))
        .will(returnValue(HCCL_SUCCESS));

    void* mockDeviceVa = reinterpret_cast<void*>(0x1000);
    MOCKER(aclrtMalloc)
        .stubs()
        .with(outBoundPointee(mockDeviceVa), sizeof(uint8_t) + 4096ULL, ACL_MEM_MALLOC_HUGE_ONLY)
        .will(returnValue(ACL_SUCCESS));

    uint64_t mockAccessVa = 0xABCD;
    MOCKER(halSvmRegister)
        .stubs()
        .with(0, any(), sizeof(uint8_t), SVM_REGISTER_FLAG_WITH_ACCESS_VA, outBoundPointee(mockAccessVa))
        .will(returnValue(0));

    HcclResult ret = memNotify.Alloc();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Mock for Wait - simulate device flag set
    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtHalGetDeviceInfo)
        .stubs()
        .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(1LL))
        .will(returnValue(HCCL_SUCCESS));

    // Mock aclrtMemcpy to return flag = 1
    MOCKER(aclrtMemcpy)
        .stubs()
        .with(any(), sizeof(uint8_t), mockDeviceVa, sizeof(uint8_t), ACL_MEMCPY_DEVICE_TO_HOST)
        .will(do {
            uint8_t* dst = (uint8_t*)arg0;
            *dst = 1;
            return ACL_ERROR_NONE;
        });

    // Mock reset memcpy
    MOCKER(aclrtMemcpy)
        .expects(least(1))
        .with(mockDeviceVa, sizeof(uint8_t), any(), sizeof(uint8_t), ACL_MEMCPY_HOST_TO_DEVICE)
        .will(returnValue(ACL_ERROR_NONE));

    ret = memNotify.Wait();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// Helper that prepares mocks and returns a CpuThread already initialized successfully.
static CpuThread CreateInitCpuThread() {
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    EXPECT_EQ(cpuThread.Init(), HCCL_SUCCESS);
    return cpuThread;
}


TEST_F(TestCpuThread, Ut_CpuThread_Init_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    uint32_t notifyNum = cpuThread.GetNotifyNum();
    EXPECT_EQ(2, notifyNum);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_hrtGetDeviceFailed_Expect_Return_HCCL_E_DRV)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .will(returnValue(HCCL_E_DRV));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_DRV);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_aclrtGetCurrentContextFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_rtSetXpuDeviceFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_PrepareDpuKernelResourceFailed_Expect_Return_HCCL_E_OPEN_FILE_FAILURE)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_FILE));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_OPEN_FILE_FAILURE);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_LaunchDpuKernelFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_GetThreadEntity_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtGetDevice)
        .stubs()
        .with(outBound(0))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(Hccl::rtSetXpuDevice)
        .stubs()
        .with(Hccl::RT_DEV_TYPE_DPU, 0)
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .expects(least(1))
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtCreateStreamWithConfig)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryGetFunction)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtLaunchKernelWithHostArgs)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtSetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtMalloc)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtMemcpy)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    CpuThread cpuThread(StreamType::STREAM_TYPE_DEVICE, 2, NotifyLoadType::DEVICE_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* threadEntity = nullptr;
    ret = cpuThread.GetThreadEntity(threadEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
