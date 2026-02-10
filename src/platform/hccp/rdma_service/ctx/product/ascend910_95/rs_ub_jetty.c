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
#include "dl_net_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub.h"
#include "rs_ub_jetty.h"

STATIC int rs_res_addr_munmap(struct rs_ctx_jetty_cb *jettyCb, struct udma_va_info *vaInfo)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jettyCb->jetty->jetty_id.id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct udma_va_info);
    resInfoIn.priv = (void *)vaInfo;
    ret = DlHalResAddrUnmapV2(jettyCb->dev_cb->rscb->logicId, &resInfoIn);
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrUnmapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), -ret);

    return ret;
}

STATIC int rs_res_addr_mmap(struct rs_ctx_jetty_cb *jettyCb, struct udma_va_info *vaInfo,
    struct res_map_info_out *resInfoOut)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jettyCb->jetty->jetty_id.id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct udma_va_info);
    resInfoIn.priv = (void *)vaInfo;
    ret = DlHalResAddrMapV2(jettyCb->dev_cb->rscb->logicId, &resInfoIn, resInfoOut);
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrMapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), -ret);

    return ret;
}

STATIC void rs_munmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb)
{
    struct udma_va_info vaInfo = {0};

    if ((jettyCb->jetty_mode != JETTY_MODE_CACHE_LOCK_DWQE) && (jettyCb->jetty_mode != JETTY_MODE_USER_CTL_NORMAL)) {
        return;
    }

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JETTY;
    vaInfo.va = jettyCb->sq_buff_va;
    vaInfo.len = WQE_BB_SIZE * jettyCb->tx_depth * WQEBB_NUM_PER_SQE;
    vaInfo.pid = getpid();
    (void)rs_res_addr_munmap(jettyCb, &vaInfo);

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    vaInfo.va = ALIGN_DOWN(jettyCb->db_addr, PAGE_4K);
    vaInfo.len = sizeof(uint64_t);
    vaInfo.pid = getpid();
    (void)rs_res_addr_munmap(jettyCb, &vaInfo);
}

STATIC int rs_mmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb)
{
    struct res_map_info_out jettyVaInfoOut = {0};
    struct res_map_info_out dbVaInfoOut = {0};
    struct udma_va_info jettyVaInfo = {0};
    struct udma_va_info dbVaInfo = {0};
    uint64_t dbOffset = 0;
    int ret = 0;

    jettyVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JETTY;
    jettyVaInfo.va = jettyCb->sq_buff_va;
    jettyVaInfo.len = WQE_BB_SIZE * jettyCb->tx_depth * WQEBB_NUM_PER_SQE;
    jettyVaInfo.pid = getpid();
    ret = rs_res_addr_mmap(jettyCb, &jettyVaInfo, &jettyVaInfoOut);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_res_addr_mmap failed, res_type:%u ret:%d",
        jettyVaInfo.resType, ret), ret);
    jettyCb->sq_buff_va = jettyVaInfoOut.va;

    dbVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    dbVaInfo.va = ALIGN_DOWN(jettyCb->db_addr, PAGE_4K);
    dbOffset = jettyCb->db_addr - dbVaInfo.va;
    dbVaInfo.len = sizeof(uint64_t);
    dbVaInfo.pid = getpid();
    ret = rs_res_addr_mmap(jettyCb, &dbVaInfo, &dbVaInfoOut);
    if (ret != 0) {
        hccp_err("rs_res_addr_mmap failed, res_type:%u ret:%d", dbVaInfo.resType, ret);
        goto munmap_sq_buff_va;
    }
    jettyCb->db_addr = dbVaInfoOut.va + dbOffset;
    return ret;

munmap_sq_buff_va:
    jettyVaInfo.va = jettyVaInfoOut.va;
    ret += rs_res_addr_munmap(jettyCb, &jettyVaInfo);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_res_addr_munmap failed, res_type:%u ret:%d",
        jettyVaInfo.resType, ret), ret);
    return ret;
}

void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jettyCb)
{
    int ret = 0;

    rs_munmap_jetty_va(jettyCb);
    ret = rs_urma_deactive_jetty(jettyCb->jetty);
    if (ret != 0) {
        hccp_err("rs_urma_deactive_jetty failed, ret:%d errno:%d", ret, errno);
    }

    ret = rs_urma_free_jetty(jettyCb->jetty);
    if (ret != 0) {
        hccp_err("rs_urma_free_jetty failed, ret:%d errno:%d", ret, errno);
    }

    if (jettyCb->jetty_mode == JETTY_MODE_CACHE_LOCK_DWQE) {
        ret = rs_net_free_jetty_id(jettyCb->dev_cb->urma_dev->name, jettyCb->jetty_mode, jettyCb->jetty_id);
        if (ret != 0) {
            hccp_err("rs_net_free_jetty_id failed, jetty_id:%u ret:%d", jettyCb->jetty_id, ret);
        }
    }

    return;
}

STATIC int rs_set_jetty_opt(struct rs_ctx_jetty_cb *jettyCb)
{
    uint8_t db_cstm = jettyCb->ext_mode.cstm_flag.bs.db_cstm;
    uint16_t pi_type = jettyCb->ext_mode.pi_type;
    int ret = 0;

    hccp_dbg("sq.buff:0x%llx, sq.buff_size:%u, pi_type:%d, sqebb_num:%u, db_cstm:%u", 
        jettyCb->ext_mode.sq.buff_va,jettyCb->ext_mode.sq.buff_size, pi_type, jettyCb->ext_mode.sqebb_num, db_cstm);

    ret = rs_urma_set_jetty_opt(jettyCb->jetty, URMA_JFS_DB_STATUS, (void *)&db_cstm, sizeof(uint8_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_set_jetty_opt URMA_JFS_DB_STATUS failed, ret:%d, errno:%d",
        ret, errno), -EOPENSRC);

    ret = rs_urma_set_jetty_opt(jettyCb->jetty, URMA_JFS_PI_TYPE, (void *)&pi_type, sizeof(uint16_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_set_jetty_opt URMA_JFS_PI_TYPE failed, ret:%d, errno:%d",
        ret, errno), -EOPENSRC);

    if (jettyCb->jetty_mode == JETTY_MODE_CCU) {
        ret = rs_urma_set_jetty_opt(jettyCb->jetty, URMA_JFS_SQE_BASE_ADDR,
            (void *)&jettyCb->ext_mode.sq.buff_va, sizeof(uint64_t));
        CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_set_jetty_opt URMA_JFS_SQE_BASE_ADDR failed, ret:%d, errno:%d",
            ret, errno), -EOPENSRC);
    }

    return ret;
}

STATIC int rs_get_jetty_opt(struct rs_ctx_jetty_cb *jettyCb)
{
    uint64_t sqBuffVa = 0, dbVa = 0;
    int ret = 0;

    ret = rs_urma_get_jetty_opt(jettyCb->jetty, URMA_JFS_SQE_BASE_ADDR, &sqBuffVa, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_jetty_opt URMA_JFS_SQE_BASE_ADDR failed, ret:%d, errno:%d",
        ret, errno), -EOPENSRC);

    ret = rs_urma_get_jetty_opt(jettyCb->jetty, URMA_JFS_DB_ADDR, &dbVa, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_jetty_opt URMA_JFS_DB_ADDR failed, ret:%d, errno:%d",
        ret, errno), -EOPENSRC);

    jettyCb->sq_buff_va = sqBuffVa;
    jettyCb->db_addr = dbVa;
    if ((jettyCb->jetty_mode == JETTY_MODE_CACHE_LOCK_DWQE) || (jettyCb->jetty_mode == JETTY_MODE_USER_CTL_NORMAL)) {
        ret = rs_mmap_jetty_va(jettyCb);
        CHK_PRT_RETURN(ret != 0, hccp_err("rs_mmap_jetty_va failed, ret:%d", ret), ret);
    }

    return ret;
}

STATIC int rs_free_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id)
{
    int ret = 0;

    if (jetty_mode != JETTY_MODE_CACHE_LOCK_DWQE) {
        return 0;
    }

    // only stars jetty need to free jetty id
    ret = rs_net_free_jetty_id(udev_name, jetty_mode, jetty_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_net_free_jetty_id failed, jetty_id:%u ret:%d", jetty_id, ret), ret);

    return ret;
}

STATIC int rs_jetty_attr_init(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    int ret = 0;

    CHK_PRT_RETURN(jettyCb->ext_mode.cstm_flag.bs.sq_cstm == 1 && jettyCb->jetty_mode != JETTY_MODE_CCU,
        hccp_err("Non-CCU jetty cannot be created by specifying va, sq_cstm:%u jetty_mode:%u",
        jettyCb->ext_mode.cstm_flag.bs.sq_cstm, jettyCb->jetty_mode), -EINVAL);

    if (jettyCb->jetty_mode == JETTY_MODE_CACHE_LOCK_DWQE) {
        ret = rs_net_alloc_jetty_id(jettyCb->dev_cb->urma_dev->name, jettyCb->jetty_mode, &jettyCfg->id);
        CHK_PRT_RETURN(ret != 0, hccp_err("rs_net_alloc_jetty_id failed, ret:%d", ret), ret);
        jettyCb->jetty_id = jettyCfg->id;
    }

    ret = rs_urma_alloc_jetty(jettyCb->dev_cb->urma_ctx, jettyCfg, &jettyCb->jetty);
    if (ret != 0) {
        ret = -EOPENSRC;
        rs_free_jetty_id(jettyCb->dev_cb->urma_dev->name, jettyCb->jetty_mode, jettyCb->jetty_id);
        hccp_err("urma_alloc_jetty failed, ret:%d, errno:%d", ret, errno);
    }

    return ret;
}

STATIC int rs_ccu_jetty_db_reg(struct rs_ctx_jetty_cb *jettyCb)
{
    struct udma_u_jetty_info jetty_info = {0};
    int ret = 0;

    if (jettyCb->jetty_mode != JETTY_MODE_CCU) {
        return 0;
    }

    // only ccu jetty requires db registration
    jetty_info.dwqe_addr = (void *)(ALIGN_DOWN(jettyCb->db_addr, PAGE_4K));
    ret = rs_ub_ctx_reg_jetty_db(jettyCb, &jetty_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_reg_jetty_db failed, ret:%d", ret), ret);

    return ret;
}

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    int ret = 0;

    ret = rs_jetty_attr_init(jettyCb, jettyCfg);
    if (ret != 0) {
        jettyCb->jetty = NULL;
        return;
    }

    ret = rs_set_jetty_opt(jettyCb);
    if (ret != 0) {
        hccp_err("rs_set_jetty_opt failed, ret:%d", ret);
        goto free_jetty;
    }

    ret = rs_urma_active_jetty(jettyCb->jetty);
    if (ret != 0) {
        hccp_err("rs_urma_active_jetty failed, ret:%d, errno:%d", ret, errno);
        ret = -EOPENSRC;
        goto free_jetty;
    }

    ret = rs_get_jetty_opt(jettyCb);
    if (ret != 0) {
        hccp_err("rs_get_jetty_opt failed, ret:%d", ret);
        goto deactive_jetty;
    }

    ret = rs_ccu_jetty_db_reg(jettyCb);
    if (ret != 0) {
        goto deactive_jetty;
    }
    return;

deactive_jetty:
    ret = rs_urma_deactive_jetty(jettyCb->jetty);
    if (ret != 0) {
        hccp_err("rs_urma_deactive_jetty failed, ret:%d errno:%d", ret, errno);
    }
free_jetty:
    ret = rs_urma_free_jetty(jettyCb->jetty);
    if (ret != 0) {
        hccp_err("rs_urma_free_jetty failed, ret:%d errno:%d", ret, errno);
    }

    (void)rs_free_jetty_id(jettyCb->dev_cb->urma_dev->name, jettyCb->jetty_mode, jettyCb->jetty_id);
    jettyCb->jetty = NULL;
}

void rs_ub_va_munmap_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    unsigned int i;

    for (i = 0; i < num; ++i) {
        rs_munmap_jetty_va(jettyCbArr[i]);
    }
}

void rs_ub_free_jetty_id_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num)
{
    unsigned int i;

    for (i = 0; i < num; ++i) {
        (void)rs_free_jetty_id(jettyCbArr[i]->dev_cb->urma_dev->name, jettyCbArr[i]->jetty_mode,
            jettyCbArr[i]->jetty_id);
    }
}
