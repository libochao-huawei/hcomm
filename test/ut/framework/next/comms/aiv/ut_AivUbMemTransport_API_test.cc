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
#include "mockcpp/mokc.h"
#include <mockcpp/mockcpp.hpp>
#include "orion_adapter_hccp.h"
#include "hcomm_c_adpt.h"

#define private public
#define protected public

#include "aiv_ub_mem_transport.h"
#include "exchange_ipc_buffer_dto.h"
#include "env_config/env_config.h"
#include "base_config.h"

#undef protected
#undef private

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
        fakeSocket = new Hccl::Socket(nullptr, localIp, 100, remoteIp, "test", Hccl::SocketRole::SERVER, Hccl::NicType::HOST_NIC_TYPE);
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        delete fakeSocket;
        std::cout << "A Test case in AivUbMemTransportTest TearDown" << std::endl;
    }

    Hccl::Socket *fakeSocket;
    Hccl::RdmaHandle rdmaHandle = (void *)0x1000000;
};

class LocalRmaBufferStub : public Hccl::LocalRmaBuffer {
public:
    LocalRmaBufferStub(std::shared_ptr<Hccl::Buffer> buf, u64 mockAddr, u64 mockSize, u64 mockOffset, u32 mockPid)
        : Hccl::LocalRmaBuffer(buf, Hccl::RmaType::IPC),
        mockAddr_(mockAddr), mockSize_(mockSize), mockOffset_(mockOffset), mockPid_(mockPid) {}
    
    ~LocalRmaBufferStub() override = default;

    std::string Describe() const override {
        return "LocalRmaBufferStub";
    }

    std::unique_ptr<Hccl::Serializable> GetExchangeDto() override {
        return std::make_unique<Hccl::ExchangeIpcBufferDto>(mockAddr_, mockSize_, mockOffset_, mockPid_);
    }

    u64 mockAddr_{0};
    u64 mockSize_{0};
    u64 mockOffset_{0};
    u32 mockPid_{0};
};

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    auto rmtBuffer0 = std::make_unique<Hccl::RemoteIpcRmaBuffer>();
    aivTransport->rmtBufferVec_.push_back(std::move(rmtBuffer0));
    auto rmtBuffer1 = std::make_unique<Hccl::RemoteIpcRmaBuffer>();
    rmtBuffer1->addr = (uintptr_t)0x101;
    rmtBuffer1->size = (u64)0x101;
    rmtBuffer1->memType = HcclMemType::HCCL_MEM_TYPE_HOST;
    aivTransport->rmtBufferVec_.push_back(std::move(rmtBuffer1));

    std::array<char, HCCL_RES_TAG_MAX_LEN> memTag0{};
    std::string tag0 = "cclBuffer";
    memcpy_s(memTag0.data(), memTag0.size(), tag0.c_str(), tag0.size());
    aivTransport->remoteUserMemTag_.push_back(memTag0);
    std::array<char, HCCL_RES_TAG_MAX_LEN> memTag1{};
    std::string tag1 = "buffer1";
    memcpy_s(memTag1.data(), memTag1.size(), tag1.c_str(), tag1.size());
    aivTransport->remoteUserMemTag_.push_back(memTag1);

    CommMem *remoteMems;
    char **memTags;
    u32 memNum;
    HcclResult ret = aivTransport->GetUserRemoteMem(&remoteMems, &memTags, &memNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    std::string memTag = memTags[0];
    EXPECT_EQ(memTag, "buffer1");
    EXPECT_EQ(remoteMems[0].type, HcclMemType::HCCL_MEM_TYPE_HOST);
    EXPECT_EQ(remoteMems[0].addr, (void *)0x101);
    EXPECT_EQ(remoteMems[0].size, (uint64_t)0x101);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_GetUserRemoteMem_When_bufferNumIs0_Expect_ReturnIsHCCL_E_PARA)
{
    HcommChannelDesc desc{};
    desc.memHandleNum = 0;
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    HcclResult ret = aivTransport->Init();
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_Normal_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    auto initBuffer = std::make_shared<Hccl::Buffer>(0x100, 0x100);
    auto initMockBuffer = std::make_shared<LocalRmaBufferStub>(initBuffer, 0x100, 0x100, 0, 100);
    aivTransport->localRmaBufferVec_.push_back(reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(initMockBuffer.get()));

    std::array<char, HCCL_RES_TAG_MAX_LEN> initTag{};
    std::string initTagStr = "initBuffer";
    memcpy_s(initTag.data(), initTag.size(), initTagStr.c_str(), initTagStr.size());
    aivTransport->localUserMemTag_.push_back(initTag);

    size_t initialVecSize = aivTransport->localRmaBufferVec_.size();
    size_t initialTagSize = aivTransport->localUserMemTag_.size();

    auto buffer1 = std::make_shared<Hccl::Buffer>(0x1000, 0x1000);
    auto mockBuffer1 = std::make_shared<LocalRmaBufferStub>(buffer1, 0x1000, 0x1000, 0, 1000);
    auto buffer2 = std::make_shared<Hccl::Buffer>(0x2000, 0x2000);
    auto mockBuffer2 = std::make_shared<LocalRmaBufferStub>(buffer2, 0x2000, 0x2000, 0, 2000);

    CommMemInfo memInfo1{};
    memInfo1.bufferHandle = reinterpret_cast<void*>(mockBuffer1.get());
    std::string memTag1 = "newBuffer1";
    memcpy_s(memInfo1.memTag, sizeof(memInfo1.memTag), memTag1.c_str(), memTag1.size());
    CommMemInfo memInfo2{};
    memInfo2.bufferHandle = reinterpret_cast<void*>(mockBuffer2.get());
    std::string memTag2 = "newBuffer2";
    memcpy_s(memInfo2.memTag, sizeof(memInfo2.memTag), memTag2.c_str(), memTag2.size());
    void* memHandles[2] = { &memInfo1, &memInfo2 };

    MOCKER(&Hccl::EnvConfig::Parse).stubs().will(ignoreReturnValue());
    Hccl::EnvSocketConfig fakeSocketConfig{};
    MOCKER(&Hccl::EnvConfig::GetSocketConfig).stubs().will(returnValue(fakeSocketConfig));
    MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(aivTransport->localRmaBufferVec_.size(), initialVecSize + 2);
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize],
        reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(mockBuffer1.get()));
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize + 1],
        reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(mockBuffer2.get()));
    EXPECT_EQ(aivTransport->localUserMemTag_.size(), initialTagSize + 2);
    std::string tag1(aivTransport->localUserMemTag_[initialTagSize].data());
    std::string tag2(aivTransport->localUserMemTag_[initialTagSize + 1].data());
    EXPECT_EQ(tag1, "newBuffer1");
    EXPECT_EQ(tag2, "newBuffer2");
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_SocketTimeout_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    HcommChannelDesc desc{};
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    auto buffer = std::make_shared<Hccl::Buffer>(0x1000, 0x1000);
    auto mockBuffer = std::make_shared<LocalRmaBufferStub>(buffer, 0x1000, 0x1000, 0, 1000);
    CommMemInfo memInfo{};
    memInfo.bufferHandle = reinterpret_cast<void*>(mockBuffer.get());
    std::string memTag = "testBuffer";
    memcpy_s(memInfo.memTag, sizeof(memInfo.memTag), memTag.c_str(), memTag.size());
    void* memHandles[1] = { &memInfo };

    MOCKER(&Hccl::EnvConfig::Parse).stubs().will(ignoreReturnValue());
    Hccl::EnvSocketConfig fakeSocketConfig{};
    MOCKER(&Hccl::EnvConfig::GetSocketConfig).stubs().will(returnValue(fakeSocketConfig));
    MOCKER(&Hccl::EnvSocketConfig::GetLinkTimeOut).stubs().will(returnValue(100));
    Hccl::SocketStatus fakeSocketStatus = Hccl::SocketStatus::TIMEOUT;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(fakeSocketStatus));
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_bufferNumIs0_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = std::make_shared<hcomm::AivUbMemTransport>(fakeSocket, desc);
    CommMemInfo memInfo{};
    void* memHandles[1] = { &memInfo };
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}