/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ascend_hal.h"
#include "ccu_u_api.h"
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "dl_urma_function.h"
#include "dl_ccu_function.h"
#include "dl_net_function.h"
#include "hccp_ctx.h"
#include "ra_rs_ctx.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "rs_ccu.h"
#include "rs_ub.h"
#include "rs_ctx.h"

int rs_get_chip_protocol(unsigned int chip_id, enum NetworkMode hccp_mode, enum ProtocolTypeT *protocol,
    unsigned int logic_id)
{
#define CHIP_NAME_950 "950"
#define CHIP_NAME_910_96 "910_96"
    halChipInfo chip_info = {0};
    int ret;

    // set default protocol to RDMA for compatibility issue
    *protocol = PROTOCOL_RDMA;
    // other modes skip to get protocol
    if (hccp_mode != NETWORK_OFFLINE) {
        return 0;
    }

    ret = DlHalGetChipInfo(logic_id, &chip_info);
    CHK_PRT_RETURN(ret != 0, hccp_warn("hal get chip info unsuccessful, chip_id[%u], logic_id[%u], ret[%d]",
        chip_id, logic_id, ret), 0);

    if ((strncmp((char *)chip_info.name, CHIP_NAME_950, sizeof(CHIP_NAME_950) - 1) == 0) ||
        (strncmp((char *)chip_info.name, CHIP_NAME_910_96, sizeof(CHIP_NAME_910_96) - 1) == 0)) {
        *protocol = PROTOCOL_UDMA;
    }
    return 0;
}

int rs_ctx_api_init(enum NetworkMode hccp_mode, enum ProtocolTypeT protocol)
{
    int ret = 0;

    // other modes skip to init api
    if (hccp_mode != NETWORK_OFFLINE) {
        return ret;
    }

    switch (protocol) {
        case PROTOCOL_RDMA:
            ret = RsApiInit();
            CHK_PRT_RETURN(ret != 0, hccp_err("RsApiInit failed, protocol[%u], ret[%d]", protocol, ret), ret);
            break;
        case PROTOCOL_UDMA:
            ret = rs_ub_api_init();
            CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_api_init failed, protocol[%u], ret[%d]", protocol, ret), ret);
            ret = rs_ccu_api_init();
            if (ret != 0) {
                hccp_err("rs_ccu_api_init failed, protocol[%u], ret[%d]", protocol, ret);
                rs_ub_api_deinit();
                return ret;
            }
            ret = rs_net_api_init();
            if (ret != 0) {
                hccp_err("rs_net_api_init failed, protocol[%u], ret[%d]", protocol, ret);
                rs_ccu_api_deinit();
                rs_ub_api_deinit();
                return ret;
            }
            break;
        default:
            hccp_err("unsupported protocol[%u]", protocol);
            return -EINVAL;
    }

    return ret;
}

int rs_ctx_api_deinit(enum NetworkMode hccp_mode, enum ProtocolTypeT protocol)
{
    // other modes skip to deinit api
    if (hccp_mode != NETWORK_OFFLINE) {
        return 0;
    }

    switch (protocol) {
        case PROTOCOL_RDMA:
            RsApiDeinit();
            break;
        case PROTOCOL_UDMA:
            rs_ub_api_deinit();
            rs_ccu_api_deinit();
            rs_net_api_deinit();
            break;
        default:
            hccp_err("unsupported protocol[%u]", protocol);
            return -EINVAL;
    }
    return 0;
}

RS_ATTRI_VISI_DEF int rs_get_dev_eid_info_num(unsigned int phy_id, unsigned int *num)
{
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = rs_ub_get_dev_eid_info_num(phy_id, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_get_dev_eid_info_num failed, phy_id:%u, ret:%d", phy_id, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[],
    unsigned int start_index, unsigned int count)
{
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(info_list);

    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_get_dev_eid_info_list failed, phy_id:%u, ret:%d", phy_id, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_init(struct ctx_init_attr *attr, unsigned int *dev_index, struct dev_base_attr *dev_attr)
{
    struct rs_cb *rscb = NULL;
    unsigned int phy_id;
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(dev_index);
    RS_CHECK_POINTER_NULL_RETURN_INT(dev_attr);
    phy_id = attr->phy_id;
    ret = RsGetRsCb(phy_id, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsGetRsCb failed, ret:%d", ret), ret);

#ifdef CUSTOM_INTERFACE
    // setup sharemem for pingmesh
    ret = RsSetupSharemem(rscb, false, phy_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsSetupSharemem failed, phy_id:%u, ret:%d", phy_id, ret), ret);
#endif

    ret = rs_ub_ctx_init(rscb, attr, dev_index, dev_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_init failed, dev_index:0x%x, ret:%d", *dev_index, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_get_async_events(struct RaRsDevInfo *dev_info, struct async_event async_events[],
    unsigned int *num)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(async_events);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    CHK_PRT_RETURN(*num == 0, hccp_err("num can not be 0"), -EINVAL);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    rs_ub_ctx_get_async_events(dev_cb, async_events, num);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_deinit(struct RaRsDevInfo *dev_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb fail, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    (void)rs_ub_ctx_deinit(dev_cb);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_eid_by_ip(struct RaRsDevInfo *dev_info, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(ip);
    RS_CHECK_POINTER_NULL_RETURN_INT(eid);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_get_eid_by_ip(dev_cb, ip, eid, num);
    CHK_PRT_RETURN(ret != 0, hccp_err("get_eid_by_ip failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_tp_info_list(struct RaRsDevInfo *dev_info, struct get_tp_cfg *cfg,
    struct tp_info info_list[], unsigned int *num)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(cfg);
    RS_CHECK_POINTER_NULL_RETURN_INT(info_list);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    switch (rscb->protocol) {
        case PROTOCOL_UDMA:
            ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
            CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex),
                ret);
            ret = rs_ub_get_tp_info_list(dev_cb, cfg, info_list, num);
            break;
        default:
            hccp_err("protocol[%d] not support", rscb->protocol);
            return -EINVAL;
    }
    return ret;
}

RS_ATTRI_VISI_DEF int rs_get_tp_attr(struct RaRsDevInfo *dev_info, unsigned int *attr_bitmap,
    const uint64_t tp_handle, struct tp_attr *attr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(attr_bitmap);
    RS_CHECK_POINTER_NULL_RETURN_INT(attr);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);
    ret = rs_ub_get_tp_attr(dev_cb, attr_bitmap, tp_handle, attr);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_set_tp_attr(struct RaRsDevInfo *dev_info, const unsigned int attr_bitmap,
    const uint64_t tp_handle, struct tp_attr *attr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(attr);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);
    ret = rs_ub_set_tp_attr(dev_cb, attr_bitmap, tp_handle, attr);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_token_id_alloc(struct RaRsDevInfo *dev_info, unsigned long long *addr,
    unsigned int *token_id)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(addr);
    RS_CHECK_POINTER_NULL_RETURN_INT(token_id);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, addr, token_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_token_id_alloc failed, ret:%d, dev_index:0x%x",
        dev_info->devIndex, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_token_id_free(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_token_id_free failed, ret:%d, dev_index:0x%x",
        dev_info->devIndex, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_lmem_reg(struct RaRsDevInfo *dev_info, struct mem_reg_attr_t *mem_attr,
    struct mem_reg_info_t *mem_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(mem_attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(mem_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_lmem_reg(dev_cb, mem_attr, mem_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_lmem_reg failed, ret:%d, dev_index:0x%x",
        dev_info->devIndex, ret), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_lmem_unreg(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_lmem_unreg(dev_cb, addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_lmem_unreg failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_rmem_import(struct RaRsDevInfo *dev_info, struct mem_import_attr_t *mem_attr,
    struct mem_import_info_t *mem_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(mem_attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(mem_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, mem_attr, mem_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_rmem_import failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_rmem_unimport(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_rmem_unimport(dev_cb, addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_rmem_unimport failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_chan_create(struct RaRsDevInfo *dev_info, union data_plane_cstm_flag data_plane_flag,
    unsigned long long *addr, int *fd)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(addr);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_chan_create(dev_cb, data_plane_flag, addr, fd);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_chan_create failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_chan_destroy(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_chan_destroy(dev_cb, addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_chan_destroy failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_cq_create(struct RaRsDevInfo *dev_info, struct ctx_cq_attr *attr,
    struct ctx_cq_info *info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, attr, info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jfc_create failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_cq_destroy(struct RaRsDevInfo *dev_info, unsigned long long addr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jfc_destroy failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_create(struct RaRsDevInfo *dev_info, struct ctx_qp_attr *qp_attr,
    struct qp_create_info *qp_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(qp_attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(qp_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, qp_attr, qp_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_create failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_destroy(struct RaRsDevInfo *dev_info, unsigned int id)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_destroy failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_import(struct RaRsDevInfo *dev_info, struct rs_jetty_import_attr *import_attr,
    struct rs_jetty_import_info *import_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(import_attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(import_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, import_attr, import_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_import failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_unimport(struct RaRsDevInfo *dev_info, unsigned int rem_jetty_id)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, rem_jetty_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_unimport failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_bind(struct RaRsDevInfo *dev_info, struct rs_ctx_qp_info *local_qp_info,
    struct rs_ctx_qp_info *remote_qp_info)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(local_qp_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(remote_qp_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, local_qp_info, remote_qp_info);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_bind failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_unbind(struct RaRsDevInfo *dev_info, unsigned int qp_id)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, qp_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_jetty_unbind failed, ret:%d dev_index:0x%x",
        ret, dev_info->devIndex), ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_batch_send_wr(struct wrlist_base_info *base_info, struct batch_send_wr_data *wr_data,
    struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num)
{
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(base_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr_data);
    RS_CHECK_POINTER_NULL_RETURN_INT(wr_resp);
    RS_CHECK_POINTER_NULL_RETURN_INT(wrlist_num);

    ret = RsGetRsCb(base_info->phy_id, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    switch (rscb->protocol) {
        case PROTOCOL_UDMA:
            ret = rs_ub_ctx_batch_send_wr(rscb, base_info, wr_data, wr_resp, wrlist_num);
            break;
        default:
            hccp_err("protocol[%d] not support", rscb->protocol);
            return -EINVAL;
    }
    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_update_ci(struct RaRsDevInfo *dev_info, unsigned int qp_id, uint16_t ci)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    switch (rscb->protocol) {
        case PROTOCOL_UDMA:
            ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
            CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex),
                ret);
            ret = rs_ub_ctx_jetty_update_ci(dev_cb, qp_id, ci);
            break;
        default:
            hccp_err("protocol[%d] not support", rscb->protocol);
            return -EINVAL;
    }
    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_custom_channel(const struct custom_chan_info_in *in, struct custom_chan_info_out *out)
{
    struct channel_info_out chan_out = {0};
    struct channel_info_in chan_in = {0};
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(in);
    RS_CHECK_POINTER_NULL_RETURN_INT(out);

    ret = memcpy_s(&chan_in, sizeof(struct channel_info_in), in, sizeof(struct custom_chan_info_in));
    CHK_PRT_RETURN(ret != 0, hccp_err("[ccu]memcpy_s in failed, ret[%d]", ret), -ESAFEFUNC);

    ret = rs_ctx_ccu_custom_channel(&chan_in, &chan_out);
    CHK_PRT_RETURN(ret != 0, hccp_err("[ccu]rs_ctx_ccu_custom_channel failed, ret[%d]", ret), ret);

    // prepare output data
    ret = memcpy_s(out, sizeof(struct custom_chan_info_out), &chan_out, sizeof(struct channel_info_out));
    CHK_PRT_RETURN(ret != 0, hccp_err("[ccu]memcpy_s out failed, ret[%d]", ret), -ESAFEFUNC);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_destroy_batch(struct RaRsDevInfo *dev_info, unsigned int ids[], unsigned int *num)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(ids);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);
    ret = rs_ub_ctx_jetty_destroy_batch(dev_cb, ids, num);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_qp_query_batch(struct RaRsDevInfo *dev_info, unsigned int ids[],
    struct jetty_attr attr[], unsigned int *num)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(ids);
    RS_CHECK_POINTER_NULL_RETURN_INT(attr);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);
    ret = rs_ub_ctx_query_jetty_batch(dev_cb, ids, attr, num);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_get_aux_info(struct RaRsDevInfo *dev_info, struct aux_info_in *info_in,
    struct aux_info_out *info_out)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct rs_cb *rscb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(info_in);
    RS_CHECK_POINTER_NULL_RETURN_INT(info_out);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    ret = rs_ub_ctx_get_aux_info(dev_cb, info_in, info_out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_ctx_get_aux_info failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex),
        ret);

    return ret;
}

RS_ATTRI_VISI_DEF int rs_ctx_get_cr_err_info_list(struct RaRsDevInfo *dev_info, struct CrErrInfo info_list[],
    unsigned int *num)
{
    struct rs_ctx_jetty_cb *jetty_cb_curr = NULL;
    struct rs_ctx_jetty_cb *jetty_cb_next = NULL;
    struct rs_ub_dev_cb *dev_cb = NULL;
    unsigned int cr_err_idx = 0;
    struct rs_cb *rscb = NULL;
    unsigned int num_tmp = 0;
    int ret = 0;

    RS_CHECK_POINTER_NULL_RETURN_INT(dev_info);
    RS_CHECK_POINTER_NULL_RETURN_INT(info_list);
    RS_CHECK_POINTER_NULL_RETURN_INT(num);

    ret = RsGetRsCb(dev_info->phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rscb failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_cb(rscb, dev_info->devIndex, &dev_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get dev_cb failed, ret:%d dev_index:0x%x", ret, dev_info->devIndex), ret);

    if (RsListEmpty(&dev_cb->jetty_list)) {
        *num = 0;
        return ret;
    }

    num_tmp = *num;
    RS_LIST_GET_HEAD_ENTRY(jetty_cb_curr, jetty_cb_next, &dev_cb->jetty_list, list, struct rs_ctx_jetty_cb);
    for (; (&jetty_cb_curr->list) != &dev_cb->jetty_list; jetty_cb_curr = jetty_cb_next,
        jetty_cb_next = list_entry(jetty_cb_next->list.next, struct rs_ctx_jetty_cb, list)) {
        if (jetty_cb_curr->cr_err_info.info.status != 0) {
            RS_PTHREAD_MUTEX_LOCK(&jetty_cb_curr->cr_err_info.mutex);
            info_list[cr_err_idx].status = jetty_cb_curr->cr_err_info.info.status;
            info_list[cr_err_idx].jetty_id = jetty_cb_curr->cr_err_info.info.jetty_id;
            info_list[cr_err_idx].time = jetty_cb_curr->cr_err_info.info.time;
            jetty_cb_curr->cr_err_info.info.status = 0;
            RS_PTHREAD_MUTEX_ULOCK(&jetty_cb_curr->cr_err_info.mutex);
            RS_PTHREAD_MUTEX_LOCK(&jetty_cb_curr->dev_cb->cqeErrCntMutex);
            jetty_cb_curr->dev_cb->cqeErrCnt--;
            RS_PTHREAD_MUTEX_ULOCK(&jetty_cb_curr->dev_cb->cqeErrCntMutex);
            cr_err_idx++;
            if (cr_err_idx == num_tmp) {
                break;
            }
        }
    }

    *num = cr_err_idx;
    return ret;
}