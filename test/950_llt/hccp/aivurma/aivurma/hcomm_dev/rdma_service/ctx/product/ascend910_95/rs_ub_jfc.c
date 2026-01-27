/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma service ub extern jfc interface
 * Create: 2025-12-17
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
#include "rs_ub_jetty.h"
#include "rs_ub_jfc.h"

struct ext_jfc_attr {
    urma_jfc_t *jfc;
    unsigned int jfc_id;
    unsigned long long cqe_base_addr_va;
};

STATIC int rs_init_jfc_attr(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    int ret = 0;

    if (ctx_jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        return ret;
    }

    if (ctx_jfc_cb->jfc_type == JFC_MODE_CCU_POLL) {
        ret = rs_ccu_get_cqe_base_addr(ctx_jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_ccu_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, ctx_jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            return ret;
        }

    } else {
        ret = rs_net_get_cqe_base_addr(ctx_jfc_cb->dev_cb->dev_attr.ub.die_id, &jfc_attr->cqe_base_addr_va);
        if (ret != 0 || jfc_attr->cqe_base_addr_va == 0) {
            hccp_err("rs_net_get_cqe_base_addr failed, ret:%d, die_id:%u", ret, ctx_jfc_cb->dev_cb->dev_attr.ub.die_id);
            ret = -EOPENSRC;
            return ret;
        }
    }

    ret = rs_net_alloc_jfc_id(ctx_jfc_cb->dev_cb->urma_dev->name, ctx_jfc_cb->jfc_type, &jfc_attr->jfc_id);
    if (ret != 0) {
        hccp_err("rs_net_alloc_jfc_id failed, ret:%d", ret);
        return ret;
    }

    return 0;
}

STATIC void rs_deinit_jfc_attr(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t *jfc_cfg, struct ext_jfc_attr *jfc_attr)
{
    if (ctx_jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        return;
    }
    (void)rs_net_free_jfc_id(ctx_jfc_cb->dev_cb->urma_dev->name, ctx_jfc_cb->jfc_type, jfc_attr->jfc_id);
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

STATIC int rs_res_addr_munmap(struct rs_ctx_jfc_cb *jfc_cb, struct jetty_va_info *vaInfo)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jfc_cb->jfc_id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = (enum res_addr_type)vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct jetty_va_info);
    resInfoIn.priv = (void *)vaInfo;
    hccp_info("[chydebug]rs_res_addr_munmap, dev_id:%u, phy_id:%u, jetty_id:%u, res_type:%u, in_va:0x%llx pid:%d",
        jfc_cb->dev_cb->rscb->logicId, jfc_cb->dev_cb->phyId, resInfoIn.res_id, resInfoIn.res_type, vaInfo->va, vaInfo->pid);
    ret = DlHalResAddrUnmapV2(jfc_cb->dev_cb->rscb->logicId, &resInfoIn);
    ret = ret > 0 ? -ret : ret;
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrUnmapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), ret);

    return ret;
}

STATIC int rs_res_addr_mmap(struct rs_ctx_jfc_cb *jfc_cb, struct jetty_va_info *vaInfo,
    struct res_map_info_out *resInfoOut)
{
    struct res_map_info_in resInfoIn = {0};
    int ret = 0;

    resInfoIn.res_id = jfc_cb->jfc_id;
    resInfoIn.target_proc_type = PROCESS_CP1;
    resInfoIn.res_type = (enum res_addr_type)vaInfo->resType;
    resInfoIn.priv_len = sizeof(struct jetty_va_info);
    resInfoIn.priv = (void *)vaInfo;
    hccp_info("[chydebug]rs_res_addr_mmap, dev_id:%u, phy_id:%u, jetty_id:%u, res_type:%u, in_va:0x%llx pid:%d",
        jfc_cb->dev_cb->rscb->logicId, jfc_cb->dev_cb->phyId, resInfoIn.res_id, resInfoIn.res_type, vaInfo->va, vaInfo->pid);
    ret = DlHalResAddrMapV2(jfc_cb->dev_cb->rscb->logicId, &resInfoIn, resInfoOut);
    ret = ret > 0 ? -ret : ret;
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalResAddrMapV2 failed, res_type:%d ret:%d, errno:%d",
        resInfoIn.res_type, ret, errno), ret);

    return ret;
}

STATIC void rs_munmap_jfc_va(struct rs_ctx_jfc_cb *jfc_cb)
{
    struct jetty_va_info vaInfo = {0};

    if (jfc_cb->jfc_type != JFC_MODE_USER_CTL_NORMAL) {
        return;
    }

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JFC;
    vaInfo.va = jfc_cb->buf_addr;
    vaInfo.len = WQE_BB_SIZE * jfc_cb->depth;
    vaInfo.pid = getpid();
    (void)rs_res_addr_munmap(jfc_cb, &vaInfo);

    vaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    vaInfo.va = jfc_cb->swdb_addr;
    vaInfo.len = sizeof(uint64_t);
    vaInfo.pid = getpid();
    (void)rs_res_addr_munmap(jfc_cb, &vaInfo);
}

STATIC int rs_mmap_jfc_va(struct rs_ctx_jfc_cb *jfc_cb)
{
    struct res_map_info_out resInfoOut = {0};
    struct jetty_va_info jfcVaInfo = {0};
    struct jetty_va_info dbVaInfo = {0};
    int ret_tmp = 0;
    int ret = 0;

    jfcVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_JFC;
    jfcVaInfo.va = jfc_cb->buf_addr;
    jfcVaInfo.len = WQE_BB_SIZE * jfc_cb->depth;
    jfcVaInfo.pid = getpid();
    hccp_info("[chydebug][jfc]resType:%u in_sq_buff_va:0x%llx len:%u pid:%u", jfcVaInfo.resType, jfcVaInfo.va, jfcVaInfo.len, jfcVaInfo.pid);
    ret = rs_res_addr_mmap(jfc_cb, &jfcVaInfo, &resInfoOut);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_res_addr_mmap failed, res_type:%u ret:%d", jfcVaInfo.resType, ret),
        ret);
    jfc_cb->buf_addr = resInfoOut.va;
    hccp_info("[chydebug]in_sq_buff_va:0x%llx, out_sq_buff_va:0x%llx", jfcVaInfo.va, jfc_cb->buf_addr);

    dbVaInfo.resType = RES_ADDR_TYPE_HCCP_URMA_DB;
    dbVaInfo.va = jfc_cb->swdb_addr;
    dbVaInfo.len = sizeof(uint64_t);
    dbVaInfo.pid = getpid();
    hccp_info("[chydebug][db]resType:%u in_sq_buff_va:0x%llx len:%u pid:%u", dbVaInfo.resType, dbVaInfo.va, dbVaInfo.len, dbVaInfo.pid);
    ret = rs_res_addr_mmap(jfc_cb, &dbVaInfo, &resInfoOut);
    if (ret != 0) {
        hccp_err("rs_ub_ctx_res_addr_mmap failed, res_type:%u ret:%d", dbVaInfo.resType, ret);
        goto munmap_jfc_buff_va;
    }

    jfc_cb->swdb_addr = resInfoOut.va;
    hccp_info("[chydebug]in_db_addr:0x%llx, out_db_addr:0x%llx",dbVaInfo.va, jfc_cb->swdb_addr);
    return ret;

munmap_jfc_buff_va:
    jfcVaInfo.va = jfc_cb->buf_addr;
    ret_tmp = rs_res_addr_munmap(jfc_cb, &jfcVaInfo);
    CHK_PRT_RETURN(ret_tmp != 0, hccp_err("rs_res_addr_munmap failed, res_type:%u ret:%d",
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
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_mmap_jfc_va failed, ret:%d", ret), ret);

    return ret;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc)
{
    struct ext_jfc_attr jfc_attr = {0};
    int ret = 0;

    ret = rs_urma_alloc_jfc(ctx_jfc_cb->dev_cb->urma_ctx, &jfc_cfg, &jfc_attr.jfc);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_alloc_jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    ret = rs_init_jfc_attr(ctx_jfc_cb, &jfc_cfg, &jfc_attr);
    if (ret != 0) {
        hccp_err("rs_init_jfc_attr failed, ret:%d", ret);
        goto free_jfc;
    }

    ret = rs_set_jfc_opt(ctx_jfc_cb, &jfc_attr);
    if (ret != 0) {
        hccp_err("rs_set_jfc_attr failed, ret:%d", ret);
        ret = -EOPENSRC;
        goto deinit_attr;
    }

    ret = rs_urma_active_jfc(jfc_attr.jfc);
    if (ret != 0) {
        hccp_err("rs_urma_active_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_attr.jfc_id, ret, errno);
        ret = -EOPENSRC;
        goto deinit_attr;
    }
    ctx_jfc_cb->jfc_id = jfc_attr.jfc->jfc_id.id;

    ret = rs_get_jfc_opt(ctx_jfc_cb, jfc_attr.jfc);
    if (ret != 0) {
        hccp_err("rs_get_jfc_opt failed, jfc_id:%u, ret:%d, errno:%d", jfc_attr.jfc_id, ret, errno);
        ret = -EOPENSRC;
        goto deactive_jfc;
    }

    *jfc = jfc_attr.jfc;
    return 0;

deactive_jfc:
    (void)rs_urma_deactive_jfc(jfc_attr.jfc);
deinit_attr:
    (void)rs_deinit_jfc_attr(ctx_jfc_cb, &jfc_cfg, &jfc_attr);
free_jfc:
    (void)rs_urma_free_jfc(jfc_attr.jfc);
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
    }

    ret = rs_urma_free_jfc(jfc);
    if (ret != 0) {
        hccp_err("rs_urma_free_jfc failed, jfc_id:%u, ret:%d, errno:%d", jfc_id, ret, errno);
    }

    if (jfc_cb->jfc_type != JFC_MODE_USER_CTL_NORMAL) {
        ret = rs_net_free_jfc_id(dev_cb->urma_dev->name, jfc_cb->jfc_type, jfc_id);
        if (ret != 0) {
            hccp_err("rs_net_free_jfc_id failed, jfc_id:%u, ret:%d", jfc_id, ret);
        }
    }

    return ret;
}
