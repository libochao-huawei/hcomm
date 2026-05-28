/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * This SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../../ut_hcomm_base.h"
#define private public
#define protected public
#include "channel_process.h"
#undef protected
#undef private

class TestChannelProcess : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
    }
};

// CreateChannelsLoop 空指针测试
TEST_F(TestChannelProcess, Ut_TestCreateChannelsLoop_When_EndpointHandleNullptr_Return_HCCL_E_PTR)
{
    HcommChannelDesc desc = {};
    ChannelHandle outHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::CreateChannelsLoop(nullptr, COMM_ENGINE_AICPU, &desc, 1, outHandles);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

// ConnectChannels 空指针和参数校验测试
TEST_F(TestChannelProcess, Ut_TestConnectChannels_When_TargetChannelsNullptr_Return_HCCL_E_PTR)
{
    HcclResult ret = hcomm::ChannelProcess::ConnectChannels(nullptr, 1, COMM_ENGINE_AICPU);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestConnectChannels_When_ChannelNumZero_Return_HCCL_E_PARA)
{
    ChannelHandle channels[1] = {};
    HcclResult ret = hcomm::ChannelProcess::ConnectChannels(channels, 0, COMM_ENGINE_AICPU);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// FillChannelD2HMap 空指针和参数校验测试
TEST_F(TestChannelProcess, Ut_TestFillChannelD2HMap_When_DeviceChannelHandlesNullptr_Return_HCCL_E_PTR)
{
    ChannelHandle hostHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(nullptr, hostHandles, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestFillChannelD2HMap_When_HostChannelHandlesNullptr_Return_HCCL_E_PTR)
{
    ChannelHandle deviceHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles, nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestFillChannelD2HMap_When_ListNumZero_Return_HCCL_E_PARA)
{
    ChannelHandle deviceHandles[1] = {};
    ChannelHandle hostHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::FillChannelD2HMap(deviceHandles, hostHandles, 0);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// LaunchChannelKernelCommon 空指针和参数校验测试
TEST_F(TestChannelProcess, Ut_TestLaunchChannelKernelCommon_When_ChannelHandlesNullptr_Return_HCCL_E_PTR)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle hostHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::LaunchChannelKernelCommon(
        nullptr, hostHandles, hcommDescs, 1, "test_tag", nullptr, "test_kernel", false);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestLaunchChannelKernelCommon_When_HostChannelHandlesNullptr_Return_HCCL_E_PTR)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle deviceHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::LaunchChannelKernelCommon(
        deviceHandles, nullptr, hcommDescs, 1, "test_tag", nullptr, "test_kernel", false);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestLaunchChannelKernelCommon_When_ListNumZero_Return_HCCL_E_PARA)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle deviceHandles[1] = {};
    ChannelHandle hostHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::LaunchChannelKernelCommon(
        deviceHandles, hostHandles, hcommDescs, 0, "test_tag", nullptr, "test_kernel", false);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// SaveChannels 空指针和参数校验测试
TEST_F(TestChannelProcess, Ut_TestSaveChannels_When_TargetChannelsNullptr_Return_HCCL_E_PTR)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle userChannels[1] = {};
    HcclResult ret = hcomm::ChannelProcess::SaveChannels(
        nullptr, userChannels, hcommDescs, 1, COMM_ENGINE_AICPU, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestSaveChannels_When_UserChannelsNullptr_Return_HCCL_E_PTR)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle targetChannels[1] = {};
    HcclResult ret = hcomm::ChannelProcess::SaveChannels(
        targetChannels, nullptr, hcommDescs, 1, COMM_ENGINE_AICPU, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestSaveChannels_When_ChannelNumZero_Return_HCCL_E_PARA)
{
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle targetChannels[1] = {};
    ChannelHandle userChannels[1] = {};
    HcclResult ret = hcomm::ChannelProcess::SaveChannels(
        targetChannels, userChannels, hcommDescs, 0, COMM_ENGINE_AICPU, nullptr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// ChannelGetUserRemoteMem 空指针测试
TEST_F(TestChannelProcess, Ut_TestChannelGetUserRemoteMem_When_RemoteMemNullptr_Return_HCCL_E_PTR)
{
    char** memTag = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = hcomm::ChannelProcess::ChannelGetUserRemoteMem(0, nullptr, &memTag, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestChannelGetUserRemoteMem_When_MemTagNullptr_Return_HCCL_E_PTR)
{
    CommMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    HcclResult ret = hcomm::ChannelProcess::ChannelGetUserRemoteMem(0, &remoteMem, nullptr, &memNum);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_TestChannelGetUserRemoteMem_When_MemNumNullptr_Return_HCCL_E_PTR)
{
    CommMem* remoteMem = nullptr;
    char** memTag = nullptr;
    HcclResult ret = hcomm::ChannelProcess::ChannelGetUserRemoteMem(0, &remoteMem, &memTag, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_ChannelClean_NullList_Returns_E_PARA) {
    // Passing null pointer should return parameter error
    auto ret = hcomm::ChannelProcess::ChannelClean(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_ChannelResumeConcurrency_ZeroChannels_Returns_SUCCESS) {
    // Zero channel count should be a no-op and return success
    ChannelHandle dummyList[1] = {0};
    auto ret = hcomm::ChannelProcess::ChannelResumeConcurrency(dummyList, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TestChannelProcess, Ut_ChannelResume_When_ResumeConcurrencyFails_ReturnsError) {
    ChannelHandle list[1] = { (ChannelHandle)0x1 };
    // Mock ChannelResumeConcurrency to return internal error
    MOCKER_CPP(&hcomm::ChannelProcess::ChannelResumeConcurrency, HcclResult(const ChannelHandle*, uint32_t))
        .stubs()
        .with(any(), any())
        .will(returnValue(HCCL_E_INTERNAL));

    auto ret = hcomm::ChannelProcess::ChannelResume(list, 1);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

TEST_F(TestChannelProcess, Ut_ChannelResume_NullList_Returns_E_PARA) {
    auto ret = hcomm::ChannelProcess::ChannelResume(nullptr, 1);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_LaunchChannelKernel_When_ChannelKindIsUBOE_CallsChannelKernelLaunchForBase) {
    HcommChannelDesc hcommDescs[1] = {};
    ChannelHandle deviceHandles[1] = {};

    class FakeUboeChannel : public hcomm::Channel {
    public:
        hcomm::HcommChannelKind GetChannelKind() const override {
            return hcomm::HcommChannelKind::AICPU_TS_UBOE;
        }
        HcclResult Init() override {
            return HCCL_SUCCESS;
        }
        HcclResult GetNotifyNum(uint32_t *notifyNum) const override {
            return HCCL_SUCCESS;
        }
        HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override {
            return HCCL_SUCCESS;
        }
        hcomm::ChannelStatus GetStatus() override {
            return hcomm::ChannelStatus::READY;
        }
        HcclResult Clean() override {
            return HCCL_SUCCESS;
        }
        HcclResult Resume() override {
            return HCCL_SUCCESS;
        }
        HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override {
            return HCCL_SUCCESS;
        }
        HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override {
            return HCCL_SUCCESS;
        }
        HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override {
            return HCCL_SUCCESS;
        }
        HcclResult Write(void *dst, const void *src, uint64_t len) override {
            return HCCL_SUCCESS;
        }
        HcclResult Read(void *dst, const void *src, uint64_t len) override {
            return HCCL_SUCCESS;
        }
        HcclResult ChannelFence() override {
            return HCCL_SUCCESS;
        }
    };

    FakeUboeChannel fakeChannel;
    ChannelHandle hostHandles[1] = {reinterpret_cast<ChannelHandle>(&fakeChannel)};

    MOCKER_CPP(&hcomm::ChannelProcess::LaunchChannelKernelCommon,
        HcclResult(ChannelHandle*, ChannelHandle*, HcommChannelDesc*, uint32_t, const std::string&, aclrtBinHandle, const std::string&, bool))
        .stubs()
        .with(any(), any(), any(), any(), any(), any(), any(), any())
        .will(returnValue(HCCL_SUCCESS));

    auto ret = hcomm::ChannelProcess::LaunchChannelKernel(deviceHandles, hostHandles, hcommDescs, 1, nullptr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

namespace {
constexpr int32_t LOCKFIX_TEST_DEVICE_ID = 0;
constexpr ChannelHandle LOCKFIX_TEST_DEV_HANDLE = 0x100;
constexpr ChannelHandle LOCKFIX_TEST_HOST_HANDLE = 0x200;

HcclResult StubHrtGetDeviceForLockFix(s32 *deviceLogicId)
{
    if (deviceLogicId != nullptr) {
        *deviceLogicId = LOCKFIX_TEST_DEVICE_ID;
    }
    return HCCL_SUCCESS;
}

class FakeChannelLockFix : public hcomm::Channel {
public:
    hcomm::HcommChannelKind GetChannelKind() const override { return hcomm::HcommChannelKind::AICPU_TS_URMA; }
    HcclResult Init() override { return HCCL_SUCCESS; }
    HcclResult GetNotifyNum(uint32_t *notifyNum) const override { return HCCL_SUCCESS; }
    HcclResult GetRemoteMem(HcclMem **remoteMem, uint32_t *memNum, char **memTags) override { return HCCL_SUCCESS; }
    hcomm::ChannelStatus GetStatus() override { return hcomm::ChannelStatus::READY; }
    HcclResult Clean() override { return HCCL_SUCCESS; }
    HcclResult Resume() override { return HCCL_SUCCESS; }
    HcclResult NotifyRecord(const uint32_t remoteNotifyIdx) override { return HCCL_SUCCESS; }
    HcclResult NotifyWait(const uint32_t localNotifyIdx, const uint32_t timeout) override { return HCCL_SUCCESS; }
    HcclResult WriteWithNotify(void *dst, const void *src, const uint64_t len, uint32_t remoteNotifyIdx) override { return HCCL_SUCCESS; }
    HcclResult Write(void *dst, const void *src, uint64_t len) override { return HCCL_SUCCESS; }
    HcclResult Read(void *dst, const void *src, uint64_t len) override { return HCCL_SUCCESS; }
    HcclResult ChannelFence() override { return HCCL_SUCCESS; }
    HcclResult UpdateMemInfo(HcommMemHandle *memHandles, uint32_t memHandleNum) override { return HCCL_SUCCESS; }
};

void LockFixClearGlobalMaps()
{
    hcomm::ChannelProcess::g_ChannelD2HMap.clear();
    hcomm::ChannelProcess::g_ChannelMap.clear();
}
}

TEST_F(TestChannelProcess, Ut_ChannelUpdateMemInfo_When_HandleNotFoundInD2HMap_Returns_E_NOT_FOUND)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    HcommMemHandle memHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::ChannelUpdateMemInfo(memHandles, 1, LOCKFIX_TEST_DEV_HANDLE);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelUpdateMemInfo_When_ChannelNotFoundInChannelMap_Returns_E_INTERNAL)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, LOCKFIX_TEST_HOST_HANDLE);

    HcommMemHandle memHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::ChannelUpdateMemInfo(memHandles, 1, LOCKFIX_TEST_DEV_HANDLE);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelUpdateMemInfo_When_NullSharedPtrInChannelMap_Returns_E_INTERNAL)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, LOCKFIX_TEST_HOST_HANDLE);
    hcomm::ChannelProcess::g_ChannelMap.emplace(LOCKFIX_TEST_HOST_HANDLE, std::shared_ptr<hcomm::Channel>());

    HcommMemHandle memHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::ChannelUpdateMemInfo(memHandles, 1, LOCKFIX_TEST_DEV_HANDLE);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelUpdateMemInfo_When_ChannelFound_Returns_SUCCESS)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    auto fakeChannel = std::make_shared<FakeChannelLockFix>();
    ChannelHandle hostHandle = reinterpret_cast<ChannelHandle>(fakeChannel.get());

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, hostHandle);
    hcomm::ChannelProcess::g_ChannelMap.emplace(hostHandle, fakeChannel);

    HcommMemHandle memHandles[1] = {};
    HcclResult ret = hcomm::ChannelProcess::ChannelUpdateMemInfo(memHandles, 1, LOCKFIX_TEST_DEV_HANDLE);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelGet_When_ChannelOutputNull_Returns_E_PTR)
{
    HcclResult ret = hcomm::ChannelProcess::ChannelGet(LOCKFIX_TEST_DEV_HANDLE, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(TestChannelProcess, Ut_ChannelGet_When_HandleNotFoundInD2HMap_Returns_E_NOT_FOUND)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    void *channelOut = nullptr;
    HcclResult ret = hcomm::ChannelProcess::ChannelGet(LOCKFIX_TEST_DEV_HANDLE, &channelOut);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelGet_When_ChannelNotFoundInChannelMap_Returns_E_NOT_FOUND)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, LOCKFIX_TEST_HOST_HANDLE);

    void *channelOut = nullptr;
    HcclResult ret = hcomm::ChannelProcess::ChannelGet(LOCKFIX_TEST_DEV_HANDLE, &channelOut);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelGet_When_NullSharedPtrInChannelMap_Returns_E_INTERNAL)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, LOCKFIX_TEST_HOST_HANDLE);
    hcomm::ChannelProcess::g_ChannelMap.emplace(LOCKFIX_TEST_HOST_HANDLE, std::shared_ptr<hcomm::Channel>());

    void *channelOut = nullptr;
    HcclResult ret = hcomm::ChannelProcess::ChannelGet(LOCKFIX_TEST_DEV_HANDLE, &channelOut);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_ChannelGet_When_ChannelFound_Returns_SUCCESS)
{
    LockFixClearGlobalMaps();
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDeviceForLockFix));

    auto fakeChannel = std::make_shared<FakeChannelLockFix>();
    ChannelHandle hostHandle = reinterpret_cast<ChannelHandle>(fakeChannel.get());

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, hostHandle);
    hcomm::ChannelProcess::g_ChannelMap.emplace(hostHandle, fakeChannel);

    void *channelOut = nullptr;
    HcclResult ret = hcomm::ChannelProcess::ChannelGet(LOCKFIX_TEST_DEV_HANDLE, &channelOut);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(channelOut, nullptr);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_RemoveSingleChannel_When_HandleNotFoundInD2HMap_Returns_E_NOT_FOUND)
{
    LockFixClearGlobalMaps();

    std::vector<ChannelHandle> deviceHandles;
    HcclResult ret = hcomm::ChannelProcess::RemoveSingleChannel(
        LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE, deviceHandles);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_RemoveSingleChannel_When_ChannelNotFoundInChannelMap_Returns_E_NOT_FOUND)
{
    LockFixClearGlobalMaps();

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, LOCKFIX_TEST_HOST_HANDLE);

    std::vector<ChannelHandle> deviceHandles;
    HcclResult ret = hcomm::ChannelProcess::RemoveSingleChannel(
        LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE, deviceHandles);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);

    LockFixClearGlobalMaps();
}

TEST_F(TestChannelProcess, Ut_RemoveSingleChannel_When_Found_ErasesFromBothMaps_Returns_SUCCESS)
{
    LockFixClearGlobalMaps();

    auto fakeChannel = std::make_shared<FakeChannelLockFix>();
    ChannelHandle hostHandle = reinterpret_cast<ChannelHandle>(fakeChannel.get());

    hcomm::DeviceChannelKey key{LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE};
    hcomm::ChannelProcess::g_ChannelD2HMap.emplace(key, hostHandle);
    hcomm::ChannelProcess::g_ChannelMap.emplace(hostHandle, fakeChannel);

    std::vector<ChannelHandle> deviceHandles;
    HcclResult ret = hcomm::ChannelProcess::RemoveSingleChannel(
        LOCKFIX_TEST_DEVICE_ID, LOCKFIX_TEST_DEV_HANDLE, deviceHandles);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(deviceHandles.size(), 1u);
    EXPECT_EQ(deviceHandles[0], LOCKFIX_TEST_DEV_HANDLE);
    EXPECT_EQ(hcomm::ChannelProcess::g_ChannelMap.count(hostHandle), 0u);
    EXPECT_EQ(hcomm::ChannelProcess::g_ChannelD2HMap.size(), 0u);

    LockFixClearGlobalMaps();
}
