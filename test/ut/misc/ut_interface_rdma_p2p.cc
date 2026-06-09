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
#include <mockcpp/mockcpp.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string>

#define private public
#define protected public
#include "typical_qp_manager.h"
#include "typical_mr_manager.h"
#include "rdma_resource_manager.h"
#include "externalinput.h"
#include "interface_hccl.h"
#include "adapter_hccp.h"
#include "adapter_rts.h"
#undef private

#include "hccl/base.h"
#include <iostream>

using namespace std;
using namespace hccl;

// Forward declare CheckDepth (not in header, only in interface_hccl.cc)
HcclResult CheckDepth(uint32_t depth);

static RdmaHandle g_fakeRdmaHandle = reinterpret_cast<RdmaHandle>(0xDEADBEEF);
static void* g_fakeCqHandle = reinterpret_cast<void*>(0xCAFEBABE);
static QpHandle g_fakeQpHandle = reinterpret_cast<QpHandle>(0xBEEF0001);
static u32 g_nextCqn = 100;
static u32 g_nextQpn = 200;

class InterfaceRdmaP2p : public testing::Test {
protected:
    virtual void SetUp()
    {
        TypicalQpManager::GetInstance().rdmaHandle_ = g_fakeRdmaHandle;
        TypicalMrManager::GetInstance().rdmaHandle_ = g_fakeRdmaHandle;
        RdmaResourceManager::GetInstance().rdmaHandle_ = g_fakeRdmaHandle;
        std::cout << "InterfaceRdmaP2p SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        TypicalQpManager::GetInstance().cqMap_.clear();
        TypicalQpManager::GetInstance().qpMap_.clear();
        TypicalMrManager::GetInstance().regedMrMap_.clear();
        GlobalMockObject::verify();
        std::cout << "InterfaceRdmaP2p TearDown" << std::endl;
    }
};

// =============== Stub functions ===============

HcclResult stub_hrtGetDeviceRefresh_success(s32* deviceLogicId)
{
    *deviceLogicId = 0;
    return HCCL_SUCCESS;
}

static HcclResult stub_CreateTypicalCq_Success(RdmaHandle rdmaHandle, u32 cqDepth, u32 &cqn, void **cqHandle)
{
    cqn = g_nextCqn++;
    *cqHandle = g_fakeCqHandle;
    return HCCL_SUCCESS;
}

static HcclResult stub_CreateTypicalCq_Fail(RdmaHandle rdmaHandle, u32 cqDepth, u32 &cqn, void **cqHandle)
{
    return HCCL_E_INTERNAL;
}

static HcclResult stub_DestroyTypicalCq_Success(RdmaHandle rdmaHandle, u32 cqn, void *cqHandle)
{
    return HCCL_SUCCESS;
}

static HcclResult stub_DestroyTypicalCq_Fail(RdmaHandle rdmaHandle, u32 cqn, void *cqHandle)
{
    return HCCL_E_INTERNAL;
}

static HcclResult stub_CreateQpWithCQConfig_Success(RdmaHandle rdmaHandle, s32 qpMode,
    const QpConfigWithCQInfo& qpConfig, QpHandle &qpHandle, struct TypicalQp& qpInfo)
{
    qpHandle = g_fakeQpHandle;
    qpInfo.qpn = g_nextQpn++;
    return HCCL_SUCCESS;
}

static HcclResult stub_CreateQpWithCQConfig_Fail(RdmaHandle rdmaHandle, s32 qpMode,
    const QpConfigWithCQInfo& qpConfig, QpHandle &qpHandle, struct TypicalQp& qpInfo)
{
    return HCCL_E_INTERNAL;
}

static HcclResult stub_HrtRaQpDestroyWithoutCQ_Success(QpHandle handle)
{
    return HCCL_SUCCESS;
}

static HcclResult stub_HrtRaQpDestroyWithoutCQ_Fail(QpHandle handle)
{
    return HCCL_E_INTERNAL;
}

s32 stub_HrtRaPollTypicalCq_Success(void* cqHandle, u32 num, void *wc)
{
    return 1;
}

s32 stub_HrtRaPollTypicalCq_Fail(void* cqHandle, u32 num, void *wc)
{
    return -1;
}

HcclResult stub_HrtRaSendWrVerbs_Success(QpHandle handle, struct SendWrVerbs *wr, struct SendWrRsp *opRsp)
{
    opRsp->db.dbIndex = 0;
    opRsp->db.dbInfo = (unsigned long)0x1000;
    return HCCL_SUCCESS;
}

HcclResult stub_HrtRaSendWrVerbs_Fail(QpHandle handle, struct SendWrVerbs *wr, struct SendWrRsp *opRsp)
{
    return HCCL_E_INTERNAL;
}

HcclResult stub_HrtRaRecvWrVerbs_Success(QpHandle handle, struct RecvWrVerbs *wr)
{
    return HCCL_SUCCESS;
}

HcclResult stub_HrtRaRecvWrVerbs_Fail(QpHandle handle, struct RecvWrVerbs *wr)
{
    return HCCL_E_INTERNAL;
}

HcclResult stub_hrtRDMADBSend_Success(u32 dbIndex, void* dbInfo, aclrtStream stream)
{
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaTypicalQpModify_Success(QpHandle qpHandle, struct TypicalQp* localQpInfo,
    struct TypicalQp* remoteQpInfo)
{
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaTypicalQpModify_Fail(QpHandle qpHandle, struct TypicalQp* localQpInfo,
    struct TypicalQp* remoteQpInfo)
{
    return HCCL_E_INTERNAL;
}

HcclResult stub_hrtRaRegGlobalMr_Success(const RdmaHandle rdmaHandle, struct MrInfoT &mrInfo, MrHandle &mrHandle)
{
    mrHandle = reinterpret_cast<MrHandle>(0xFEED0001);
    mrInfo.lkey = 0xABCD;
    mrInfo.rkey = 0x1234;
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaRegGlobalMr_Fail(const RdmaHandle rdmaHandle, struct MrInfoT &mrInfo, MrHandle &mrHandle)
{
    return HCCL_E_INTERNAL;
}

HcclResult stub_hrtRaDeRegGlobalMr_Success(const RdmaHandle rdmaHandle, MrHandle mrHandle)
{
    return HCCL_SUCCESS;
}

HcclResult stub_hrtRaDeRegGlobalMr_Fail(const RdmaHandle rdmaHandle, MrHandle mrHandle)
{
    return HCCL_E_INTERNAL;
}

static QpHandle g_fakeQpHandle2 = reinterpret_cast<QpHandle>(0xBEEF0002);

HcclResult stub_GetQpHandleByQpn_stub(TypicalQpManager* mgr, u32 qpn, QpHandle& qpHandle)
{
    qpHandle = g_fakeQpHandle2;
    return HCCL_SUCCESS;
}

// Helper to pre-populate a CQ in cqMap_
static void PrePopulateCq(u32 cqn, u32 cqDepth)
{
    AscendCQInfo cqInfo;
    cqInfo.cqn = cqn;
    cqInfo.cqDepth = cqDepth;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(cqn, std::make_pair(cqInfo, g_fakeCqHandle)));
}

// Helper to pre-populate a QP in qpMap_
static void PrePopulateQp(u32 qpn)
{
    struct TypicalQp qpInfo;
    qpInfo.qpn = qpn;
    TypicalQpManager::GetInstance().qpMap_.insert(
        std::make_pair(qpn, std::make_pair(qpInfo, g_fakeQpHandle)));
}

// =============== CheckDepth Tests ===============

TEST_F(InterfaceRdmaP2p, CheckDepth_128_Valid)
{
    EXPECT_EQ(CheckDepth(128), HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_256_Valid)
{
    EXPECT_EQ(CheckDepth(256), HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_32768_Valid)
{
    EXPECT_EQ(CheckDepth(32768), HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_64_TooSmall)
{
    EXPECT_EQ(CheckDepth(64), HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_65536_TooLarge)
{
    EXPECT_EQ(CheckDepth(65536), HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_129_NotPowerOfTwo)
{
    EXPECT_EQ(CheckDepth(129), HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, CheckDepth_Zero)
{
    EXPECT_EQ(CheckDepth(0), HCCL_E_PARA);
}

// =============== hcclCreateAscendCQWithAttr Tests ===============

TEST_F(InterfaceRdmaP2p, CreateAscendCQWithAttr_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(CreateTypicalCq).stubs().will(invoke(stub_CreateTypicalCq_Success));

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 256;
    cqInfo.cqn = 0;

    HcclResult ret = hcclCreateAscendCQWithAttr(&cqInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(cqInfo.cqn, static_cast<uint32_t>(0));
}

TEST_F(InterfaceRdmaP2p, CreateAscendCQWithAttr_NullPtr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    HcclResult ret = hcclCreateAscendCQWithAttr(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, CreateAscendCQWithAttr_InvalidDepth)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 64; // below minimum

    HcclResult ret = hcclCreateAscendCQWithAttr(&cqInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, CreateAscendCQWithAttr_CreateFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(CreateTypicalCq).stubs().will(invoke(stub_CreateTypicalCq_Fail));

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 256;

    HcclResult ret = hcclCreateAscendCQWithAttr(&cqInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclDestroyAscendCQ Tests ===============

TEST_F(InterfaceRdmaP2p, DestroyAscendCQ_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(DestroyTypicalCq).stubs().will(invoke(stub_DestroyTypicalCq_Success));

    AscendCQInfo cqInfo;
    cqInfo.cqn = 42;
    cqInfo.cqDepth = 256;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(42), std::make_pair(cqInfo, g_fakeCqHandle)));

    HcclResult ret = hcclDestroyAscendCQ(&cqInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.find(42),
        TypicalQpManager::GetInstance().cqMap_.end());
}

TEST_F(InterfaceRdmaP2p, DestroyAscendCQ_NullPtr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    HcclResult ret = hcclDestroyAscendCQ(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, DestroyAscendCQ_DestroyFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(DestroyTypicalCq).stubs().will(invoke(stub_DestroyTypicalCq_Fail));

    AscendCQInfo cqInfo;
    cqInfo.cqn = 42;
    cqInfo.cqDepth = 256;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(42), std::make_pair(cqInfo, g_fakeCqHandle)));

    HcclResult ret = hcclDestroyAscendCQ(&cqInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclPollAscendCQ Tests ===============

TEST_F(InterfaceRdmaP2p, PollAscendCQ_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaPollTypicalCq).stubs().will(invoke(stub_HrtRaPollTypicalCq_Success));

    PrePopulateCq(100, 256);

    AscendCQInfo cqInfo;
    cqInfo.cqn = 100;
    uint32_t polledNum = 0;
    struct AscendWc wc[8];

    HcclResult ret = hcclPollAscendCQ(&cqInfo, 8, &polledNum, wc);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(polledNum, static_cast<uint32_t>(1));
}

TEST_F(InterfaceRdmaP2p, PollAscendCQ_NullCqInfo)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    uint32_t polledNum = 0;
    struct AscendWc wc[8];

    HcclResult ret = hcclPollAscendCQ(nullptr, 8, &polledNum, wc);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, PollAscendCQ_NullPolledNum)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    PrePopulateCq(100, 128);
    AscendCQInfo cqInfo;
    cqInfo.cqn = 100;
    struct AscendWc wc[8];

    HcclResult ret = hcclPollAscendCQ(&cqInfo, 8, nullptr, wc);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, PollAscendCQ_NullWc)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    PrePopulateCq(100, 128);
    AscendCQInfo cqInfo;
    cqInfo.cqn = 100;
    uint32_t polledNum = 0;

    HcclResult ret = hcclPollAscendCQ(&cqInfo, 8, &polledNum, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, PollAscendCQ_PollFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaPollTypicalCq).stubs().will(invoke(stub_HrtRaPollTypicalCq_Fail));

    PrePopulateCq(100, 128);
    AscendCQInfo cqInfo;
    cqInfo.cqn = 100;
    uint32_t polledNum = 0;
    struct AscendWc wc[8];

    HcclResult ret = hcclPollAscendCQ(&cqInfo, 8, &polledNum, wc);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclCreateAscendQPWithCQWithAttr Tests ===============

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_SuccessDefaultCaps)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(CreateQpWithCQConfig).stubs().will(invoke(stub_CreateQpWithCQConfig_Success));

    PrePopulateCq(10, 256); // send CQ
    PrePopulateCq(20, 128); // recv CQ

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 10;
    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;

    AscendVerbsQPInfo qpInfo = {};
    // maxSendWr=0 and maxRecvWr=0 -> use defaults

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, &recvCQInfo, &qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpInfo.qpn, static_cast<uint32_t>(0));
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_SuccessCustomCaps)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(CreateQpWithCQConfig).stubs().will(invoke(stub_CreateQpWithCQConfig_Success));

    PrePopulateCq(10, 32768); // send CQ
    PrePopulateCq(20, 128);   // recv CQ

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 10;
    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;

    AscendVerbsQPInfo qpInfo = {};
    qpInfo.cap.maxSendWr = 1024;
    qpInfo.cap.maxRecvWr = 256;
    qpInfo.cap.maxSendSge = 4;
    qpInfo.cap.maxRecvSge = 2;
    qpInfo.cap.maxInlineData = 64;
    qpInfo.sqSigAll = 1;

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, &recvCQInfo, &qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_NullSendCQ)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;
    AscendVerbsQPInfo qpInfo = {};

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(nullptr, &recvCQInfo, &qpInfo);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_NullRecvCQ)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 10;
    AscendVerbsQPInfo qpInfo = {};

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, nullptr, &qpInfo);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_NullQPInfo)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 10;
    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, &recvCQInfo, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_SendCQNotFound)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    // PrePopulate only recv CQ, not send CQ
    PrePopulateCq(20, 128);

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 999; // does not exist
    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;
    AscendVerbsQPInfo qpInfo = {};

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, &recvCQInfo, &qpInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, CreateAscendQPWithCQWithAttr_CreateQpFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(CreateQpWithCQConfig).stubs().will(invoke(stub_CreateQpWithCQConfig_Fail));

    PrePopulateCq(10, 256);
    PrePopulateCq(20, 128);

    AscendCQInfo sendCQInfo;
    sendCQInfo.cqn = 10;
    AscendCQInfo recvCQInfo;
    recvCQInfo.cqn = 20;
    AscendVerbsQPInfo qpInfo = {};

    HcclResult ret = hcclCreateAscendQPWithCQWithAttr(&sendCQInfo, &recvCQInfo, &qpInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclDestroyAscendVerbsQP Tests ===============

TEST_F(InterfaceRdmaP2p, DestroyAscendVerbsQP_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaQpDestroyWithoutCQ).stubs().will(invoke(stub_HrtRaQpDestroyWithoutCQ_Success));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    HcclResult ret = hcclDestroyAscendVerbsQP(&qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().qpMap_.find(300),
        TypicalQpManager::GetInstance().qpMap_.end());
}

TEST_F(InterfaceRdmaP2p, DestroyAscendVerbsQP_NullPtr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    HcclResult ret = hcclDestroyAscendVerbsQP(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, DestroyAscendVerbsQP_QpNotFound)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 999; // does not exist

    HcclResult ret = hcclDestroyAscendVerbsQP(&qpInfo);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

// =============== hcclModifyAscendVerbsQPEx Tests ===============

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaTypicalQpModify).stubs().will(invoke(stub_hrtRaTypicalQpModify_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    PrePopulateQp(300);

    AscendVerbsQPInfo localQPInfo;
    localQPInfo.qpn = 300;
    AscendVerbsQPInfo remoteQPInfo;
    remoteQPInfo.qpn = 400;

    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 8; // multiple of 4

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_NullLocalQP)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo remoteQPInfo;
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 8;

    HcclResult ret = hcclModifyAscendVerbsQPEx(nullptr, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_NullRemoteQP)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo localQPInfo;
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 8;

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, nullptr, &qpQos);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_NullQos)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo localQPInfo;
    AscendVerbsQPInfo remoteQPInfo;

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_InvalidSL)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo localQPInfo;
    AscendVerbsQPInfo remoteQPInfo;
    AscendQPQos qpQos;
    qpQos.sl = 8; // max is 7
    qpQos.tc = 8;

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_InvalidTC)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo localQPInfo;
    AscendVerbsQPInfo remoteQPInfo;
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 256; // max is 255

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_TcNotMultipleOf4)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo localQPInfo;
    AscendVerbsQPInfo remoteQPInfo;
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 7; // not multiple of 4

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============== hcclModifyAscendVerbsQP Tests ===============

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQP_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaTypicalQpModify).stubs().will(invoke(stub_hrtRaTypicalQpModify_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));
    MOCKER(GetExternalInputRdmaTrafficClass).stubs().will(returnValue(4));
    MOCKER(GetExternalInputRdmaServerLevel).stubs().will(returnValue(2));

    PrePopulateQp(300);

    AscendVerbsQPInfo localQPInfo;
    localQPInfo.qpn = 300;
    AscendVerbsQPInfo remoteQPInfo;
    remoteQPInfo.qpn = 400;

    HcclResult ret = hcclModifyAscendVerbsQP(&localQPInfo, &remoteQPInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

// =============== hcclAscendPostSend Tests ===============

TEST_F(InterfaceRdmaP2p, AscendPostSend_SuccessSingleWR)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaSendWrVerbs).stubs().will(invoke(stub_HrtRaSendWrVerbs_Success));
    MOCKER(hrtRDMADBSend).stubs().will(invoke(stub_hrtRDMADBSend_Success));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    struct AscendSge sge;
    sge.addr = 0x1000;
    sge.len = 64;
    sge.lkey = 0xABCD;

    struct AscendSendWr sendWr;
    sendWr.wrId = 1;
    sendWr.next = nullptr;
    sendWr.sgList = &sge;
    sendWr.numSge = 1;
    sendWr.opcode = ASCEND_WR_RDMA_WRITE;
    sendWr.sendFlags = ASCEND_SEND_SIGNALED;
    sendWr.immData = 0;
    sendWr.wr.rdma.remoteAddr = 0x2000;
    sendWr.wr.rdma.rkey = 0x5678;

    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(&qpInfo, &sendWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(badWr, nullptr);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_NullQpInfo)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    struct AscendSendWr sendWr;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(nullptr, &sendWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_NullSendWr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(&qpInfo, nullptr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_NullBadWr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;
    struct AscendSendWr sendWr;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);

    HcclResult ret = hcclAscendPostSend(&qpInfo, &sendWr, stream, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_NullStream)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;
    struct AscendSendWr sendWr;
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(&qpInfo, &sendWr, nullptr, &badWr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_SgeExceedsMax)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    struct AscendSendWr sendWr;
    sendWr.wrId = 1;
    sendWr.next = nullptr;
    sendWr.numSge = 17; // exceeds MAX_SGLIST_VERBS (16)
    sendWr.sgList = reinterpret_cast<struct AscendSge*>(0xBAD);

    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(&qpInfo, &sendWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(InterfaceRdmaP2p, AscendPostSend_SendWrVerbsFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaSendWrVerbs).stubs().will(invoke(stub_HrtRaSendWrVerbs_Fail));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    struct AscendSge sge;
    sge.addr = 0x1000;
    sge.len = 64;
    sge.lkey = 0xABCD;

    struct AscendSendWr sendWr;
    sendWr.wrId = 1;
    sendWr.next = nullptr;
    sendWr.sgList = &sge;
    sendWr.numSge = 1;
    sendWr.opcode = ASCEND_WR_SEND;
    sendWr.sendFlags = ASCEND_SEND_SIGNALED;
    sendWr.immData = 0;
    sendWr.wr.rdma.remoteAddr = 0x2000;
    sendWr.wr.rdma.rkey = 0x5678;

    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendSendWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostSend(&qpInfo, &sendWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclAscendPostRecv Tests ===============

TEST_F(InterfaceRdmaP2p, AscendPostRecv_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaRecvWrVerbs).stubs().will(invoke(stub_HrtRaRecvWrVerbs_Success));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    struct AscendSge sge;
    sge.addr = 0x1000;
    sge.len = 64;
    sge.lkey = 0xABCD;

    struct AscendRecvWr recvWr;
    recvWr.wrId = 1;
    recvWr.next = nullptr;
    recvWr.sgList = &sge;
    recvWr.numSge = 1;

    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendRecvWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostRecv(&qpInfo, &recvWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(badWr, nullptr);
}

TEST_F(InterfaceRdmaP2p, AscendPostRecv_NullQpInfo)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    struct AscendRecvWr recvWr;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendRecvWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostRecv(nullptr, &recvWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostRecv_NullRecvWr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendRecvWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostRecv(&qpInfo, nullptr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostRecv_NullBadWr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;
    struct AscendRecvWr recvWr;
    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);

    HcclResult ret = hcclAscendPostRecv(&qpInfo, &recvWr, stream, nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, AscendPostRecv_RecvWrVerbsFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(HrtRaRecvWrVerbs).stubs().will(invoke(stub_HrtRaRecvWrVerbs_Fail));

    PrePopulateQp(300);

    AscendVerbsQPInfo qpInfo;
    qpInfo.qpn = 300;

    struct AscendSge sge;
    sge.addr = 0x1000;
    sge.len = 64;
    sge.lkey = 0xABCD;

    struct AscendRecvWr recvWr;
    recvWr.wrId = 1;
    recvWr.next = nullptr;
    recvWr.sgList = &sge;
    recvWr.numSge = 1;

    aclrtStream stream = reinterpret_cast<aclrtStream>(0xF000);
    struct AscendRecvWr *badWr = nullptr;

    HcclResult ret = hcclAscendPostRecv(&qpInfo, &recvWr, stream, &badWr);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclRdmaMemRegister Tests ===============

TEST_F(InterfaceRdmaP2p, RdmaMemRegister_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaRegGlobalMr).stubs().will(invoke(stub_hrtRaRegGlobalMr_Success));

    AscendMrAttr memInfo;
    memInfo.addr = 0x10000000;
    memInfo.size = 4096;

    HcclResult ret = hcclRdmaMemRegister(&memInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(memInfo.lkey, static_cast<uint32_t>(0));
    EXPECT_NE(memInfo.rkey, static_cast<uint32_t>(0));
}

TEST_F(InterfaceRdmaP2p, RdmaMemRegister_NullPtr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    HcclResult ret = hcclRdmaMemRegister(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, RdmaMemRegister_RegisterFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaRegGlobalMr).stubs().will(invoke(stub_hrtRaRegGlobalMr_Fail));

    AscendMrAttr memInfo;
    memInfo.addr = 0x10000000;
    memInfo.size = 4096;

    HcclResult ret = hcclRdmaMemRegister(&memInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}

// =============== hcclRdmaMemDeRegister Tests ===============

TEST_F(InterfaceRdmaP2p, RdmaMemDeRegister_Success)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaRegGlobalMr).stubs().will(invoke(stub_hrtRaRegGlobalMr_Success));
    MOCKER(hrtRaDeRegGlobalMr).stubs().will(invoke(stub_hrtRaDeRegGlobalMr_Success));

    // First register to populate regedMrMap_
    AscendMrAttr memInfo;
    memInfo.addr = 0x10000000;
    memInfo.size = 4096;
    EXPECT_EQ(hcclRdmaMemRegister(&memInfo), HCCL_SUCCESS);

    // Then deregister
    HcclResult ret = hcclRdmaMemDeRegister(&memInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(InterfaceRdmaP2p, RdmaMemDeRegister_NullPtr)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    HcclResult ret = hcclRdmaMemDeRegister(nullptr);
    EXPECT_EQ(ret, HCCL_E_PTR);
}

TEST_F(InterfaceRdmaP2p, RdmaMemDeRegister_NotFound)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));

    AscendMrAttr memInfo;
    memInfo.addr = 0x10000000;
    memInfo.size = 4096;
    memInfo.lkey = 0x9999; // not registered

    HcclResult ret = hcclRdmaMemDeRegister(&memInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============== hcclModifyAscendVerbsQPEx with Modify Failure ===============

TEST_F(InterfaceRdmaP2p, ModifyAscendVerbsQPEx_ModifyFails)
{
    MOCKER(hrtGetDeviceRefresh).stubs().will(invoke(stub_hrtGetDeviceRefresh_success));
    MOCKER(hrtRaTypicalQpModify).stubs().will(invoke(stub_hrtRaTypicalQpModify_Fail));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    PrePopulateQp(300);

    AscendVerbsQPInfo localQPInfo;
    localQPInfo.qpn = 300;
    AscendVerbsQPInfo remoteQPInfo;
    remoteQPInfo.qpn = 400;
    AscendQPQos qpQos;
    qpQos.sl = 4;
    qpQos.tc = 8;

    HcclResult ret = hcclModifyAscendVerbsQPEx(&localQPInfo, &remoteQPInfo, &qpQos);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
}
