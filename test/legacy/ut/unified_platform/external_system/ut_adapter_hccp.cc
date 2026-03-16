/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "hccp.h"
#include "hccp_ctx.h"
#include "hccp_common.h"
#include "orion_adapter_hccp.h"
#include "network_api_exception.h"
#include "internal_exception.h"
#include "hccp_async.h"
#include "ub_memory_transport.h"
using namespace Hccl;

class AdapterHccpTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AdapterHccp tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AdapterHccp tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AdapterHccp SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AdapterHccp TearDown" << std::endl;
    }

    FdHandle fakeFdHandle = nullptr;
    void *fakeData = (void *)0x100;
};

TEST_F(AdapterHccpTest, RaTlvInit_ok)
{
    // Given
    MOCKER(RaTlvInit).stubs().will(returnValue(0));
    HRaTlvInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when
    HrtRaTlvInit(config);
    // then
}

TEST_F(AdapterHccpTest, RaTlvInit_nok)
{
    // Given
    MOCKER(RaTlvInit).stubs().will(returnValue(1));
    HRaTlvInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when

    // then
    EXPECT_THROW(HrtRaTlvInit(config), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaTlvRequest_ok)
{
    // Given
    MOCKER(RaTlvRequest).stubs().will(returnValue(0));
    void* tlv_handle;
    // when
    HrtRaTlvRequest(tlv_handle, 1, 1);
    // then
}

TEST_F(AdapterHccpTest, RaTlvRequest_nok)
{
    // Given
    MOCKER(RaTlvRequest).stubs().will(returnValue(1));
    void* tlv_handle;

    // when

    // then
    EXPECT_THROW(HrtRaTlvRequest(tlv_handle, 1, 1), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaTlvDeInit_ok)
{
    // Given
    MOCKER(RaTlvDeinit).stubs().will(returnValue(0));
    void* tlv_handle;

    // when
    HrtRaTlvDeInit(tlv_handle);
    // then
}

TEST_F(AdapterHccpTest, RaTlvDeInit_nok)
{
    // Given
    MOCKER(RaTlvDeinit).stubs().will(returnValue(1));
    void* tlv_handle;

    // when
    
    // then
    EXPECT_THROW(HrtRaTlvDeInit(tlv_handle), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaInit_ok)
{
    // Given
    MOCKER(RaInit).stubs().will(returnValue(0));
    HRaInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when
    HrtRaInit(config);
    // then
}

TEST_F(AdapterHccpTest, HrtRaInit_nok_no_limit_rate)
{
    // Given
    MOCKER(RaInit).stubs().will(returnValue(SOCK_ESOCKCLOSED));
    HRaInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when

    // then
    EXPECT_THROW(HrtRaInit(config), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaDeInit_ok)
{
    // Given
    MOCKER(RaDeinit).stubs().will(returnValue(0));
    HRaInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when
    HrtRaInit(config);
    // then
}

TEST_F(AdapterHccpTest, HrtRaDeInit_nok_no_limit_rate)
{
    // Given
    MOCKER(RaDeinit).stubs().will(returnValue(SOCK_ESOCKCLOSED));
    HRaInitConfig config;
    config.mode = HrtNetworkMode::HDC;
    config.phyId = 0;

    // when

    // then
    EXPECT_THROW(HrtRaDeInit(config), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaSocketWhiteListAdd_nok)
{
    // Given
    MOCKER(RaSocketWhiteListAdd).stubs().will(returnValue(-1));
    vector<RaSocketWhitelist> whiteList(1);
    // when

    // then
    EXPECT_THROW(HrtRaSocketWhiteListAdd(nullptr, whiteList), NullPtrException);
}

TEST_F(AdapterHccpTest, HrtRaSocketWhiteListAdd_ok)
{
    // Given
    MOCKER(RaSocketWhiteListAdd).stubs().will(returnValue(0));
    vector<RaSocketWhitelist> whiteList(1);
    SocketHandle validSocketHandle = reinterpret_cast<SocketHandle>(0x123);
    // when

    // then
    EXPECT_NO_THROW(HrtRaSocketWhiteListAdd(validSocketHandle, whiteList));
}

TEST_F(AdapterHccpTest, HrtRaSocketWhiteListAdd_strcpy_nok)
{
    // Given
    MOCKER(strcpy_s).stubs().will(returnValue(-1));
    vector<RaSocketWhitelist> whiteList(1);
    // when

    // then
    EXPECT_THROW(HrtRaSocketWhiteListAdd(nullptr, whiteList), NullPtrException);
}

TEST_F(AdapterHccpTest, HrtRaSocketWhiteListDel_nok)
{
    // Given
    MOCKER(RaSocketWhiteListDel).stubs().will(returnValue(-1));
    vector<RaSocketWhitelist> whiteList(1);
    // when

    // then
    EXPECT_THROW(HrtRaSocketWhiteListDel(nullptr, whiteList), NullPtrException);
}

TEST_F(AdapterHccpTest, hrtRaSocketListenOneStart_again)
{
    // Given
    MOCKER(RaSocketListenStart).stubs().will(returnValue(SOCK_EAGAIN));

    // 超时时间设置为1
    MOCKER(EnvLinkTimeoutGet).stubs().will(returnValue(1));

    SocketHandle socketHandle = nullptr;


    RaSocketListenParam listenInfo(socketHandle, 0);
    // when

    // then
    EXPECT_THROW(HrtRaSocketListenOneStart(listenInfo), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaSocketInit_OK)
{
    // Given
    u32         *num          = new u32[2];
    SocketHandle socketHandle = static_cast<void *>(num);
    MOCKER(RaSocketInit)
        .stubs()
        .with(any(), any(), outBoundP(&socketHandle, sizeof(socketHandle)))
        .will(returnValue(0));

    struct RaInterface rdevInfo;
    // when

    // then
    HrtRaSocketInit(HrtNetworkMode::HDC, rdevInfo);
    delete[] num;
}

TEST_F(AdapterHccpTest, HrtRaSocketInit_NOK)
{
    // Given
    u32         *num          = new u32[2];
    SocketHandle socketHandle = static_cast<void *>(num);
    MOCKER(RaSocketInit)
        .stubs()
        .with(any(), any(), outBoundP(&socketHandle, sizeof(socketHandle)))
        .will(returnValue(1));

    struct RaInterface rdevInfo;
    // when

    EXPECT_THROW(HrtRaSocketInit(HrtNetworkMode::HDC, rdevInfo), NetworkApiException);
    delete[] num;
}

TEST_F(AdapterHccpTest, HrtHrtRaRdmaInit_NOK)
{
    // Given
    u32       *num        = new u32[1];
    RdmaHandle rdmaHandle = static_cast<void *>(num);
    MOCKER(RaRdevInit)
        .stubs()
        .with(any(), any(), any(), outBoundP(&rdmaHandle, sizeof(rdmaHandle)))
        .will(returnValue(1));

    struct RaInterface rdevInfo;
    // when

    EXPECT_THROW(HrtRaRdmaInit(HrtNetworkMode::HDC, rdevInfo), NetworkApiException);
    delete[] num;
}

TEST_F(AdapterHccpTest, HrtRaQpCreate_NOK)
{
    // Given
    u32     *num        = new u32[1];
    QpHandle connHandle = static_cast<void *>(num);
    MOCKER(RaQpCreate)
        .stubs()
        .with(any(), any(), any(), outBoundP(&connHandle, sizeof(connHandle)))
        .will(returnValue(1));

    RdmaHandle rdmaHandle;
    // when

    EXPECT_THROW(HrtRaQpCreate(rdmaHandle, 0, 0), NetworkApiException);
    delete[] num;
}

TEST_F(AdapterHccpTest, HrtGetRaQpStatus_NOK)
{
    // Given
    s32 status = 1;
    MOCKER(RaGetQpStatus).stubs().with(any(), outBoundP(&status, sizeof(status))).will(returnValue(1));

    QpHandle qpHandle;
    // when

    EXPECT_THROW(HrtGetRaQpStatus(qpHandle), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaMrReg_deReg_NOK)
{
    // Given
    MOCKER(RaMrReg).stubs().with(any(), any()).will(returnValue(1));

    QpHandle       qpHandle;
    struct RaMrInfo mrInfo;
    // when

    EXPECT_THROW(HrtRaMrReg(qpHandle, mrInfo), NetworkApiException);

    MOCKER(RaMrDereg).stubs().with(any(), any()).will(returnValue(1));
    EXPECT_THROW(HrtRaMrDereg(qpHandle, mrInfo), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaUbCtxInit_ok)
{
    // Given

    IpAddress addr(1);

    HrtRaUbCtxInitParam initParam(HrtNetworkMode::HDC, 0, addr);

    // when

    RdmaHandle rdmaHandle = HrtRaUbCtxInit(initParam);
}

TEST_F(AdapterHccpTest, HrtRaUbCtxDestroy_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbCtxDestroy(handle);
}

TEST_F(AdapterHccpTest, HrtRaUbLocalMemReg_ok)
{
    uint64_t fakeAddr       = 0;
    uint64_t fakeSize       = 0;
    uint32_t fakeTokenValue = 0;
    uint64_t fakeTokenIdHandle = 0;

    HrtRaUbLocMemRegParam inParam(fakeAddr, fakeSize, fakeTokenValue, fakeTokenIdHandle);

    RdmaHandle fakeRdmaHandle = reinterpret_cast<RdmaHandle>(0x123);

    HrtRaUbLocalMemRegOutParam result = HrtRaUbLocalMemReg(fakeRdmaHandle, inParam);

    EXPECT_EQ(0, result.tokenId);
}

TEST_F(AdapterHccpTest, Ut_RaUbLocalMemRegAsync_When_Normal_Input_Expect_No_Throw)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    uint64_t fakeAddr       = 0;
    uint64_t fakeSize       = 0;
    uint32_t fakeTokenValue = 0;
    uint64_t fakeTokenIdHandle = 0;
    HrtRaUbLocMemRegParam inParam(fakeAddr, fakeSize, fakeTokenValue, fakeTokenIdHandle);
    u64 fakeSegVa = 0x200;
    u8 fakeKey[HRT_UB_MEM_KEY_MAX_LEN]{0};
    u32 fakeTokenId = 1;
    u64 fakeMemHandle = 0x200;
    void* fakeMemHandlePtr = reinterpret_cast<void*>(fakeMemHandle);
    RequestHandle fakeReqHandle = 1;

    vector<char_t> out;
    out.resize(sizeof(struct MrRegInfoT));
    struct MrRegInfoT *info = reinterpret_cast<struct MrRegInfoT *>(out.data());
    memcpy_s(info->out.key.value, HRT_UB_MEM_KEY_MAX_LEN, fakeKey, HRT_UB_MEM_KEY_MAX_LEN);
    info->out.key.size = 4;
    info->out.ub.tokenId = fakeTokenId;
    info->out.ub.targetSegHandle = fakeSegVa;

    EXPECT_NO_THROW(RaUbLocalMemRegAsync(handle, inParam, out, fakeMemHandlePtr));
}

TEST_F(AdapterHccpTest, HrtRaUbLocalMemUnreg_ok)
{
    RdmaHandle fakeRdmaHandle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbLocalMemUnreg(fakeRdmaHandle, 0);
}

TEST_F(AdapterHccpTest, HrtRaUbRemoteMemImport_ok)
{
    uint8_t value[128];
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbRemMemImportedOutParam result = HrtRaUbRemoteMemImport(handle, value, 128, 0);
 
    EXPECT_EQ(0, result.targetSegVa);
}

TEST_F(AdapterHccpTest, HrtRaUbRemoteMemUnimport_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbRemoteMemUnimport(handle, 0);
}

TEST_F(AdapterHccpTest, HrtRaUbCreateJfc_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    u64 result = HrtRaUbCreateJfc(handle, HrtUbJfcMode::NORMAL);
    EXPECT_EQ(0, result);
}

TEST_F(AdapterHccpTest, HrtRaUbDestroyJfc_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbDestroyJfc(handle, 0);
}

TEST_F(AdapterHccpTest, HrtRaUbCreateJetty_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);

    HrtRaUbCreateJettyParam inParam1{100, 100, 100, 100, HrtJettyMode::HOST_OPBASE, 0, 100, 100, 100, 100};
    HrtRaUbJettyCreatedOutParam result1 = HrtRaUbCreateJetty(handle, inParam1);
    EXPECT_EQ(0, result1.jettyVa);

    HrtRaUbCreateJettyParam inParam2{100, 100, 100, 100, HrtJettyMode::HOST_OFFLOAD, 0, 100, 100, 100, 100};
    HrtRaUbJettyCreatedOutParam result2 = HrtRaUbCreateJetty(handle, inParam2);
    EXPECT_EQ(0, result2.jettyVa);

    HrtRaUbCreateJettyParam inParam3{100, 100, 100, 100, HrtJettyMode::CCU_CCUM_CACHE, 0, 100, 100, 100, 100};
    HrtRaUbJettyCreatedOutParam result3 = HrtRaUbCreateJetty(handle, inParam3);
    EXPECT_EQ(0, result3.jettyVa);

    HrtRaUbCreateJettyParam inParam4{100, 100, 100, 100, HrtJettyMode::DEV_USED, 0, 100, 100, 100, 100};
    HrtRaUbJettyCreatedOutParam result4 = HrtRaUbCreateJetty(handle, inParam4);
    EXPECT_EQ(0, result4.jettyVa);
}

TEST_F(AdapterHccpTest, HrtRaUbDestroyJetty_ok)
{
    HrtRaUbDestroyJetty(0);
}

TEST_F(AdapterHccpTest, RaUbTpImportJetty_throw)
{
    struct JettyImportCfg cfg = {};
    uint8_t value[128];
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    EXPECT_THROW(RaUbTpImportJetty(handle, value, 0, 0, cfg), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtRaUbUnimportJetty_ok)
{
    RdmaHandle handle = reinterpret_cast<RdmaHandle>(0x123);
    HrtRaUbUnimportJetty(handle, 0);
}

TEST_F(AdapterHccpTest, HrtRaUbJettyBind_ok)
{
    HrtRaUbJettyBind(0, 0);
}

TEST_F(AdapterHccpTest, HrtRaUbJettyUnbind_ok)
{
    HrtRaUbJettyUnbind(0);
}

TEST_F(AdapterHccpTest, HrtRaUbPostSend_ok)
{
    HrtRaUbSendWrReqParam in;
    in.inlineFlag                 = true;
    in.inlineReduceFlag           = true;
    in.opcode                     = HrtUbSendWrOpCode::WRITE_WITH_NOTIFY;
    in.reduceOp                   = ReduceOp::SUM;
    in.dataType                   = DataType::INT8;
    HrtRaUbSendWrRespParam result = HrtRaUbPostSend(0, in);
    EXPECT_EQ(0, result.dieId);
}


TEST_F(AdapterHccpTest, HrtGetHosIf_nok_ra_get_ifnum_error)
{
    MOCKER(RaGetIfnum).stubs().with(any(), any()).will(returnValue(1));
    EXPECT_THROW(HrtGetHostIf(0), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtGetHosIf_nok_ra_get_ifnum_zero)
{
    unsigned int fakeNum = 0;
    MOCKER(RaGetIfnum).stubs().with(any(), outBoundP(&fakeNum, sizeof(fakeNum))).will(returnValue(0));
    auto hostIfs = HrtGetHostIf(0);
    EXPECT_EQ(true, hostIfs.empty());
}

TEST_F(AdapterHccpTest, HrtGetHosIf_nok_ra_get_ifaddrs_error)
{

    unsigned int fakeNum = 1;
    MOCKER(RaGetIfnum).stubs().with(any(), outBoundP(&fakeNum, sizeof(fakeNum))).will(returnValue(0));
    MOCKER(RaGetIfaddrs).stubs().with(any(), any(), any()).will(returnValue(1));

    EXPECT_THROW(HrtGetHostIf(0), NetworkApiException);
}

TEST_F(AdapterHccpTest, HrtGetHosIf_ok)
{
    unsigned int fakeNum = 1;
    MOCKER(RaGetIfnum).stubs().with(any(), outBoundP(&fakeNum, sizeof(fakeNum))).will(returnValue(0));

    IpAddress             addr(1);
    struct InterfaceInfo ifAddrInfos[1];
    char                  stub_ifname[256] = {};
    stub_ifname[0]                         = 'a';
    memcpy_s(&ifAddrInfos[0].ifname, 256, &stub_ifname, 256);
    ifAddrInfos[0].ifaddr.ip.addr = addr.GetBinaryAddress().addr;
    ifAddrInfos[0].family    = addr.GetFamily();
    ifAddrInfos[0].scopeId  = addr.GetScopeID();

    MOCKER(RaGetIfaddrs)
        .stubs()
        .with(any(), outBoundP(ifAddrInfos, sizeof(ifAddrInfos)), outBoundP(&fakeNum, sizeof(fakeNum)))
        .will(returnValue(0));

    auto hostIfs = HrtGetHostIf(0);
    EXPECT_EQ(1, hostIfs.size());
    EXPECT_EQ("a", hostIfs[0].first);
    EXPECT_EQ(0, hostIfs[0].second.GetScopeID());
    EXPECT_EQ(AF_INET, (int)hostIfs[0].second.GetFamily());
    EXPECT_EQ(1, hostIfs[0].second.GetBinaryAddress().addr.s_addr);
}



TEST_F(AdapterHccpTest, HrtGetDeviceIp_nok)
{
    u32  devPhyId = 0;
    auto hostIfs  = HrtGetDeviceIp(devPhyId);
    EXPECT_EQ(true, hostIfs.empty());

    HrtGetDeviceIp(devPhyId);
}

TEST_F(AdapterHccpTest, HrtGetDeviceIp_ok)
{
    unsigned int fakeNum = 1;
    MOCKER(RaGetIfnum).stubs().with(any(), outBoundP(&fakeNum, sizeof(fakeNum))).will(returnValue(0));

    u32 devPhyId = 0;
    IpAddress addr(1);
    struct InterfaceInfo ifAddrInfos[1];
    char stub_ifname[256] = {};
    stub_ifname[0] = 'a';
    memcpy_s(&ifAddrInfos[0].ifname, 256, &stub_ifname, 256);
    ifAddrInfos[0].ifaddr.ip.addr = addr.GetBinaryAddress().addr;
    ifAddrInfos[0].family = addr.GetFamily();
    ifAddrInfos[0].scopeId = addr.GetScopeID();

    MOCKER(RaGetIfaddrs)
        .stubs()
        .with(any(), outBoundP(ifAddrInfos, sizeof(ifAddrInfos)), outBoundP(&fakeNum, sizeof(fakeNum)))
        .will(returnValue(0));

    auto deviceIps = HrtGetDeviceIp(devPhyId);
    EXPECT_EQ(1, deviceIps.size());
    EXPECT_EQ(0, deviceIps[0].GetScopeID());
    EXPECT_EQ(AF_INET, (int)deviceIps[0].GetFamily());
    EXPECT_EQ(1, deviceIps[0].GetBinaryAddress().addr.s_addr);
}


TEST_F(AdapterHccpTest, HrtRaUbPostNops_exception)
{
    MOCKER(RaBatchSendWr).stubs().with(any()).will(returnValue(1));
    EXPECT_THROW(HrtRaUbPostNops(0, 0, 1), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaUbUpdateCi_exception)
{
    JettyHandle jettyHandle = 0;
    MOCKER(RaCtxUpdateCi).stubs().will(returnValue(1));
    EXPECT_THROW(RaUbUpdateCi(jettyHandle, 100), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaGetAsyncReqResult_exception)
{
    RequestHandle reqHandle = 0;
    HrtRaGetAsyncReqResult(reqHandle);
}
 
TEST_F(AdapterHccpTest, RaUbLocalMemUnregAsync_exception)
{
    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x123);
    LocMemHandle lmemHandle;
    RaUbLocalMemUnregAsync(rdmaHandle, lmemHandle);
}
 
TEST_F(AdapterHccpTest, RaUbDestroyJettyAsync_exception)
{
    void* jettyHandle = reinterpret_cast<void*>(0x123);
    RequestHandle result = RaUbDestroyJettyAsync(jettyHandle);
}
 
TEST_F(AdapterHccpTest, RaUbUnimportJettyAsync_exception)
{
    void* targetJettyHandle = reinterpret_cast<void*>(0x123);
    RaUbUnimportJettyAsync(targetJettyHandle);
}

TEST_F(AdapterHccpTest, RaGetAsyncReqResult_return_others_eagain)
{
    MOCKER(RaGetAsyncReqResult).stubs().with(any(), any()).will(returnValue(OTHERS_EAGAIN));
    RequestHandle reqHandle = 12;
    HrtRaGetAsyncReqResult(reqHandle);
    GlobalMockObject::verify();
}

TEST_F(AdapterHccpTest, RaGetAsyncReqResult_return_error)
{
    MOCKER(RaGetAsyncReqResult).stubs().with(any(), any()).will(returnValue(OTHERS_EAGAIN+1));
    RequestHandle reqHandle = 12;
    EXPECT_THROW(HrtRaGetAsyncReqResult(reqHandle), NetworkApiException);
    GlobalMockObject::verify();
}

TEST_F(AdapterHccpTest, RaGetAsyncReqResult_return_zero_result_error)
{
    int fakeResult = 12345;
    MOCKER(RaGetAsyncReqResult).stubs().with(any(), outBoundP(&fakeResult)).will(returnValue(0));
    RequestHandle reqHandle = 12;
    EXPECT_THROW(HrtRaGetAsyncReqResult(reqHandle), NetworkApiException);
    GlobalMockObject::verify();
}

TEST_F(AdapterHccpTest, RaBlockGetSocket_return_err)
{
    int reqResult = SOCK_EAGAIN;
    MOCKER(strcpy_s).stubs().will(returnValue(-1));

    SocketHandle socketHandle = nullptr;
    IpAddress ipAddr = IpAddress();
    std::string tag = "";
    FdHandle fdHandle = nullptr;
    u32 role = 0;
    RaSocketGetParam param(socketHandle, ipAddr, tag, fdHandle);

    RequestHandle reqHandle = 12;
    EXPECT_THROW(RaGetOneSocket(role, param), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaGetOneSocket_return_err_2)
{
    u32 connectedNum = 2;
    MOCKER(RaGetSockets).stubs()
        .with(any(), any(), any(), outBoundP(&connectedNum))
        .will(returnValue(0));

    SocketHandle socketHandle = nullptr;
    IpAddress ipAddr = IpAddress();
    std::string tag = "";
    FdHandle fdHandle = nullptr;
    u32 role = 0;
    RaSocketGetParam param(socketHandle, ipAddr, tag, fdHandle);

    RequestHandle reqHandle = 12;
    EXPECT_THROW(RaGetOneSocket(role, param), NetworkApiException);
}

TEST_F(AdapterHccpTest, RaSocketCloseOneAsync_return_ok)
{
    MOCKER(RaSocketBatchCloseAsync).stubs()
        .with(any(), any(), any())
        .will(returnValue(0));

    SocketHandle socketHandle = nullptr;
    FdHandle fdHandle = nullptr;
    RaSocketCloseParam param(socketHandle, fdHandle);

    RaSocketCloseOneAsync(param);
}

TEST_F(AdapterHccpTest, RaSocketListenOneStopAsync_return_ok)
{
    MOCKER(RaSocketListenStopAsync).stubs()
        .with(any(), any(), any())
        .will(returnValue(0));

    SocketHandle socketHandle = nullptr;
    unsigned int port = 100;
    RaSocketListenParam param(socketHandle, port);

    RaSocketListenOneStopAsync(param);
};

TEST_F(AdapterHccpTest, RaUbAllocTokenIdHandle_ok)
{
    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x123);
    std::pair<TokenIdHandle, uint32_t> result = RaUbAllocTokenIdHandle(rdmaHandle);
    std::pair<TokenIdHandle, uint32_t> expectResult(0, 0);
    EXPECT_EQ(result, expectResult);
}

TEST_F(AdapterHccpTest, RaUbFreeTokenIdHandle_exception)
{
    MOCKER(RaCtxTokenIdFree).stubs().with(any()).will(returnValue(1));
    EXPECT_THROW(RaUbFreeTokenIdHandle(0, 0), NullPtrException);
}

void MockRaSocketRecv(int ret, unsigned long long recvSize)
{
    MOCKER(RaSocketRecv).stubs()
        .with(any(), any(), any(), outBoundP(&recvSize, sizeof(recvSize)))
        .will(returnValue(ret));
}

void MockEnvLinkTimeoutGet(int timeout)
{
    // 设置超时时间
    MOCKER(EnvLinkTimeoutGet).stubs().will(returnValue(timeout));
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_RecvOnceComplete_Expect_Success)
{
    MockRaSocketRecv(0, 100); // 一次接收完成
    MockEnvLinkTimeoutGet(1); // 1秒超时
    FdHandle fakeFdHandle = reinterpret_cast<FdHandle>(0x123);
    void *fakeData = reinterpret_cast<void *>(0x133);
    EXPECT_NO_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100));
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_RecvSizeExceeds_Expect_ThrowException)
{
    MockRaSocketRecv(0, 150); // 超出预期大小
    MockEnvLinkTimeoutGet(1);
    EXPECT_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100), NullPtrException);
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_RecvSizeZero_Expect_ThrowException)
{
    MockRaSocketRecv(0, 0); // 接收为0
    MockEnvLinkTimeoutGet(1);
    EXPECT_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100), NullPtrException);
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_SockClosed_Expect_ThrowException)
{
    MockRaSocketRecv(SOCK_ESOCKCLOSED, 0);
    MockEnvLinkTimeoutGet(1);
    EXPECT_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100), NullPtrException);
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_SockClose_Expect_ThrowException)
{
    MockRaSocketRecv(SOCK_CLOSE, 0);
    MockEnvLinkTimeoutGet(1);
    EXPECT_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100), NullPtrException);
}

TEST_F(AdapterHccpTest, Ut_HrtRaSocketBlockRecv_When_Timeout_Expect_ThrowException)
{
    MockRaSocketRecv(0, 0); // 模拟一直接收不到数据
    MockEnvLinkTimeoutGet(0); // 超时时间设为0
    EXPECT_THROW(HrtRaSocketBlockRecv(fakeFdHandle, fakeData, 100), NullPtrException);
}

TEST_F(AdapterHccpTest, ut_HrtRaSocketWhiteListDel_With_Enormous_WhiteList)
{
    // 大量删除接口
    RaSocketWhitelist wlist{};
    vector<RaSocketWhitelist> wlists(56, wlist);
    SocketHandle fakeFdHandle = reinterpret_cast<SocketHandle>(0x123);

    MOCKER(RaSocketWhiteListDel).stubs().will(returnValue(0));

    EXPECT_NO_THROW(HrtRaSocketWhiteListDel(fakeFdHandle, wlists));
}

TEST_F(AdapterHccpTest, Ut_HraGetRtpEnable_When_RTP_Equals_1_Expect_Return_True)
{
    DevBaseAttr out {};
    out.ub.priorityInfo[0].tpType.bs.rtp = 1;
    MOCKER(RaGetDevBaseAttr).stubs()
        .with(any(), outBoundP(&out, sizeof(out)))
        .will(returnValue(0));
    RdmaHandle handle = (void *)0x1234;

    EXPECT_EQ(HraGetRtpEnable(handle), true);
}

TEST_F(AdapterHccpTest, Ut_HraGetRtpEnable_When_RTP_Equals_0_Expect_Return_False)
{
    DevBaseAttr out {};
    MOCKER(RaGetDevBaseAttr).stubs()
        .with(any(), outBoundP(&out, sizeof(out)))
        .will(returnValue(0));
    RdmaHandle handle = (void *)0x1234;

    EXPECT_EQ(HraGetRtpEnable(handle), false);
}