/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ut_dispatch.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ra_adp.h"
#include "ra_rs_comm.h"
#include "hccp_common.h"
#include "tc_adp.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

/* Duplicate struct RsOps definition from ra_adp.c (private to that TU) */
struct RsOps {
    int (*rdevInit)(struct rdev rdevInfo, unsigned int notifyType, unsigned int *rdevIndex);
    int (*rdevInitWithBackup)(struct rdev rdevInfo, struct rdev backupRdevInfo,
        unsigned int notifyType, unsigned int *rdevIndex);
    int (*rdevGetPortStatus)(unsigned int phyId, unsigned int rdevIndex, enum PortStatus *status);
    int (*rdevDeinit)(unsigned int phyId, unsigned int notifyType, unsigned int rdevIndex);
    int (*qpCreate)(unsigned int phyId, unsigned int rdevIndex, struct RsQpNorm qpNorm,
        struct RsQpResp *qpResp);
    int (*qpCreateWithAttrs)(unsigned int phyId, unsigned int rdevIndex,
        struct RsQpNormWithAttrs *qpNorm, struct RsQpRespWithAttrs *qpResp);
    int (*qpCreateWithCQWithAttrs)(unsigned int phyId, unsigned int rdevIndex,
        unsigned int sendCqn, unsigned int recvCqn,
        struct RsQpNormWithAttrs *qpNorm, struct RsQpRespWithAttrs *qpResp);
    int (*qpDestroy)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn);
    int (*qpDestroyWithoutCQ)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn);
    int (*typicalQpModify)(unsigned int phyId, unsigned int rdevIndex, struct TypicalQp localQpInfo,
        struct TypicalQp remoteQpInfo, unsigned int *udpSport);
    int (*typicalCqCreate)(unsigned int phyId, unsigned int rdevIndex, unsigned int cqDepth,
        unsigned int *cqn);
    int (*typicalCqDestroy)(unsigned int phyId, unsigned int rdevIndex, unsigned int cqn);
    int (*getLiteCqAttr)(unsigned int phyId, unsigned int rdevIndex, unsigned int cqn,
        struct rdma_lite_device_cq_attr *deviceCqAttr);
    int (*qpBatchModify)(unsigned int phyId, unsigned int rdevIndex, int status, int qpn[], int qpnNum);
    int (*qpConnectAsync)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, int fd);
    int (*getQpStatus)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
        struct RsQpStatusInfo *qpInfo);
    int (*mrReg)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct RdmaMrRegInfo *mrRegInfo);
    int (*mrDereg)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, char *addr);
    int (*registerMr)(unsigned int phyId, unsigned int rdevIndex, struct RdmaMrRegInfo *mrRegInfo,
        void **mrHandle);
    int (*typicalRegisterMr)(unsigned int phyId, unsigned int rdevIndex, struct RdmaMrRegInfo *mrRegInfo,
        void **mrHandle);
    int (*remapMr)(unsigned int phyId, unsigned int rdevIndex, struct MemRemapInfo memList[],
        unsigned int memNum);
    int (*typicalDeregisterMr)(unsigned int phyId, unsigned int devIndex, unsigned long long addr);
    int (*sendWr)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct SendWr *wr,
        struct SendWrRsp *wrRsp);
    int (*sendWrList)(struct RsWrlistBaseInfo baseInfo, struct WrInfo *wrList,
        unsigned int sendNum, struct SendWrRsp *wrRsp, unsigned int *completeNum);
    int (*getNotifyMrInfo)(unsigned int phyId, unsigned int rdevIndex, struct MrInfoT *info);
    int (*notifyCfgSet)(unsigned int phyId, unsigned long long va, unsigned long long size);
    int (*notifyCfgGet)(unsigned int phyId, unsigned long long *va, unsigned long long *size);
    int (*setHostPid)(unsigned int phyId, pid_t hostPid, const char *pidSign);
    int (*getInterfaceVersion)(unsigned int opcode, unsigned int *version);
    int (*setTsqpDepth)(unsigned int phyId, unsigned int rdevIndex, unsigned int tempDepth, unsigned int *qpNum);
    int (*getTsqpDepth)(unsigned int phyId, unsigned int rdevIndex, unsigned int *tempDepth, unsigned int *qpNum);
    int (*setQpAttrQos)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct QosAttr *attr);
    int (*setQpAttrTimeout)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, unsigned int *timeout);
    int (*setQpAttrRetryCnt)(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn,
        unsigned int *retryCnt);
    int (*getCqeErrInfo)(struct CqeErrInfo *info);
    int (*getLiteSupport)(unsigned int phyId, unsigned int rdevIndex, int *supportLite);
    int (*getLiteRdevCap)(unsigned int phyId, unsigned int rdevIndex, struct LiteRdevCapResp *resp);
    int (*getLiteQpCqAttr)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteQpCqAttrResp *resp);
    int (*getLiteQpAttr)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteQpAttrResp *resp);
    int (*getLiteMemAttr)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteMemAttrResp *resp);
    int (*getLiteConnectedInfo)(
        unsigned int phyId, unsigned int rdevIndex, unsigned int qpn, struct LiteConnectedInfoResp *resp);
    int (*getCqeErrInfoNum)(unsigned int phyId, unsigned int rdevIdx, unsigned int *num);
    int (*getCqeErrInfoList)(unsigned int phyId, unsigned int rdevIdx, struct CqeErrInfo *info,
        unsigned int *num);
    int (*getTlsEnable)(unsigned int phyId, bool *tlsEnable);
    int (*getSecRandom)(int *value);
    int (*getHccnCfg)(unsigned int phyId, enum HccnCfgKey key, char *value, unsigned int *valueLen);
};

/* Access gRaRsOps from ra_adp.c */
extern struct RsOps gRaRsOps;

/* Stub function pointers for gRaRsOps */
static int stub_TypicalCqCreate(unsigned int phyId, unsigned int rdevIndex,
    unsigned int cqDepth, unsigned int *cqn)
{
    *cqn = 100;
    return 0;
}

static int stub_TypicalCqCreate_Fail(unsigned int phyId, unsigned int rdevIndex,
    unsigned int cqDepth, unsigned int *cqn)
{
    return -ENOMEM;
}

static int stub_TypicalCqDestroy(unsigned int phyId, unsigned int rdevIndex, unsigned int cqn)
{
    return 0;
}

static int stub_TypicalCqDestroy_Fail(unsigned int phyId, unsigned int rdevIndex, unsigned int cqn)
{
    return -EINVAL;
}

static int stub_GetLiteCqAttr(unsigned int phyId, unsigned int rdevIndex, unsigned int cqn,
    struct rdma_lite_device_cq_attr *deviceCqAttr)
{
    deviceCqAttr->cqn = cqn;
    deviceCqAttr->depth = 256;
    return 0;
}

static int stub_QpCreateWithCQ(unsigned int phyId, unsigned int rdevIndex,
    unsigned int sendCqn, unsigned int recvCqn,
    struct RsQpNormWithAttrs *qpNorm, struct RsQpRespWithAttrs *qpResp)
{
    qpResp->qpn = 200;
    return 0;
}

static int stub_QpDestroyWithoutCQ(unsigned int phyId, unsigned int rdevIndex, unsigned int qpn)
{
    return 0;
}

/* ===================================================================
 * TcAdpRaRsTypicalCqCreateDestroy - Tests adapter CQ create/destroy
 * =================================================================== */
void TcAdpRaRsTypicalCqCreateDestroy(void)
{
    int ret;

    /* Save original function pointers */
    int (*origCreate)(unsigned int, unsigned int, unsigned int, unsigned int *) = gRaRsOps.typicalCqCreate;
    int (*origDestroy)(unsigned int, unsigned int, unsigned int) = gRaRsOps.typicalCqDestroy;

    /* Test 1: TypicalCqCreate success */
    gRaRsOps.typicalCqCreate = stub_TypicalCqCreate;
    unsigned int cqn = 0;
    ret = gRaRsOps.typicalCqCreate(0, 0, 128, &cqn);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(cqn, 100);

    /* Test 2: TypicalCqCreate fail */
    gRaRsOps.typicalCqCreate = stub_TypicalCqCreate_Fail;
    cqn = 0;
    ret = gRaRsOps.typicalCqCreate(0, 0, 256, &cqn);
    EXPECT_INT_EQ(ret, -ENOMEM);

    /* Test 3: TypicalCqDestroy success */
    gRaRsOps.typicalCqDestroy = stub_TypicalCqDestroy;
    ret = gRaRsOps.typicalCqDestroy(0, 0, 100);
    EXPECT_INT_EQ(ret, 0);

    /* Test 4: TypicalCqDestroy fail */
    gRaRsOps.typicalCqDestroy = stub_TypicalCqDestroy_Fail;
    ret = gRaRsOps.typicalCqDestroy(0, 0, 999);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* Restore */
    gRaRsOps.typicalCqCreate = origCreate;
    gRaRsOps.typicalCqDestroy = origDestroy;
}

/* ===================================================================
 * TcAdpRaRsLiteCqAttr - Tests getLiteCqAttr
 * =================================================================== */
void TcAdpRaRsLiteCqAttr(void)
{
    int ret;
    struct rdma_lite_device_cq_attr deviceCqAttr;

    int (*origGetLiteCqAttr)(unsigned int, unsigned int, unsigned int, struct rdma_lite_device_cq_attr *) =
        gRaRsOps.getLiteCqAttr;

    gRaRsOps.getLiteCqAttr = stub_GetLiteCqAttr;

    memset(&deviceCqAttr, 0, sizeof(deviceCqAttr));
    ret = gRaRsOps.getLiteCqAttr(0, 0, 42, &deviceCqAttr);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(deviceCqAttr.cqn, 42);
    EXPECT_INT_EQ(deviceCqAttr.depth, 256);

    gRaRsOps.getLiteCqAttr = origGetLiteCqAttr;
}

/* ===================================================================
 * TcAdpRaRsQpCreateWithCQ - Tests qpCreateWithCQWithAttrs
 * =================================================================== */
void TcAdpRaRsQpCreateWithCQ(void)
{
    int ret;
    struct RsQpNormWithAttrs qpNorm = {0};
    struct RsQpRespWithAttrs qpResp = {0};

    int (*origQpCreateWithCQ)(unsigned int, unsigned int, unsigned int, unsigned int,
        struct RsQpNormWithAttrs *, struct RsQpRespWithAttrs *) = gRaRsOps.qpCreateWithCQWithAttrs;

    gRaRsOps.qpCreateWithCQWithAttrs = stub_QpCreateWithCQ;

    ret = gRaRsOps.qpCreateWithCQWithAttrs(0, 0, 10, 20, &qpNorm, &qpResp);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(qpResp.qpn, 200);

    gRaRsOps.qpCreateWithCQWithAttrs = origQpCreateWithCQ;
}

/* ===================================================================
 * TcAdpRaRsQpDestroyWithoutCQ - Tests qpDestroyWithoutCQ
 * =================================================================== */
void TcAdpRaRsQpDestroyWithoutCQ(void)
{
    int ret;

    int (*origQpDestroyWithoutCQ)(unsigned int, unsigned int, unsigned int) =
        gRaRsOps.qpDestroyWithoutCQ;

    gRaRsOps.qpDestroyWithoutCQ = stub_QpDestroyWithoutCQ;

    ret = gRaRsOps.qpDestroyWithoutCQ(0, 0, 200);
    EXPECT_INT_EQ(ret, 0);

    gRaRsOps.qpDestroyWithoutCQ = origQpDestroyWithoutCQ;
}
