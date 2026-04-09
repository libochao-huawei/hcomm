/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "network/hccp.h"
#include "dlra_function.h"

#define private public
#define protected public
#include "network_manager_pub.h"
#undef private

using namespace std;
using namespace hccl;

class HccpTest : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "HccpTest SetUP" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "HccpTest TearDown" << std::endl;
    }
};

TEST_F(HccpTest, ut_hrtRdmaInitWithBackupAttr_notSupport)
{
    struct RdevInitInfo redvInitInfo;
    redvInitInfo.mode = 1;
    redvInitInfo.notifyType = 0;
    redvInitInfo.enabled910aLite = false;
    redvInitInfo.disabledLiteThread = false;
    redvInitInfo.enabled2mbLite = false;

    struct rdev rdevInfo;
    rdevInfo.phyId = 0;
    struct rdev backupRdevInfo;
    backupRdevInfo.phyId = 1;
    RdmaHandle rdmaHandle;

    MOCKER(hrtRaGetInterfaceVersion).expects(atMost(1)).will(returnValue(HCCL_E_NOT_SUPPORT));
    auto ret = HrtRdmaInitWithBackupAttr(redvInitInfo, rdevInfo, backupRdevInfo, rdmaHandle);
    EXPECT_EQ(ret, HCCL_E_NOT_SUPPORT);
    GlobalMockObject::verify();
}

TEST_F(HccpTest, ut_CheckAutoListenVersion)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    HcclResult ret = NetworkManager::GetInstance(0).CheckAutoListenVersion(true);
    EXPECT_EQ(ret, (ret, HCCL_E_NOT_SUPPORT));
    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateNormalQp_SetQpAttrQosFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(hrtRaNormalQpDestroy)
    .expects(atMost(1))
    .will(returnValue(0));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    HcclResult ret = CreateNormalQp(rdmaHandle, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateNormalQp_SetQpAttrTimeOutFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrTimeOut)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(hrtRaNormalQpDestroy)
    .expects(atMost(1))
    .will(returnValue(0));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    HcclResult ret = CreateNormalQp(rdmaHandle, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateNormalQp_SetQpAttrRetryCntFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaNormalQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrTimeOut)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrRetryCnt)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(hrtRaNormalQpDestroy)
    .expects(atMost(1))
    .will(returnValue(0));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    HcclResult ret = CreateNormalQp(rdmaHandle, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateQp_SetQpAttrQosFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(HrtRaQpDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    s32 qpMode = 0;
    HcclResult ret = CreateQp(rdmaHandle, 0, qpMode, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateQp_SetQpAttrTimeOutFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrTimeOut)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(HrtRaQpDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    s32 qpMode = 0;
    HcclResult ret = CreateQp(rdmaHandle, 0, qpMode, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}

TEST_F(HccpTest, Ut_CreateQp_SetQpAttrRetryCntFail_Expect_DestoryQp)
{
    MOCKER(hrtRaGetInterfaceVersion)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(HrtRaQpCreate)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrQos)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrTimeOut)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    MOCKER(hrtRaSetQpAttrRetryCnt)
    .expects(atMost(1))
    .will(returnValue(HCCL_E_INTERNAL));

    MOCKER(HrtRaQpDestroy)
    .expects(atMost(1))
    .will(returnValue(HCCL_SUCCESS));

    QpInfo qp;
    qp.qpHandle = reinterpret_cast<QpHandle>(0x1234);
    qp.trafficClass = 4;
    qp.serviceLevel = 0;
    qp.attr.maxWr = 128;
    qp.attr.maxSendSge = 1;
    qp.attr.maxRecvSge = 1;

    RdmaHandle rdmaHandle = reinterpret_cast<RdmaHandle>(0x1234);
    s32 qpMode = 0;
    HcclResult ret = CreateQp(rdmaHandle, 0, qpMode, qp);
    EXPECT_EQ(ret, HCCL_E_INTERNAL);

    GlobalMockObject::verify();
}
