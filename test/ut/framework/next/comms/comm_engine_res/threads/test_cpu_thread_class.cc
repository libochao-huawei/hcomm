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
#include "hccl/hcomm_primitives.h"
#define private public
#define WAIT_TIMEOUT_MS 3000 // 3秒
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

drvError_t halSvmRegister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag, uint64_t *access_va)
{
    return DRV_ERROR_NONE;
}
drvError_t halSvmAccess(uint32_t dev_id, uint64_t dst, uint64_t src, uint64_t size, uint64_t flag)
{
    return DRV_ERROR_NONE;
}
drvError_t halSvmUnregister(uint32_t dev_id, uint64_t va, uint64_t size, uint64_t flag)
{
    return DRV_ERROR_NONE;
}

TEST_F(TestCpuThread, Ut_MemNotify_When_UB_Alloc_Wait_Expect_Success)
{
    hccl::MemNotify memNotify;
    HcclResult ret = memNotify.Alloc();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // Set the flag manually before calling Wait
    *reinterpret_cast<uint8_t*>(memNotify.notifyHostVa_) = 1;

    ret = memNotify.Wait();
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestCpuThread, Ut_MemNotify_When_UB_Wait_Timeout_Expect_Fail)
{
    hccl::MemNotify memNotify;
    HcclResult ret = memNotify.Alloc();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = memNotify.Wait();
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

// TEST_F(TestCpuThread, Ut_MemNotify_When_PCIE_Alloc_Wait_Expect_Success)
// {
//     hccl::MemNotify memNotify;

//     // Mock for Alloc (PCIe path)
//     MOCKER(hrtGetDevice)
//         .stubs()
//         .with(outBound(0))
//         .will(returnValue(HCCL_SUCCESS));

//     MOCKER(hrtHalGetDeviceInfo)
//         .stubs()
//         .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(1LL))
//         .will(returnValue(HCCL_SUCCESS));

//     void* mockDeviceVa = reinterpret_cast<void*>(0x1000);
//     MOCKER(aclrtMalloc)
//         .stubs()
//         .with(outBoundPointee(mockDeviceVa), sizeof(uint8_t) + 4096ULL, ACL_MEM_MALLOC_HUGE_ONLY)
//         .will(returnValue(ACL_SUCCESS));

//     uint64_t mockAccessVa = 0xABCD;
//     MOCKER(halSvmRegister)
//         .stubs()
//         .with(0, any(), sizeof(uint8_t), SVM_REGISTER_FLAG_WITH_ACCESS_VA, outBoundPointee(mockAccessVa))
//         .will(returnValue(0));

//     HcclResult ret = memNotify.Alloc();
//     EXPECT_EQ(ret, HCCL_SUCCESS);

//     // Mock for Wait - simulate device flag set
//     MOCKER(hrtGetDevice)
//         .stubs()
//         .with(outBound(0))
//         .will(returnValue(HCCL_SUCCESS));

//     MOCKER(hrtHalGetDeviceInfo)
//         .stubs()
//         .with(0, MODULE_TYPE_SYSTEM, INFO_TYPE_HD_CONNECT_TYPE, outBoundPointee(1LL))
//         .will(returnValue(HCCL_SUCCESS));

//     // Mock aclrtMemcpy to return flag = 1
//     MOCKER(aclrtMemcpy)
//         .stubs()
//         .with(any(), sizeof(uint8_t), mockDeviceVa, sizeof(uint8_t), ACL_MEMCPY_DEVICE_TO_HOST)
//         .will(do {
//             uint8_t* dst = (uint8_t*)arg0;
//             *dst = 1;
//             return ACL_ERROR_NONE;
//         });

//     ret = memNotify.Wait();
//     EXPECT_EQ(ret, HCCL_SUCCESS);
// }

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_Normal_Expect_Return_HCCL_SUCCESS)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
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

    MOCKER_CPP(&hccl::CpuThread::PrepareDpuKernelResource)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::CpuThread::LaunchDpuKernel)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
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

    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
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

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_PARAM));

    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_PrepareDpuKernelResourceFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtBinaryLoadFromFile)
        .stubs()
        .will(returnValue(ACL_ERROR_INVALID_FILE));

    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestCpuThread, Ut_CpuThread_Init_On_LaunchDpuKernelFailed_Expect_Return_HCCL_E_INTERNAL)
{
    bool isDeviceSide{false};
    MOCKER(GetRunSideIsDevice)
        .stubs()
        .with(outBound(isDeviceSide))
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
        .will(returnValue(ACL_SUCCESS));

    MOCKER(aclrtGetCurrentContext)
        .stubs()
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

    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
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

    MOCKER(aclrtGetCurrentContext)
        .stubs()
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

    MOCKER_CPP(&hccl::CpuThread::PrepareDpuKernelResource)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER_CPP(&hccl::CpuThread::LaunchDpuKernel)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    hccl::CpuThread cpuThread(hccl::StreamType::STREAM_TYPE_DEVICE, 2, hccl::NotifyLoadType::HOST_NOTIFY);
    HcclResult ret = cpuThread.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* threadEntity = nullptr;
    ret = cpuThread.GetThreadEntity(threadEntity);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
