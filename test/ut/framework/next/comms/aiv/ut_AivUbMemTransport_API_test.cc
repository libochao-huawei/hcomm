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
#include "orion_adapter_hccp.h"
#include "orion_adapter_rts.h"
#include "hcomm_c_adpt.h"

#define private public
#define protected public

#include "aiv_ub_mem_transport.h"
#include "exchange_ipc_buffer_dto.h"
#include "env_config/env_config.h"
#include "base_config.h"
#include "local_ipc_rma_buffer.h"

#undef protected
#undef private

namespace {

static int g_recvCallCount = 0;

void StubHrtDevMemAlignWithPage(void *ptr, u64 size, void *&ipcPtr, u64 &ipcSize, u64 &ipcOff)
{
    ipcPtr = ptr;
    ipcSize = size;
    ipcOff = 0;
}

void StubHrtIpcSetMemoryName(void *ptr, char_t *name, u64 ptrMaxLen, u32 nameMaxLen)
{
    (void)ptr;
    (void)ptrMaxLen;
    const char *fakeName = "stub_ipc_name";
    (void)strncpy_s(name, nameMaxLen, fakeName, Hccl::RTS_IPC_MEM_NAME_LEN - 1);
}

void StubHrtIpcDestroyMemoryName(const char_t *name)
{
    (void)name;
}

s32 StubHrtDeviceGetBareTgid()
{
    return 12345;
}

void StubSendAsync(const u8 *sendBuf, u32 size)
{
    (void)sendBuf;
    (void)size;
}

void StubRecvAsyncNormal(u8 *recvBuf, u32 size)
{
    // The flow in UpdateMemInfo calls RecvAsync twice:
    // 1st: RecvDataSize → writes exchangeDataSize_ (u32 = size of remote stream)
    // 2nd: RecvMemInfo  → writes the actual binary data
    // Remote sends binary stream with vecSize=0 (just 4 bytes)
    g_recvCallCount++;
    if (g_recvCallCount == 1) {
        *reinterpret_cast<u32 *>(recvBuf) = sizeof(u32);
    } else {
        u32 vecSize = 0;
        (void)memcpy_s(recvBuf, size, &vecSize, sizeof(vecSize));
    }
}

}  // namespace

class AivUbMemTransportTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AivUbMemTransportTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AivUbMemTransportTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AivUbMemTransportTest SetUP" << std::endl;
        Hccl::IpAddress   localIp("1.0.0.0");
        Hccl::IpAddress   remoteIp("2.0.0.0");
        fakeSocket_ = new Hccl::Socket(nullptr, localIp, 100, remoteIp, "test", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
        setenv("HCCL_DFS_CONFIG", "task_exception:on", 1);
        MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));

        // Hardware stubs for LocalIpcRmaBuffer construction
        MOCKER(Hccl::HrtDevMemAlignWithPage).stubs().will(invoke(StubHrtDevMemAlignWithPage));
        MOCKER(Hccl::HrtIpcSetMemoryName).stubs().will(invoke(StubHrtIpcSetMemoryName));
        MOCKER(Hccl::HrtIpcDestroyMemoryName).stubs().will(invoke(StubHrtIpcDestroyMemoryName));
        MOCKER(Hccl::HrtDeviceGetBareTgid).stubs().will(invoke(StubHrtDeviceGetBareTgid));

        // Socket async stubs
        MOCKER(&Hccl::Socket::SendAsync).stubs().will(invoke(StubSendAsync));

        g_recvCallCount = 0;
    }

    virtual void TearDown()
    {
        delete fakeSocket_;
        unsetenv("HCCL_DFS_CONFIG");
        GlobalMockObject::verify();
        std::cout << "A Test case in AivUbMemTransportTest TearDown" << std::endl;
    }

    std::shared_ptr<hcomm::AivUbMemTransport> CreateAivTransport(HcommChannelDesc &desc)
    {
        return std::make_shared<hcomm::AivUbMemTransport>(fakeSocket_, desc);
    }

    std::shared_ptr<Hccl::LocalIpcRmaBuffer> CreateLocalIpcRmaBuffer(
        u64 addr, u64 size, HcclMemType type, const std::string &tag)
    {
        auto buf = std::make_shared<Hccl::Buffer>(addr, size, type, tag.c_str());
        return std::make_shared<Hccl::LocalIpcRmaBuffer>(buf);
    }

    std::array<char, HCCL_RES_TAG_MAX_LEN> BuildMemTagArray(const std::string &tag)
    {
        std::array<char, HCCL_RES_TAG_MAX_LEN> memTag{};
        memcpy_s(memTag.data(), memTag.size(), tag.c_str(), tag.size());
        return memTag;
    }

    Hccl::Socket *fakeSocket_;
};

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);
    aivTransport->rmtBufferVec_.push_back(std::make_unique<Hccl::RemoteIpcRmaBuffer>());
    auto rmtBuffer1 = std::make_unique<Hccl::RemoteIpcRmaBuffer>();
    rmtBuffer1->addr = (uintptr_t)0x101;
    rmtBuffer1->size = (u64)0x101;
    rmtBuffer1->memType = HcclMemType::HCCL_MEM_TYPE_HOST;
    aivTransport->rmtBufferVec_.push_back(std::move(rmtBuffer1));

    aivTransport->remoteUserMemTag_.push_back(BuildMemTagArray("cclBuffer"));
    aivTransport->remoteUserMemTag_.push_back(BuildMemTagArray("buffer1"));

    CommMem *remoteMems;
    char **memTags;
    u32 memNum;
    HcclResult ret = aivTransport->GetUserRemoteMem(&remoteMems, &memTags, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(std::string(memTags[0]), "buffer1");
    EXPECT_EQ(remoteMems[0].type, HcclMemType::HCCL_MEM_TYPE_HOST);
    EXPECT_EQ(remoteMems[0].addr, (void *)0x101);
    EXPECT_EQ(remoteMems[0].size, (uint64_t)0x101);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_bufferNumIs0_Expect_ReturnIsHCCL_E_PARA)
{
    HcommChannelDesc desc{};
    desc.memHandleNum = 0;
    auto aivTransport = CreateAivTransport(desc);
    HcclResult ret = aivTransport->Init();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);
    auto initMockBuffer = CreateLocalIpcRmaBuffer(0x100, 0x100, HCCL_MEM_TYPE_DEVICE, "initBuffer");
    aivTransport->localRmaBufferVec_.push_back(initMockBuffer.get());
    aivTransport->localUserMemTag_.push_back(BuildMemTagArray("initBuffer"));

    size_t initialVecSize = aivTransport->localRmaBufferVec_.size();
    size_t initialTagSize = aivTransport->localUserMemTag_.size();

    // Normal case: GetAsyncStatus returns OK
    Hccl::SocketStatus okStatus = Hccl::SocketStatus::OK;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(okStatus));
    MOCKER(&Hccl::Socket::RecvAsync).stubs().will(invoke(StubRecvAsyncNormal));

    auto mockBuffer1 = CreateLocalIpcRmaBuffer(0x1000, 0x1000, HCCL_MEM_TYPE_DEVICE, "newBuffer1");
    auto mockBuffer2 = CreateLocalIpcRmaBuffer(0x2000, 0x2000, HCCL_MEM_TYPE_DEVICE, "newBuffer2");
    void* memHandles[2] = {
        reinterpret_cast<void*>(mockBuffer1.get()),
        reinterpret_cast<void*>(mockBuffer2.get())
    };

    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(aivTransport->localRmaBufferVec_.size(), initialVecSize + 2);
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize], mockBuffer1.get());
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize + 1], mockBuffer2.get());
    EXPECT_EQ(aivTransport->localUserMemTag_.size(), initialTagSize + 2);
    EXPECT_EQ(std::string(aivTransport->localUserMemTag_[initialTagSize].data()), "newBuffer1");
    EXPECT_EQ(std::string(aivTransport->localUserMemTag_[initialTagSize + 1].data()), "newBuffer2");
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_SocketTimeout_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);

    Hccl::SocketStatus fakeSocketStatus = Hccl::SocketStatus::TIMEOUT;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(fakeSocketStatus));

    auto mockBuffer = CreateLocalIpcRmaBuffer(0x1000, 0x1000, HCCL_MEM_TYPE_DEVICE, "testBuffer");
    void* memHandles[1] = { reinterpret_cast<void*>(mockBuffer.get()) };

    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_bufferNumIs0_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);
    void* memHandles[1] = { nullptr };
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}
