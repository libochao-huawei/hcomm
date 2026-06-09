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
#include "externalinput.h"
#include "interface_hccl.h"
#include "adapter_hccp.h"
#undef private

#include "hccl/base.h"
#include <iostream>

using namespace std;
using namespace hccl;

static RdmaHandle g_fakeRdmaHandle = reinterpret_cast<RdmaHandle>(0xDEADBEEF);
static void* g_fakeCqHandle = reinterpret_cast<void*>(0xCAFEBABE);
static u32 g_nextCqn = 100;

class TypicalQpManagerRdmaP2p : public testing::Test {
protected:
    virtual void SetUp()
    {
        // Set a valid rdmaHandle_ so CHK_RET(GetRdmaHandle) passes
        TypicalQpManager::GetInstance().rdmaHandle_ = g_fakeRdmaHandle;
        std::cout << "TypicalQpManagerRdmaP2p SetUp" << std::endl;
    }
    virtual void TearDown()
    {
        // Clean up maps to avoid state leakage between tests
        TypicalQpManager::GetInstance().cqMap_.clear();
        TypicalQpManager::GetInstance().qpMap_.clear();
        GlobalMockObject::verify();
        std::cout << "TypicalQpManagerRdmaP2p TearDown" << std::endl;
    }
};

// =============== Stub functions ===============

HcclResult stub_hrtGetDevice_success(s32* deviceLogicId)
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

static QpHandle g_fakeQpHandle = reinterpret_cast<QpHandle>(0xBEEF0001);
static u32 g_nextQpn = 200;

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

// =============== CreateCq Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, CreateCq_Success)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(CreateTypicalCq).stubs().will(invoke(stub_CreateTypicalCq_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 128;
    cqInfo.cqn = 0;

    HcclResult ret = TypicalQpManager::GetInstance().CreateCq(cqInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(cqInfo.cqn, static_cast<uint32_t>(0));
    // Verify CQ was inserted into cqMap_
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.size(), static_cast<size_t>(1));
    auto it = TypicalQpManager::GetInstance().cqMap_.find(cqInfo.cqn);
    EXPECT_NE(it, TypicalQpManager::GetInstance().cqMap_.end());
    EXPECT_EQ(it->second.first.cqDepth, static_cast<uint32_t>(128));
    EXPECT_EQ(it->second.second, g_fakeCqHandle);
}

TEST_F(TypicalQpManagerRdmaP2p, CreateCq_RdmaHandleNull)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));

    // Set rdmaHandle_ to nullptr to trigger CHK_PTR_NULL
    TypicalQpManager::GetInstance().rdmaHandle_ = nullptr;

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 128;

    HcclResult ret = TypicalQpManager::GetInstance().CreateCq(cqInfo);
    EXPECT_EQ(ret, HCCL_E_PTR);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.size(), static_cast<size_t>(0));
}

TEST_F(TypicalQpManagerRdmaP2p, CreateCq_CreateTypicalCqFails)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(CreateTypicalCq).stubs().will(invoke(stub_CreateTypicalCq_Fail));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 128;

    HcclResult ret = TypicalQpManager::GetInstance().CreateCq(cqInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.size(), static_cast<size_t>(0));
}

// =============== DestroyCq Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, DestroyCq_Success)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(DestroyTypicalCq).stubs().will(invoke(stub_DestroyTypicalCq_Success));

    // Pre-populate cqMap_ with a known entry
    AscendCQInfo cqInfo;
    cqInfo.cqn = 42;
    cqInfo.cqDepth = 256;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(42), std::make_pair(cqInfo, g_fakeCqHandle)));

    HcclResult ret = TypicalQpManager::GetInstance().DestroyCq(42);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.find(42),
        TypicalQpManager::GetInstance().cqMap_.end());
}

TEST_F(TypicalQpManagerRdmaP2p, DestroyCq_CqnNotFound)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));

    HcclResult ret = TypicalQpManager::GetInstance().DestroyCq(999);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TypicalQpManagerRdmaP2p, DestroyCq_DestroyTypicalCqFails)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(DestroyTypicalCq).stubs().will(invoke(stub_DestroyTypicalCq_Fail));

    // Pre-populate cqMap_
    AscendCQInfo cqInfo;
    cqInfo.cqn = 42;
    cqInfo.cqDepth = 256;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(42), std::make_pair(cqInfo, g_fakeCqHandle)));

    HcclResult ret = TypicalQpManager::GetInstance().DestroyCq(42);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    // Entry should NOT be removed on failure
    EXPECT_NE(TypicalQpManager::GetInstance().cqMap_.find(42),
        TypicalQpManager::GetInstance().cqMap_.end());
}

// =============== ValidateCq Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, ValidateCq_Exists)
{
    AscendCQInfo cqInfo;
    cqInfo.cqn = 55;
    cqInfo.cqDepth = 128;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(55), std::make_pair(cqInfo, g_fakeCqHandle)));

    HcclResult ret = TypicalQpManager::GetInstance().ValidateCq(55);
    EXPECT_EQ(ret, HCCL_SUCCESS);
}

TEST_F(TypicalQpManagerRdmaP2p, ValidateCq_NotFound)
{
    HcclResult ret = TypicalQpManager::GetInstance().ValidateCq(999);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============== GetCqDepth Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, GetCqDepth_Success)
{
    AscendCQInfo cqInfo;
    cqInfo.cqn = 77;
    cqInfo.cqDepth = 512;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(77), std::make_pair(cqInfo, g_fakeCqHandle)));

    uint32_t cqDepth = 0;
    HcclResult ret = TypicalQpManager::GetInstance().GetCqDepth(77, cqDepth);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(cqDepth, static_cast<uint32_t>(512));
}

TEST_F(TypicalQpManagerRdmaP2p, GetCqDepth_NotFound)
{
    uint32_t cqDepth = 0;
    HcclResult ret = TypicalQpManager::GetInstance().GetCqDepth(999, cqDepth);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============== GetCqHandle Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, GetCqHandle_Success)
{
    AscendCQInfo cqInfo;
    cqInfo.cqn = 88;
    cqInfo.cqDepth = 256;
    TypicalQpManager::GetInstance().cqMap_.insert(
        std::make_pair(static_cast<u32>(88), std::make_pair(cqInfo, g_fakeCqHandle)));

    void* cqHandle = nullptr;
    HcclResult ret = TypicalQpManager::GetInstance().GetCqHandle(88, cqHandle);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(cqHandle, g_fakeCqHandle);
}

TEST_F(TypicalQpManagerRdmaP2p, GetCqHandle_NotFound)
{
    void* cqHandle = nullptr;
    HcclResult ret = TypicalQpManager::GetInstance().GetCqHandle(999, cqHandle);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

// =============== CreateQpWithCQ Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, CreateQpWithCQ_Success)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(CreateQpWithCQConfig).stubs().will(invoke(stub_CreateQpWithCQConfig_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    struct TypicalQp qpInfo;
    QpConfigWithCQInfo qpConfig;
    qpConfig.sendCqn = 10;
    qpConfig.recvCqn = 20;
    qpConfig.sq_depth = 1024;
    qpConfig.rq_depth = 128;
    qpConfig.scq_depth = 1024;
    qpConfig.rcq_depth = 128;

    HcclResult ret = TypicalQpManager::GetInstance().CreateQpWithCQ(qpInfo, qpConfig);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_NE(qpInfo.qpn, static_cast<uint32_t>(0));
    EXPECT_EQ(TypicalQpManager::GetInstance().qpMap_.size(), static_cast<size_t>(1));
}

TEST_F(TypicalQpManagerRdmaP2p, CreateQpWithCQ_CreateFails)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(CreateQpWithCQConfig).stubs().will(invoke(stub_CreateQpWithCQConfig_Fail));

    struct TypicalQp qpInfo;
    QpConfigWithCQInfo qpConfig;

    HcclResult ret = TypicalQpManager::GetInstance().CreateQpWithCQ(qpInfo, qpConfig);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    EXPECT_EQ(TypicalQpManager::GetInstance().qpMap_.size(), static_cast<size_t>(0));
}

// =============== DestroyQpWithoutCQ Tests ===============

TEST_F(TypicalQpManagerRdmaP2p, DestroyQpWithoutCQ_Success)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(HrtRaQpDestroyWithoutCQ).stubs().will(invoke(stub_HrtRaQpDestroyWithoutCQ_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    // Pre-populate qpMap_ with a known QP
    struct TypicalQp qpInfo;
    qpInfo.qpn = 300;
    TypicalQpManager::GetInstance().qpMap_.insert(
        std::make_pair(static_cast<u32>(300), std::make_pair(qpInfo, g_fakeQpHandle)));

    HcclResult ret = TypicalQpManager::GetInstance().DestroyQpWithoutCQ(qpInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().qpMap_.find(300),
        TypicalQpManager::GetInstance().qpMap_.end());
}

TEST_F(TypicalQpManagerRdmaP2p, DestroyQpWithoutCQ_QpnZero)
{
    struct TypicalQp qpInfo;
    qpInfo.qpn = 0;

    HcclResult ret = TypicalQpManager::GetInstance().DestroyQpWithoutCQ(qpInfo);
    EXPECT_EQ(ret, HCCL_E_PARA);
}

TEST_F(TypicalQpManagerRdmaP2p, DestroyQpWithoutCQ_QpNotFound)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    struct TypicalQp qpInfo;
    qpInfo.qpn = 999;

    HcclResult ret = TypicalQpManager::GetInstance().DestroyQpWithoutCQ(qpInfo);
    EXPECT_EQ(ret, HCCL_E_NOT_FOUND);
}

TEST_F(TypicalQpManagerRdmaP2p, DestroyQpWithoutCQ_DestroyFails)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(HrtRaQpDestroyWithoutCQ).stubs().will(invoke(stub_HrtRaQpDestroyWithoutCQ_Fail));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    struct TypicalQp qpInfo;
    qpInfo.qpn = 400;
    TypicalQpManager::GetInstance().qpMap_.insert(
        std::make_pair(static_cast<u32>(400), std::make_pair(qpInfo, g_fakeQpHandle)));

    HcclResult ret = TypicalQpManager::GetInstance().DestroyQpWithoutCQ(qpInfo);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);
    // Entry should NOT be removed on failure
    EXPECT_NE(TypicalQpManager::GetInstance().qpMap_.find(400),
        TypicalQpManager::GetInstance().qpMap_.end());
}

// =============== Integration: CreateCq + DestroyCq ===============

TEST_F(TypicalQpManagerRdmaP2p, CreateCqAndDestroyCq_Integration)
{
    MOCKER(hrtGetDevice).stubs().will(invoke(stub_hrtGetDevice_success));
    MOCKER(CreateTypicalCq).stubs().will(invoke(stub_CreateTypicalCq_Success));
    MOCKER(DestroyTypicalCq).stubs().will(invoke(stub_DestroyTypicalCq_Success));
    MOCKER(GetExternalInputRdmaRetryCnt).stubs().will(returnValue(7));
    MOCKER(GetExternalInputRdmaTimeOut).stubs().will(returnValue(14));

    // Create
    AscendCQInfo cqInfo;
    cqInfo.cqDepth = 256;
    cqInfo.cqn = 0;
    HcclResult ret = TypicalQpManager::GetInstance().CreateCq(cqInfo);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.size(), static_cast<size_t>(1));

    // Destroy
    ret = TypicalQpManager::GetInstance().DestroyCq(cqInfo.cqn);
    EXPECT_EQ(ret, HCCL_SUCCESS);
    EXPECT_EQ(TypicalQpManager::GetInstance().cqMap_.size(), static_cast<size_t>(0));
}
