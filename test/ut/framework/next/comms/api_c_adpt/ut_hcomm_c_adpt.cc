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
#include "hcomm_c_adpt.h"
#include "hcomm_res_defs.h"
#include "channel_process.h"
#include "endpoint_map.h"
#include "env_config/env_config.h"
#include "next/comms/nic_plugin/nic_plugin_manager.h"
#include "orion_adpt_utils.h"
#include "hccp_peer_manager.h"
#include "rdma_handle_manager.h"
#include "hccp_nda.h"
#include "adapter_rts_common.h"

using namespace hcomm;

namespace {
struct FakePluginChannelState {
    uint32_t getStatusCalls = 0;
};

struct FakePluginScenario {
    uint32_t readyAfterCalls = 1;
    HcommResult getStatusRet = HCCL_SUCCESS;
    uint32_t createCalls = 0;
    uint32_t destroyCalls = 0;
};

FakePluginScenario g_fakePluginScenario;

int32_t FakePluginCreateChannel(void *epCtx, const void *channelDescRaw, uint32_t chDescLen,
    void **outCtx, HcommNicChannelOps **outOps);
int32_t FakePluginGetStatus(void *ctx, int32_t *status);
void FakePluginDestroyChannel(void *ctx);

HcommNicChannelOps g_fakeChannelOps = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    FakePluginDestroyChannel,
    FakePluginGetStatus,
    nullptr,
    nullptr
};

HcommNicPluginInfo g_fakePluginInfo = {
    HCOMM_NIC_PLUGIN_ABI_VERSION,
    "fake_plugin",
    1,
    {HCOMM_NIC_PROTO_ROCE}
};

NicPluginEntry g_fakePluginEntry = {
    nullptr,
    &g_fakePluginInfo,
    nullptr,
    FakePluginCreateChannel
};

HcommNicEndpointOps g_fakeEndpointOps = {};

int32_t FakePluginCreateChannel(void *epCtx, const void *channelDescRaw, uint32_t chDescLen,
    void **outCtx, HcommNicChannelOps **outOps)
{
    (void)epCtx;
    (void)channelDescRaw;
    (void)chDescLen;
    g_fakePluginScenario.createCalls++;
    *outCtx = new FakePluginChannelState();
    *outOps = &g_fakeChannelOps;
    return HCCL_SUCCESS;
}

int32_t FakePluginGetStatus(void *ctx, int32_t *status)
{
    if (g_fakePluginScenario.getStatusRet != HCCL_SUCCESS) {
        return g_fakePluginScenario.getStatusRet;
    }
    auto *state = static_cast<FakePluginChannelState *>(ctx);
    state->getStatusCalls++;
    *status = state->getStatusCalls >= g_fakePluginScenario.readyAfterCalls ? 0 : 1;
    return HCCL_SUCCESS;
}

void FakePluginDestroyChannel(void *ctx)
{
    g_fakePluginScenario.destroyCalls++;
    delete static_cast<FakePluginChannelState *>(ctx);
}

EndpointHandle MakeFakePluginEndpointHandle()
{
    static int endpointCtx = 0;
    static PluginEndpointCtx pluginEndpoint = {
        &g_fakeEndpointOps,
        &endpointCtx,
        &g_fakePluginEntry
    };
    return MAKE_PLUGIN_EP_HANDLE(&pluginEndpoint);
}

void ResetFakePluginScenario()
{
    g_fakePluginScenario = {};
    g_fakePluginScenario.readyAfterCalls = 1;
    g_fakePluginScenario.getStatusRet = HCCL_SUCCESS;
}
} // namespace

HcclResult StubServerSocketGetListenPort(Endpoint* /*endpoint*/, uint32_t* port)
{
    *port = 12345;
    return HCCL_SUCCESS;
}

class HcommCAdptTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "HcommCAdptTest tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "HcommCAdptTest tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in HcommCAdptTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in HcommCAdptTest TearDown" << std::endl;
    }
};

TEST_F(HcommCAdptTest, ut_HcommChannelGet_When_Normal_Expect_Success)
{
    ChannelHandle channelHandle = 0x12345;
    void* channel = nullptr;
    MOCKER(ChannelProcess::ChannelGet)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelGet(channelHandle, &channel);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_Normal_Expect_Success)
{
    ChannelHandle channelList[2] = {
        0x12345,
        0x12346
    };
    int32_t statusList[2] = {0, 0};
    HcommResult ret = HcommChannelGetStatus(channelList, 2, statusList);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(statusList[0], 0);
    EXPECT_EQ(statusList[1], 0);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_ChannelListNull_Expect_E_PTR)
{
    int32_t statusList[2] = {0, 0};
    HcommResult ret = HcommChannelGetStatus(nullptr, 2, statusList);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_StatusListNull_Expect_E_PTR)
{
    ChannelHandle channelList[2] = {
        0x12345,
        0x12346
    };
    HcommResult ret = HcommChannelGetStatus(channelList, 2, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetStatus_When_ListNumZero_Expect_E_PARA)
{
    ChannelHandle channelList[2] = {
        0x12345,
        0x12346
    };
    int32_t statusList[2] = {0, 0};
    HcommResult ret = HcommChannelGetStatus(channelList, 0, statusList);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetNotifyNum_When_Normal_Expect_Success)
{
    ChannelHandle channelHandle = 0x12345;
    uint32_t notifyNum = 0;
    MOCKER(ChannelProcess::ChannelGetNotifyNum)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelGetNotifyNum(channelHandle, &notifyNum);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDestroy_When_Normal_Expect_Success)
{
    ChannelHandle channels[2] = {
        0x12345,
        0x12346
    };
    MOCKER(ChannelProcess::ChannelDestroy)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelDestroy(channels, 2);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetRemoteMem_When_Normal_Expect_Success)
{
    ChannelHandle channelHandle = 0x12345;
    CommMem* remoteMem = nullptr;
    uint32_t memNum = 0;
    char* memTagsStorage[2] = {nullptr, nullptr};
    char** memTags = memTagsStorage;
    MOCKER(ChannelProcess::ChannelGetRemoteMem)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelGetRemoteMem(channelHandle, &remoteMem, &memNum, memTags);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelGetRemoteMems_When_Normal_Expect_Success)
{
    ChannelHandle channelHandle = 0x12345;
    CommMem* remoteMems = nullptr;
    char** memTags = nullptr;
    uint32_t memNum = 0;
    MOCKER(ChannelProcess::ChannelGetUserRemoteMem)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelGetRemoteMems(channelHandle, &memNum, &remoteMems, &memTags);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_When_Normal_Expect_Success)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER(ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_When_ChannelDescsNull_Expect_E_PTR)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, nullptr, 1, channels);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_When_ChannelNumZero_Expect_E_PARA)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, &channelDesc, 0, channels);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_ChannelDescsNull_Expect_E_PTR)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, nullptr, 1, channels);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_ChannelNumZero_Expect_E_PARA)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU_TS, &channelDesc, 0, channels);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGet_When_NotFound_Expect_E_NOT_FOUND)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    void* endpoint = nullptr;
    HcommResult ret = HcommEndpointGet(handle, &endpoint);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGet_When_EndpointPtrNull_Expect_E_PTR)
{
    EndpointHandle handle = reinterpret_cast<EndpointHandle>(0x12345678);
    HcommResult ret = HcommEndpointGet(handle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_When_NotAiCpu_Expect_Success)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER(ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(ChannelProcess::ConnectChannels)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(ChannelProcess::SaveChannels)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_Plugin_When_StatusReadyLater_Expect_BlockUntilReady)
{
    ResetFakePluginScenario();
    g_fakePluginScenario.readyAfterCalls = 3;
    EndpointHandle endpointHandle = MakeFakePluginEndpointHandle();
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};

    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);

    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(g_fakePluginScenario.createCalls, 1);
    EXPECT_TRUE(IS_PLUGIN_HANDLE(channels[0]));
    auto *pluginChannel = PLUGIN_CH_CTX(channels[0]);
    auto *state = static_cast<FakePluginChannelState *>(pluginChannel->ctx);
    EXPECT_GE(state->getStatusCalls, 3U);
    EXPECT_EQ(g_fakePluginScenario.destroyCalls, 0);
    EXPECT_EQ(HcommChannelDestroy(channels, 1), HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_Plugin_When_GetStatusError_Expect_DestroyAndReturnError)
{
    ResetFakePluginScenario();
    g_fakePluginScenario.getStatusRet = HCCL_E_PARA;
    EndpointHandle endpointHandle = MakeFakePluginEndpointHandle();
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};

    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);

    EXPECT_EQ(ret, HCCL_E_PARA);
    EXPECT_EQ(g_fakePluginScenario.createCalls, 1);
    EXPECT_EQ(g_fakePluginScenario.destroyCalls, 1);
    EXPECT_EQ(channels[0], 0);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_Plugin_When_Timeout_Expect_DestroyAndReturnTimeout)
{
    ResetFakePluginScenario();
    g_fakePluginScenario.readyAfterCalls = 0xFFFFFFFF;
    EndpointHandle endpointHandle = MakeFakePluginEndpointHandle();
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER_CPP(&Hccl::EnvSocketConfig::GetLinkTimeOut)
        .stubs()
        .with(any())
        .will(returnValue((s32)(0)));

    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);

    EXPECT_EQ(ret, HCCL_E_TIMEOUT);
    EXPECT_EQ(g_fakePluginScenario.createCalls, 1);
    EXPECT_EQ(g_fakePluginScenario.destroyCalls, 1);
    EXPECT_EQ(channels[0], 0);
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_Plugin_When_EngineNotCpu_Expect_NotSupport)
{
    ResetFakePluginScenario();
    EndpointHandle endpointHandle = MakeFakePluginEndpointHandle();
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};

    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU, &channelDesc, 1, channels);

    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    EXPECT_EQ(g_fakePluginScenario.createCalls, 0);
    EXPECT_EQ(g_fakePluginScenario.destroyCalls, 0);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CPU_Expect_Success)
{
    void* dstCtx = malloc(1024);
    void* srcCtx = malloc(1024);
    ASSERT_NE(dstCtx, nullptr);
    ASSERT_NE(srcCtx, nullptr);
    memset(dstCtx, 0, 1024);
    memset(srcCtx, 1, 1024);
    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CPU, dstCtx, srcCtx, 1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    free(dstCtx);
    free(srcCtx);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CPU_TS_Expect_Success)
{
    void* dstCtx = malloc(1024);
    void* srcCtx = malloc(1024);
    ASSERT_NE(dstCtx, nullptr);
    ASSERT_NE(srcCtx, nullptr);
    memset(dstCtx, 0, 1024);
    memset(srcCtx, 1, 1024);
    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CPU_TS, dstCtx, srcCtx, 1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    free(dstCtx);
    free(srcCtx);
}

TEST_F(HcommCAdptTest, ut_HcommEngineCtxCopy_When_CCU_Expect_Success)
{
    void* dstCtx = malloc(1024);
    void* srcCtx = malloc(1024);
    ASSERT_NE(dstCtx, nullptr);
    ASSERT_NE(srcCtx, nullptr);
    memset(dstCtx, 0, 1024);
    memset(srcCtx, 1, 1024);
    HcommResult ret = HcommEngineCtxCopy(COMM_ENGINE_CCU, dstCtx, srcCtx, 1024);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    free(dstCtx);
    free(srcCtx);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDescInit_When_Normal_Expect_Success)
{
    HcommChannelDesc channelDesc{};
    HcommResult ret = HcommChannelDescInit(&channelDesc, 1);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommChannelDescInit_When_Normal_Expect_Version2AndFullSize)
{
    HcommChannelDesc channelDesc{};
    ASSERT_EQ(HcommChannelDescInit(&channelDesc, 1), HCCL_SUCCESS);
    EXPECT_EQ(channelDesc.header.version, HCOMM_CHANNEL_VERSION);
    EXPECT_EQ(channelDesc.header.size, sizeof(HcommChannelDesc));
    EXPECT_GE(sizeof(HcommChannelDesc), HCOMM_CHANNEL_DESC_ABI_V1_SIZE + sizeof(uint32_t));
}

TEST_F(HcommCAdptTest, ut_HcommChannelCreate_AICPU_Expect_LoadKernel)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER(ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(ChannelProcess::ConnectChannels)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    MOCKER(ChannelProcess::SaveChannels)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommChannelCreate(endpointHandle, COMM_ENGINE_AICPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_CPU_Expect_Success)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER(ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_CCU_Expect_Success)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    (void)HcommChannelDescInit(&channelDesc, 1);
    ChannelHandle channels[1] = {0};
    MOCKER(ChannelProcess::CreateChannelsLoop)
        .stubs()
        .will(returnValue(HCCL_SUCCESS));
    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_CCU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

namespace {

uint32_t gCapturedChannelDescQos = 0U;

HcclResult CaptureCreateChannelsLoop(EndpointHandle, CommEngine, HcommChannelDesc *channelDescs, uint32_t channelNum,
    ChannelHandle *)
{
    if (channelDescs != nullptr && channelNum > 0U) {
        gCapturedChannelDescQos = channelDescs[0].qos;
    }
    return HCCL_SUCCESS;
}

} // namespace

TEST_F(HcommCAdptTest, ut_HcommCollectiveChannelCreate_V1Desc_ClearsQosField)
{
    gCapturedChannelDescQos = 0U;
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommChannelDesc channelDesc{};
    ASSERT_EQ(HcommChannelDescInit(&channelDesc, 1), HCCL_SUCCESS);
    channelDesc.header.version = HCOMM_CHANNEL_VERSION_ONE;
    channelDesc.header.size = HCOMM_CHANNEL_DESC_ABI_V1_SIZE;
    channelDesc.qos = 5U;
    ChannelHandle channels[1] = {0};

    MOCKER(ChannelProcess::CreateChannelsLoop).stubs().will(invoke(CaptureCreateChannelsLoop));

    HcommResult ret = HcommCollectiveChannelCreate(endpointHandle, COMM_ENGINE_CPU, &channelDesc, 1, channels);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(gCapturedChannelDescQos, 0xFFFFFFFFU);
}

TEST_F(HcommCAdptTest, ut_HcommResMgrInit_When_Normal_Expect_Success)
{
    HcommResult ret = HcommResMgrInit(0);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommResMgrInit_MultiDevice_Expect_Success)
{
    HcommResult ret1 = HcommResMgrInit(0);
    HcommResult ret2 = HcommResMgrInit(1);
    EXPECT_EQ(ret1, HCCL_SUCCESS);
    EXPECT_EQ(ret2, HCCL_SUCCESS);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGetListenPort_When_PortNull_Expect_E_PTR)
{
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    HcommResult ret = HcommEndpointGetListenPort(endpointHandle, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointGetListenPort_When_HandleInvalid_Expect_E_NOT_FOUND)
{
    uint32_t port = 0;
    HcommResult ret = HcommEndpointGetListenPort(nullptr, &port);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}


class StubEndpoint final : public Endpoint {
public:
    explicit StubEndpoint() : Endpoint(EndpointDesc{}) {}
    HcclResult Init() override { return HCCL_SUCCESS; }
    HcclResult ServerSocketListen(const uint32_t port) override { return HCCL_SUCCESS; }
    HcclResult RegisterMemory(HcommMem mem, const char *memTag, void **memHandle) override { return HCCL_SUCCESS; }
    HcclResult UnregisterMemory(void* memHandle) override { return HCCL_SUCCESS; }
    HcclResult MemoryExport(void *memHandle, void **memDesc, uint32_t *memDescLen) override { return HCCL_SUCCESS; }
    HcclResult MemoryImport(const void *memDesc, uint32_t descLen, HcommMem *outMem) override { return HCCL_SUCCESS; }
    HcclResult MemoryUnimport(const void *memDesc, uint32_t descLen) override { return HCCL_SUCCESS; }
    HcclResult GetAllMemHandles(void **memHandles, uint32_t *memHandleNum) override { return HCCL_SUCCESS; }
};

TEST_F(HcommCAdptTest, ut_HcommEndpointGetListenPort_When_ServerSocketNotSupport_Expect_E_NOT_SUPPORT)
{
    uint32_t port = 0;
    EndpointHandle endpointHandle = reinterpret_cast<EndpointHandle>(0x12345);
    StubEndpoint stubEndpoint;

    MOCKER_CPP(&HcommEndpointMap::GetEndpoint, Endpoint*(HcommEndpointMap::*)(EndpointHandle))
        .stubs()
        .will(returnValue(static_cast<Endpoint*>(&stubEndpoint)));

    HcommResult ret = HcommEndpointGetListenPort(endpointHandle, &port);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
}

TEST_F(HcommCAdptTest, ut_HcommEndpointCheckFeature_When_SupportedFeature_Expect_True)
{
    EndpointDesc endpointDesc{};
    (void)memset_s(&endpointDesc, sizeof(endpointDesc), 0, sizeof(endpointDesc));
    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;

    MOCKER(hcomm::CommAddrToIpAddress).stubs().will(returnValue(HCCL_SUCCESS));

    s32 devId = 0;
    MOCKER(hrtGetDevice).stubs().with(outBoundP(&devId)).will(returnValue(HCCL_SUCCESS));

    MOCKER_CPP(&Hccl::HccpPeerManager::Init).stubs().with(any()).will(ignoreReturnValue());

    u32 devPhyId = 0;
    MOCKER(hrtGetDevicePhyIdByIndex).stubs().with(any(), outBound(devPhyId)).will(returnValue(HCCL_SUCCESS));

    void *fakeRdmaHandle = reinterpret_cast<void *>(0x12345678);
    MOCKER_CPP(&Hccl::RdmaHandleManager::GetByAddr).stubs().will(returnValue(fakeRdmaHandle));

    s32 directFlag = DIRECT_FLAG_PCIE;
    MOCKER(RaNdaGetDirectFlag).stubs().with(any(), outBoundP(&directFlag)).will(returnValue(0));

    bool value = false;
    HcommResult ret = HcommEndpointCheckFeature(HCOMM_ENDPOINT_FEATURE_NDA, &endpointDesc, &value);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(value, true);
}
