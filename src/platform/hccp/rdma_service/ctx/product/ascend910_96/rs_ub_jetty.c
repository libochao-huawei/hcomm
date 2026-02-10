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

void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb)
{
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    unsigned int devIndex;
    int out_buff = 0;
    int ret;

    devIndex = jetty_cb->dev_cb->index;
    in.addr = (uint64_t)(uintptr_t)&jetty_cb->jetty;
    in.len = sizeof(urma_jetty_t *);
    in.opcode = (jetty_cb->jetty_mode == JETTY_MODE_CCU_TA_CACHE) ?
        UDMA_U_USER_CTL_DELETE_LOCK_BUFFER_JETTY_EX : UDMA_U_USER_CTL_DELETE_JETTY_EX;
    out.addr = (uint64_t)(uintptr_t)&out_buff;
    out.len = sizeof(int);

    ret = rs_urma_user_ctl(jetty_cb->dev_cb->urma_ctx, &in, &out);
    if (ret != 0) {
        hccp_err("rs_urma_user_ctl delete jetty_id:%u failed, devIndex:%u ret:%d errno:%d", jetty_cb->jetty_id,
            devIndex, ret, errno);
    }

    return;
}

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg)
{
    struct udma_u_jetty_cfg_ex jetty_ex_cfg = {0};
    struct udma_u_jetty_info jetty_info = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret;

    jetty_ex_cfg.base_cfg = *jetty_cfg;
    jetty_ex_cfg.jfs_cstm.flag.bs.sq_cstm = jetty_cb->ext_mode.cstm_flag.bs.sq_cstm;
    jetty_ex_cfg.jfs_cstm.sq.buff = (void *)(uintptr_t)jetty_cb->ext_mode.sq.buff_va;
    jetty_ex_cfg.jfs_cstm.sq.buff_size = jetty_cb->ext_mode.sq.buff_size;
    jetty_ex_cfg.pi_type = jetty_cb->ext_mode.pi_type;
    jetty_ex_cfg.sqebb_num = jetty_cb->ext_mode.sqebb_num;
    // JETTY_MODE_USER_CTL_NORMAL jetty need to use tgid to mmap db addr to support write value
    if (jetty_cb->jetty_mode == JETTY_MODE_USER_CTL_NORMAL) {
        jetty_ex_cfg.jetty_type = UDMA_U_NORMAL_JETTY_TYPE;
        jetty_ex_cfg.jfs_cstm.flag.bs.tg_id = 1;
        jetty_ex_cfg.jfs_cstm.tgid = (uint32_t)jetty_cb->dev_cb->rscb->aicpuPid;
    } else if (jetty_cb->jetty_mode == JETTY_MODE_CACHE_LOCK_DWQE) {
        jetty_ex_cfg.jetty_type = UDMA_U_CACHE_LOCK_DWQE_JETTY_TYPE;
    } else {
        jetty_ex_cfg.jetty_type = UDMA_U_CCU_JETTY_TYPE;
    }

    in.len = (uint32_t)sizeof(struct udma_u_jetty_cfg_ex);
    in.addr = (uint64_t)(uintptr_t)&jetty_ex_cfg;
    in.opcode = UDMA_U_USER_CTL_CREATE_JETTY_EX;

    hccp_dbg("sq.buff:0x%llx, sq.buff_size:%u, tgid:%u, pi_type:%d, sqebb_num:%u", 
        jetty_cb->ext_mode.sq.buff_va,jetty_cb->ext_mode.sq.buff_size, jetty_ex_cfg.jfs_cstm.tgid, 
        jetty_cb->ext_mode.pi_type, jetty_cb->ext_mode.sqebb_num);

    out.addr = (uint64_t)(uintptr_t)&jetty_info;
    out.len = sizeof(struct udma_u_jetty_info);
    ret = rs_urma_user_ctl(jetty_cb->dev_cb->urma_ctx, &in, &out);
    if (ret != 0) {
        jetty_cb->jetty = NULL;
        hccp_err("rs_urma_user_ctl create jetty failed, ret:%d, errno:%d", ret, errno);
        return;
    }

    jetty_cb->jetty = jetty_info.jetty;
    jetty_cb->db_addr = (uint64_t)(uintptr_t)jetty_info.db_addr;

    // ccu jetty reg db addr
    if (jetty_cb->jetty_mode == JETTY_MODE_CCU) {
        ret = rs_ub_ctx_reg_jetty_db(jetty_cb, &jetty_info);
        if (ret != 0) {
            rs_ub_ctx_ext_jetty_delete(jetty_cb);
            jetty_cb->jetty = NULL;
            hccp_err("rs_ub_ctx_reg_jetty_db failed, ret:%d", ret);
        }
    }
}

void rs_ub_va_munmap_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    return; // The current version does not require processing
}

void rs_ub_free_jetty_id_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    return; // The current version does not require processing
}
