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
#include "securec.h"
#include "urma_opcode.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "dl_urma_function.h"
#include "dl_net_function.h"
#include "dl_ccu_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub_jfc.h"

struct ext_jfc_attr {
    urma_jfc_t *jfc;
    unsigned int jfc_id;
    unsigned long long cqe_base_addr_va;
};

STATIC int rs_init_jfc_attr(struct rs_ctx_jfc_cb *jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    int ret = 0;

    ret = rs_urma_alloc_jfc(jfc_cb->dev_cb->urma_ctx, jfc_cfg, &jfc_attr->jfc);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_alloc_jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    if (jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        return 0;
    }

    if (jfc_cb->jfc_type == JFC_MODE_CCU_POLL) {
        ret = rs_ccu_get_cqe_base_addr(jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_ccu_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            goto free_jfc;
        }
    } else {
        ret = rs_net_get_cqe_base_addr(jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_net_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            goto free_jfc;
        }
    }

    ret = rs_net_alloc_jfc_id(jfc_cb->dev_cb->urma_dev->name, jfc_cb->jfc_type, &jfc_attr->jfc_id);
    if (ret != 0) {
        hccp_err("rs_net_alloc_jfc_id failed, ret:%d", ret);
        goto free_jfc;
    }

    return 0;

free_jfc:
    (void)rs_urma_free_jfc(jfc_attr->jfc);
    return ret;
}

STATIC void rs_deinit_jfc_attr(struct rs_ctx_jfc_cb *jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    (void)rs_urma_free_jfc(jfc_attr->jfc);
    if (jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        return;
    }
    (void)rs_net_free_jfc_id(jfc_cb->dev_cb->urma_dev->name, jfc_cb->jfc_type, jfc_attr->jfc_id);
}

STATIC int rs_set_jfc_opt(struct rs_ctx_jfc_cb *jfc_cb, struct ext_jfc_attr *jfc_attr)
{
    int ret = 0;

    if (jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        return ret;
    }

    ret = rs_urma_set_jfc_opt(jfc_attr->jfc, URMA_JFC_ID, (void *)&jfc_attr->jfc_id, sizeof(uint32_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_ID failed, ret:%d, errno:%d", ret, errno), -EOPENSRC);

    ret = rs_urma_set_jfc_opt(jfc_attr->jfc, URMA_JFC_CQE_BASE_ADDR,
        (void *)&jfc_attr->cqe_base_addr_va, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0,
        hccp_err("rs_urma_set_jfc_opt URMA_JFC_CQE_BASE_ADDR failed, ret:%d, errno:%d", ret, errno), -EOPENSRC);

    return 0;
}

STATIC int rs_jfc_res_addr_munmap(struct rs_ctx_jfc_cb *jfc_cb, struct udma_va_info *vaInfo)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jfc_cb->jfc_id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = (enum res_addr_type)vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct udma_va_info);
    resInfoIn.priv = (void *)vaInfo;
    ret = DlHalResAddrUnmapV2(jfc_cb->dev_cb->rscb->logicId, &resInfoIn);
    ret = ret > 0 ? -ret : ret;
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrUnmapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), ret);

    return ret;
}

STATIC int rs_jfc_res_addr_mmap(struct rs_ctx_jfc_cb *jfc_cb, struct udma_va_info *vaInfo,
    struct res_map_info_out *resInfoOut)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jfc_cb->jfc_id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = (enum res_addr_type)vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct udma_va_info);
    resInfoIn.priv = (void *)vaInfo;
    ret = DlHalResAddrMapV2(jfc_cb->dev_cb->rscb->logicId, &resInfoIn, resInfoOut);
    ret = ret > 0 ? -ret : ret;
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrMapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), ret);

    return ret;
}

STATIC void rs_munmap_jfc_va(struct rs_ctx_jfc_cb *jfc_cb)
{
    struct udma_va_info vaInfo = {0};

    if (jfc_cb->jfc_type != JFC_MODE_USER_CTL_NORMAL) {
        return;
    }

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JFC;
    vaInfo.va = jfc_cb->buf_addr;
    vaInfo.len = WQE_BB_SIZE * jfc_cb->depth;
    vaInfo.pid = getpid();
    (void)rs_jfc_res_addr_munmap(jfc_cb, &vaInfo);

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    vaInfo.va = jfc_cb->swdb_addr;
    vaInfo.len = sizeof(uint64_t);
    vaInfo.pid = getpid();
    (void)rs_jfc_res_addr_munmap(jfc_cb, &vaInfo);
}

STATIC int rs_mmap_jfc_va(struct rs_ctx_jfc_cb *jfc_cb)
{
    struct res_map_info_out resInfoOut = {0};
    struct udma_va_info jfcVaInfo = {0};
    struct udma_va_info dbVaInfo = {0};
    int ret_tmp = 0;
    int ret = 0;

    jfcVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JFC;
    jfcVaInfo.va = jfc_cb->buf_addr;
    jfcVaInfo.len = WQE_BB_SIZE * jfc_cb->depth;
    jfcVaInfo.pid = getpid();
    ret = rs_jfc_res_addr_mmap(jfc_cb, &jfcVaInfo, &resInfoOut);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_jfc_res_addr_mmap failed, res_type:%u ret:%d", jfcVaInfo.resType, ret),
        ret);
    jfc_cb->buf_addr = resInfoOut.va;

    dbVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    dbVaInfo.va = jfc_cb->swdb_addr;
    dbVaInfo.len = sizeof(uint64_t);
    dbVaInfo.pid = getpid();
    ret = rs_jfc_res_addr_mmap(jfc_cb, &dbVaInfo, &resInfoOut);
    if (ret != 0) {
        hccp_err("rs_jfc_res_addr_mmap failed, res_type:%u ret:%d", dbVaInfo.resType, ret);
        goto munmap_jfc_buff_va;
    }

    jfc_cb->swdb_addr = resInfoOut.va;
    return ret;

munmap_jfc_buff_va:
    jfcVaInfo.va = jfc_cb->buf_addr;
    ret_tmp = rs_jfc_res_addr_munmap(jfc_cb, &jfcVaInfo);
    CHK_PRT_RETURN(ret_tmp != 0, hccp_err("rs_jfc_res_addr_munmap failed, res_type:%u ret:%d",
        jfcVaInfo.resType, ret_tmp), ret_tmp);
    return ret;
}

STATIC int rs_get_jfc_opt(struct rs_ctx_jfc_cb *jfc_cb, urma_jfc_t *jfc)
{
    uint64_t cqBuffVa = 0, dbVa = 0;
    int ret = 0;

    if (jfc_cb->jfc_type != JFC_MODE_USER_CTL_NORMAL) {
        return ret;
    }

    ret = rs_urma_get_jfc_opt(jfc, URMA_JFC_CQE_BASE_ADDR, &cqBuffVa, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_jfc_opt URMA_JFC_CQE_BASE_ADDR failed, ret:%d, errno:%d", ret, errno),
        -EOPENSRC);

    ret = rs_urma_get_jfc_opt(jfc, URMA_JFC_DB_ADDR, &dbVa, sizeof(uint64_t));
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_jfc_opt URMA_JFC_DB_ADDR failed, ret:%d, errno:%d",
        ret, errno), -EOPENSRC);

    jfc_cb->buf_addr = cqBuffVa;
    jfc_cb->swdb_addr = dbVa;

    ret = rs_mmap_jfc_va(jfc_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_mmap_jfc_va failed, ret:%d", ret), ret);

    return ret;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *jfc_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **jfc)
{
    struct ext_jfc_attr jfc_attr = {0};
    int ret = 0;

    ret = rs_init_jfc_attr(jfc_cb, jfc_cfg, &jfc_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_init_jfc_attr failed, ret:%d", ret), ret);

    ret = rs_set_jfc_opt(jfc_cb, &jfc_attr);
    if (ret != 0) {
        hccp_err("rs_set_jfc_attr failed, ret:%d", ret);
        goto deinit_attr;
    }

    ret = rs_urma_active_jfc(jfc_attr.jfc);
    if (ret != 0) {
        hccp_err("rs_urma_active_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_attr.jfc->jfc_id.id, ret, errno);
        ret = -EOPENSRC;
        goto deinit_attr;
    }
    jfc_cb->jfc_id = jfc_attr.jfc->jfc_id.id;

    ret = rs_get_jfc_opt(jfc_cb, jfc_attr.jfc);
    if (ret != 0) {
        hccp_err("rs_get_jfc_opt failed, jfc_id:%u, ret:%d, errno:%d", jfc_attr.jfc->jfc_id.id, ret, errno);
        goto deactive_jfc;
    }

    *jfc = jfc_attr.jfc;
    return 0;

deactive_jfc:
    (void)rs_urma_deactive_jfc(jfc_attr.jfc);
deinit_attr:
    (void)rs_deinit_jfc_attr(jfc_cb, jfc_cfg, &jfc_attr);
    *jfc = NULL;
    return ret;
}

int rs_ub_delete_jfc_ext(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb)
{
    urma_jfc_t *jfc = (urma_jfc_t *)(uintptr_t)(jfc_cb->jfc_addr);
    unsigned int jfc_id = jfc->jfc_id.id;
    int ret = 0;

    rs_munmap_jfc_va(jfc_cb);

    ret = rs_urma_deactive_jfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_deactive_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_id, ret, errno);
        ret = -EOPENSRC;
    }

    ret = rs_urma_free_jfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_free_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_id, ret, errno);
        ret = -EOPENSRC;
    }

    if (jfc_cb->jfc_type != JFC_MODE_USER_CTL_NORMAL) {
        ret = rs_net_free_jfc_id(dev_cb->urma_dev->name, jfc_cb->jfc_type, jfc_id);
        if (ret != 0) {
            hccp_err("rs_net_free_jfc_id failed, jfc_id:%u, ret:%d", jfc_id, ret);
        }
    }

    return ret;
}