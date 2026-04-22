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

    std::shared_ptr<LocalRmaBufferStub> CreateLocalRmaBuffer(u64 addr, u64 size, u32 pid)
    {
        auto buffer = std::make_shared<Hccl::Buffer>(addr, size);
        return std::make_shared<LocalRmaBufferStub>(buffer, addr, size, 0, pid);
    }

    std::array<char, HCCL_RES_TAG_MAX_LEN> BuildMemTagArray(const std::string &tag)
    {
        std::array<char, HCCL_RES_TAG_MAX_LEN> memTag{};
        memcpy_s(memTag.data(), memTag.size(), tag.c_str(), tag.size());
        return memTag;
    }

    CommMemInfo BuildMemInfo(void *bufferHandle, const std::string &tag)
    {
        CommMemInfo memInfo{};
        memInfo.bufferHandle = bufferHandle;
        memcpy_s(memInfo.memTag, sizeof(memInfo.memTag), tag.c_str(), tag.size());
        return memInfo;
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
    auto initMockBuffer = CreateLocalRmaBuffer(0x100, 0x100, 100);
    aivTransport->localRmaBufferVec_.push_back(reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(initMockBuffer.get()));
    aivTransport->localUserMemTag_.push_back(BuildMemTagArray("initBuffer"));

    size_t initialVecSize = aivTransport->localRmaBufferVec_.size();
    size_t initialTagSize = aivTransport->localUserMemTag_.size();

    auto mockBuffer1 = CreateLocalRmaBuffer(0x1000, 0x1000, 1000);
    auto mockBuffer2 = CreateLocalRmaBuffer(0x2000, 0x2000, 2000);
    auto memInfo1 = BuildMemInfo(reinterpret_cast<void*>(mockBuffer1.get()), "newBuffer1");
    auto memInfo2 = BuildMemInfo(reinterpret_cast<void*>(mockBuffer2.get()), "newBuffer2");
    void* memHandles[2] = { &memInfo1, &memInfo2 };

    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    EXPECT_EQ(aivTransport->localRmaBufferVec_.size(), initialVecSize + 2);
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize],
        reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(mockBuffer1.get()));
    EXPECT_EQ(aivTransport->localRmaBufferVec_[initialVecSize + 1],
        reinterpret_cast<Hccl::LocalIpcRmaBuffer*>(mockBuffer2.get()));
    EXPECT_EQ(aivTransport->localUserMemTag_.size(), initialTagSize + 2);
    EXPECT_EQ(std::string(aivTransport->localUserMemTag_[initialTagSize].data()), "newBuffer1");
    EXPECT_EQ(std::string(aivTransport->localUserMemTag_[initialTagSize + 1].data()), "newBuffer2");
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_SocketTimeout_Expect_ReturnIsHCCL_E_TIMEOUT)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);
    auto mockBuffer = CreateLocalRmaBuffer(0x1000, 0x1000, 1000);
    auto memInfo = BuildMemInfo(reinterpret_cast<void*>(mockBuffer.get()), "testBuffer");
    void* memHandles[1] = { &memInfo };

    Hccl::SocketStatus fakeSocketStatus = Hccl::SocketStatus::TIMEOUT;
    MOCKER(&Hccl::Socket::GetAsyncStatus).stubs().will(returnValue(fakeSocketStatus));
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 1);
    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
}

TEST_F(AivUbMemTransportTest, ut_AivUbMemTransport_UpdateMemInfo_When_bufferNumIs0_Expect_ReturnIsHCCL_SUCCESS)
{
    HcommChannelDesc desc{};
    auto aivTransport = CreateAivTransport(desc);
    CommMemInfo memInfo{};
    void* memHandles[1] = { &memInfo };
    HcclResult ret = aivTransport->UpdateMemInfo(memHandles, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}