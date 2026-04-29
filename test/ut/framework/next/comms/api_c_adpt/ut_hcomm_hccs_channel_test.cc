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
#include "hccl_net_dev.h"
#include "hccl_network.h"
#include "hccl_socket.h"
#include "network_manager_pub.h"
#include "reged_mems/reged_mem_mgr.h"
#include "mem_name_repository_pub.h"
#include "global_net_dev_manager.h"
#include "transport_p2p_pub.h"
#include "transport_device_p2p_pub.h"


using namespace hcomm;
using namespace hccl;

namespace {
static s32 deviceCurLogicId_ = 0;
static s32 deviceCurPhyId_ = 0;

void StubSetDevice(s32 deviceLogicId)
{
    deviceCurLogicId_ = deviceLogicId;
    deviceCurPhyId_ = deviceLogicId;
}

HcclResult StubHrtGetDevice(s32 *deviceLogicId)
{
    if (deviceLogicId != nullptr) {
        *deviceLogicId = deviceCurLogicId_;
    }
    return HCCL_SUCCESS;
}

HcclResult StubHrtGetDevicePhyIdByIndex(u32 deviceLogicId, u32 &devicePhyId, bool isRefresh)
{
    devicePhyId = deviceLogicId;
    return HCCL_SUCCESS;
}

HcclResult StubHrtGetDeviceIndexByPhyId(u32 devicePhyId, u32 &deviceLogicId)
{
    deviceLogicId = devicePhyId;
    return HCCL_SUCCESS;
}

HcclResult StubHcclSocketAcceptForEp(hccl::HcclSocket * /*self*/, const std::string & /*tag*/,
    std::shared_ptr<hccl::HcclSocket> &socket, u32 /*acceptTimeOut*/)
{
    socket = std::make_shared<hccl::HcclSocket>(static_cast<HcclNetDevCtx>(nullptr), 16666);
    return HCCL_SUCCESS;
}

HcclResult StubGetDeviceVnicIP(u32 devicePhyId, u32 superDeviceId, hccl::HcclIpAddress &vnicIP)
{
    std::string ip = "127.0.0." + std::to_string(devicePhyId + 1);
    (void)vnicIP.SetReadableAddress(ip);
    return HCCL_SUCCESS;
}

HcclResult StubHcclNetOpenDev(
    HcclNetDevCtx *netDevCtx, NicType nicType, s32 devicePhyId, s32 deviceLogicId, hccl::HcclIpAddress localIp,
    hccl::HcclIpAddress backupIp)
{
    static hccl::NetDevContext kNetDevCtx[MAX_MODULE_DEVICE_NUM];
    static bool initialized[MAX_MODULE_DEVICE_NUM] = {false};
    if (!initialized[devicePhyId]) {
        hccl::HcclIpAddress localIp;
        std::string ip = "127.0.0." + std::to_string(devicePhyId + 1);
        (void)localIp.SetReadableAddress(ip);
        kNetDevCtx[devicePhyId].Init(NicType::VNIC_TYPE, 0, 0, localIp);
        initialized[devicePhyId] = true;
    }
    *netDevCtx = reinterpret_cast<HcclNetDev>(&kNetDevCtx[devicePhyId]);
    return HCCL_SUCCESS;
}

void StubHcclNetCloseDev(HcclNetDevCtx netDevCtx)
{
}

#define TEST_HCOMM_HCCS_CHANNEL_BUF_LEN (8 * 1024 * 1024)
static u32 socket_data_len;
static u8 socket_data[TEST_HCOMM_HCCS_CHANNEL_BUF_LEN];
HcclResult StubHrtRaSocketBlockSend(const FdHandle fdHandle, const void *data, u64 sendSize, std::function<bool()> needStop)
{
    (void)memcpy_s(socket_data, TEST_HCOMM_HCCS_CHANNEL_BUF_LEN, data, size);
    socket_data_len = size;
    return HCCL_SUCCESS;
}

HcclResult StubHrtRaSocketBlockRecv(const FdHandle fdHandle, void *data, u64 size, std::function<bool()> needStop, u32 timeout)
{
    (void)timeout;
    (void)memcpy_s(data, size, socket_data, socket_data_len);
    socket_data_len = 0;
    return HCCL_SUCCESS;
}

class TestHcommHccsChannel : public TestHcommCAdptBase {
public:
    void SetUp() override {
        TestHcommCAdptBase::SetUp();
        MOCKER(hrtGetDevice).stubs().with(any()).will(invoke(StubHrtGetDevice));
        MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), outBound(0U)).will(invoke(StubHrtGetDevicePhyIdByIndex));
        MOCKER(hrtGetDeviceIndexByPhyId).stubs().with(any(), outBound(0U)).will(invoke(StubHrtGetDeviceIndexByPhyId));
        MOCKER(hrtRaSocketBlockSend).stubs().with(any(), outBound(0U)).will(invoke(StubHrtRaSocketBlockSend));
        MOCKER(hrtRaSocketBlockRecv).stubs().with(any(), outBound(0U)).will(invoke(StubHrtRaSocketBlockRecv));

        MOCKER(HcclNetOpenDev).stubs().will(invoke(StubHcclNetOpenDev));
        MOCKER(HcclNetCloseDev).stubs().will(invoke(StubHcclNetCloseDev));
        MOCKER(&hccl::HcclSocket::Accept).stubs().will(invoke(StubHcclSocketAcceptForEp));

        MOCKER_CPP(&GlobalNetDevMgr::GetDeviceVnicIP).stubs().will(invoke(StubGetDeviceVnicIP));
        MOCKER_CPP(&MemNameRepository::SetIpcMem, HcclResult(MemNameRepository::*)(void *, u64, u8 *, u32)).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&MemNameRepository::FindIpcMem).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&MemNameRepository::OpenIpcMem).stubs().will(returnValue(HCCL_SUCCESS));
        MOCKER_CPP(&MemNameRepository::CloseIpcMem).stubs().will(returnValue(HCCL_SUCCESS));

        socket_data_len = 0;
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
    StubSetDevice(0);
    HcommResult ret = CreateHccsEndpoint(0, 0, endpointDesc, &endpointHandle1);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    void* endpointHandle2{nullptr};
    EndpointDesc endpointDesc2;
    StubSetDevice(1);
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

    ChannelHandle channels[1];
    HcommChannelDesc channelDesc;
    HcommChannelDescInit(&channelDesc, 1);
    channelDesc.remoteEndpoint = endpointDesc2;
    StubSetDevice(0);
    ret = HcommChannelCreate(endpointHandle1, COMM_ENGINE_AICPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);

    ChannelHandle channels2[1];
    HcommChannelDesc channelDesc2;
    HcommChannelDescInit(&channelDesc2, 1);
    channelDesc2.remoteEndpoint = endpointDesc;
    StubSetDevice(1);
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
}