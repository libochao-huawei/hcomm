/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <urma_opcode.h>
#include <udma_u_ctl.h>
#include "securec.h"
#include "user_log.h"
#include "dl_urma_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub.h"
#include "rs_ub_jetty.h"

void RsUbCtxExtJettyDelete(struct RsCtxJettyCb *jettyCb)
{
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    unsigned int devIndex;
    int outBuff = 0;
    int ret;

    devIndex = jettyCb->devCb->index;
    in.addr = (uint64_t)(uintptr_t)&jettyCb->jetty;
    in.len = sizeof(urma_jetty_t *);
    in.opcode = (jettyCb->jettyMode == JETTY_MODE_CCU_TA_CACHE) ?
        UDMA_U_USER_CTL_DELETE_LOCK_BUFFER_JETTY_EX : UDMA_U_USER_CTL_DELETE_JETTY_EX;
    out.addr = (uint64_t)(uintptr_t)&outBuff;
    out.len = sizeof(int);

    ret = RsUrmaUserCtl(jettyCb->devCb->urmaCtx, &in, &out);
    if (ret != 0) {
        hccp_err("rs_urma_user_ctl delete jetty_id:%u failed, devIndex:%u ret:%d errno:%d", jettyCb->jettyId,
            devIndex, ret, errno);
    }

    return;
}

void RsUbCtxExtJettyCreate(struct RsCtxJettyCb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    struct udma_u_jetty_cfg_ex jettyExCfg = {0};
    struct udma_u_jetty_info jettyInfo = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret;

    jettyExCfg.base_cfg = *jettyCfg;
    jettyExCfg.jfs_cstm.flag.bs.sq_cstm = jettyCb->extMode.cstmFlag.bs.sqCstm;
    jettyExCfg.jfs_cstm.sq.buff = (void *)(uintptr_t)jettyCb->extMode.sq.buffVa;
    jettyExCfg.jfs_cstm.sq.buff_size = jettyCb->extMode.sq.buffSize;
    jettyExCfg.pi_type = jettyCb->extMode.piType;
    jettyExCfg.sqebb_num = jettyCb->extMode.sqebbNum;
    // JETTY_MODE_USER_CTL_NORMAL jetty need to use tgid to mmap db addr to support write value
    if (jettyCb->jettyMode == JETTY_MODE_USER_CTL_NORMAL) {
        jettyExCfg.jetty_type = UDMA_U_NORMAL_JETTY_TYPE;
        jettyExCfg.jfs_cstm.flag.bs.tg_id = 1;
        jettyExCfg.jfs_cstm.tgid = (uint32_t)jettyCb->devCb->rscb->aicpuPid;
    } else if (jettyCb->jettyMode == JETTY_MODE_CACHE_LOCK_DWQE) {
        jettyExCfg.jetty_type = UDMA_U_CACHE_LOCK_DWQE_JETTY_TYPE;
    } else {
        jettyExCfg.jetty_type = UDMA_U_CCU_JETTY_TYPE;
    }

    in.len = (uint32_t)sizeof(struct udma_u_jetty_cfg_ex);
    in.addr = (uint64_t)(uintptr_t)&jettyExCfg;
    in.opcode = UDMA_U_USER_CTL_CREATE_JETTY_EX;

    hccp_dbg("sq.buff:0x%llx, sq.buffSize:%u, tgid:%u, piType:%d, sqebbNum:%u", 
        jettyCb->extMode.sq.buffVa,jettyCb->extMode.sq.buffSize, jettyExCfg.jfs_cstm.tgid, 
        jettyCb->extMode.piType, jettyCb->extMode.sqebbNum);

    out.addr = (uint64_t)(uintptr_t)&jettyInfo;
    out.len = sizeof(struct udma_u_jetty_info);
    ret = RsUrmaUserCtl(jettyCb->devCb->urmaCtx, &in, &out);
    if (ret != 0) {
        jettyCb->jetty = NULL;
        hccp_err("rs_urma_user_ctl create jetty failed, ret:%d, errno:%d", ret, errno);
        return;
    }

    jettyCb->jetty = jettyInfo.jetty;
    jettyCb->dbAddr = (uint64_t)(uintptr_t)jettyInfo.db_addr;

    // ccu jetty reg db addr
    if (jettyCb->jettyMode == JETTY_MODE_CCU) {
        ret = RsUbCtxRegJettyDb(jettyCb, &jettyInfo);
        if (ret != 0) {
            RsUbCtxExtJettyDelete(jettyCb);
            jettyCb->jetty = NULL;
            hccp_err("rs_ub_ctx_reg_jetty_db failed, ret:%d", ret);
        }
    }
}

void RsUbVaMunmapBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    return; // The current version does not require processing
}

void RsUbFreeJettyIdBatch(struct RsCtxJettyCb **jettyCbArr, unsigned int num)
{
    return; // The current version does not require processing
}
