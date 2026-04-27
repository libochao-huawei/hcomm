/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
 
#include "../../hccl_api_base_test.h"
#include "endpoint_pair.h"
using namespace hcomm;
class TestEndpointPair : public BaseInit {
public:
    void SetUp() override {
        BaseInit::SetUp();
    }
    void TearDown() override {
        BaseInit::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestEndpointPair, Ut_EndpointPair_Construct_Expect_HCCL_SUCCESS)
{
    EndpointDesc localEndpointDesc{};
    localEndpointDesc.protocol = COMM_PROTOCOL_ROCE;
    localEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress localIp("192.168.100.100");
    localEndpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    localEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    EndpointDesc remoteEndpointDesc{};
    remoteEndpointDesc.protocol = COMM_PROTOCOL_ROCE;
    remoteEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress remoteIp("192.168.100.101");
    remoteEndpointDesc.commAddr.addr = remoteIp.GetBinaryAddress().addr;
    remoteEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;

    MOCKER_CPP(&SocketMgr::GetSocket).stubs().with(any(), any()).will(returnValue(HCCL_SUCCESS));

    EndpointPair endpointPair(localEndpointDesc, remoteEndpointDesc);
    HcclResult ret = endpointPair.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    Hccl::Socket* socket = nullptr;
    ret = endpointPair.GetSocket(0, 1, "Hccl_Test_Group", 60001, 0, socket, 0, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = endpointPair.GetSocket(1, 0, "Hccl_Test_Group", 60001, 0, socket, 0, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试销毁不存在的channel，返回HCCL_SUCCESS
TEST_F(TestEndpointPair, Ut_DestroyChannel_When_Channel_Not_Exist_Expect_SUCCESS)
{
    MOCKER(HcommChannelDestroy).stubs().will(returnValue(static_cast<int>(HCCL_SUCCESS)));

    EndpointDesc localEndpointDesc{};
    localEndpointDesc.protocol = COMM_PROTOCOL_UBC_CTP;
    localEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress localIp("192.168.100.100");
    localEndpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    localEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointDesc remoteEndpointDesc{};
    remoteEndpointDesc.protocol = COMM_PROTOCOL_UBC_CTP;
    remoteEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress remoteIp("192.168.100.101");
    remoteEndpointDesc.commAddr.addr = remoteIp.GetBinaryAddress().addr;
    remoteEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointPair endpointPair(localEndpointDesc, remoteEndpointDesc);
    HcclResult ret = endpointPair.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(endpointPair.channelHandles_.size(), 0);

    ret = endpointPair.DestroyChannel(COMM_ENGINE_CCU, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// 测试销毁channel，返回HCCL_SUCCESS，预期channelHandles_[COMM_ENGINE_CCU]为空
TEST_F(TestEndpointPair, Ut_DestroyChannel_When_Channel_Exist_Expect_SUCCESS)
{
    MOCKER(HcommChannelDestroy).stubs().will(returnValue(static_cast<int>(HCCL_SUCCESS)));

    EndpointDesc localEndpointDesc{};
    localEndpointDesc.protocol = COMM_PROTOCOL_UBC_CTP;
    localEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress localIp("192.168.100.100");
    localEndpointDesc.commAddr.addr = localIp.GetBinaryAddress().addr;
    localEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointDesc remoteEndpointDesc{};
    remoteEndpointDesc.protocol = COMM_PROTOCOL_UBC_CTP;
    remoteEndpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    Hccl::IpAddress remoteIp("192.168.100.101");
    remoteEndpointDesc.commAddr.addr = remoteIp.GetBinaryAddress().addr;
    remoteEndpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
    EndpointPair endpointPair(localEndpointDesc, remoteEndpointDesc);
    HcclResult ret = endpointPair.Init();
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // channelHandles_[COMM_ENGINE_CCU]中添加三个不同handle
    ChannelHandle fakeChannelHandle = 100;
    endpointPair.channelHandles_[COMM_ENGINE_CCU].push_back(fakeChannelHandle);
    endpointPair.channelHandles_[COMM_ENGINE_CCU].push_back(fakeChannelHandle + 1);
    endpointPair.channelHandles_[COMM_ENGINE_CCU].push_back(fakeChannelHandle + 2);

    // 删除index为0的channel
    ret = endpointPair.DestroyChannel(COMM_ENGINE_CCU, 0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(endpointPair.channelHandles_[COMM_ENGINE_CCU].size(), 2);
    EXPECT_EQ(endpointPair.channelHandles_[COMM_ENGINE_CCU][0], fakeChannelHandle + 1);
    EXPECT_EQ(endpointPair.channelHandles_[COMM_ENGINE_CCU][1], fakeChannelHandle + 2);
}