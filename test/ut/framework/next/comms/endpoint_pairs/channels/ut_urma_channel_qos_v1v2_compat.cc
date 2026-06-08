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

#include "env_config/env_config.h"
#include "hccl/hccl_types.h"
#include "orion_adpt_utils.h"
#include "tp_manager.h"

#define private public
#include "dev_ub_connection.h"
#include "host_ub_connection.h"
#undef private

#define private public
#define protected public
#include "aicpu/aicpu_ts_urma_channel.h"
#include "host_cpu_urma_channel.h"
#include "aiv_urma_channel.h"
#undef protected
#undef private

using namespace hcomm;
using namespace Hccl;

namespace {

HcclResult StubHrtGetDevice(s32 *deviceLogicId)
{
    if (deviceLogicId != nullptr) {
        *deviceLogicId = 0;
    }
    return HCCL_SUCCESS;
}

HcclResult SetupUbChannelEndpoints(EndpointDesc &localEp, EndpointDesc &remoteEp, CommProtocol protocol)
{
    localEp.protocol = protocol;
    IpAddress localIp("1.1.1.1");
    IpAddress remoteIp("2.2.2.2");
    HcclResult ret = IpAddressToCommAddr(localIp, localEp.commAddr);
    if (ret != HCCL_SUCCESS) {
        return ret;
    }
    return IpAddressToCommAddr(remoteIp, remoteEp.commAddr);
}

void MockBuildConnectionDeps()
{
    MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDevice));
    MOCKER_CPP(&TpManager::Init).stubs().will(ignoreReturnValue());
}

template<typename DevConnT>
void ExpectDevConnQos(const std::vector<std::unique_ptr<DevUbConnection>> &connections, u8 expectedQos)
{
    ASSERT_EQ(connections.size(), 1U);
    auto *conn = dynamic_cast<DevConnT *>(connections[0].get());
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->qos_, expectedQos);
}

void ExpectHostConnQos(const std::vector<std::unique_ptr<HostUbConnection>> &connections, u8 expectedQos)
{
    ASSERT_EQ(connections.size(), 1U);
    EXPECT_EQ(connections[0]->qos_, expectedQos);
}

} // namespace

class UrmaChannelQosV1V2CompatTest : public testing::Test {
protected:
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_AicpuTsUrmaChannel_V1NormalizedQos_UsesUbQosDefault)
{
    HcommChannelDesc desc{};
    desc.qos = HCCL_COMM_QOS_CONFIG_NOT_SET;
    AicpuTsUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_TP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectDevConnQos<DevUbTpConnection>(ch.connections_, static_cast<u8>(Hccl::UB_QOS_DEFAULT));
}

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_AicpuTsUrmaChannel_V2Qos_PreservedOnConnection)
{
    constexpr uint32_t kTestQos = 5U;
    HcommChannelDesc desc{};
    desc.qos = kTestQos;
    AicpuTsUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_TP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectDevConnQos<DevUbTpConnection>(ch.connections_, static_cast<u8>(kTestQos));
}

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_HostCpuUrmaChannel_V1NormalizedQos_UsesUbQosDefault)
{
    HcommChannelDesc desc{};
    desc.qos = HCCL_COMM_QOS_CONFIG_NOT_SET;
    HostCpuUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_CTP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectHostConnQos(ch.connections_, static_cast<u8>(Hccl::UB_QOS_DEFAULT));
}

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_HostCpuUrmaChannel_V2Qos_PreservedOnConnection)
{
    constexpr uint32_t kTestQos = 3U;
    HcommChannelDesc desc{};
    desc.qos = kTestQos;
    HostCpuUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_CTP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectHostConnQos(ch.connections_, static_cast<u8>(kTestQos));
}

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_AivUrmaChannel_V1NormalizedQos_UsesUbQosDefault)
{
    HcommChannelDesc desc{};
    desc.qos = HCCL_COMM_QOS_CONFIG_NOT_SET;
    AivUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_TP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectDevConnQos<DevUbTpConnection>(ch.connections_, static_cast<u8>(Hccl::UB_QOS_DEFAULT));
}

TEST_F(UrmaChannelQosV1V2CompatTest, Ut_AivUrmaChannel_V2Qos_PreservedOnConnection)
{
    constexpr uint32_t kTestQos = 6U;
    HcommChannelDesc desc{};
    desc.qos = kTestQos;
    AivUrmaChannel ch(reinterpret_cast<EndpointHandle>(0x1), desc);
    ASSERT_EQ(SetupUbChannelEndpoints(ch.localEp_, ch.remoteEp_, COMM_PROTOCOL_UBC_TP), HCCL_SUCCESS);
    ch.rdmaHandle_ = reinterpret_cast<RdmaHandle>(0x1000);
    MockBuildConnectionDeps();

    ASSERT_EQ(ch.BuildConnection(), HCCL_SUCCESS);
    ExpectDevConnQos<DevUbTpConnection>(ch.connections_, static_cast<u8>(kTestQos));
}
