/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <arpa/inet.h>

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "aicpu/aicpu_ts_roce_channel.h"
#include "aicpu/aicpu_ts_urma_channel.h"
#include "channel.h"
#include "host/host_cpu_roce_channel.h"
#include "hcomm_res_defs.h"
#include "next/comms/endpoints/endpoint.h"

using namespace hcomm;

namespace {
class StubEndpointForChannelFactory : public Endpoint {
public:
    explicit StubEndpointForChannelFactory(const EndpointDesc &desc, void *rdma = reinterpret_cast<void *>(0x10U))
        : Endpoint(desc)
    {
        ctxHandle_ = rdma;
    }
    HcclResult Init() override
    {
        return HCCL_SUCCESS;
    }
    HcclResult ServerSocketListen(const uint32_t) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult RegisterMemory(HcommMem, const char *, void **) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult UnregisterMemory(void *) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult MemoryExport(void *, void **, uint32_t *) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult MemoryImport(const void *, uint32_t, HcommMem *) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult MemoryUnimport(const void *, uint32_t) override
    {
        return HCCL_SUCCESS;
    }
    HcclResult GetAllMemHandles(void **, uint32_t *) override
    {
        return HCCL_SUCCESS;
    }
};
} // namespace

class UtChannelRoceFactory : public testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(UtChannelRoceFactory, CreateChannel_NullEndpoint_AicpuTs_Roce_Returns_E_PTR)
{
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(nullptr, COMM_ENGINE_AICPU_TS, desc, ch), HCCL_E_PTR);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_NullEndpoint_Aicpu_Roce_Returns_E_PTR)
{
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(nullptr, COMM_ENGINE_AICPU, desc, ch), HCCL_E_PTR);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_InvalidEngine_Returns_NOT_FOUND)
{
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    ep.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "10.20.30.40", &ep.commAddr.addr), 1);
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), static_cast<CommEngine>(0x7fff0001), desc, ch),
        HCCL_E_NOT_FOUND);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_CpuTs_Returns_NOT_SUPPORT)
{
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), COMM_ENGINE_CPU_TS, desc, ch),
        HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_CpuNonRoce_Returns_NOT_SUPPORT)
{
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_UB_MEM;
    std::unique_ptr<Channel> ch;
    EXPECT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), COMM_ENGINE_CPU, desc, ch),
        HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_CpuRoce_MockInit_Returns_SUCCESS)
{
    MOCKER_CPP(&HostCpuRoceChannel::Init).stubs().will(returnValue(HCCL_SUCCESS));
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    ep.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "10.20.30.41", &ep.commAddr.addr), 1);
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    ASSERT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), COMM_ENGINE_CPU, desc, ch), HCCL_SUCCESS);
    ASSERT_NE(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_AicpuTsRoce_MockInit_Returns_SUCCESS)
{
    MOCKER_CPP(&AicpuTsRoceChannel::Init).stubs().will(returnValue(HCCL_SUCCESS));
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    ep.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    ASSERT_EQ(inet_pton(AF_INET, "10.20.30.42", &ep.commAddr.addr), 1);
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_ROCE;
    std::unique_ptr<Channel> ch;
    ASSERT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), COMM_ENGINE_AICPU_TS, desc, ch), HCCL_SUCCESS);
    ASSERT_NE(ch.get(), nullptr);
}

TEST_F(UtChannelRoceFactory, CreateChannel_AicpuNonRoce_MockUrmaInit_Returns_SUCCESS)
{
    MOCKER_CPP(&AicpuTsUrmaChannel::Init).stubs().will(returnValue(HCCL_SUCCESS));
    EndpointDesc ep{};
    ep.protocol = COMM_PROTOCOL_ROCE;
    ep.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    StubEndpointForChannelFactory stub(ep);
    HcommChannelDesc desc{};
    desc.remoteEndpoint.protocol = COMM_PROTOCOL_UB_MEM;
    std::unique_ptr<Channel> ch;
    ASSERT_EQ(Channel::CreateChannel(reinterpret_cast<EndpointHandle>(&stub), COMM_ENGINE_AICPU, desc, ch), HCCL_SUCCESS);
    ASSERT_NE(ch.get(), nullptr);
}
