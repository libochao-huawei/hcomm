/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "../../ut_hcomm_base.h"

class TestHcommHccsChannel : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
    }
    void TearDown() override {
        TestHcommCAdptBase::TearDown();
    }
    void SetEndpointDesc(uint32_t id, uint32_t devPhyId, EndpointDesc &endpointDesc)
    {
        endpointDesc.protocol = COMM_PROTOCOL_HCCS;
        endpointDesc.commAddr.type = COMM_ADDR_TYPE_ID;
        endpointDesc.commAddr.id = id;
        endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_DEVICE;
        endpointDesc.loc.device.devPhyId = devPhyId;
        endpointDesc.loc.device.superDevId = static_cast<uint32_t>(-1);
        endpointDesc.loc.device.serverIdx = 0;
        endpointDesc.loc.device.superPodIdx = 0;
    }
    CommMem CreateCommMem(void* addr, size_t size, CommMemType type)
    {
        CommMem mem;
        mem.type = type;
        mem.size = size;
        mem.addr = addr;
        return mem;
    }
    HcommResult CreateHccsEndpoint(uint32_t id, uint32_t devPhyId, EndpointDesc &endpointDesc, void** endpointHandle)
    {
        SetEndpointDesc(id, devPhyId, endpointDesc);
        return HcommEndpointCreate(&endpointDesc, endpointHandle);
    }
};

TEST_F(TestHcommHccsChannel, Ut_TestHcommChannelCreate_When_DescsNullptr_Return_HCCL_E_PTR)
{
    void* endpointHandle1{nullptr};
    EndpointDesc endpointDesc;
    HcommResult ret = CreateHccsEndpoint(0, 0, endpointDesc, &endpointHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* endpointHandle2{nullptr};
    EndpointDesc endpointDesc2;
    ret = CreateHccsEndpoint(1, 1, endpointDesc2, &endpointHandle2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    CommMem mem1 = CreateCommMem((void*)0x01, 10, COMM_MEM_TYPE_DEVICE);
    CommMem mem2 = CreateCommMem((void*)0x02, 20, COMM_MEM_TYPE_DEVICE);
    void *memHandle1, *memHandle2;

    // 在第一个endpoint上注册内存
    ret = HcommMemReg(endpointHandle1, "memTag1", &mem1, &memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    // 在第二个endpoint上注册内存
    ret = HcommMemReg(endpointHandle2, "memTag2", &mem2, &memHandle2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    
    void *memDesc = nullptr;
    uint32_t memDescLen = 0;
    CommMem outMem;

    HcommMemGrantInfo remoteGrantInfo;
    (void)aclrtDeviceGetBareTgid(&remoteGrantInfo.pid);
    remoteGrantInfo.sdid = static_cast<uint32_t>(-1);
    ret = HcommMemGrant(endpointHandle1, &remoteGrantInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommMemExport(endpointHandle1, memHandle1, &memDesc, &memDescLen);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommMemImport(endpointHandle2, memDesc, memDescLen, &outMem);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    MOCKER(HcclSocket::Init)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclSocket::DeInit)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclSocket::Listen)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclSocket::Accept)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(HcclSocket::Connect)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));

    MOCKER(GlobalNetDevMgr::ConnectToServer)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(GlobalNetDevMgr::AcceptClient)
    .stubs()
    .will(returnValue(HCCL_SUCCESS));

    ChannelHandle channels[1];
    HcommChannelDesc channelDesc;
    HcommChannelDescInit(&channelDesc, 1);
    channelDesc.remoteEndpoint = endpointDesc2;
    ret = HcommChannelCreate(endpointHandle1, COMM_ENGINE_AICPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ChannelHandle channels2[1];
    HcommChannelDesc channelDesc2;
    HcommChannelDescInit(&channelDesc2, 1);
    channelDesc2.remoteEndpoint = endpointDesc;
    ret = HcommChannelCreate(endpointHandle2, COMM_ENGINE_AICPU, &channelDesc2, 1, channels2);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommChannelDestroy(channels2, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommChannelDestroy(channels, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ret = HcommMemUnimport(endpointHandle2, memDesc, memDescLen);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    // 正确注销：各自在自己的endpoint上注销自己的内存
    ret = HcommMemUnreg(endpointHandle1, memHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    ret = HcommMemUnreg(endpointHandle2, memHandle2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}