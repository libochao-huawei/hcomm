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
#include "dl_hal_function.h"
#include "dl_urma_function.h"
#include "ra_rs_err.h"
#include "ra_rs_ctx.h"
#include "rs_inner.h"
#include "rs_epoll.h"
#include "rs_ctx_inner.h"
#include "rs_ctx.h"
#include "rs_ub_jetty.h"
#include "rs_ub_jfc.h"
#include "rs_ub.h"

urma_cr_t g_cr_buf[RS_WC_NUM];

int rs_ub_get_dev_eid_info_num(unsigned int phy_id, unsigned int *num)
{
    urma_eid_info_t *eid_list = NULL;
    urma_device_t **dev_list = NULL;
    unsigned int total_num = 0;
    unsigned int eid_num = 0;
    int dev_num = 0;
    int ret = 0;
    int i = 0;

    ret = rs_ub_api_init();
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_api_init failed, ret:%d", ret), ret);

    dev_list = rs_urma_get_device_list(&dev_num);
    if (dev_list == NULL) {
        hccp_err("rs_urma_get_device_list failed, errno:%d", errno);
        ret = -EINVAL;
        goto ub_api_deinit;
    }

    for (i = 0; i < dev_num; i++) {
        eid_list = rs_urma_get_eid_list(dev_list[i], &eid_num);
        // normal case, should continue to get eid_list from the rest of device
        if (eid_list == NULL) {
            hccp_warn("rs_urma_get_eid_list i=%u unsuccessful, eid_list is NULL, errno:%d", i, errno);
            continue;
        }

        total_num += eid_num;
        rs_urma_free_eid_list(eid_list);
    }

    *num = total_num;

    rs_urma_free_device_list(dev_list);
ub_api_deinit:
    rs_ub_api_deinit();
    return ret;
}

STATIC int rs_ub_create_ctx(urma_device_t *urma_dev, unsigned int eid_index, urma_context_t **urma_ctx)
{
    *urma_ctx = rs_urma_create_context(urma_dev, eid_index);
    CHK_PRT_RETURN(*urma_ctx == NULL, hccp_err("rs_urma_create_context failed! errno:%d, eid_index:%u",
        errno, eid_index), -ENODEV);

    return 0;
}

int rs_ub_get_ue_info(urma_context_t *urma_ctx, struct dev_base_attr *dev_attr)
{
#ifdef CUSTOM_INTERFACE
    struct udma_u_ue_info ue_info = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret;

    in.opcode = UDMA_U_USER_CTL_QUERY_UE_INFO;
    out.addr = (uint64_t)(uintptr_t)&ue_info;
    out.len = sizeof(struct udma_u_ue_info);
    ret = rs_urma_user_ctl(urma_ctx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl query ue info failed! ret:%d errno:%d", ret, errno), -EOPENSRC);

    dev_attr->ub.die_id = ue_info.die_id;
    dev_attr->ub.chip_id = ue_info.chip_id;
    dev_attr->ub.func_id = ue_info.ue_id;
    hccp_info("func_id:%u, chip_id:%u, die_id:%u", dev_attr->ub.func_id, dev_attr->ub.chip_id, dev_attr->ub.die_id);
#endif
    return 0;
}

STATIC int rs_ub_fill_dev_eid_info_list(struct dev_eid_info *total_list, unsigned int index, urma_device_t *device,
    urma_eid_info_t *eid_info)
{
    struct dev_base_attr dev_attr = {0};
    urma_context_t *urma_ctx = NULL;
    int ret = 0;

    total_list[index].eid_index = eid_info->eid_index;
    total_list[index].type = device->type;

    (void)memcpy_s(total_list[index].eid.raw, sizeof(total_list[index].eid.raw), eid_info->eid.raw,
        sizeof(eid_info->eid.raw));

    ret = strcpy_s(total_list[index].name, DEV_EID_INFO_MAX_NAME, device->name);
    CHK_PRT_RETURN(ret != 0, hccp_err("strcpy device name failed, ret:%d", ret), -ESAFEFUNC);

    ret = rs_ub_create_ctx(device, eid_info->eid_index, &urma_ctx);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_create_ctx failed, ret:%d, eid_index:%u", ret, eid_info->eid_index), ret);

    ret = rs_ub_get_ue_info(urma_ctx, &dev_attr);
    if (ret != 0) {
        hccp_err("rs_ub_get_ue_info failed, ret:%d", ret);
        goto free_ctx;
    }

    total_list[index].die_id = dev_attr.ub.die_id;
    total_list[index].chip_id = dev_attr.ub.chip_id;
    total_list[index].func_id = dev_attr.ub.func_id;

free_ctx:
    (void)rs_urma_delete_context(urma_ctx);
    return ret;
}

STATIC int rs_ub_fill_info_by_eid_list(urma_device_t *curr_dev, unsigned int *index, struct dev_eid_info *total_list,
    unsigned int total_num)
{
    urma_eid_info_t *eid_list = NULL;
    unsigned int eid_num = 0;
    unsigned int j;
    int ret = 0;

    eid_list = rs_urma_get_eid_list(curr_dev, &eid_num);
    // normal case, should continue to get eid_list from the rest of device
    if (eid_list == NULL) {
        hccp_warn("rs_urma_get_eid_list unsuccessful, eid_list is NULL, errno:%d", errno);
        return 0;
    }

    for (j = 0; j < eid_num; j++) {
        if (*index >= total_num) {
            hccp_err("index out of range, index:%u, total_num:%u", *index, total_num);
            ret = -EINVAL;
            goto free_eid_list;
        }
        ret = rs_ub_fill_dev_eid_info_list(total_list, *index, curr_dev, &eid_list[j]);
        if (ret != 0) {
            hccp_err("rs_ub_fill_dev_eid_info_list failed, index:%u, ret:%d", *index, ret);
            goto free_eid_list;
        }
        *index += 1;
    }

free_eid_list:
    rs_urma_free_eid_list(eid_list);
    return ret;
}

int rs_ub_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[], unsigned int start_index,
    unsigned int count)
{
    struct dev_eid_info *total_list = NULL;
    urma_device_t **dev_list = NULL;
    unsigned int total_num = 0;
    unsigned int index = 0;
    int dev_num, ret, i;

    ret = rs_ub_api_init();
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_api_init failed, ret:%d", ret), ret);

    ret = rs_ub_get_dev_eid_info_num(phy_id, &total_num);
    if ((ret != 0) || (start_index > UINT_MAX - count) || (start_index + count > total_num)) {
        hccp_err("rs_ub_get_dev_eid_info_num failed ret:%d data size exceeds the max offset range "
            "or start_index:%u + count:%u > total_num:%u", ret, start_index, count, total_num);
        ret = -EINVAL;
        goto ub_api_deinit;
    }

    total_list = calloc(total_num, sizeof(struct dev_eid_info));
    if (total_list == NULL) {
        hccp_err("calloc total_list failed, errno:%d", errno);
        ret = -ENOMEM;
        goto ub_api_deinit;
    }

    dev_list = rs_urma_get_device_list(&dev_num);
    if (dev_list == NULL) {
        hccp_err("rs_urma_get_device_list failed, errno: %d", errno);
        ret = -EINVAL;
        goto free_total_list;
    }

    for (i = 0; i < dev_num; i++) {
        ret = rs_ub_fill_info_by_eid_list(dev_list[i], &index, total_list, total_num);
        if (ret != 0) {
            goto free_dev_list;
        }
    }

    // start_index + count <= total_num make sure count is valid
    (void)memcpy_s(info_list, sizeof(struct dev_eid_info) * count,
        total_list + start_index, sizeof(struct dev_eid_info) * count);

free_dev_list:
    rs_urma_free_device_list(dev_list);
free_total_list:
    free(total_list);
    total_list = NULL;
ub_api_deinit:
    rs_ub_api_deinit();
    return ret;
}

int rs_ub_get_dev_cb(struct rs_cb *rscb, unsigned int devIndex, struct rs_ub_dev_cb **dev_cb)
{
    struct rs_ub_dev_cb *dev_cb_curr = NULL;
    struct rs_ub_dev_cb *dev_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(dev_cb_curr, dev_cb_next, &rscb->rdevList, list, struct rs_ub_dev_cb);
    for (; (&dev_cb_curr->list) != &rscb->rdevList;
         dev_cb_curr = dev_cb_next,
         dev_cb_next = list_entry(dev_cb_next->list.next, struct rs_ub_dev_cb, list)) {
        if (dev_cb_curr->index == devIndex) {
            *dev_cb = dev_cb_curr;
            return 0;
        }
    }

    *dev_cb = NULL;
    hccp_err("dev_cb for devIndex:0x%x do not available!", devIndex);
    return -ENODEV;
}

STATIC int rs_ub_get_dev_attr(struct rs_ub_dev_cb *dev_cb, struct dev_base_attr *dev_attr, unsigned int *devIndex)
{
    urma_device_attr_t attr = {0};
    int ret;

    ret = rs_urma_query_device(dev_cb->urma_dev, &attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_query_device failed ret:%d", ret), -EOPENSRC);

    dev_attr->sq_max_depth = attr.dev_cap.max_jfs_depth;
    dev_attr->rq_max_depth = attr.dev_cap.max_jfr_depth;
    dev_attr->sq_max_sge = attr.dev_cap.max_jfs_sge;
    dev_attr->rq_max_sge = attr.dev_cap.max_jfr_sge;
    dev_attr->ub.max_jfs_inline_len = attr.dev_cap.max_jfs_inline_len;
    dev_attr->ub.max_jfs_rsge = attr.dev_cap.max_jfs_rsge;
    ret = rs_ub_get_ue_info(dev_cb->urma_ctx, dev_attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_get_ue_info failed ret:%d", ret), -EOPENSRC);

    (void)memcpy_s(&dev_cb->dev_attr, sizeof(struct dev_base_attr), dev_attr, sizeof(struct dev_base_attr));
    dev_cb->rscb->devCnt++;
    *devIndex = rs_generate_dev_index(dev_cb->rscb->devCnt, dev_attr->ub.die_id, dev_attr->ub.func_id);
    dev_cb->index = *devIndex;

    hccp_info("max_jetty:%u, max_jfs_inline_len:%u, max_jfs_depth:%u, max_jfr_depth:%u, max_jfs_sge:%u, max_jfr_sge:%u",
        attr.dev_cap.max_jetty, dev_attr->ub.max_jfs_inline_len, dev_attr->sq_max_depth,
        dev_attr->rq_max_depth, dev_attr->sq_max_sge, dev_attr->rq_max_sge);
    return 0;
}

STATIC int rs_ub_dev_cb_init(struct ctx_init_attr *attr, struct rs_ub_dev_cb *dev_cb, struct rs_cb *rscb,
    unsigned int *devIndex, struct dev_base_attr *dev_attr)
{
    int ret;

    dev_cb->rscb = rscb;
    dev_cb->phyId = attr->phy_id;
    dev_cb->eid_index = attr->ub.eid_index;
    dev_cb->eid = attr->ub.eid;

    ret = pthread_mutex_init(&dev_cb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("mutex_init failed ret:%d", ret), -ESYSFUNC);

    RS_INIT_LIST_HEAD(&dev_cb->async_event_list);
    RS_INIT_LIST_HEAD(&dev_cb->jfce_list);
    RS_INIT_LIST_HEAD(&dev_cb->jfc_list);
    RS_INIT_LIST_HEAD(&dev_cb->jetty_list);
    RS_INIT_LIST_HEAD(&dev_cb->rjetty_list);
    RS_INIT_LIST_HEAD(&dev_cb->token_id_list);
    RS_INIT_LIST_HEAD(&dev_cb->lseg_list);
    RS_INIT_LIST_HEAD(&dev_cb->rseg_list);

    ret = rs_ub_create_ctx(dev_cb->urma_dev, attr->ub.eid_index, &(dev_cb->urma_ctx));
    if (ret != 0) {
        hccp_err("rs_ub_create_ctx failed, ret:%d", ret);
        goto destroy_mutex;
    }

    ret = RsEpollCtl(dev_cb->rscb->connCb.epollfd, EPOLL_CTL_ADD, dev_cb->urma_ctx->async_fd, EPOLLIN | EPOLLRDHUP);
    if (ret != 0) {
        hccp_err("rs_epoll_ctl failed, ret:%d fd:%d", ret, dev_cb->urma_ctx->async_fd);
        goto close_dev;
    }

    ret = rs_ub_get_dev_attr(dev_cb, dev_attr, devIndex);
    if (ret != 0) {
        hccp_err("rs_ub_get_dev_attr failed, ret:%d", ret);
        goto epoll_del;
    }

    return 0;

epoll_del:
    (void)RsEpollCtl(dev_cb->rscb->connCb.epollfd, EPOLL_CTL_DEL, dev_cb->urma_ctx->async_fd, EPOLLIN | EPOLLRDHUP);
close_dev:
    (void)rs_urma_delete_context(dev_cb->urma_ctx);
destroy_mutex:
    pthread_mutex_destroy(&dev_cb->mutex);
    return ret;
}

int rs_ub_ctx_init(struct rs_cb *rs_cb, struct ctx_init_attr *attr, unsigned int *devIndex,
    struct dev_base_attr *dev_attr)
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    urma_eid_t eid;
    int ret;

    ret = rs_ub_api_init();
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ub_api_init failed, ret:%d", ret), ret);

    dev_cb = calloc(1, sizeof(struct rs_ub_dev_cb));
    if (dev_cb == NULL) {
        hccp_err("calloc for dev_cb failed, errno:%d", errno);
        ret = -ENOMEM;
        goto ub_api_deinit;
    }

    (void)memcpy_s(eid.raw, sizeof(eid.raw), attr->ub.eid.raw, sizeof(attr->ub.eid.raw));

    dev_cb->urma_dev = rs_urma_get_device_by_eid(eid, URMA_TRANSPORT_UB);
    if (dev_cb->urma_dev == NULL) {
        hccp_err("rs_urma_get_device_by_eid failed, urma_dev is NULL, errno:%d eid:%016llx:%016llx", errno,
            (unsigned long long)be64toh(eid.in6.subnet_prefix), (unsigned long long)be64toh(eid.in6.interface_id));
        ret = -EINVAL;
        goto free_dev_cb;
    }

    ret = RsSensorNodeRegister(attr->phy_id, rs_cb);
    if (ret != 0) {
        hccp_err("rs_sensor_node_register failed, phy_id(%u), ret(%d)", attr->phy_id, ret);
        goto free_dev_cb;
    }

    ret = rs_ub_dev_cb_init(attr, dev_cb, rs_cb, devIndex, dev_attr);
    if (ret != 0) {
        RsSensorNodeUnregister(dev_cb->rscb);
        hccp_err("rs_ub_dev_cb_init failed ret:%d, eid_index:%u eid:%016llx:%016llx", ret, attr->ub.eid_index,
            (unsigned long long)be64toh(eid.in6.subnet_prefix), (unsigned long long)be64toh(eid.in6.interface_id));
        goto free_dev_cb;
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->mutex);
    RsListAddTail(&dev_cb->list, &rs_cb->rdevList);
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->mutex);

    hccp_run_info("[init][rs_ctx]phy_id:%u, eid_index:%u, devIndex:0x%x init success", attr->phy_id,
        attr->ub.eid_index, *devIndex);

    return 0;

free_dev_cb:
    free(dev_cb);
    dev_cb = NULL;
ub_api_deinit:
    rs_ub_api_deinit();
    return ret;
}

int rs_ub_get_eid_by_ip(struct rs_ub_dev_cb *dev_cb, struct IpInfo ip[], union hccp_eid eid[], unsigned int *num)
{
    urma_net_addr_t net_addr = {0};
    unsigned int ip_num = *num;
    urma_eid_t urma_eid = {0};
    unsigned int i;
    int ret = 0;

    *num = 0;
    for (i = 0; i < ip_num; i++) {
        net_addr.sin_family = (sa_family_t)ip[i].family;
        if (net_addr.sin_family == AF_INET) {
            net_addr.in4 = ip[i].ip.addr;
        } else {
            net_addr.in6 = ip[i].ip.addr6;
        }
        ret = rs_urma_get_eid_by_ip(dev_cb->urma_ctx, &net_addr, &urma_eid);
        CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_eid_by_ip failed, ret:%d dev_index:0x%x", ret, dev_cb->index),
            -EOPENSRC);
        (void)memcpy_s(eid[i].raw, sizeof(union hccp_eid), urma_eid.raw, sizeof(urma_eid_t));
        (*num)++;
    }

    return ret;
}

STATIC int rs_ub_get_jfc_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr, struct rs_ctx_jfc_cb **jfc_cb)
{
    struct rs_ctx_jfc_cb **temp_jfc_cb = jfc_cb;
    struct rs_ctx_jfc_cb *jfc_cb_curr = NULL;
    struct rs_ctx_jfc_cb *jfc_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(jfc_cb_curr, jfc_cb_next, &dev_cb->jfc_list, list, struct rs_ctx_jfc_cb);
    for (; (&jfc_cb_curr->list) != &dev_cb->jfc_list;
        jfc_cb_curr = jfc_cb_next,
        jfc_cb_next = list_entry(jfc_cb_next->list.next, struct rs_ctx_jfc_cb, list)) {
        if (jfc_cb_curr->jfc_addr == addr) {
            *temp_jfc_cb = jfc_cb_curr;
            return 0;
        }
    }

    *temp_jfc_cb = NULL;
    hccp_err("jfc_cb for jfc_addr:0x%llx do not available!", addr);

    return -ENODEV;
}

STATIC int rs_ub_free_jfc_cb(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfc_cb *jfc_cb)
{
    urma_jfc_t *urma_jfc = NULL;
    int ret = 0;

    RsListDel(&jfc_cb->list);
    dev_cb->jfc_cnt--;

    if (jfc_cb->jfc_type == JFC_MODE_STARS_POLL || jfc_cb->jfc_type == JFC_MODE_CCU_POLL ||
        jfc_cb->jfc_type == JFC_MODE_USER_CTL_NORMAL) {
        (void)rs_ub_delete_jfc_ext(dev_cb, jfc_cb);
        hccp_info("[deinit][rs_jfc]destroy success, dev jfc_cnt:%u", dev_cb->jfc_cnt);
    } else if (jfc_cb->jfc_type == JFC_MODE_NORMAL) {
        urma_jfc = (urma_jfc_t *)(uintptr_t)jfc_cb->jfc_addr;
        (void)rs_urma_delete_jfc(urma_jfc);
        hccp_info("[deinit][rs_jfc]destroy success, dev jfc_cnt:%u", dev_cb->jfc_cnt);
    } else {
        hccp_err("jfc_type:%d is invalid, not support!", jfc_cb->jfc_type);
        ret = -EINVAL;
    }

    free(jfc_cb);
    jfc_cb = NULL;
    return ret;
}

int rs_ub_ctx_jfc_destroy(struct rs_ub_dev_cb *dev_cb, unsigned long long addr)
{
    struct rs_ctx_jfc_cb *jfc_cb = NULL;
    int ret;

    hccp_info("[deinit][rs_jfc]destroy addr:0x%llx", addr);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    ret = rs_ub_get_jfc_cb(dev_cb, addr, &jfc_cb);
    if (ret != 0) {
        hccp_err("get jfc_cb failed, ret:%d, jfc addr:0x%llx", ret, addr);
        goto out;
    }

    ret = rs_ub_free_jfc_cb(dev_cb, jfc_cb);
out:
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    return ret;
}

STATIC void rs_ub_free_jfc_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jfc_list)
{
    struct rs_ctx_jfc_cb *jfc_curr = NULL;
    struct rs_ctx_jfc_cb *jfc_next = NULL;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    if (!RsListEmpty(jfc_list)) {
        hccp_warn("jfc list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(jfc_curr, jfc_next, jfc_list, list, struct rs_ctx_jfc_cb);
        for (; (&jfc_curr->list) != jfc_list;
            jfc_curr = jfc_next, jfc_next = list_entry(jfc_next->list.next, struct rs_ctx_jfc_cb, list)) {
            ret = rs_ub_free_jfc_cb(dev_cb, jfc_curr);
            if (ret != 0) {
                hccp_err("rs_ub_free_jfc_cb failed, ret:%d", ret);
            }
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

STATIC int rs_ub_free_seg_cb(struct rs_ub_dev_cb *dev_cb, struct rs_seg_cb *seg_cb)
{
    int ret;

    ret = rs_urma_unregister_seg(seg_cb->segment);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_unregister_seg failed ret:%d", ret), -EOPENSRC);

    RsListDel(&seg_cb->list);
    dev_cb->lseg_cnt--;
    free(seg_cb);
    seg_cb = NULL;

    return 0;
}

STATIC void rs_ub_free_seg_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *lseg_list,
                                   struct RsListHead *rseg_list)
{
    struct rs_seg_cb *seg_curr = NULL;
    struct rs_seg_cb *seg_next = NULL;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    if (!RsListEmpty(lseg_list)) {
        hccp_warn("lseg list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(seg_curr, seg_next, lseg_list, list, struct rs_seg_cb);
        for (; (&seg_curr->list) != lseg_list;
            seg_curr = seg_next, seg_next = list_entry(seg_next->list.next, struct rs_seg_cb, list)) {
            ret = rs_ub_free_seg_cb(dev_cb, seg_curr);
            if (ret != 0) {
                hccp_err("rs_ub_free_seg_cb failed, ret:%d", ret);
            }
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    if (!RsListEmpty(rseg_list)) {
        hccp_warn("rseg list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(seg_curr, seg_next, rseg_list, list, struct rs_seg_cb);
        for (; (&seg_curr->list) != rseg_list;
            seg_curr = seg_next, seg_next = list_entry(seg_next->list.next, struct rs_seg_cb, list)) {
            (void)rs_urma_unimport_seg(seg_curr->segment);
            RsListDel(&seg_curr->list);
            free(seg_curr);
            seg_curr = NULL;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

STATIC void rs_ub_ctx_free_jetty_cb(struct rs_ctx_jetty_cb *jetty_cb)
{
    struct rs_ctx_jetty_cb *tmp_jetty_cb = jetty_cb;

#ifdef CUSTOM_INTERFACE
    (void)DlHalBuffFree((void *)(uintptr_t)jetty_cb->qp_share_info_addr);
#endif

    pthread_mutex_destroy(&tmp_jetty_cb->cr_err_info.mutex);
    pthread_mutex_destroy(&tmp_jetty_cb->mutex);
    free(tmp_jetty_cb);
    tmp_jetty_cb = NULL;
}

STATIC void rs_ub_ctx_drv_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb)
{
    if (jetty_cb->jetty_mode == JETTY_MODE_URMA_NORMAL) {
        (void)rs_urma_delete_jetty(jetty_cb->jetty);
    } else {
        rs_ub_ctx_ext_jetty_delete(jetty_cb);
    }

    // ccu jetty unreg db addr
    if (jetty_cb->jetty_mode == JETTY_MODE_CCU || jetty_cb->jetty_mode == JETTY_MODE_CCU_TA_CACHE) {
        (void)rs_ub_ctx_lmem_unreg(jetty_cb->dev_cb, jetty_cb->db_seg_handle);
    }

    (void)rs_urma_delete_jfr(jetty_cb->jfr);
}

STATIC int rs_ub_free_rem_jetty_cb(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_rem_jetty_cb *rjetty_cb)
{
    unsigned int rem_jetty_id = rjetty_cb->tjetty->id.id;
    unsigned int devIndex = dev_cb->index;
    int ret;

    RsListDel(&rjetty_cb->list);
    dev_cb->rjetty_cnt--;

    ret = rs_urma_unimport_jetty(rjetty_cb->tjetty);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_unimport_jetty failed, ret:%d, devIndex:0x%x, rem_jetty_id %u",
        ret, devIndex, rem_jetty_id), -EOPENSRC);

    free(rjetty_cb);
    rjetty_cb = NULL;

    return 0;
}

STATIC void rs_ub_unbind_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jetty_list)
{
    struct rs_ctx_jetty_cb *jetty_curr = NULL;
    struct rs_ctx_jetty_cb *jetty_next = NULL;
    int ret;

    if (!RsListEmpty(jetty_list)) {
        hccp_warn("jetty list do not empty! start to unbind");
        RS_LIST_GET_HEAD_ENTRY(jetty_curr, jetty_next, jetty_list, list, struct rs_ctx_jetty_cb);
        for (; (&jetty_curr->list) != jetty_list;
            jetty_curr = jetty_next, jetty_next = list_entry(jetty_next->list.next, struct rs_ctx_jetty_cb, list)) {
            // no need to unbind
            if (jetty_curr->state != RS_JETTY_STATE_BIND) {
                continue;
            }
            hccp_info("jetty_id[%u] will be unbind", jetty_curr->jetty->jetty_id.id);
            ret = rs_urma_unbind_jetty(jetty_curr->jetty);
            if (ret != 0) {
                hccp_err("rs_urma_unbind_jetty failed, ret:%d errno:%d devIndex:0x%x jetty_id:%u",
                    ret, errno, dev_cb->index, jetty_curr->jetty->jetty_id.id);
            }
            jetty_curr->state = RS_JETTY_STATE_CREATED;
        }
    }
}

STATIC void rs_ub_unimport_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *rjetty_list)
{
    struct rs_ctx_rem_jetty_cb *rem_jetty_curr = NULL;
    struct rs_ctx_rem_jetty_cb *rem_jetty_next = NULL;
    int ret;

    if (!RsListEmpty(rjetty_list)) {
        hccp_warn("rjetty list do not empty! start to unimport");
        RS_LIST_GET_HEAD_ENTRY(rem_jetty_curr, rem_jetty_next, rjetty_list, list, struct rs_ctx_rem_jetty_cb);
        for (; (&rem_jetty_curr->list) != rjetty_list;
            rem_jetty_curr = rem_jetty_next, rem_jetty_next = list_entry(rem_jetty_next->list.next,
            struct rs_ctx_rem_jetty_cb, list)) {
            // no need to unimport
            if (rem_jetty_curr->state != RS_JETTY_STATE_IMPORTED) {
                continue;
            }
            hccp_info("rjetty_id[%u] will be destroyed", rem_jetty_curr->tjetty->id.id);
            ret = rs_ub_free_rem_jetty_cb(dev_cb, rem_jetty_curr);
            if (ret != 0) {
                hccp_err("rs_ub_ctx_jetty_unimport failed, ret:%d", ret);
            }
        }
    }
}

STATIC int rs_ub_calloc_jetty_batch_info(struct jetty_destroy_batch_info *batch_info, unsigned int num)
{
    batch_info->jetty_cb_arr = calloc(num, sizeof(struct rs_ctx_jetty_cb *));
    CHK_PRT_RETURN(batch_info->jetty_cb_arr == NULL, hccp_err("calloc jetty_cb_arr failed"), -ENOMEM);
    batch_info->jetty_arr = calloc(num, sizeof(urma_jetty_t *));
    CHK_PRT_RETURN(batch_info->jetty_arr == NULL, hccp_err("calloc jetty_arr failed"), -ENOMEM);
    batch_info->jfr_arr = calloc(num, sizeof(urma_jfr_t *));
    CHK_PRT_RETURN(batch_info->jfr_arr == NULL, hccp_err("calloc jfr_arr failed"), -ENOMEM);

    return 0;
}

STATIC void rs_ub_free_jetty_batch_info(struct jetty_destroy_batch_info *batch_info)
{
    if (batch_info->jetty_cb_arr != NULL) {
        free(batch_info->jetty_cb_arr);
        batch_info->jetty_cb_arr = NULL;
    }
    if (batch_info->jetty_arr != NULL) {
        free(batch_info->jetty_arr);
        batch_info->jetty_arr = NULL;
    }
    if (batch_info->jfr_arr != NULL) {
        free(batch_info->jfr_arr);
        batch_info->jfr_arr = NULL;
    }
}

STATIC void rs_ub_free_jetty_cb_batch(struct jetty_destroy_batch_info *batch_info, unsigned int *num,
    urma_jetty_t *bad_jetty, urma_jfr_t *bad_jfr)
{
    unsigned int jetty_destroy_num = *num;
    unsigned int jfr_destroy_num = *num;
    unsigned int i;

    for (i = 0; i < *num; ++i) {
        // ccu jetty unreg db addr
        if (batch_info->jetty_cb_arr[i]->jetty_mode == JETTY_MODE_CCU ||
            batch_info->jetty_cb_arr[i]->jetty_mode == JETTY_MODE_CCU_TA_CACHE) {
            (void)rs_ub_ctx_lmem_unreg(batch_info->jetty_cb_arr[i]->dev_cb, batch_info->jetty_cb_arr[i]->db_seg_handle);
        }
        rs_ub_ctx_free_jetty_cb(batch_info->jetty_cb_arr[i]);

        if (batch_info->jetty_arr[i] == bad_jetty) {
            jetty_destroy_num = i;
        }
        if (batch_info->jfr_arr[i] == bad_jfr) {
            jfr_destroy_num = i;
        }
    }
    *num = jetty_destroy_num < jfr_destroy_num ? jetty_destroy_num: jfr_destroy_num;
}

STATIC int rs_ub_destroy_jetty_cb_batch(struct jetty_destroy_batch_info *batch_info, unsigned int *num)
{
    urma_jetty_t *bad_jetty = NULL;
    urma_jfr_t *bad_jfr = NULL;
    int jetty_destroy_ret = 0;
    int jfr_destroy_ret = 0;

    rs_ub_va_munmap_batch(batch_info->jetty_cb_arr, *num);
    jetty_destroy_ret = rs_urma_delete_jetty_batch(batch_info->jetty_arr, (int)*num, &bad_jetty);
    if (jetty_destroy_ret != 0) {
        hccp_err("rs_urma_delete_jetty_batch failed, jetty_destroy_ret:%d, num:%u", jetty_destroy_ret, *num);
    }

    jfr_destroy_ret = rs_urma_delete_jfr_batch(batch_info->jfr_arr, (int)*num, &bad_jfr);
    if (jfr_destroy_ret != 0) {
        hccp_err("rs_urma_delete_jfr_batch failed, jfr_destroy_ret:%d, num:%u", jfr_destroy_ret, *num);
    }

    rs_ub_free_jetty_id_batch(batch_info->jetty_cb_arr, *num);
    rs_ub_free_jetty_cb_batch(batch_info, num, bad_jetty, bad_jfr);
    return jetty_destroy_ret + jfr_destroy_ret;
}

STATIC void rs_ub_destroy_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jetty_list)
{
    struct jetty_destroy_batch_info batch_info = {0};
    struct rs_ctx_jetty_cb *jetty_curr = NULL;
    struct rs_ctx_jetty_cb *jetty_next = NULL;
    unsigned int num = 0, i = 0;
    int ret;

    if (RsListEmpty(jetty_list)) {
        return;
    }

    hccp_warn("jetty list is not empty! start to delete");
    ret = rs_ub_calloc_jetty_batch_info(&batch_info, dev_cb->jetty_cnt);
    if (ret != 0) {
        hccp_err("rs_ub_calloc_jetty_batch_info failed, ret:%d", ret);
        goto free_batch_info;
    }

    RS_LIST_GET_HEAD_ENTRY(jetty_curr, jetty_next, jetty_list, list, struct rs_ctx_jetty_cb);
    for (; (&jetty_curr->list) != jetty_list;
        jetty_curr = jetty_next, jetty_next = list_entry(jetty_next->list.next, struct rs_ctx_jetty_cb, list)) {
        // no need to destroy
        if (jetty_curr->state != RS_JETTY_STATE_CREATED) {
            continue;
        }
        hccp_info("jetty_id[%u] will be destroyed", jetty_curr->jetty->jetty_id.id);
        batch_info.jetty_cb_arr[i] = jetty_curr;
        batch_info.jetty_arr[i] = jetty_curr->jetty;
        batch_info.jfr_arr[i] = jetty_curr->jfr;

        RsListDel(&jetty_curr->list);
        dev_cb->jetty_cnt--;
        i++;
    }

    num = i;
    if (num != 0) {
        ret = rs_ub_destroy_jetty_cb_batch(&batch_info, &num);
        if (ret != 0) {
            hccp_err("rs_ub_ctx_jetty_destroy_batch failed, ret:%d, need to be destroyed:%u, actually destroyed:%u",
                ret, i, num);
        }
    }

free_batch_info:
    rs_ub_free_jetty_batch_info(&batch_info);
}

void rs_ub_free_jetty_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jetty_list,
    struct RsListHead *rjetty_list)
{
    // free jetty step: unbind -> unimport -> delete
    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    rs_ub_unbind_jetty_cb_list(dev_cb, jetty_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    rs_ub_unimport_jetty_cb_list(dev_cb, rjetty_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    rs_ub_destroy_jetty_cb_list(dev_cb, jetty_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

int rs_ub_ctx_chan_create(struct rs_ub_dev_cb *dev_cb, union data_plane_cstm_flag data_plane_flag,
    unsigned long long *addr, int *fd)
{
    struct rs_ctx_jfce_cb *jfce_cb = NULL;
    urma_jfce_t *out_jfce = NULL;
    int ret = 0;

    jfce_cb = calloc(1, sizeof(struct rs_ctx_jfce_cb));
    CHK_PRT_RETURN(jfce_cb == NULL, hccp_err("calloc jfce_cb failed"), -ENOMEM);

    jfce_cb->dev_cb = dev_cb;
    out_jfce = rs_urma_create_jfce(dev_cb->urma_ctx);
    if (out_jfce == NULL) {
        hccp_err("rs_urma_create_jfce failed, errno:%d", errno);
        ret = -EOPENSRC;
        goto free_ctx_jfce_cb;
    }

    jfce_cb->jfce_addr = (uint64_t)(uintptr_t)out_jfce; // urma_jfce_t *
    *addr = jfce_cb->jfce_addr;

    if (data_plane_flag.bs.poll_cq_cstm == 0) {
        ret = RsEpollCtl(dev_cb->rscb->connCb.epollfd, EPOLL_CTL_ADD, out_jfce->fd, EPOLLIN | EPOLLRDHUP);
        if (ret != 0) {
            hccp_err("rs_epoll_ctl failed ret:%d, fd:%d", ret, dev_cb->rscb->connCb.epollfd);
            goto delete_jfce;
        }
    }
    jfce_cb->data_plane_flag = data_plane_flag;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListAddTail(&jfce_cb->list, &dev_cb->jfce_list);
    jfce_cb->dev_cb->jfce_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    *fd = out_jfce->fd;
    hccp_info("dev_index:0x%x jfce addr:0x%llx fd:%d", dev_cb->index, jfce_cb->jfce_addr, out_jfce->fd);
    return ret;

delete_jfce:
    (void)rs_urma_delete_jfce(out_jfce);
free_ctx_jfce_cb:
    free(jfce_cb);
    jfce_cb = NULL;
    return ret;
}

STATIC int rs_ub_get_jfce_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr, struct rs_ctx_jfce_cb **jfce_cb)
{
    struct rs_ctx_jfce_cb **temp_jfce_cb = jfce_cb;
    struct rs_ctx_jfce_cb *jfce_cb_curr = NULL;
    struct rs_ctx_jfce_cb *jfce_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(jfce_cb_curr, jfce_cb_next, &dev_cb->jfce_list, list, struct rs_ctx_jfce_cb);
    for (; (&jfce_cb_curr->list) != (&dev_cb->jfce_list);
        jfce_cb_curr = jfce_cb_next,
        jfce_cb_next = list_entry(jfce_cb_next->list.next, struct rs_ctx_jfce_cb, list)) {
        if (jfce_cb_curr->jfce_addr == addr) {
            *temp_jfce_cb = jfce_cb_curr;
            return 0;
        }
    }

    *temp_jfce_cb = NULL;
    hccp_err("jfce_cb for jfce_addr:0x%llx do not available!", addr);

    return -ENODEV;
}

int rs_ub_ctx_chan_destroy(struct rs_ub_dev_cb *dev_cb, unsigned long long addr)
{
    struct rs_ctx_jfce_cb *jfce_cb = NULL;
    urma_jfce_t *jfce = NULL;
    int ret;

    hccp_info("[rs_ctx_chan]jfce destroy addr:0x%llx", addr);

    ret = rs_ub_get_jfce_cb(dev_cb, addr, &jfce_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get jfce_cb failed, ret:%d, jfce addr:0x%llx", ret, addr), ret);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&jfce_cb->list);
    jfce_cb->dev_cb->jfce_cnt--;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    jfce = (urma_jfce_t *)(uintptr_t)jfce_cb->jfce_addr;
    if (jfce_cb->data_plane_flag.bs.poll_cq_cstm == 0) {
        (void)RsEpollCtl(dev_cb->rscb->connCb.epollfd, EPOLL_CTL_DEL, jfce->fd, EPOLLIN | EPOLLRDHUP);
    }

    ret = rs_urma_delete_jfce(jfce);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_delete_jfce failed, ret:%d errno:%d jfce addr:0x%llx", ret, errno,
        jfce_cb->jfce_addr), -EOPENSRC);

    free(jfce_cb);
    jfce_cb = NULL;

    hccp_info("rs ctx jfce destroy success, dev jfce num is %u", dev_cb->jfce_cnt);
    return ret;
}

STATIC int rs_ub_free_jfce_cb(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jfce_cb *jfce_cb)
{
    int ret = 0;

    RsListDel(&jfce_cb->list);
    dev_cb->jfce_cnt--;

    ret = rs_urma_delete_jfce((urma_jfce_t *)(uintptr_t)jfce_cb->jfce_addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("[rs_ctx_chan]rs_ub_delete_jfce failed, ret:%d, jfce addr:0x%llx",
        ret, jfce_cb->jfce_addr), -EOPENSRC);

    free(jfce_cb);
    jfce_cb = NULL;

    return 0;
}

STATIC void rs_ub_free_jfce_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *jfce_list)
{
    struct rs_ctx_jfce_cb *jfce_curr = NULL;
    struct rs_ctx_jfce_cb *jfce_next = NULL;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    if (!RsListEmpty(jfce_list)) {
        hccp_warn("jfce list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(jfce_curr, jfce_next, jfce_list, list, struct rs_ctx_jfce_cb);
        for (; (&jfce_curr->list) != jfce_list;
            jfce_curr = jfce_next, jfce_next = list_entry(jfce_next->list.next, struct rs_ctx_jfce_cb, list)) {
            ret = rs_ub_free_jfce_cb(dev_cb, jfce_curr);
            if (ret != 0) {
                hccp_err("rs_ub_free_jfce_cb failed, ret:%d", ret);
            }
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

int rs_ub_ctx_token_id_alloc(struct rs_ub_dev_cb *dev_cb, unsigned long long *addr,
    unsigned int *token_id)
{
    struct rs_token_id_cb *token_id_cb = NULL;

    token_id_cb = calloc(1, sizeof(struct rs_token_id_cb));
    CHK_PRT_RETURN(token_id_cb == NULL, hccp_err("calloc token_id_cb failed"), -ENOMEM);

    token_id_cb->dev_cb = dev_cb;
    token_id_cb->token_id = rs_urma_alloc_token_id(dev_cb->urma_ctx);
    if (token_id_cb->token_id == NULL) {
        hccp_err("rs_urma_alloc_token_id failed, errno:%d, devIndex:0x%x", errno, dev_cb->index);
        goto free_ctx_token_id_cb;
    }

    *addr = (uint64_t)(uintptr_t)token_id_cb->token_id; // urma_token_id_t *
    *token_id = token_id_cb->token_id->token_id;
    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListAddTail(&token_id_cb->list, &dev_cb->token_id_list);
    token_id_cb->dev_cb->token_id_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    hccp_info("alloc success, token_id addr:0x%llx, devIndex:0x%x", *addr, dev_cb->index);
    return 0;

free_ctx_token_id_cb:
    free(token_id_cb);
    token_id_cb = NULL;
    return -EOPENSRC;
}

STATIC int rs_ub_get_token_id_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long addr,
    struct rs_token_id_cb **token_id_cb)
{
    struct rs_token_id_cb **temp_token_id_cb = token_id_cb;
    struct rs_token_id_cb *token_id_cb_curr = NULL;
    struct rs_token_id_cb *token_id_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(token_id_cb_curr, token_id_cb_next, &dev_cb->token_id_list, list, struct rs_token_id_cb);
    for (; (&token_id_cb_curr->list) != (&dev_cb->token_id_list);
        token_id_cb_curr = token_id_cb_next,
        token_id_cb_next = list_entry(token_id_cb_next->list.next, struct rs_token_id_cb, list)) {
        if ((uint64_t)(uintptr_t)token_id_cb_curr->token_id == addr) {
            *temp_token_id_cb = token_id_cb_curr;
            return 0;
        }
    }

    *temp_token_id_cb = NULL;
    hccp_err("token_id_cb for token_id addr:0x%llx do not available! devIndex:0x%x", addr, dev_cb->index);

    return -ENODEV;
}

STATIC int rs_ub_free_token_id_cb(struct rs_ub_dev_cb *dev_cb, struct rs_token_id_cb *token_id_cb)
{
    int ret = 0;

    RsListDel(&token_id_cb->list);
    dev_cb->token_id_cnt--;

    ret = rs_urma_free_token_id(token_id_cb->token_id);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_free_token_id failed, ret:%d, devIndex:0x%x, token_id addr:0x%llx",
        ret, dev_cb->index, (uint64_t)(uintptr_t)token_id_cb->token_id), -EOPENSRC);

    free(token_id_cb);
    token_id_cb = NULL;
    return 0;
}

int rs_ub_ctx_token_id_free(struct rs_ub_dev_cb *dev_cb, unsigned long long addr)
{
    struct rs_token_id_cb *token_id_cb = NULL;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    ret = rs_ub_get_token_id_cb(dev_cb, addr, &token_id_cb);
    if (ret != 0) {
        hccp_err("get token_id_cb failed! ret %d, devIndex:0x%x, token_id addr:0x%llx", ret, dev_cb->index, addr);
        goto free_lock;
    }
    ret = rs_ub_free_token_id_cb(dev_cb, token_id_cb);
    if (ret != 0) {
        hccp_err("free_token_id_cb failed, ret:%d, devIndex:0x%x, token_id addr:0x%llx", ret, dev_cb->index, addr);
        goto free_lock;
    }
    hccp_info("rs token id free success, dev token_id num is %u, devIndex:0x%x, token_id addr:0x%llx",
        dev_cb->token_id_cnt, dev_cb->index, addr);

free_lock:
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    return ret;
}

STATIC void rs_ub_free_token_id_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *token_id_list)
{
    struct rs_token_id_cb *token_id_curr = NULL;
    struct rs_token_id_cb *token_id_next = NULL;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    if (!RsListEmpty(token_id_list)) {
        hccp_warn("token_id list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(token_id_curr, token_id_next, token_id_list, list, struct rs_token_id_cb);
        for (; (&token_id_curr->list) != token_id_list; token_id_curr = token_id_next,
            token_id_next = list_entry(token_id_next->list.next, struct rs_token_id_cb, list)) {
            (void)rs_ub_free_token_id_cb(dev_cb, token_id_curr);
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    return;
}

STATIC void rs_ub_free_async_event_cb(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_async_event_cb *async_event_cb)
{
    RsListDel(&async_event_cb->list);
    dev_cb->async_event_cnt--;

    free(async_event_cb);
    async_event_cb = NULL;
}

STATIC void rs_ub_free_async_event_cb_list(struct rs_ub_dev_cb *dev_cb, struct RsListHead *async_event_list)
{
    struct rs_ctx_async_event_cb *async_event_curr = NULL;
    struct rs_ctx_async_event_cb *async_event_next = NULL;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    (void)RsEpollCtl(dev_cb->rscb->connCb.epollfd, EPOLL_CTL_DEL, dev_cb->urma_ctx->async_fd, EPOLLIN | EPOLLRDHUP);
    if (!RsListEmpty(async_event_list)) {
        hccp_run_warn("async_event list do not empty!");
        RS_LIST_GET_HEAD_ENTRY(async_event_curr, async_event_next, async_event_list, list,
            struct rs_ctx_async_event_cb);
        for (; (&async_event_curr->list) != async_event_list; async_event_curr = async_event_next,
            async_event_next = list_entry(async_event_next->list.next, struct rs_ctx_async_event_cb, list)) {
            rs_ub_free_async_event_cb(dev_cb, async_event_curr);
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

int rs_ub_ctx_deinit(struct rs_ub_dev_cb *dev_cb)
{
    int ret;

    hccp_info("[deinit][rs_ctx]start deinit, phy_id:%u, devIndex:0x%x", dev_cb->phyId, dev_cb->index);

    rs_ub_free_seg_cb_list(dev_cb, &dev_cb->lseg_list, &dev_cb->rseg_list);
    rs_ub_free_jetty_cb_list(dev_cb, &dev_cb->jetty_list, &dev_cb->rjetty_list);
    rs_ub_free_jfc_cb_list(dev_cb, &dev_cb->jfc_list);
    rs_ub_free_jfce_cb_list(dev_cb, &dev_cb->jfce_list);
    rs_ub_free_token_id_cb_list(dev_cb, &dev_cb->token_id_list);
    rs_ub_free_async_event_cb_list(dev_cb, &dev_cb->async_event_list);

    ret = rs_urma_delete_context(dev_cb->urma_ctx);
    if (ret != 0) {
        hccp_err("rs_urma_delete_context failed, ret:%d", ret);
    }

    pthread_mutex_destroy(&dev_cb->mutex);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->rscb->mutex);
    RsListDel(&dev_cb->list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->rscb->mutex);
    RsSensorNodeUnregister(dev_cb->rscb);

    rs_ub_api_deinit();

    hccp_run_info("[deinit][rs_ctx]deinit success, phy_id:%u, devIndex:0x%x", dev_cb->phyId, dev_cb->index);
    free(dev_cb);
    dev_cb = NULL;
    return 0;
}

int rs_ub_get_tp_info_list(struct rs_ub_dev_cb *dev_cb, struct get_tp_cfg *cfg, struct tp_info info_list[],
    unsigned int *num)
{
    urma_tp_info_t tp_list[HCCP_MAX_TPID_INFO_NUM] = {0};
    urma_get_tp_cfg_t udma_cfg = {0};
    unsigned int tp_cnt = *num;
    unsigned int i;
    int ret = 0;

    udma_cfg.flag.value = cfg->flag.value;
    udma_cfg.trans_mode = (urma_transport_mode_t)cfg->trans_mode;
    (void)memcpy_s(udma_cfg.local_eid.raw, sizeof(udma_cfg.local_eid.raw),
        cfg->local_eid.raw, sizeof(cfg->local_eid.raw));
    (void)memcpy_s(udma_cfg.peer_eid.raw, sizeof(udma_cfg.peer_eid.raw), cfg->peer_eid.raw, sizeof(cfg->peer_eid.raw));

    ret = rs_urma_get_tp_list(dev_cb->urma_ctx, &udma_cfg, &tp_cnt, tp_list);
    CHK_PRT_RETURN(ret != 0, hccp_err("[rs_ub_ctx]rs_urma_get_tp_list failed, ret[%d] trans_mode:%u flag:0x%x "
        "tp_cnt:%u local_eid:%016llx:%016llx peer_eid:%016llx:%016llx", ret, cfg->trans_mode, cfg->flag.value, tp_cnt,
        (unsigned long long)be64toh(cfg->local_eid.in6.subnet_prefix),
        (unsigned long long)be64toh(cfg->local_eid.in6.interface_id),
        (unsigned long long)be64toh(cfg->peer_eid.in6.subnet_prefix),
        (unsigned long long)be64toh(cfg->peer_eid.in6.interface_id)), -EOPENSRC);

    *num = (tp_cnt > *num) ? *num : tp_cnt;
    for (i = 0; i < *num; ++i) {
        info_list[i].tp_handle = tp_list[i].tp_handle;
    }

    return ret;
}

uint8_t rs_get_bitmap_count(unsigned int attr_bitmap)
{
    unsigned int bitmap = attr_bitmap;
    uint8_t bitmap_cnt = 0;

    while(bitmap != 0) {
        bitmap &= (bitmap - 1); // clear the last digit 1
        bitmap_cnt++;
    }
    return bitmap_cnt;
}

int rs_ub_get_tp_attr(struct rs_ub_dev_cb *dev_cb, unsigned int *attr_bitmap, const uint64_t tp_handle,
    struct tp_attr *attr)
{
    uint8_t tp_attr_cnt = 0;
    int ret;

    tp_attr_cnt = rs_get_bitmap_count(*attr_bitmap);
    ret = rs_urma_get_tp_attr(dev_cb->urma_ctx, tp_handle, &tp_attr_cnt, attr_bitmap,
        (urma_tp_attr_value_t *)attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_get_tp_attr failed, attr_bitmap:%u ret:%d errno:%d",
        *attr_bitmap, ret, errno), ret);

    return ret;
}

int rs_ub_set_tp_attr(struct rs_ub_dev_cb *dev_cb, const unsigned int attr_bitmap, const uint64_t tp_handle,
    struct tp_attr *attr)
{
    uint8_t tp_attr_cnt = 0;
    int ret;

    tp_attr_cnt = rs_get_bitmap_count(attr_bitmap);
    ret = rs_urma_set_tp_attr(dev_cb->urma_ctx, tp_handle, tp_attr_cnt, attr_bitmap,
        (urma_tp_attr_value_t *)attr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_set_tp_attr failed, attr_bitmap:%u ret:%d errno:%d",
        attr_bitmap, ret, errno), ret);

    return ret;
}

STATIC int rs_ub_query_seg_cb(struct rs_ub_dev_cb *dev_cb, uint64_t addr, struct rs_seg_cb **seg_cb,
    struct RsListHead *seg_list)
{
    struct rs_seg_cb *seg_curr = NULL;
    struct rs_seg_cb *seg_next = NULL;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RS_LIST_GET_HEAD_ENTRY(seg_curr, seg_next, seg_list, list, struct rs_seg_cb);
    for (; (&seg_curr->list) != seg_list;
        seg_curr = seg_next, seg_next = list_entry(seg_next->list.next, struct rs_seg_cb, list)) {
        if ((seg_curr->seg_info.addr <= addr) && (addr < seg_curr->seg_info.addr + seg_curr->seg_info.len)) {
            *seg_cb = seg_curr;
            RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
            return 0;
        }
    }

    *seg_cb = NULL;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    hccp_info("cannot find seg_cb for addr@0x%lx", addr);
    return -ENODEV;
}

STATIC int rs_ub_init_seg_cb(struct mem_reg_attr_t *mem_attr, struct rs_ub_dev_cb *dev_cb, struct rs_seg_cb *seg_cb)
{
    struct rs_token_id_cb *token_id_cb = NULL;
    urma_seg_cfg_t seg_cfg = {0};
    int ret = 0;

    seg_cfg.flag.value = mem_attr->ub.flags.value;
    seg_cfg.va = mem_attr->mem.addr;
    seg_cfg.len = mem_attr->mem.size;
    seg_cfg.token_value = seg_cb->token_value;
    seg_cfg.user_ctx = (uintptr_t)NULL;
    seg_cfg.iova = 0;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);

    // token id in cfg is valid, get token id by mem_attr->ub.token_id_addr
    if (seg_cfg.flag.bs.token_id_valid == URMA_TOKEN_ID_VALID) {
        ret = rs_ub_get_token_id_cb(dev_cb, mem_attr->ub.token_id_addr, &token_id_cb);
        if (ret != 0) {
            hccp_err("get token_id_cb failed! ret %d, devIndex:0x%x, token_id addr:0x%llx", ret, dev_cb->index,
                mem_attr->ub.token_id_addr);
            goto free_lock;
        }
        seg_cfg.token_id = token_id_cb->token_id;
    }

    seg_cb->segment = rs_urma_register_seg(dev_cb->urma_ctx, &seg_cfg);
    if (seg_cb->segment == NULL) {
        hccp_err("[init][rs_ctx_lmem]rs_urma_register_seg len[0x%llx] failed, errno:%d", seg_cfg.len, errno);
        ret = -EOPENSRC;
        goto free_lock;
    }

    seg_cb->seg_info.seg = seg_cb->segment->seg;
    // resv len as 1 to save addr for later unreg to query
    seg_cb->seg_info.addr = (uint64_t)(uintptr_t)seg_cb->segment;
    seg_cb->seg_info.len = 1;
    RsListAddTail(&seg_cb->list, &dev_cb->lseg_list);
    dev_cb->lseg_cnt++;

free_lock:
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    return ret;
}

STATIC void rs_ub_deinit_seg_cb(struct rs_ub_dev_cb *dev_cb, struct rs_seg_cb *seg_cb)
{
    (void)rs_urma_unregister_seg(seg_cb->segment);
    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&seg_cb->list);
    dev_cb->lseg_cnt--;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}

int rs_ub_ctx_lmem_reg(struct rs_ub_dev_cb *dev_cb, struct mem_reg_attr_t *mem_attr, struct mem_reg_info_t *mem_info)
{
    struct rs_seg_cb *lseg_cb = NULL;
    int ret;

    CHK_PRT_RETURN(mem_attr->mem.size == 0, hccp_err("[init][rs_ctx_lmem]mem_attr->mem.size is 0"), -EINVAL);

    hccp_run_info("[init][rs_ctx_lmem]devIndex:0x%x addr:0x%llx, len[0x%llx], access[0x%x]",
        dev_cb->index, mem_attr->mem.addr, mem_attr->mem.size, mem_attr->ub.flags.bs.access);

    lseg_cb = calloc(1, sizeof(struct rs_seg_cb));
    CHK_PRT_RETURN(lseg_cb == NULL, hccp_err("[init][rs_ctx_lmem]calloc lseg_cb failed"), -ENOMEM);
    lseg_cb->dev_cb = dev_cb;
    lseg_cb->token_value.token = mem_attr->ub.token_value;

    ret = rs_ub_init_seg_cb(mem_attr, dev_cb, lseg_cb);
    if (ret != 0) {
        hccp_err("[init][rs_ctx_lmem]rs_ub_init_seg_cb failed, ret:%d", ret);
        goto init_err;
    }

    ret = memcpy_s(mem_info->key.value, sizeof(mem_info->key.value), &lseg_cb->seg_info.seg, sizeof(urma_seg_t));
    if (ret != 0) {
        hccp_err("[init][rs_ctx_lmem]memcpy_s for seg failed ret:%d", ret);
        ret = -ESAFEFUNC;
        goto reg_err;
    }
    mem_info->key.size = sizeof(urma_seg_t);
    mem_info->ub.token_id = lseg_cb->segment->token_id->token_id;
    mem_info->ub.target_seg_handle = (uintptr_t)lseg_cb->segment;

    hccp_info("[init][rs_ctx_lmem]reg succ, devIndex:0x%x addr:0x%llx, len[0x%llx], access[0x%x]",
        dev_cb->index, mem_attr->mem.addr, mem_attr->mem.size, mem_attr->ub.flags.bs.access);
    return 0;

reg_err:
    rs_ub_deinit_seg_cb(dev_cb, lseg_cb);
init_err:
    free(lseg_cb);
    lseg_cb = NULL;
    return ret;
}

int rs_ub_ctx_lmem_unreg(struct rs_ub_dev_cb *dev_cb, unsigned long long addr)
{
    struct rs_seg_cb *lseg_cb = NULL;
    int ret;

    hccp_info("[deinit][rs_ctx_lmem]devIndex:0x%x addr:0x%llx", dev_cb->index, addr);

    ret = rs_ub_query_seg_cb(dev_cb, addr, &lseg_cb, &dev_cb->lseg_list);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][rs_ctx_lmem]rs_ub_query_seg_cb failed ret:%d ", ret), ret);

    ret = rs_urma_unregister_seg(lseg_cb->segment);
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][rs_ctx_lmem]rs_urma_unregister_seg failed ret:%d ", ret), -EOPENSRC);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&lseg_cb->list);
    dev_cb->lseg_cnt--;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    hccp_run_info("[deinit][rs_ctx_lmem]devIndex:0x%x addr:0x%llx unregister segment success", dev_cb->index, addr);
    free(lseg_cb);
    lseg_cb = NULL;
    return 0;
}

int rs_ub_ctx_rmem_import(struct rs_ub_dev_cb *dev_cb, struct mem_import_attr_t *mem_attr,
    struct mem_import_info_t *mem_info)
{
    struct rs_seg_cb *rem_seg_cb = NULL;
    urma_import_seg_flag_t flag = {0};
    urma_token_t token_value = {0};
    uint64_t mapping_addr = 0;
    int ret;

    rem_seg_cb = calloc(1, sizeof(struct rs_seg_cb));
    CHK_PRT_RETURN(rem_seg_cb == NULL, hccp_err("[init][rs_ctx_rmem]calloc rem_seg_cb failed"), -ENOMEM);

    ret = memcpy_s(&rem_seg_cb->seg_info.seg, sizeof(urma_seg_t), &mem_attr->key.value, mem_attr->key.size);
    if (ret != 0) {
        hccp_err("[init][rs_ctx_rmem]memcpy_s failed, ret:%d", ret);
        ret = -ESAFEFUNC;
        goto free_rem_seg_cb;
    }

    token_value.token = mem_attr->ub.token_value;
    flag.value = mem_attr->ub.flags.value;
    // mapping_addr is needed if flag mapping set value
    if (mem_attr->ub.flags.bs.mapping == 1) {
        mapping_addr = mem_attr->ub.mapping_addr;
    }
    rem_seg_cb->segment = rs_urma_import_seg(dev_cb->urma_ctx, &rem_seg_cb->seg_info.seg, &token_value, mapping_addr,
        flag);
    if (rem_seg_cb->segment == NULL) {
        hccp_err("[init][rs_ctx_rmem]rs_urma_import_seg failed, errno:%d", errno);
        ret = -EOPENSRC;
        goto free_rem_seg_cb;
    }

    mem_info->ub.target_seg_handle = (uintptr_t)rem_seg_cb->segment;
    // resv len as 1 to save addr for later unimport to query
    rem_seg_cb->seg_info.addr = mem_info->ub.target_seg_handle;
    rem_seg_cb->seg_info.len = 1;

    hccp_run_info("[init][rs_ctx_rmem]devIndex:0x%x import addr:0x%llx", dev_cb->index, rem_seg_cb->seg_info.addr);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListAddTail(&rem_seg_cb->list, &dev_cb->rseg_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    return 0;

free_rem_seg_cb:
    free(rem_seg_cb);
    rem_seg_cb = NULL;
    return ret;
}

int rs_ub_ctx_rmem_unimport(struct rs_ub_dev_cb *dev_cb, unsigned long long addr)
{
    struct rs_seg_cb *rem_seg_cb = NULL;
    int ret;

    hccp_run_info("[deinit][rs_ctx_rmem]devIndex:0x%x unimport addr:0x%llx", dev_cb->index, addr);

    ret = rs_ub_query_seg_cb(dev_cb, addr, &rem_seg_cb, &dev_cb->rseg_list);
    if (ret != 0) {
        hccp_warn("[deinit][rs_ctx_rmem]can not find rem seg cb for addr:0x%llx", addr);
        return 0;
    }

    rs_urma_unimport_seg(rem_seg_cb->segment);
    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&rem_seg_cb->list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    free(rem_seg_cb);
    rem_seg_cb = NULL;
    return 0;
}

STATIC int rs_ub_ctx_jfc_create_normal(struct rs_ub_dev_cb *dev_cb, urma_jfc_cfg_t *jfc_cfg, urma_jfc_t **out_jfc)
{
    uint64_t jfce_addr = (uint64_t)(uintptr_t)jfc_cfg->jfce;
    struct rs_ctx_jfce_cb *jfce_cb = NULL;
    int ret = 0;

    if (jfc_cfg->jfce != NULL) {
        ret = rs_ub_get_jfce_cb(dev_cb, jfce_addr, &jfce_cb);
        CHK_PRT_RETURN(ret != 0, hccp_err("get jfce_cb failed, ret:%d, jfce addr:0x%llx", ret, jfce_addr), ret);
    }

    *out_jfc = rs_urma_create_jfc(dev_cb->urma_ctx, jfc_cfg);
    CHK_PRT_RETURN(*out_jfc == NULL, hccp_err("rs_urma_create_jfc failed, errno:%d", errno), -EOPENSRC);

    ret = rs_urma_rearm_jfc(*out_jfc, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_rearm_jfc failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    return ret;
}

STATIC void rs_ub_fill_jfc_info(struct rs_ctx_jfc_cb *jfc_cb, struct ctx_cq_info *info)
{
    info->addr = jfc_cb->jfc_addr;
    info->ub.id = jfc_cb->jfc_id;
    info->ub.cqe_size = WQE_BB_SIZE;
    info->ub.buf_addr = jfc_cb->buf_addr;
    info->ub.swdb_addr = jfc_cb->swdb_addr;
}

int rs_ub_ctx_jfc_create(struct rs_ub_dev_cb *dev_cb, struct ctx_cq_attr *attr, struct ctx_cq_info *info)
{
    struct rs_ctx_jfc_cb *jfc_cb = NULL;
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    jfc_cb = (struct rs_ctx_jfc_cb *)calloc(1, sizeof(struct rs_ctx_jfc_cb));
    CHK_PRT_RETURN(jfc_cb == NULL, hccp_err("calloc jfc_cb failed"), -ENOMEM);

    jfc_cb->dev_cb = dev_cb;
    jfc_cb->jfc_type = attr->ub.mode;
    jfc_cb->depth = attr->depth;
    jfc_cfg.depth = attr->depth;
    jfc_cfg.flag.value = attr->ub.flag.value;
    jfc_cfg.user_ctx = attr->ub.user_ctx;
    jfc_cfg.ceqn = attr->ub.ceqn;
    jfc_cfg.jfce = attr->chan_addr == 0 ? NULL : (urma_jfce_t *)(uintptr_t)attr->chan_addr;
    if (attr->ub.mode == JFC_MODE_STARS_POLL || attr->ub.mode == JFC_MODE_CCU_POLL ||
        attr->ub.mode == JFC_MODE_USER_CTL_NORMAL) {
        if (attr->ub.mode == JFC_MODE_CCU_POLL && attr->ub.ccu_ex_cfg.valid) {
            jfc_cb->ccu_ex_cfg.valid = attr->ub.ccu_ex_cfg.valid;
            jfc_cb->ccu_ex_cfg.cqe_flag = attr->ub.ccu_ex_cfg.cqe_flag;
        }
        ret = rs_ub_ctx_jfc_create_ext(jfc_cb, &jfc_cfg, &out_jfc);
        if (ret != 0) {
            hccp_err("rs_ub_ctx_jfc_create_ext jfc_mode:%d failed, ret:%d", attr->ub.mode, ret);
            goto jfc_cb_init_err;
        }
    } else if (attr->ub.mode == JFC_MODE_NORMAL) {
        jfc_cfg.jfce = (attr->chan_addr == 0) ? NULL : (urma_jfce_t *)(uintptr_t)attr->chan_addr;
        ret = rs_ub_ctx_jfc_create_normal(dev_cb, &jfc_cfg, &out_jfc);
        if (ret != 0) {
            hccp_err("rs_ub_ctx_jfc_create_normal failed, jfc_mode:%d ret:%d", attr->ub.mode, ret);
            goto jfc_cb_init_err;
        }
    } else {
        hccp_err("jfc_type %d is invalid, not support!", attr->ub.mode);
        ret = -EINVAL;
        goto jfc_cb_init_err;
    }
    jfc_cb->jfc_addr = (uint64_t)(uintptr_t)out_jfc; // urma_jfc_t *
    rs_ub_fill_jfc_info(jfc_cb, info);

    hccp_info("jfc addr:0x%llx", jfc_cb->jfc_addr);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    jfc_cb->dev_cb->jfc_cnt++;
    RsListAddTail(&jfc_cb->list, &dev_cb->jfc_list);
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    return 0;

jfc_cb_init_err:
    free(jfc_cb);
    jfc_cb = NULL;

    return ret;
}

STATIC bool rs_ub_is_jetty_mode_valid(int jetty_mode, bool lock_flag)
{
    if (jetty_mode < 0 || jetty_mode >= JETTY_MODE_MAX || (jetty_mode == JETTY_MODE_CCU_TA_CACHE && !lock_flag)) {
        return false;
    }

    return true;
}

STATIC int rs_ub_jetty_cb_init(struct rs_ub_dev_cb *dev_cb, struct ctx_qp_attr *jetty_attr,
    struct rs_ctx_jetty_cb *jetty_cb)
{
    bool lock_flag = jetty_attr->ub.ta_cache_mode.lock_flag;
    int jetty_mode = jetty_attr->ub.mode;

    if (!rs_ub_is_jetty_mode_valid(jetty_mode, lock_flag)) {
        hccp_err("unsupported jetty_mode:%d, lock_flag:%d", jetty_mode, lock_flag);
        return -EINVAL;
    }

    jetty_cb->tx_depth = jetty_attr->sq_depth;
    jetty_cb->rx_depth = jetty_attr->rq_depth;
    jetty_cb->dev_cb = dev_cb;
    jetty_cb->jetty_mode = jetty_mode;
    jetty_cb->jetty_id = jetty_attr->ub.jetty_id;
    jetty_cb->transport_mode = jetty_attr->transport_mode;
    jetty_cb->state = RS_QP_STATUS_DISCONNECT;
    jetty_cb->flag.value = jetty_attr->ub.flag.value;
    jetty_cb->jfs_flag.value = jetty_attr->ub.jfs_flag.value;
    jetty_cb->token_id_addr = jetty_attr->ub.token_id_addr;
    jetty_cb->token_value = jetty_attr->ub.token_value;
    jetty_cb->priority = jetty_attr->ub.priority;
    jetty_cb->rnr_retry = jetty_attr->ub.rnr_retry;
    jetty_cb->err_timeout = jetty_attr->ub.err_timeout;

    if (jetty_cb->jetty_mode == JETTY_MODE_CCU_TA_CACHE) {
        jetty_cb->ta_cache_mode.lock_flag = jetty_attr->ub.ta_cache_mode.lock_flag;
        jetty_cb->ta_cache_mode.sqe_buf_idx = jetty_attr->ub.ta_cache_mode.sqe_buf_idx;
    } else {
        jetty_cb->ext_mode.sq = jetty_attr->ub.ext_mode.sq;
        jetty_cb->ext_mode.pi_type = jetty_attr->ub.ext_mode.pi_type;
        jetty_cb->ext_mode.cstm_flag = jetty_attr->ub.ext_mode.cstm_flag;
        jetty_cb->ext_mode.sqebb_num = jetty_attr->ub.ext_mode.sqebb_num;
    }
    return 0;
}

#ifdef CUSTOM_INTERFACE
STATIC int rs_ub_jetty_cb_buff_alloc(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_jetty_cb *jetty_cb)
{
    unsigned int logic_devid = 0;
    unsigned long flag = 0;
    int ret = 0;

    ret = rsGetLocalDevIDByHostDevID(dev_cb->phyId, &logic_devid);
    if (ret != 0) {
        hccp_err("rsGetLocalDevIDByHostDevID failed, phy_id(%u), ret(%d)", dev_cb->phyId, ret);
        return ret;
    }

    flag = ((unsigned long)logic_devid << BUFF_FLAGS_DEVID_OFFSET) | BUFF_SP_SVM;
    ret = DlHalBuffAllocAlignEx(sizeof(struct ctx_qp_share_info), CI_ADDR_BUFFER_ALIGN_4K_PAGE_SIZE, flag,
        (int)dev_cb->rscb->grpId, (void **)&jetty_cb->qp_share_info_addr);
    if (ret != 0) {
        hccp_err("dl_hal_buff_alloc_align_ex failed, length:0x%llx, dev_id:0x%x, flag:0x%lx, grp_id:%u, ret:%d",
            sizeof(struct ctx_qp_share_info), logic_devid, flag, dev_cb->rscb->grpId, ret);
    }

    return ret;
}
#endif

STATIC int rs_ub_ctx_init_jetty_cb(struct rs_ub_dev_cb *dev_cb, struct ctx_qp_attr *attr,
    struct rs_ctx_jetty_cb **jetty_cb)
{
    struct rs_ctx_jetty_cb *tmp_jetty_cb = NULL;
    int ret;

    tmp_jetty_cb = calloc(1, sizeof(struct rs_ctx_jetty_cb));
    CHK_PRT_RETURN(tmp_jetty_cb == NULL, hccp_err("calloc tmp_jetty_cb failed, errno:%d", errno), -ENOMEM);

    ret = pthread_mutex_init(&tmp_jetty_cb->mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init failed, ret:%d", ret);
        goto pthread_mutex_init_err;
    }

    ret = pthread_mutex_init(&tmp_jetty_cb->cr_err_info.mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init failed, ret:%d", ret);
        goto cr_err_mutex_init_err;
    }

    ret = rs_ub_jetty_cb_init(dev_cb, attr, tmp_jetty_cb);
    if (ret != 0) {
        hccp_err("jetty_cb init failed ret:%d", ret);
        goto jetty_cb_init_err;
    }

#ifdef CUSTOM_INTERFACE
    ret = rs_ub_jetty_cb_buff_alloc(dev_cb, tmp_jetty_cb);
    if (ret != 0) {
        hccp_err("jetty_cb buff alloc failed ret:%d", ret);
        goto jetty_cb_init_err;
    }
#endif

    *jetty_cb = tmp_jetty_cb;
    return 0;

jetty_cb_init_err:
    pthread_mutex_destroy(&tmp_jetty_cb->cr_err_info.mutex);
cr_err_mutex_init_err:
    pthread_mutex_destroy(&tmp_jetty_cb->mutex);
pthread_mutex_init_err:
    free(tmp_jetty_cb);
    tmp_jetty_cb = NULL;

    return ret;
}

STATIC void rs_ub_ctx_fill_jfs_cfg(struct rs_ctx_jetty_cb *jetty_cb, struct rs_ctx_jfc_cb *send_jfc_cb,
    urma_jfs_cfg_t *jfs_cfg)
{
    jfs_cfg->depth = (uint32_t)jetty_cb->tx_depth;
    jfs_cfg->flag.value = jetty_cb->jfs_flag.value;
    jfs_cfg->trans_mode = jetty_cb->transport_mode;
    jfs_cfg->priority = jetty_cb->priority;
    jfs_cfg->max_sge = (uint8_t)jetty_cb->dev_cb->dev_attr.sq_max_sge;
    jfs_cfg->max_rsge = (uint8_t)jetty_cb->dev_cb->dev_attr.ub.max_jfs_rsge;
    jfs_cfg->max_inline_data = jetty_cb->dev_cb->dev_attr.ub.max_jfs_inline_len;
    jfs_cfg->rnr_retry = jetty_cb->rnr_retry;
    jfs_cfg->err_timeout = jetty_cb->err_timeout;
    jfs_cfg->jfc = (urma_jfc_t *)(uintptr_t)send_jfc_cb->jfc_addr;
    jfs_cfg->user_ctx = (uint64_t)NULL;
}

STATIC void rs_ub_ctx_fill_jfr_cfg(struct rs_ctx_jetty_cb *jetty_cb, struct rs_ctx_jfc_cb *recv_jfc_cb,
    urma_jfr_cfg_t *jfr_cfg)
{
    jfr_cfg->id = 0;  // the system will randomly assign a non-0 value
    jfr_cfg->depth = (uint32_t)jetty_cb->rx_depth;
    jfr_cfg->flag.bs.tag_matching = URMA_NO_TAG_MATCHING;
    jfr_cfg->trans_mode = jetty_cb->transport_mode;
    jfr_cfg->max_sge = (uint8_t)jetty_cb->dev_cb->dev_attr.rq_max_sge;
    jfr_cfg->min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER;
    jfr_cfg->jfc = (urma_jfc_t *)(uintptr_t)recv_jfc_cb->jfc_addr;
    jfr_cfg->token_value.token = jetty_cb->token_value;
    jfr_cfg->user_ctx = (uint64_t)NULL;
}

int rs_ub_ctx_reg_jetty_db(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jetty_info *jetty_info)
{
    struct mem_reg_attr_t mem_attr = { 0 };
    struct mem_reg_info_t mem_info = { 0 };
    int ret;

    // register dwqe_addr with page size 4096, return db_addr to use, specify ub to alloc token id
    hccp_dbg("jetty_info->dwqe_addr:%pK, jetty_info->db_addr:%pK", jetty_info->dwqe_addr, jetty_info->db_addr);
    mem_attr.mem.addr = (uint64_t)(uintptr_t)jetty_info->dwqe_addr;
    mem_attr.mem.size = 4096U;
    mem_attr.ub.flags.value = 0;
    mem_attr.ub.flags.bs.token_policy = URMA_TOKEN_PLAIN_TEXT;
    mem_attr.ub.flags.bs.cacheable = URMA_NON_CACHEABLE;
    mem_attr.ub.flags.bs.access = MEM_SEG_ACCESS_READ | MEM_SEG_ACCESS_WRITE;
    mem_attr.ub.flags.bs.non_pin = 1;
    // use user specified token id to register
    if (jetty_cb->token_id_addr != 0) {
        mem_attr.ub.flags.bs.token_id_valid = URMA_TOKEN_ID_VALID;
        mem_attr.ub.token_id_addr = jetty_cb->token_id_addr;
    }
    mem_attr.ub.token_value = jetty_cb->token_value;
    ret = rs_ub_ctx_lmem_reg(jetty_cb->dev_cb, &mem_attr, &mem_info);
    if (ret != 0) {
        hccp_err("rs_ub_ctx_lmem_reg failed, ret=%d", ret);
        return ret;
    }

    jetty_cb->db_token_id = mem_info.ub.token_id;
    jetty_cb->db_seg_handle = mem_info.ub.target_seg_handle;
    return 0;
}

STATIC void rs_ub_ctx_ext_jetty_create_ta_cache(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg)
{
    struct udma_u_lock_jetty_cfg jetty_ex_cfg_ta = {0};
    struct udma_u_jetty_info jetty_info = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret;

    jetty_ex_cfg_ta.base_cfg = *jetty_cfg;
    jetty_ex_cfg_ta.jetty_type = jetty_cb->ta_cache_mode.lock_flag;
    jetty_ex_cfg_ta.buf_idx = jetty_cb->ta_cache_mode.sqe_buf_idx;
    in.len = (uint32_t)sizeof(struct udma_u_lock_jetty_cfg);
    in.addr = (uint64_t)(uintptr_t)&jetty_ex_cfg_ta;
    in.opcode = UDMA_U_USER_CTL_CREATE_LOCK_BUFFER_JETTY_EX;

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
    ret = rs_ub_ctx_reg_jetty_db(jetty_cb, &jetty_info);
    if (ret != 0) {
        rs_ub_ctx_ext_jetty_delete(jetty_cb);
        jetty_cb->jetty = NULL;
        hccp_err("rs_ub_ctx_reg_jetty_db failed, ret:%d", ret);
    }
}

STATIC int rs_ub_ctx_drv_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, struct rs_ctx_jfc_cb *send_jfc_cb,
    struct rs_ctx_jfc_cb *recv_jfc_cb)
{
    urma_jetty_cfg_t jetty_init_cfg = {0};
    urma_jfs_cfg_t jfs_cfg = {0};
    urma_jfr_cfg_t jfr_cfg = {0};
    int ret = 0;

    jetty_init_cfg.id = jetty_cb->jetty_id;
    jetty_init_cfg.flag = jetty_cb->flag;
    rs_ub_ctx_fill_jfs_cfg(jetty_cb, send_jfc_cb, &jfs_cfg);
    jetty_init_cfg.jfs_cfg = jfs_cfg;

    rs_ub_ctx_fill_jfr_cfg(jetty_cb, recv_jfc_cb, &jfr_cfg);
    jetty_cb->jfr = rs_urma_create_jfr(jetty_cb->dev_cb->urma_ctx, &jfr_cfg);
    CHK_PRT_RETURN(jetty_cb->jfr == NULL, hccp_err("rs_urma_create_jfr failed, errno=%d", errno), -ENOMEM);

    jetty_init_cfg.shared.jfr = jetty_cb->jfr;
    jetty_init_cfg.shared.jfc = (urma_jfc_t *)(uintptr_t)recv_jfc_cb->jfc_addr;

    if (jetty_cb->jetty_mode == JETTY_MODE_URMA_NORMAL) {
        jetty_cb->jetty = rs_urma_create_jetty(jetty_cb->dev_cb->urma_ctx, &jetty_init_cfg);
        if (jetty_cb->jetty == NULL) {
            hccp_err("rs_urma_create_jetty failed, errno=%d", errno);
        }
    } else if (jetty_cb->jetty_mode == JETTY_MODE_CCU_TA_CACHE) {
        rs_ub_ctx_ext_jetty_create_ta_cache(jetty_cb, &jetty_init_cfg);
    } else {
        rs_ub_ctx_ext_jetty_create(jetty_cb, &jetty_init_cfg);
    }

    // create jetty failed, should delete jfr
    if (jetty_cb->jetty == NULL) {
        ret = rs_urma_delete_jfr(jetty_cb->jfr);
        CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_delete_jfr failed, ret:%d", ret), -EOPENSRC);
        return -ENOMEM;
    }

    jetty_cb->state = RS_JETTY_STATE_CREATED;

    hccp_run_info("chip_id:%u, devIndex:0x%x, jetty_id:%u jetty create succ", jetty_cb->dev_cb->rscb->chipId,
        jetty_cb->dev_cb->index, jetty_cb->jetty->jetty_id.id);

    return 0;
}

STATIC int rs_ub_fill_jetty_info(struct rs_ctx_jetty_cb *jetty_cb, struct qp_create_info *jetty_info)
{
    struct rs_jetty_key_info jetty_key_info = {0};
    int ret;

    jetty_key_info.jetty_id = jetty_cb->jetty->jetty_id;
    jetty_key_info.trans_mode = jetty_cb->transport_mode;
    ret = memcpy_s(jetty_info->key.value, DEV_QP_KEY_SIZE, &jetty_key_info, sizeof(struct rs_jetty_key_info));
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy jetty_key_info failed, ret:%d", ret), -ESAFEFUNC);

    jetty_info->key.size = (uint8_t)sizeof(struct rs_jetty_key_info);
    jetty_info->ub.uasid = jetty_cb->jetty->jetty_id.uasid;
    jetty_info->ub.id = jetty_cb->jetty->jetty_id.id;
    jetty_info->ub.db_addr = jetty_cb->db_addr;
    jetty_info->ub.sq_buff_va = jetty_cb->sq_buff_va;
    jetty_info->ub.wqebb_size = WQE_BB_SIZE;
    jetty_info->ub.db_token_id = jetty_cb->db_token_id;
    jetty_info->va = (uint64_t)(uintptr_t)jetty_cb->jetty;
    jetty_info->ub.share_info_addr = (uint64_t)(uintptr_t)jetty_cb->qp_share_info_addr;
    jetty_info->ub.share_info_len = sizeof(struct ctx_qp_share_info);

    return 0;
}

STATIC int rs_ub_query_jfc_cb(struct rs_ub_dev_cb *dev_cb, unsigned long long scq_index, unsigned long long rcq_index,
                              struct rs_ctx_jfc_cb **send_jfc_cb, struct rs_ctx_jfc_cb **recv_jfc_cb)
{
    int ret;

    ret = rs_ub_get_jfc_cb(dev_cb, scq_index, send_jfc_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get send_jfc_cb failed, ret:%d scq_index:0x%llx", ret, scq_index), ret);

    ret = rs_ub_get_jfc_cb(dev_cb, rcq_index, recv_jfc_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get recv_jfc_cb failed, ret:%d rcq_index:0x%llx", ret, rcq_index), ret);

    return 0;
}

int rs_ub_ctx_jetty_create(struct rs_ub_dev_cb *dev_cb, struct ctx_qp_attr *attr, struct qp_create_info *info)
{
    struct rs_ctx_jfc_cb *send_jfc_cb = NULL;
    struct rs_ctx_jfc_cb *recv_jfc_cb = NULL;
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    int ret;

    ret = rs_ub_ctx_init_jetty_cb(dev_cb, attr, &jetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("alloc mem for jetty_cb failed, ret:%d", ret), ret);

    ret = rs_ub_query_jfc_cb(dev_cb, attr->scq_index, attr->rcq_index, &send_jfc_cb, &recv_jfc_cb);
    if (ret != 0) {
        hccp_err("query jfc_cb scq_index:0x%llx rcq_index:0x%llx failed, ret:%d", attr->scq_index, attr->rcq_index,
            ret);
        goto free_jetty_cb;
    }

    ret = rs_ub_ctx_drv_jetty_create(jetty_cb, send_jfc_cb, recv_jfc_cb);
    if (ret != 0) {
        hccp_err("rs_ub_ctx_drv_jetty_create failed, ret:%d", ret);
        goto free_jetty_cb;
    }

    ret = rs_ub_fill_jetty_info(jetty_cb, info);
    if (ret != 0) {
        hccp_err("rs_ub_fill_jetty_info failed, ret:%d", ret);
        goto fill_jetty_info_err;
    }

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListAddTail(&jetty_cb->list, &dev_cb->jetty_list);
    dev_cb->jetty_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    hccp_run_info("[init][rs_ctx]devIndex:0x%x qp_id:%u create success, jetty_cnt:%u",
        dev_cb->index, info->ub.id, dev_cb->jetty_cnt);

    return 0;

fill_jetty_info_err:
    rs_ub_ctx_drv_jetty_delete(jetty_cb);
free_jetty_cb:
    rs_ub_ctx_free_jetty_cb(jetty_cb);

    return ret;
}

STATIC int rs_ub_get_jetty_cb(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id, struct rs_ctx_jetty_cb **jetty_cb)
{
    struct rs_ctx_jetty_cb **temp_jetty_cb = jetty_cb;
    struct rs_ctx_jetty_cb *jetty_cb_curr = NULL;
    struct rs_ctx_jetty_cb *jetty_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(jetty_cb_curr, jetty_cb_next, &dev_cb->jetty_list, list, struct rs_ctx_jetty_cb);
    for (; (&jetty_cb_curr->list) != &dev_cb->jetty_list;
         jetty_cb_curr = jetty_cb_next,
         jetty_cb_next = list_entry(jetty_cb_next->list.next, struct rs_ctx_jetty_cb, list)) {
        if (jetty_cb_curr->jetty != NULL && jetty_cb_curr->jetty->jetty_id.id == jetty_id) {
            *temp_jetty_cb = jetty_cb_curr;
            return 0;
        }
    }

    *temp_jetty_cb = NULL;
    return -ENODEV;
}

int rs_ub_ctx_jetty_destroy(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    int ret;

    ret = rs_ub_get_jetty_cb(dev_cb, jetty_id, &jetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("get jetty_cb unsuccessful, ret:%d, jetty_id %u", ret, jetty_id), ret);
    if (jetty_cb->state != RS_JETTY_STATE_CREATED) {
        hccp_err("jetty_cb->state:%u not support to destroy, jetty_id:%u", jetty_cb->state, jetty_id);
        return -EINVAL;
    }

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&jetty_cb->list);
    dev_cb->jetty_cnt--;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    rs_ub_ctx_drv_jetty_delete(jetty_cb);

    rs_ub_ctx_free_jetty_cb(jetty_cb);

    hccp_run_info("[deinit][rs_ctx]devIndex:0x%x qp_id:%u destroy success, jetty_cnt:%u",
        dev_cb->index, jetty_id, dev_cb->jetty_cnt);

    return 0;
}

int rs_ub_ctx_jetty_free(struct rs_cb *rscb, unsigned int ue_info, unsigned int jetty_id)
{
    struct rs_ub_dev_cb *dev_cb_curr = NULL;
    struct rs_ub_dev_cb *dev_cb_next = NULL;
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    int ret = 0;

    RS_LIST_GET_HEAD_ENTRY(dev_cb_curr, dev_cb_next, &rscb->rdevList, list, struct rs_ub_dev_cb);
    for (; (&dev_cb_curr->list) != &rscb->rdevList;
         dev_cb_curr = dev_cb_next, dev_cb_next = list_entry(dev_cb_next->list.next, struct rs_ub_dev_cb, list)) {
        if ((dev_cb_curr->index & DEV_INDEX_UE_INFO_MASK) != ue_info) {
            continue;
        }

        RS_PTHREAD_MUTEX_LOCK(&dev_cb_curr->mutex);
        ret = rs_ub_get_jetty_cb(dev_cb_curr, jetty_id, &jetty_cb);
        if (ret == 0) {
            goto jetty_found;
        }
        RS_PTHREAD_MUTEX_ULOCK(&dev_cb_curr->mutex);
    }

    hccp_run_warn("get jetty_cb unsuccessful, ue_info:0x%x, jetty_id:%u", ue_info, jetty_id);
    return -ENODEV;

jetty_found:
    RsListDel(&jetty_cb->list);
    dev_cb_curr->jetty_cnt--;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb_curr->mutex);

    if (jetty_cb->state == RS_JETTY_STATE_BIND) {
        hccp_info("jetty_id:%u will be unbind, devIndex:0x%x", jetty_id, dev_cb_curr->index);
        (void)rs_urma_unbind_jetty(jetty_cb->jetty);
    }

    if (jetty_cb->state == RS_JETTY_STATE_BIND || jetty_cb->state == RS_JETTY_STATE_CREATED) {
        hccp_info("jetty_id:%u will be destroyed, devIndex:0x%x", jetty_id, dev_cb_curr->index);
        rs_ub_ctx_drv_jetty_delete(jetty_cb);
    }

    rs_ub_ctx_free_jetty_cb(jetty_cb);
    return 0;
}

STATIC int rs_ub_ctx_init_rjetty_cb(struct rs_ub_dev_cb *dev_cb, struct rs_jetty_import_attr *import_attr,
    struct rs_ctx_rem_jetty_cb **rjetty_cb)
{
    struct rs_ctx_rem_jetty_cb *tmp_rjetty_cb = NULL;

    tmp_rjetty_cb = calloc(1, sizeof(struct rs_ctx_rem_jetty_cb));
    CHK_PRT_RETURN(tmp_rjetty_cb == NULL, hccp_err("calloc tmp_rjetty_cb failed, errno:%d", errno), -ENOMEM);

    tmp_rjetty_cb->dev_cb = dev_cb;
    tmp_rjetty_cb->jetty_key = import_attr->key;
    tmp_rjetty_cb->mode = import_attr->attr.mode;
    tmp_rjetty_cb->token_value = import_attr->attr.token_value;
    tmp_rjetty_cb->policy = import_attr->attr.policy;
    tmp_rjetty_cb->type = import_attr->attr.type;
    tmp_rjetty_cb->flag = import_attr->attr.flag;
    tmp_rjetty_cb->tp_type = import_attr->attr.tp_type;
    tmp_rjetty_cb->exp_import_cfg = import_attr->attr.exp_import_cfg;

    *rjetty_cb = tmp_rjetty_cb;
    return 0;
}

STATIC void rs_ub_ctx_exp_jetty_import(struct rs_ctx_rem_jetty_cb *rjetty_cb, urma_rjetty_t *rjetty,
    urma_token_t *token_value)
{
    urma_import_jetty_ex_cfg_t exp_cfg = {0};

    exp_cfg.tp_handle = rjetty_cb->exp_import_cfg.tp_handle;
    exp_cfg.peer_tp_handle = rjetty_cb->exp_import_cfg.peer_tp_handle;
    exp_cfg.tag = rjetty_cb->exp_import_cfg.tag;
    exp_cfg.tp_attr.tx_psn = rjetty_cb->exp_import_cfg.tx_psn;
    exp_cfg.tp_attr.rx_psn = rjetty_cb->exp_import_cfg.rx_psn;

    rjetty_cb->tjetty = rs_urma_import_jetty_ex(rjetty_cb->dev_cb->urma_ctx, rjetty, token_value, &exp_cfg);
}

STATIC int rs_ub_ctx_drv_jetty_import(struct rs_ctx_rem_jetty_cb *rjetty_cb)
{
    struct rs_jetty_key_info *jetty_key_info;
    urma_token_t token_value = {0};
    urma_rjetty_t rjetty = {0};

    token_value.token = rjetty_cb->token_value;
    jetty_key_info = (struct rs_jetty_key_info *)rjetty_cb->jetty_key.value;
    rjetty.jetty_id = jetty_key_info->jetty_id;
    rjetty.trans_mode = jetty_key_info->trans_mode;
    rjetty.policy = (urma_jetty_grp_policy_t)rjetty_cb->policy;
    rjetty.type = (urma_target_type_t)rjetty_cb->type;
    rjetty.flag.value = rjetty_cb->flag.value;
    rjetty.tp_type = rjetty_cb->tp_type;

    if (rjetty_cb->mode == JETTY_IMPORT_MODE_NORMAL) {
        rjetty_cb->tjetty = rs_urma_import_jetty(rjetty_cb->dev_cb->urma_ctx, &rjetty, &token_value);
    }  else { // rjetty_cb->mode == JETTY_IMPORT_MODE_EXP
        rs_ub_ctx_exp_jetty_import(rjetty_cb, &rjetty, &token_value);
    }
    CHK_PRT_RETURN(rjetty_cb->tjetty == NULL, hccp_err("import_jetty failed, mode:%d errno:%d", rjetty_cb->mode, errno),
        -EOPENSRC);
    return 0;
}

int rs_ub_ctx_jetty_import(struct rs_ub_dev_cb *dev_cb, struct rs_jetty_import_attr *import_attr,
    struct rs_jetty_import_info *import_info)
{
    struct rs_ctx_rem_jetty_cb *rjetty_cb = NULL;
    int ret;

    ret = rs_ub_ctx_init_rjetty_cb(dev_cb, import_attr, &rjetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("alloc mem for rjetty_cb failed, ret:%d", ret), ret);

    ret = rs_ub_ctx_drv_jetty_import(rjetty_cb);
    if (ret != 0) {
        hccp_err("rs_ub_ctx_drv_jetty_import failed, ret:%d", ret);
        goto free_rjetty_cb;
    }

    import_info->rem_jetty_id = rjetty_cb->tjetty->id.id;
    import_info->info.tjetty_handle = (uint64_t)(uintptr_t)rjetty_cb->tjetty;
    import_info->info.tpn = rjetty_cb->tjetty->tp.tpn;

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListAddTail(&rjetty_cb->list, &dev_cb->rjetty_list);
    dev_cb->rjetty_cnt++;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    rjetty_cb->state = RS_JETTY_STATE_IMPORTED;

    hccp_run_info("[init][rs_ctx]devIndex:0x%x rem_qp_id:%u mode:%d import success, rjetty_cnt:%u",
        dev_cb->index, import_info->rem_jetty_id, import_attr->attr.mode, dev_cb->rjetty_cnt);

    return 0;

free_rjetty_cb:
    free(rjetty_cb);
    rjetty_cb = NULL;

    return ret;
}

STATIC int rs_ub_get_rem_jetty_cb(struct rs_ub_dev_cb *dev_cb, unsigned int rem_jetty_id,
                                  struct rs_ctx_rem_jetty_cb **rjetty_cb)
{
    struct rs_ctx_rem_jetty_cb **temp_jetty_cb = rjetty_cb;
    struct rs_ctx_rem_jetty_cb *jetty_cb_curr = NULL;
    struct rs_ctx_rem_jetty_cb *jetty_cb_next = NULL;

    RS_LIST_GET_HEAD_ENTRY(jetty_cb_curr, jetty_cb_next, &dev_cb->rjetty_list, list, struct rs_ctx_rem_jetty_cb);
    for (; (&jetty_cb_curr->list) != &dev_cb->rjetty_list;
         jetty_cb_curr = jetty_cb_next,
         jetty_cb_next = list_entry(jetty_cb_next->list.next, struct rs_ctx_rem_jetty_cb, list)) {
        if (jetty_cb_curr->tjetty == NULL) {
            hccp_warn("rem_jetty_id:%u jetty_cb_curr->tjetty is NULL", rem_jetty_id);
            continue;
        }
        if (jetty_cb_curr->tjetty->id.id == rem_jetty_id) {
            *temp_jetty_cb = jetty_cb_curr;
            return 0;
        }
    }

    *temp_jetty_cb = NULL;
    hccp_err("rjetty_cb for rem_jetty %u do not available!", rem_jetty_id);

    return -ENODEV;
}

int rs_ub_ctx_jetty_unimport(struct rs_ub_dev_cb *dev_cb, unsigned int rem_jetty_id)
{
    struct rs_ctx_rem_jetty_cb *rjetty_cb = NULL;
    unsigned int rjetty_cnt;
    int ret;

    ret = rs_ub_get_rem_jetty_cb(dev_cb, rem_jetty_id, &rjetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rjetty_cb failed, ret:%d rem_jetty_id:%u", ret, rem_jetty_id), ret);
    CHK_PRT_RETURN(rjetty_cb->state != RS_JETTY_STATE_IMPORTED, hccp_err("rjetty_cb->state:%u not support to "
        "unimport, jetty_id:%u", rjetty_cb->state, rem_jetty_id), -EINVAL);

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RsListDel(&rjetty_cb->list);
    dev_cb->rjetty_cnt--;
    rjetty_cnt = dev_cb->rjetty_cnt;
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

    ret = rs_urma_unimport_jetty(rjetty_cb->tjetty);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_unimport_jetty failed, ret:%d, rem_jetty_id %u", ret, rem_jetty_id),
        -EOPENSRC);

    hccp_run_info("[deinit][rs_qp]unimport jetty_id:%u success, rjetty_cnt:%u, devIndex:0x%x",
        rem_jetty_id, rjetty_cnt, dev_cb->index);
    free(rjetty_cb);
    rjetty_cb = NULL;
    return 0;
}

int rs_ub_ctx_jetty_bind(struct rs_ub_dev_cb *dev_cb, struct rs_ctx_qp_info *jetty_info,
    struct rs_ctx_qp_info *rjetty_info)
{
    struct rs_ctx_rem_jetty_cb *rjetty_cb = NULL;
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    int ret;

    ret = rs_ub_get_jetty_cb(dev_cb, jetty_info->id, &jetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get jetty_cb failed, ret:%d, jetty_id %u", ret, jetty_info->id), ret);

    ret = rs_ub_get_rem_jetty_cb(dev_cb, rjetty_info->id, &rjetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rjetty_cb failed, ret:%d, rem_jetty_id %u", ret, rjetty_info->id), ret);

    if (jetty_cb->state != RS_JETTY_STATE_CREATED || rjetty_cb->state != RS_JETTY_STATE_IMPORTED) {
        hccp_err("local jetty id:%u state:%u or remote jetty id:%u state:%u not support to bind",
            jetty_info->id, jetty_cb->state, rjetty_info->id, rjetty_cb->state);
        return -EINVAL;
    }

    ret = rs_urma_bind_jetty(jetty_cb->jetty, rjetty_cb->tjetty);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_bind_jetty failed, ret:%d errno:%d", ret, errno), -EOPENSRC);

    jetty_cb->state = RS_JETTY_STATE_BIND;

    hccp_run_info("rs ctx local jetty %u bind rem_jetty %u success, devIndex:0x%x",
        jetty_info->id, rjetty_info->id, dev_cb->index);

    return 0;
}

int rs_ub_ctx_jetty_unbind(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    int ret;

    ret = rs_ub_get_jetty_cb(dev_cb, jetty_id, &jetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("get jetty_cb unsuccessful, ret:%d, jetty_id %u", ret, jetty_id), ret);
    if (jetty_cb->state != RS_JETTY_STATE_BIND) {
        hccp_err("jetty_cb->state:%u not support to unbind, jetty_id:%u", jetty_cb->state, jetty_id);
        return -EINVAL;
    }

    ret = rs_urma_unbind_jetty(jetty_cb->jetty);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_unbind_jetty failed, ret:%d jetty_id %u", ret, jetty_id), -EOPENSRC);

    jetty_cb->state = RS_JETTY_STATE_CREATED;

    hccp_run_info("rs ctx local jetty %u unbind rem jetty success, devIndex:0x%x", jetty_id, dev_cb->index);

    return 0;
}

STATIC int rs_ub_ctx_fill_lsge(struct rs_ub_dev_cb *dev_cb, urma_sge_t *lsge, struct batch_send_wr_data *wr_data,
    unsigned int *total_len, bool is_inline)
{
    struct rs_seg_cb *seg_cb = NULL;
    unsigned int total_len_tmp = 0;
    unsigned int i;
    int ret;

    if (is_inline == false) {
        for (i = 0; i < wr_data->num_sge; i++) {
            lsge[i].addr = wr_data->sges[i].addr;
            lsge[i].len = wr_data->sges[i].len;
            total_len_tmp += lsge[i].len;
            ret = rs_ub_query_seg_cb(dev_cb, wr_data->sges[i].dev_lmem_handle, &seg_cb, &dev_cb->lseg_list);
            if (ret != 0) {
                hccp_err("[send][rs_ub_ctx]can not find lmem seg cb for addr:0x%llx", lsge[i].addr);
                return ret;
            }
            lsge[i].tseg = seg_cb->segment;
        }
    } else {
        lsge[0].addr = (uint64_t)(uintptr_t)wr_data->inline_data;
        lsge[0].len = wr_data->inline_size;
        lsge[0].tseg = NULL;
    }

    *total_len = total_len_tmp;
    return 0;
}

STATIC int rs_ub_ctx_fill_rsge(struct rs_ub_dev_cb *dev_cb, urma_sge_t *rsge, struct batch_send_wr_data *wr_data,
    unsigned int total_len, urma_opcode_t opcode)
{
    struct rs_seg_cb *seg_cb = NULL;
    int ret;

    rsge[0].addr = wr_data->remote_addr;
    rsge[0].len = total_len;
    ret = rs_ub_query_seg_cb(dev_cb, wr_data->dev_rmem_handle, &seg_cb, &dev_cb->rseg_list);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]can not find rmem seg cb for addr:0x%llx", rsge[0].addr);
        return ret;
    }
    rsge[0].tseg = seg_cb->segment;

    if (opcode == URMA_OPC_WRITE_NOTIFY) {
        rsge[1].addr = wr_data->ub.notify_info.notify_addr;
        rsge[1].len = 8; /* notify data is fixed 8 bytes */
        ret = rs_ub_query_seg_cb(dev_cb, wr_data->ub.notify_info.notify_handle, &seg_cb, &dev_cb->rseg_list);
        if (ret != 0) {
            hccp_err("[send][rs_ub_ctx]can not find rmem seg cb for addr:0x%llx", rsge[1].addr);
            return ret;
        }
        rsge[1].tseg = seg_cb->segment;
    }

    return 0;
}

STATIC int rs_ub_ctx_init_rw_wr(struct rs_ub_dev_cb *dev_cb, urma_jfs_wr_t *ub_wr, struct batch_send_wr_data *wr_data,
    urma_sge_t *lsge, urma_sge_t *rsge)
{
    unsigned int lsge_num, rsge_num;
    unsigned int total_len = 0;
    int ret;

    lsge_num = (ub_wr->flag.bs.inline_flag == 0) ? wr_data->num_sge : 1;
    ret = rs_ub_ctx_fill_lsge(dev_cb, lsge, wr_data, &total_len, ub_wr->flag.bs.inline_flag);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]fill lsge failed, ret:%d", ret);
        return ret;
    }

    /* write with norify have 2 dst sge, sge[0] is data sge, sge[1] is notify sge */
    rsge_num = (ub_wr->opcode == URMA_OPC_WRITE_NOTIFY) ? 2 : 1;
    ret = rs_ub_ctx_fill_rsge(dev_cb, rsge, wr_data, total_len, ub_wr->opcode);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]fill rsge failed, ret:%d", ret);
        return ret;
    }

    if (ub_wr->opcode == URMA_OPC_READ) {
        ub_wr->rw.src.sge = rsge;
        ub_wr->rw.src.num_sge = rsge_num;
        ub_wr->rw.dst.sge = lsge;
        ub_wr->rw.dst.num_sge = lsge_num;
    } else {
        ub_wr->rw.src.sge = lsge;
        ub_wr->rw.src.num_sge = lsge_num;
        ub_wr->rw.dst.sge = rsge;
        ub_wr->rw.dst.num_sge = rsge_num;
    }

    // assign notify data & imm data
    if (ub_wr->opcode == URMA_OPC_WRITE_NOTIFY) {
        ub_wr->rw.notify_data = wr_data->ub.notify_info.notify_data;
    } else {
        ub_wr->rw.notify_data = (uint64_t)wr_data->imm_data;
    }

    return 0;
}

STATIC int rs_ub_ctx_init_jfs_wr(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jfs_wr_ex *ub_wr,
    struct batch_send_wr_data *wr_data, urma_sge_t *lsge, urma_sge_t *rsge)
{
    urma_opcode_t opcode = (urma_opcode_t)wr_data->ub.opcode;
    struct rs_ctx_rem_jetty_cb *rjetty_cb = NULL;
    int ret;

    if (wr_data->num_sge > MAX_SGE_NUM) {
        hccp_err("[send][rs_ub_ctx]num_sge is invalid, num_sge[%d]", wr_data->num_sge);
        return -EINVAL;
    }

    ub_wr->wr.opcode = opcode;
    ub_wr->wr.flag.value = wr_data->ub.flags.value;
    ub_wr->wr.user_ctx = wr_data->ub.user_ctx;

    if (opcode == URMA_OPC_NOP) {
        return 0;
    }

    // only write & write with notify & read op support inline reduce
    if (opcode == URMA_OPC_READ || opcode == URMA_OPC_WRITE || opcode == URMA_OPC_WRITE_NOTIFY) {
        ub_wr->reduce_en = wr_data->ub.reduce_info.reduce_en;
        ub_wr->reduce_opcode = wr_data->ub.reduce_info.reduce_opcode;
        ub_wr->reduce_data_type = wr_data->ub.reduce_info.reduce_data_type;
        hccp_dbg("[send][rs_ub_ctx]opcode[%d] reduce_en[%d] reduce_opcode[%d] reduce_data_type[%d]",
            opcode, ub_wr->reduce_en, ub_wr->reduce_opcode, ub_wr->reduce_data_type);
    }

    if (opcode == URMA_OPC_READ || opcode == URMA_OPC_WRITE || opcode == URMA_OPC_WRITE_NOTIFY ||
        opcode == URMA_OPC_WRITE_IMM) {
        ret = rs_ub_get_rem_jetty_cb(jetty_cb->dev_cb, (unsigned int)wr_data->ub.rem_jetty, &rjetty_cb);
        if (ret != 0) {
            hccp_err("[send][rs_ub_ctx]get rjetty_cb failed, ret:%d rem_jetty_id:%llu", ret, wr_data->ub.rem_jetty);
            return ret;
        }
        ub_wr->wr.tjetty = rjetty_cb->tjetty;
        return rs_ub_ctx_init_rw_wr(jetty_cb->dev_cb, &ub_wr->wr, wr_data, lsge, rsge);
    }

    hccp_err("[send][rs_ub_ctx]invalid opcode[%d]", opcode);
    return -EINVAL;
}

STATIC int rs_ub_ctx_batch_send_wr_ext(struct rs_ctx_jetty_cb *jetty_cb, struct batch_send_wr_data *wr_data,
    struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num)
{
#define UB_DWQE_BB_NUM  2U
#define UB_DWQE_BB_SIZE 64U
    unsigned int send_num = wrlist_num->sendNum;
    struct udma_u_jfs_wr_ex *bad_wr = NULL;
    struct udma_u_post_info wr_out = {0};
    urma_sge_t rsge[MAX_RSGE_NUM] = {0};
    urma_sge_t lsge[MAX_SGE_NUM] = {0};
    struct udma_u_jfs_wr_ex ub_wr = {0};
    struct udma_u_wr_ex wr_in = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    unsigned int i;
    int ret;

    for (i = 0; i < send_num; i++) {
        ret = rs_ub_ctx_init_jfs_wr(jetty_cb, &ub_wr, &wr_data[i], &lsge[0], &rsge[0]);
        if (ret != 0) {
            hccp_err("[send][rs_ub_ctx]init jfs wr failed, ret:%d, curr_num[%u], send_num[%u]", ret, i, send_num);
            break;
        }

        wr_in.is_jetty = true;
        wr_in.jetty = jetty_cb->jetty;
        wr_in.wr = &ub_wr;
        wr_in.bad_wr = &bad_wr;

        in.addr = (uint64_t)(uintptr_t)&wr_in;
        in.len = (uint32_t)sizeof(struct udma_u_wr_ex);
        in.opcode = UDMA_U_USER_CTL_POST_WR;

        out.addr = (uint64_t)(uintptr_t)&wr_out;
        out.len = sizeof(struct udma_u_post_info);

        ret = rs_urma_user_ctl(jetty_cb->dev_cb->urma_ctx, &in, &out);
        if (ret != 0) {
            hccp_err("rs_urma_user_ctl batch send wr failed, ret:%d, wr[%u], send_num[%u] errno:%d",
                ret, i, send_num, errno);
            ret = -EOPENSRC;
            break;
        }

        wr_resp[i].doorbell_info.dieId = (uint16_t)jetty_cb->dev_cb->dev_attr.ub.die_id;
        wr_resp[i].doorbell_info.funcId = (uint16_t)jetty_cb->dev_cb->dev_attr.ub.func_id;
        wr_resp[i].doorbell_info.jettyId = (uint16_t)jetty_cb->jetty->jetty_id.id;
        wr_resp[i].doorbell_info.piVal = (uint16_t)wr_out.pi;
        // prepare dwqe doorbell info, only support 2BB, each BB size is 64B
        if (wr_out.pi - jetty_cb->last_pi <= UB_DWQE_BB_NUM) {
            wr_resp[i].doorbell_info.dwqe_size = (uint16_t)(wr_out.pi - jetty_cb->last_pi) * UB_DWQE_BB_SIZE;
            ret = memcpy_s(wr_resp[i].doorbell_info.dwqe, wr_resp[i].doorbell_info.dwqe_size,
                wr_out.ctrl, wr_resp[i].doorbell_info.dwqe_size);
            if (ret != 0) {
                hccp_err("[send][rs_ub_ctx]memcpy_s failed, ret:%d, wr[%u], send_num[%u]", ret, i, send_num);
                ret = -ESAFEFUNC;
                break;
            }
        }
        jetty_cb->last_pi = wr_out.pi;

        // record doorbell info
        hccp_dbg("jetty_id %u post info: dwqe_addr:%pK, db_addr:%pK, ctrl:%pK, pi:%u",
            jetty_cb->jetty->jetty_id.id, wr_out.dwqe_addr, wr_out.db_addr, wr_out.ctrl, wr_out.pi);
    }

    *wrlist_num->completeNum = i;
    return ret;
}

int rs_ub_ctx_batch_send_wr(struct rs_cb *rs_cb, struct wrlist_base_info *base_info,
    struct batch_send_wr_data *wr_data, struct send_wr_resp *wr_resp, struct WrlistSendCompleteNum *wrlist_num)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    if (wrlist_num->sendNum > MAX_CTX_WR_NUM || wrlist_num->sendNum == 0) {
        hccp_err("[send][rs_ub_ctx] send_num[%u] is invalid", wrlist_num->sendNum);
        return -EINVAL;
    }

    ret = rs_ub_get_dev_cb(rs_cb, base_info->dev_index, &dev_cb);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]get dev_cb failed, ret:%d, devIndex:0x%x", ret, base_info->dev_index);
        return ret;
    }

    ret = rs_ub_get_jetty_cb(dev_cb, base_info->qpn, &jetty_cb);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]get jetty_cb failed, ret:%d, jetty_id[%u]", ret, base_info->qpn);
        return ret;
    }

    ret = rs_ub_ctx_batch_send_wr_ext(jetty_cb, wr_data, wr_resp, wrlist_num);
    if (ret != 0) {
        hccp_err("[send][rs_ub_ctx]send wr ext failed, ret:%d, send_num[%u], complete_num[%u]",
            ret, wrlist_num->sendNum, *wrlist_num->completeNum);
        return ret;
    }

    return 0;
}

int rs_ub_ctx_jetty_update_ci(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_id, uint16_t ci)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    struct udma_u_update_ci ci_data = { 0 };
    urma_user_ctl_out_t out = { 0 };
    urma_user_ctl_in_t in = { 0 };
    int ret;

    ret = rs_ub_get_jetty_cb(dev_cb, jetty_id, &jetty_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("get jetty_cb failed, ret:%d, jetty_id:%u", ret, jetty_id), ret);

    ci_data.is_jetty = true;
    ci_data.ci = ci;
    ci_data.jetty = jetty_cb->jetty;
    in.addr = (uint64_t)(uintptr_t)&ci_data;
    in.len = (uint32_t)sizeof(struct udma_u_update_ci);
    in.opcode = UDMA_U_USER_CTL_UPDATE_CI;
    ret = rs_urma_user_ctl(jetty_cb->dev_cb->urma_ctx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl update ci failed, ret:%d errno:%d jetty_id:%u ci:%u",
        ret, errno, jetty_id, ci), -EOPENSRC);

    hccp_info("[update_ci]devIndex:0x%x jetty_id:%u update ci:%u success", dev_cb->index, jetty_id, ci);

    return 0;
}

STATIC int rs_ub_get_jetty_destroy_batch_info(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[],
    struct jetty_destroy_batch_info *batch_info, unsigned int *num)
{
    unsigned int i;
    int ret = 0;

    for (i = 0; i < *num; ++i) {
        ret = rs_ub_get_jetty_cb(dev_cb, jetty_ids[i], &batch_info->jetty_cb_arr[i]);
        CHK_PRT_RETURN(ret != 0, hccp_err("get jetty_cb[%u] failed, jetty_id:%u, ret:%d", i, jetty_ids[i], ret), ret);
        CHK_PRT_RETURN(batch_info->jetty_cb_arr[i]->state != RS_JETTY_STATE_CREATED, hccp_err("jetty_cb[%u]->state:%u "
        "not support to destroy, jetty_id:%u", i, batch_info->jetty_cb_arr[i]->state, jetty_ids[i]), -EINVAL);

        RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
        RsListDel(&batch_info->jetty_cb_arr[i]->list);
        dev_cb->jetty_cnt--;
        RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);

        batch_info->jetty_arr[i] = batch_info->jetty_cb_arr[i]->jetty;
        batch_info->jfr_arr[i] = batch_info->jetty_cb_arr[i]->jfr;
    }

    return ret;
}

int rs_ub_ctx_jetty_destroy_batch(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[], unsigned int *num)
{
    struct jetty_destroy_batch_info batch_info = {0};
    int ret;

    CHK_PRT_RETURN(*num == 0, hccp_err("num(%u) = 0, no need to destroy batch", *num), -EINVAL);

    ret = rs_ub_calloc_jetty_batch_info(&batch_info, *num);
    if (ret != 0) {
        *num = 0;
        hccp_err("rs_ub_calloc_jetty_batch_info failed, ret:%d", ret);
        goto free_batch_info;
    }

    ret = rs_ub_get_jetty_destroy_batch_info(dev_cb, jetty_ids, &batch_info, num);
    if (ret != 0) {
        *num = 0;
        hccp_err("get jetty destroy batch info failed, ret:%d", ret);
        goto free_batch_info;
    }

    ret = rs_ub_destroy_jetty_cb_batch(&batch_info, num);
    if (ret != 0) {
        hccp_err("rs_ub_destroy_jetty_cb_batch failed, ret:%d", ret);
    }

free_batch_info:
    rs_ub_free_jetty_batch_info(&batch_info);
    return ret;
}

int rs_ub_ctx_query_jetty_batch(struct rs_ub_dev_cb *dev_cb, unsigned int jetty_ids[], struct jetty_attr attr[],
    unsigned int *num)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    urma_jetty_attr_t attr_tmp = {0};
    urma_jetty_cfg_t cfg = {0};
    unsigned int i = 0;
    int ret = 0;

    for (i = 0; i < *num; ++i) {
        (void)memset_s(&attr_tmp, sizeof(urma_jetty_attr_t), 0, sizeof(urma_jetty_attr_t));
        ret = rs_ub_get_jetty_cb(dev_cb, jetty_ids[i], &jetty_cb);
        if (ret != 0) {
            hccp_err("get jetty_cb failed, ret:%d, jetty_id[%u]:%u", ret, i, jetty_ids[i]);
            break;
        }

        ret = rs_urma_query_jetty(jetty_cb->jetty, &cfg, &attr_tmp);
        if (ret != 0) {
            hccp_err("rs_urma_query_jetty failed, ret:%d, jetty_id[%u]:%u", ret, i, jetty_ids[i]);
            break;
        }

        (void)memcpy_s(&attr[i], sizeof(urma_jetty_attr_t), &attr_tmp, sizeof(urma_jetty_attr_t));
    }

    *num = i;
    return ret;
}

STATIC int rs_ub_ctx_get_cqe_aux_info(struct rs_ub_dev_cb *dev_cb, struct aux_info_in *info_in,
    struct aux_info_out *info_out)
{
    struct udma_u_cqe_aux_info_out cqe_info_out = {0};
    struct udma_u_cqe_info_in cqe_info_in = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret = 0;

    cqe_info_out.aux_info_num = AUX_INFO_NUM_MAX;
    cqe_info_out.aux_info_type = info_out->aux_info_type;
    cqe_info_out.aux_info_value = info_out->aux_info_value;
    cqe_info_in.status = info_in->cqe.status;
    cqe_info_in.s_r = info_in->cqe.s_r;
    in.addr = (uint64_t)(uintptr_t)&cqe_info_in;
    in.len = (uint32_t)sizeof(struct udma_u_cqe_info_in);
    in.opcode = UDMA_U_USER_CTL_QUERY_CQE_AUX_INFO;
    out.addr = (uint64_t)(uintptr_t)&cqe_info_out;
    out.len = (uint32_t)sizeof(struct udma_u_cqe_aux_info_out);
    ret = rs_urma_user_ctl(dev_cb->urma_ctx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl query cqe aux info failed, ret:%d errno:%d", ret, errno),
        -EOPENSRC);

    info_out->aux_info_num = cqe_info_out.aux_info_num;
    return ret;
}

STATIC int rs_ub_ctx_get_ae_aux_info(struct rs_ub_dev_cb *dev_cb, struct aux_info_in *info_in,
    struct aux_info_out *info_out)
{
    struct udma_u_ae_aux_info_out ae_info_out = {0};
    struct udma_u_ae_info_in ae_info_in = {0};
    urma_user_ctl_out_t out = {0};
    urma_user_ctl_in_t in = {0};
    int ret = 0;

    ae_info_out.aux_info_num = AUX_INFO_NUM_MAX;
    ae_info_out.aux_info_type = info_out->aux_info_type;
    ae_info_out.aux_info_value = info_out->aux_info_value;
    ae_info_in.event_type = info_in->ae.event_type;
    in.addr = (uint64_t)(uintptr_t)&ae_info_in;
    in.len = (uint32_t)sizeof(struct udma_u_ae_info_in);
    in.opcode = UDMA_U_USER_CTL_QUERY_AE_AUX_INFO;
    out.addr = (uint64_t)(uintptr_t)&ae_info_out;
    out.len = (uint32_t)sizeof(struct udma_u_ae_aux_info_out);
    ret = rs_urma_user_ctl(dev_cb->urma_ctx, &in, &out);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_urma_user_ctl query ae aux info failed, ret:%d errno:%d", ret, errno),
        -EOPENSRC);

    info_out->aux_info_num = ae_info_out.aux_info_num;
    return ret;
}

int rs_ub_ctx_get_aux_info(struct rs_ub_dev_cb *dev_cb, struct aux_info_in *info_in, struct aux_info_out *info_out)
{
    int ret = 0;

    if (info_in->type == AUX_INFO_IN_TYPE_CQE) {
        ret = rs_ub_ctx_get_cqe_aux_info(dev_cb, info_in, info_out);
    } else if (info_in->type == AUX_INFO_IN_TYPE_AE) {
        ret = rs_ub_ctx_get_ae_aux_info(dev_cb, info_in, info_out);
    } else {
        hccp_err("invalid info_in->type[%d]", info_in->type);
        ret = -EINVAL;
    }

    return ret;
}

STATIC void rs_udma_retry_timeout_exception_check(struct SensorNode *sensorNode, urma_cr_t *cr)
{
    int ret = 0;

    if (cr->status != URMA_CR_RNR_RETRY_CNT_EXC_ERR) {
        return;
    }

    ret = RsRetryTimeoutExceptionCheck(sensorNode);

    hccp_warn("update sensor state logic_devid(%u), jetty_id(%u), sensor_update_cnt(%d), ret(%d)\n",
        sensorNode->logicDevid, cr->local_id, sensorNode->sensorUpdateCnt, ret);
}

STATIC void rs_udma_save_cqe_err_info(uint32_t status, struct rs_ctx_jetty_cb *jetty_cb)
{
    RS_PTHREAD_MUTEX_LOCK(&jetty_cb->cr_err_info.mutex);
    if (jetty_cb->cr_err_info.info.status != 0) {
        RS_PTHREAD_MUTEX_ULOCK(&jetty_cb->cr_err_info.mutex);
        return;
    }
    jetty_cb->cr_err_info.info.status = status;
    jetty_cb->cr_err_info.info.jetty_id = jetty_cb->jetty->jetty_id.id;
    RsGetCurTime(&jetty_cb->cr_err_info.info.time);
    RS_PTHREAD_MUTEX_ULOCK(&jetty_cb->cr_err_info.mutex);

    RS_PTHREAD_MUTEX_LOCK(&jetty_cb->dev_cb->cqeErrCntMutex);
    jetty_cb->dev_cb->cqeErrCnt++;
    RS_PTHREAD_MUTEX_ULOCK(&jetty_cb->dev_cb->cqeErrCntMutex);
}

STATIC void rs_jfc_callback_process(struct rs_ctx_jetty_cb *jetty_cb, urma_cr_t *cr, urma_jfc_t *jfc)
{
    if (cr->status != URMA_CR_SUCCESS) {
        rs_udma_save_cqe_err_info(cr->status, jetty_cb);
        rs_udma_retry_timeout_exception_check(&jetty_cb->dev_cb->rscb->sensorNode, cr);
    }
}

STATIC int rs_handle_epoll_poll_jfc(struct rs_ub_dev_cb *dev_cb, urma_jfce_t *jfce)
{
    struct rs_ctx_jetty_cb *jetty_cb = NULL;
    urma_jfc_t *ev_jfc = NULL;
    uint32_t jetty_id = 0;
    uint32_t ack_cnt = 1;
    int polled_cnt, i;
    int wait_cnt = 0;
    int ret_tmp = 0;
    int ret = 0;

    wait_cnt = rs_urma_wait_jfc(jfce, 1, 0, &ev_jfc);
    if (wait_cnt == 0) {
        return -EAGAIN;
    }
    if (wait_cnt != 1) {
        hccp_run_warn("rs_urma_wait_jfc failed, ret:%d errno:%d", wait_cnt, errno);
        return -EOPENSRC;
    }
    rs_urma_ack_jfc((urma_jfc_t **)&ev_jfc, &ack_cnt, 1);

    polled_cnt = rs_urma_poll_jfc(ev_jfc, RS_WC_NUM, g_cr_buf);
    if (polled_cnt > RS_WC_NUM || polled_cnt < 0) {
        hccp_run_warn("rs_urma_poll_jfc failed, ret:%d errno:%d", polled_cnt, errno);
        ret = -EOPENSRC;
        goto rearm_jfc;
    }

    for (i = 0; i < polled_cnt; i++) {
        jetty_id = g_cr_buf[i].local_id;
        RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
        ret = rs_ub_get_jetty_cb(dev_cb, jetty_id, &jetty_cb);
        if (ret != 0) {
            hccp_err("get jetty_cb failed, ret:%d, jetty_id[%u]:%u", ret, i, jetty_id);
            RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
            break;
        }
        jetty_cb->qp_share_info_addr->ci_val += 1;
        rs_jfc_callback_process(jetty_cb, &(g_cr_buf[i]), ev_jfc);
        RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
    }

rearm_jfc:
    ret_tmp = rs_urma_rearm_jfc(ev_jfc, false);
    CHK_PRT_RETURN(ret_tmp != 0, hccp_err("rs_urma_rearm_jfc failed, ret_tmp:%d errno:%d", ret_tmp, errno), -EOPENSRC);
    return ret;
}

STATIC int rs_handle_jfc_epoll_event(struct rs_ub_dev_cb *dev_cb, int fd)
{
    struct rs_ctx_jfce_cb *jfce_cb_curr = NULL;
    struct rs_ctx_jfce_cb *jfce_cb_next = NULL;
    urma_jfce_t *jfce_tmp = NULL;

    RS_LIST_GET_HEAD_ENTRY(jfce_cb_curr, jfce_cb_next, &dev_cb->jfce_list, list, struct rs_ctx_jfce_cb);
    for (; (&jfce_cb_curr->list) != &dev_cb->jfce_list;
        jfce_cb_curr = jfce_cb_next, jfce_cb_next = list_entry(jfce_cb_next->list.next, struct rs_ctx_jfce_cb, list)) {
        jfce_tmp = (urma_jfce_t *)(uintptr_t)jfce_cb_curr->jfce_addr;
        if (jfce_tmp->fd == fd) {
            return rs_handle_epoll_poll_jfc(dev_cb, jfce_tmp);
        }
    }

    return -ENODEV;
}

int rs_epoll_event_jfc_in_handle(struct rs_cb *rs_cb, int fd)
{
    struct rs_ub_dev_cb *dev_cb_curr = NULL;
    struct rs_ub_dev_cb *dev_cb_next = NULL;
    int ret = 0;

    if (rs_cb->protocol != PROTOCOL_UDMA) {
        return -ENODEV;
    }

    RS_LIST_GET_HEAD_ENTRY(dev_cb_curr, dev_cb_next, &rs_cb->rdevList, list, struct rs_ub_dev_cb);
    for (; (&dev_cb_curr->list) != &rs_cb->rdevList;
        dev_cb_curr = dev_cb_next, dev_cb_next = list_entry(dev_cb_next->list.next, struct rs_ub_dev_cb, list)) {
        ret = rs_handle_jfc_epoll_event(dev_cb_curr, fd);
        if (ret == -ENODEV) {
            continue;
        }
        return ret;
    }

    return -ENODEV;
}

STATIC void rs_ub_get_async_event_res_id(urma_async_event_t *event, struct rs_ub_dev_cb *dev_cb,
    unsigned int *res_id)
{
    switch (event->event_type) {
        case URMA_EVENT_JFC_ERR:
            *res_id = event->element.jfc->jfc_id.id;
            break;
        case URMA_EVENT_JFS_ERR:
            *res_id = event->element.jfs->jfs_id.id;
            break;
        case URMA_EVENT_JFR_ERR:
        case URMA_EVENT_JFR_LIMIT:
            *res_id = event->element.jfr->jfr_id.id;
            break;
        case URMA_EVENT_JETTY_ERR:
        case URMA_EVENT_JETTY_LIMIT:
            *res_id = event->element.jetty->jetty_id.id;
            break;
        case URMA_EVENT_JETTY_GRP_ERR:
            *res_id = event->element.jetty_grp->jetty_grp_id.id;
            break;
        case URMA_EVENT_PORT_ACTIVE:
        case URMA_EVENT_PORT_DOWN:
            *res_id = event->element.port_id;
            break;
        case URMA_EVENT_EID_CHANGE:
            *res_id = event->element.eid_idx;
            break;
        case URMA_EVENT_DEV_FATAL:
        case URMA_EVENT_ELR_ERR:
        case URMA_EVENT_ELR_DONE:
            *res_id = dev_cb->index;
            break;
        default:
            hccp_err("invalid event_type:%d dev_index:0x%x", event->event_type, dev_cb->index);
            break;
    }
}

STATIC int rs_ub_get_save_async_event(struct rs_ub_dev_cb *dev_cb)
{
    struct rs_ctx_async_event_cb *async_event_cb = NULL;
    urma_async_event_t *event = NULL;
    int ret = 0;

    async_event_cb = calloc(1, sizeof(struct rs_ctx_async_event_cb));
    CHK_PRT_RETURN(async_event_cb == NULL, hccp_err("calloc async_event_cb failed"), -ENOMEM);
    async_event_cb->dev_cb = dev_cb;
    event = &async_event_cb->async_event;

    ret = rs_urma_get_async_event(dev_cb->urma_ctx, event);
    if (ret != 0) {
        hccp_err("rs_urma_get_async_event failed, ret:%d errno:%d dev_index:0x%x", ret, errno, dev_cb->index);
        ret = -EOPENSRC;
        goto free_event_cb;
    }
    rs_urma_ack_async_event(event);

    rs_ub_get_async_event_res_id(event, dev_cb, &async_event_cb->res_id);
    hccp_run_info("get async_event_type:%d res_id:%u dev_index:0x%x", event->event_type, async_event_cb->res_id,
        dev_cb->index);

    RsListAddTail(&async_event_cb->list, &dev_cb->async_event_list);
    dev_cb->async_event_cnt++;

    return ret;

free_event_cb:
    free(async_event_cb);
    async_event_cb = NULL;
    return ret;
}

int RsEpollEventUrmaAsyncEventInHandle(struct rs_cb *rsCb, int fd)
{
    struct rs_ub_dev_cb *dev_cb_curr = NULL;
    struct rs_ub_dev_cb *dev_cb_next = NULL;
    int ret = 0;

    if (rsCb->protocol != PROTOCOL_UDMA) {
        return -ENODEV;
    }

    RS_LIST_GET_HEAD_ENTRY(dev_cb_curr, dev_cb_next, &rsCb->rdevList, list, struct rs_ub_dev_cb);
    for (; (&dev_cb_curr->list) != &rsCb->rdevList;
        dev_cb_curr = dev_cb_next, dev_cb_next = list_entry(dev_cb_next->list.next, struct rs_ub_dev_cb, list)) {
        RS_PTHREAD_MUTEX_LOCK(&dev_cb_curr->mutex);
        if (dev_cb_curr->urma_ctx != NULL && dev_cb_curr->urma_ctx->async_fd == fd) {
            ret = rs_ub_get_save_async_event(dev_cb_curr);
            if (ret != 0) {
                hccp_err("rs_ub_get_save_async_event failed, ret:%d dev_index:0x%x", ret, dev_cb_curr->index);
            }
            RS_PTHREAD_MUTEX_ULOCK(&dev_cb_curr->mutex);
            return ret;
        }
        RS_PTHREAD_MUTEX_ULOCK(&dev_cb_curr->mutex);
    }

    return -ENODEV;
}

void rs_ub_ctx_get_async_events(struct rs_ub_dev_cb *dev_cb, struct async_event async_events[], unsigned int *num)
{
    struct rs_ctx_async_event_cb *event_cb_curr = NULL;
    struct rs_ctx_async_event_cb *event_cb_next = NULL;
    unsigned int expected_num = *num;

    *num = 0;
    if (RsListEmpty(&dev_cb->async_event_list)) {
        return;
    }

    RS_PTHREAD_MUTEX_LOCK(&dev_cb->mutex);
    RS_LIST_GET_HEAD_ENTRY(event_cb_curr, event_cb_next, &dev_cb->async_event_list, list, struct rs_ctx_async_event_cb);
    for (; (&event_cb_curr->list) != &dev_cb->async_event_list; event_cb_curr = event_cb_next,
        event_cb_next = list_entry(event_cb_next->list.next, struct rs_ctx_async_event_cb, list)) {
        async_events[*num].res_id = event_cb_curr->res_id;
        async_events[*num].event_type = event_cb_curr->async_event.event_type;
        (*num)++;
        rs_ub_free_async_event_cb(dev_cb, event_cb_curr);
        if (*num == expected_num) {
            break;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&dev_cb->mutex);
}