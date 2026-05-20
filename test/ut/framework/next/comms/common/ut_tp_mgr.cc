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
#include <cstring>

#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>

#include "tp_mgr.h"
#include "orion_adpt_utils.h"
#include "rdma_handle_manager.h"
#include "hccp.h"

using namespace hcomm;

namespace {

void FillIpv4CommAddr(CommAddr &ca, const char *dotted)
{
    ca.type = COMM_ADDR_TYPE_IP_V4;
    EXPECT_EQ(inet_pton(AF_INET, dotted, &ca.addr), 1);
}

GetTpInfoParam MakeParam(const char *loc, const char *rmt, TpProtocol proto, uint32_t qos = 0U)
{
    GetTpInfoParam param;
    FillIpv4CommAddr(param.locAddr, loc);
    FillIpv4CommAddr(param.rmtAddr, rmt);
    param.tpProtocol = proto;
    param.qos = qos;
    return param;
}

HcclResult PollGetTpInfo(TpMgr &mgr, const GetTpInfoParam &param, TpInfo &tpInfo)
{
    for (int i = 0; i < 8; ++i) {
        const HcclResult ret = mgr.GetTpInfo(param, tpInfo);
        if (ret != HCCL_E_AGAIN) {
            return ret;
        }
    }
    return HCCL_E_AGAIN;
}

int StubRaGetTpAttrAsyncUboe(void *ctxHandle, uint64_t tpHandle, uint32_t *attrBitmap, struct TpAttr *attr,
    void **reqHandle)
{
    static char kUboeTpAttrReq{};
    (void)ctxHandle;
    (void)tpHandle;
    (void)attrBitmap;
    if (attr != nullptr) {
        (void)std::memset(attr, 0, sizeof(struct TpAttr));
        attr->slBitmap = 0x7U;
        attr->dscpConfigMode = 0U;
    }
    if (reqHandle != nullptr) {
        *reqHandle = &kUboeTpAttrReq;
    }
    return 0;
}

int StubRaGetTpInfoListAsyncUboeEight(void *ctxHandle, struct GetTpCfg *cfg, struct HccpTpInfo infoList[],
    unsigned int *num, void **reqHandle)
{
    static int kUboeTpListReq = 22378;
    (void)ctxHandle;
    (void)cfg;
    if (infoList != nullptr) {
        for (unsigned int i = 0; i < 8U; ++i) {
            infoList[i].tpHandle = 0x100ULL + static_cast<uint64_t>(i);
        }
    }
    if (num != nullptr) {
        *num = 8U;
    }
    if (reqHandle != nullptr) {
        *reqHandle = &kUboeTpListReq;
    }
    return 0;
}

int StubRaGetTpAttrAsyncUboeSl789(void *ctxHandle, uint64_t tpHandle, uint32_t *attrBitmap, struct TpAttr *attr,
    void **reqHandle)
{
    static char kUboeTpAttrSl789Req{};
    (void)ctxHandle;
    (void)tpHandle;
    (void)attrBitmap;
    if (attr != nullptr) {
        (void)std::memset(attr, 0, sizeof(struct TpAttr));
        attr->slBitmap = (1U << 7U) | (1U << 8U) | (1U << 9U);
        attr->dscpConfigMode = 0U;
    }
    if (reqHandle != nullptr) {
        *reqHandle = &kUboeTpAttrSl789Req;
    }
    return 0;
}

} // namespace

class TpMgrTest : public testing::Test {
protected:
    void SetUp() override
    {
        MOCKER_CPP(&Hccl::RdmaHandleManager::GetByIp)
            .stubs()
            .will(returnValue(reinterpret_cast<Hccl::RdmaHandle>(static_cast<uintptr_t>(0x12345678U))));
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_Rtp_WithQos_Expect_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.1.1", "10.10.1.2", TpProtocol::RTP, 5U);
    TpInfo tpInfo{};
    EXPECT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_NE(tpInfo.tpHandle, 0U);
    EXPECT_TRUE(tpInfo.hasMappedJettyPriority);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_CacheHit_Expect_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.2.1", "10.10.2.2", TpProtocol::RTP, 3U);
    TpInfo tpInfo{};
    ASSERT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_EQ(mgr.GetTpInfo(param, tpInfo), HCCL_SUCCESS);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_LoopFirstTpLowestSl_Expect_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    GetTpInfoParam param = MakeParam("10.10.3.1", "10.10.3.2", TpProtocol::RTP, 6U);
    param.loopFirstTpLowestSl = true;
    TpInfo tpInfo{};
    EXPECT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_TRUE(tpInfo.hasMappedJettyPriority);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_Ctp_Expect_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.4.1", "10.10.4.2", TpProtocol::CTP, 2U);
    TpInfo tpInfo{};
    EXPECT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_Uboe_WithQos_Expect_Success)
{
    MOCKER(RaGetTpAttrAsync).stubs().will(invoke(StubRaGetTpAttrAsyncUboe));

    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.5.1", "10.10.5.2", TpProtocol::UBOE, 4U);
    TpInfo tpInfo{};
    EXPECT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_Uboe_EightTp_Sl789_Qos0_Expect_LastTp_Sl9)
{
    MOCKER(RaGetTpInfoListAsync).stubs().will(invoke(StubRaGetTpInfoListAsyncUboeEight));
    MOCKER(RaGetTpAttrAsync).stubs().will(invoke(StubRaGetTpAttrAsyncUboeSl789));

    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.10.1", "10.10.10.2", TpProtocol::UBOE, 0U);
    TpInfo tpInfo{};
    ASSERT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_EQ(tpInfo.tpHandle, 0x107ULL);
    EXPECT_TRUE(tpInfo.hasMappedJettyPriority);
    EXPECT_EQ(tpInfo.mappedJettyPriority, 9U);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_Uboe_EightTp_Sl789_Qos7_Expect_FirstTp_Sl7)
{
    MOCKER(RaGetTpInfoListAsync).stubs().will(invoke(StubRaGetTpInfoListAsyncUboeEight));
    MOCKER(RaGetTpAttrAsync).stubs().will(invoke(StubRaGetTpAttrAsyncUboeSl789));

    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.10.3", "10.10.10.4", TpProtocol::UBOE, 7U);
    TpInfo tpInfo{};
    ASSERT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_EQ(tpInfo.tpHandle, 0x100ULL);
    EXPECT_EQ(tpInfo.mappedJettyPriority, 7U);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_DifferentQosKeys_Expect_Both_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam paramQos0 = MakeParam("10.10.6.1", "10.10.6.2", TpProtocol::RTP, 0U);
    const GetTpInfoParam paramQos7 = MakeParam("10.10.6.1", "10.10.6.2", TpProtocol::RTP, 7U);
    TpInfo tpInfo0{};
    TpInfo tpInfo7{};
    EXPECT_EQ(PollGetTpInfo(mgr, paramQos0, tpInfo0), HCCL_SUCCESS);
    EXPECT_EQ(PollGetTpInfo(mgr, paramQos7, tpInfo7), HCCL_SUCCESS);
    EXPECT_NE(tpInfo0.tpHandle, 0U);
    EXPECT_NE(tpInfo7.tpHandle, 0U);
}

TEST_F(TpMgrTest, Ut_TpMgr_ReleaseTpInfo_AfterGet_Expect_Success)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.7.1", "10.10.7.2", TpProtocol::RTP, 1U);
    TpInfo tpInfo{};
    ASSERT_EQ(PollGetTpInfo(mgr, param, tpInfo), HCCL_SUCCESS);
    EXPECT_EQ(mgr.ReleaseTpInfo(param, tpInfo), HCCL_SUCCESS);
}

TEST_F(TpMgrTest, Ut_TpMgr_ReleaseTpInfo_NotFound_Expect_NotFound)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    const GetTpInfoParam param = MakeParam("10.10.8.1", "10.10.8.2", TpProtocol::RTP, 0U);
    TpInfo tpInfo{};
    EXPECT_EQ(mgr.ReleaseTpInfo(param, tpInfo), HCCL_E_NOT_FOUND);
}

TEST_F(TpMgrTest, Ut_TpMgr_GetTpInfo_UnsupportedProtocol_Expect_NotSupport)
{
    TpMgr &mgr = TpMgr::GetInstance(0);
    GetTpInfoParam param = MakeParam("10.10.9.1", "10.10.9.2", TpProtocol::RTP, 0U);
    param.tpProtocol = TpProtocol::INVALID;
    TpInfo tpInfo{};
    EXPECT_EQ(mgr.GetTpInfo(param, tpInfo), HCCL_E_NOT_SUPPORT);
}

TEST_F(TpMgrTest, GetTpTotalTimeout_ValidAtGear_ReturnsCorrectTimeout)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 0;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 16U);

    tpAttrInfo.tpAttr.at = 3;
    tpAttrInfo.tpAttr.retryTimesInit = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 4000U);
}

TEST_F(TpMgrTest, GetTpTotalTimeout_InvalidAtGear_UsesDefault)
{
    TpAttrInfo tpAttrInfo{};
    tpAttrInfo.tpAttr.at = 5;
    tpAttrInfo.tpAttr.retryTimesInit = 0;

    uint32_t tpTimeOutMs = 0;
    EXPECT_EQ(TpMgr::GetTpTotalTimeout(tpAttrInfo, tpTimeOutMs), HCCL_SUCCESS);
    EXPECT_EQ(tpTimeOutMs, 1000U);
}
