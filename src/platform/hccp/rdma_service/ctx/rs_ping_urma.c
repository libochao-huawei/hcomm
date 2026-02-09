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
#include <errno.h>
#include <urma_opcode.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_urma_function.h"
#include "hccp_common.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_drv_rdma.h"
#include "rs_inner.h"
#include "rs_epoll.h"
#include "rs_socket.h"
#include "rs_ub.h"
#include "rs_ping_inner.h"
#include "rs_ping_urma.h"

urma_cr_t g_ping_jetty_recv_cr[RS_PING_URMA_RECV_WC_NUM] = {0};
urma_cr_t g_pong_jetty_recv_cr[RS_PING_URMA_RECV_WC_NUM] = {0};

STATIC bool rs_ping_urma_check_fd(struct RsPingCtxCb *ping_cb, int fd)
{
    if (ping_cb->ping_jetty.jfce != NULL && ping_cb->ping_jetty.jfce->fd == fd) {
        hccp_dbg("ping_jetty jfce->fd:%d poll jfc", fd);
        return true;
    }
    return false;
}

STATIC bool rs_pong_urma_check_fd(struct RsPingCtxCb *ping_cb, int fd)
{
    if (ping_cb->pong_jetty.jfce != NULL && ping_cb->pong_jetty.jfce->fd == fd) {
        hccp_dbg("pong_jetty jfce->fd:%d poll jfc", fd);
        return true;
    }
    return false;
}

STATIC void rs_get_jetty_info(struct PingQpInfo *qp_info, urma_jetty_id_t *jetty_id, urma_eid_t *eid)
{
    urma_jetty_id_t jetty_key_info = {0};
    int ret = 0;

    ret = memcpy_s(&jetty_key_info, sizeof(urma_jetty_id_t), qp_info->ub.key, qp_info->ub.size);
    if (ret != 0) {
        hccp_err("memcpy jetty_key_info failed, ret:%d", ret);
        return;
    }

    if (jetty_id != NULL) {
        *jetty_id = jetty_key_info;
    }
    if (eid != NULL) {
        *eid = jetty_key_info.eid;
    }
}

STATIC bool rs_ping_common_compare_ub_info(struct PingQpInfo *a, struct PingQpInfo *b)
{
    if (a->ub.size != b->ub.size) {
        return false;
    }
    if (memcmp(&a->ub.key, &b->ub.key, sizeof(a->ub.key)) != 0) {
        return false;
    }
    return true;
}

STATIC int rs_ping_cb_get_urma_context_and_index(struct RsPingCtxCb *ping_cb, struct PingInitAttr *attr)
{
    struct dev_base_attr dev_attr = {0};
    int ret;

    ping_cb->udev_cb.urma_ctx = rs_urma_create_context(ping_cb->udev_cb.urma_dev, attr->dev.ub.eid_index);
    CHK_PRT_RETURN(ping_cb->udev_cb.urma_ctx == NULL, hccp_err("urma_create_context failed, errno:%d, "
        "eid_index:%u", errno, attr->dev.ub.eid_index), -ENODEV);

    ret = rs_ub_get_ue_info(ping_cb->udev_cb.urma_ctx, &dev_attr);
    if (ret != 0) {
        hccp_err("rs_ub_get_ue_info failed, ret:%d errno:%d", ret, errno);
        ret = -EOPENSRC;
        goto free_urma_ctx;
    }

    ping_cb->devIndex = rs_generate_dev_index(PING_URMA_DEV_CNT, dev_attr.ub.die_id, dev_attr.ub.func_id);
    return 0;

free_urma_ctx:
    (void)rs_urma_delete_context(ping_cb->udev_cb.urma_ctx);
    ping_cb->udev_cb.urma_ctx = NULL;
    return ret;
}

STATIC int rs_ping_common_init_jfce(struct RsPingCtxCb *ping_cb, struct rs_ping_local_jetty_cb *jetty_cb)
{
    jetty_cb->jfce = rs_urma_create_jfce(ping_cb->udev_cb.urma_ctx);
    CHK_PRT_RETURN(jetty_cb->jfce == NULL, hccp_err("urma_create_jfce failed, errno:%d", errno), -EOPENSRC);

    hccp_run_info("eid:%016llx:%016llx init jfce success, fd:%d", ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix,
        ping_cb->udev_cb.eid_info.eid.in6.interface_id, jetty_cb->jfce->fd);
    return 0;
}

STATIC int rs_ping_common_init_send_jfc_with_attr(struct rs_cb *rscb, struct RsPingCtxCb *ping_cb,
    union PingQpAttr *attr, struct rs_ping_local_jetty_cb *jetty_cb)
{
    urma_jfc_cfg_t send_jfc_cfg = {
        .depth = attr->ub.cq_attr.sendCqDepth,
        .flag = {.value = 0},
        .jfce = NULL,
        .user_ctx = 0,
    };
    jetty_cb->send_jfc.depth = attr->ub.cq_attr.sendCqDepth;
    jetty_cb->send_jfc.num_events = 0;
    jetty_cb->send_jfc.max_recv_wc_num = RS_PING_URMA_RECV_WC_NUM;
    jetty_cb->send_jfc.jfc = rs_urma_create_jfc(ping_cb->udev_cb.urma_ctx, &send_jfc_cfg);
    CHK_PRT_RETURN(jetty_cb->send_jfc.jfc == NULL, hccp_err("urma_create_jfc failed, errno:%d", errno), -EOPENSRC);

    hccp_run_info("eid:%016llx:%016llx init send jfc success, jfc_id:%u",
        ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix, ping_cb->udev_cb.eid_info.eid.in6.interface_id,
        jetty_cb->send_jfc.jfc->jfc_id.id);
    return 0;
}

STATIC int rs_ping_common_init_recv_jfc_with_attr(struct RsPingCtxCb *ping_cb,
    union PingQpAttr *attr, struct rs_ping_local_jetty_cb *jetty_cb)
{
    urma_jfc_cfg_t recv_jfc_cfg = {
        .depth = attr->ub.cq_attr.recvCqDepth,
        .flag = {.value = 0},
        .jfce = jetty_cb->jfce,
        .user_ctx = 0,
    };
    jetty_cb->recv_jfc.depth = attr->ub.cq_attr.recvCqDepth;
    jetty_cb->recv_jfc.num_events = 0;
    jetty_cb->recv_jfc.max_recv_wc_num = RS_PING_URMA_RECV_WC_NUM;
    jetty_cb->recv_jfc.jfc = rs_urma_create_jfc(ping_cb->udev_cb.urma_ctx, &recv_jfc_cfg);
    CHK_PRT_RETURN(jetty_cb->recv_jfc.jfc == NULL, hccp_err("urma_create_jfc failed, errno:%d", errno), -EOPENSRC);

    hccp_run_info("eid:%016llx:%016llx init recv jfc success, jfc_id:%u",
        ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix, ping_cb->udev_cb.eid_info.eid.in6.interface_id,
        jetty_cb->recv_jfc.jfc->jfc_id.id);
    return 0;
}

STATIC int rs_ping_common_init_jetty_with_attr(struct RsPingCtxCb *ping_cb,
    union PingQpAttr *attr, struct rs_ping_local_jetty_cb *jetty_cb)
{
    urma_jetty_cfg_t jetty_cfg = {0};
    urma_jfs_cfg_t jfs_cfg = {0};
    urma_jfr_cfg_t jfr_cfg = {0};
    int ret;

    jfs_cfg.depth = attr->ub.qp_attr.cap.maxSendWr;
    jfs_cfg.trans_mode = URMA_TM_UM;
    jfs_cfg.max_sge = (uint8_t)attr->ub.qp_attr.cap.maxSendSge;
    jfs_cfg.max_inline_data = attr->ub.qp_attr.cap.maxInlineData;
    jfs_cfg.rnr_retry = URMA_TYPICAL_RNR_RETRY;
    jfs_cfg.jfc = jetty_cb->send_jfc.jfc;
    jfs_cfg.user_ctx = 0;

    jfr_cfg.depth = attr->ub.qp_attr.cap.maxRecvWr;
    jfr_cfg.trans_mode = URMA_TM_UM;
    jfr_cfg.max_sge = (uint8_t)attr->ub.qp_attr.cap.maxRecvSge;
    jfr_cfg.min_rnr_timer = URMA_TYPICAL_MIN_RNR_TIMER;
    jfr_cfg.jfc = jetty_cb->recv_jfc.jfc;
    jfr_cfg.token_value.token = attr->ub.qp_attr.token_value;

    jetty_cb->jfr = rs_urma_create_jfr(ping_cb->udev_cb.urma_ctx, &jfr_cfg);
    CHK_PRT_RETURN(jetty_cb->jfr == NULL, hccp_err("urma_create_jfr failed, errno:%d", errno), -ENOMEM);

    jetty_cfg.flag.bs.share_jfr = URMA_SHARE_JFR;
    jetty_cfg.jfs_cfg = jfs_cfg;
    jetty_cfg.shared.jfr = jetty_cb->jfr;
    jetty_cfg.shared.jfc = jetty_cb->recv_jfc.jfc;

    jetty_cb->jetty = rs_urma_create_jetty(ping_cb->udev_cb.urma_ctx, &jetty_cfg);
    if (jetty_cb->jetty == NULL) {
        hccp_err("urma_create_jetty failed, errno:%d", errno);
        ret = -ENOMEM;
        goto create_jetty_fail;
    }

    jetty_cb->token_value = attr->ub.qp_attr.token_value;
    hccp_run_info("eid:%016llx:%016llx init jetty success, jetty_id:%u",
        ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix, ping_cb->udev_cb.eid_info.eid.in6.interface_id,
        jetty_cb->jetty->jetty_id.id);
    return 0;

create_jetty_fail:
    (void)rs_urma_delete_jfr(jetty_cb->jfr);
    jetty_cb->jfr = NULL;
    return ret;
}

STATIC int rs_ping_common_init_local_jetty(struct rs_cb *rscb, struct RsPingCtxCb *ping_cb, union PingQpAttr *attr,
    struct rs_ping_local_jetty_cb *jetty_cb)
{
    int ret;

    hccp_info("eid:%016llx:%016llx cap{%u %u %u %u %u} start init local jettys",
        ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix, ping_cb->udev_cb.eid_info.eid.in6.interface_id,
        attr->ub.qp_attr.cap.maxSendWr, attr->ub.qp_attr.cap.maxRecvWr, attr->ub.qp_attr.cap.maxSendSge,
        attr->ub.qp_attr.cap.maxRecvSge, attr->ub.qp_attr.cap.maxInlineData);

    ret = rs_ping_common_init_jfce(ping_cb, jetty_cb);
    if (ret != 0) {
        hccp_err("init jfce failed, ret:%d", ret);
        goto init_jfce_fail;
    }

    ret = RsEpollCtl(rscb->connCb.epollfd, EPOLL_CTL_ADD, jetty_cb->jfce->fd, EPOLLIN | EPOLLRDHUP);
    if (ret != 0) {
        hccp_err("RsEpollCtl failed! epollfd:%d fd:%d ret:%d", rscb->connCb.epollfd, jetty_cb->jfce->fd, ret);
        goto epoll_ctl_fail;
    }

    ret = rs_ping_common_init_send_jfc_with_attr(rscb, ping_cb, attr, jetty_cb);
    if (ret != 0) {
        hccp_err("init send jfc failed, ret:%d", ret);
        goto init_send_jfc_fail;
    }

    ret = rs_ping_common_init_recv_jfc_with_attr(ping_cb, attr, jetty_cb);
    if (ret != 0) {
        hccp_err("init recv jfc failed, ret:%d", ret);
        goto init_recv_jfc_fail;
    }

    ret = rs_ping_common_init_jetty_with_attr(ping_cb, attr, jetty_cb);
    if (ret != 0) {
        hccp_err("init jetty failed, ret:%d", ret);
        goto init_jetty_fail;
    }
    return 0;

init_jetty_fail:
    (void)rs_urma_delete_jfc(jetty_cb->recv_jfc.jfc);
    jetty_cb->recv_jfc.jfc = NULL;
init_recv_jfc_fail:
    (void)rs_urma_delete_jfc(jetty_cb->send_jfc.jfc);
    jetty_cb->send_jfc.jfc = NULL;
init_send_jfc_fail:
    (void)RsEpollCtl(rscb->connCb.epollfd, EPOLL_CTL_DEL, jetty_cb->jfce->fd, EPOLLIN | EPOLLRDHUP);
epoll_ctl_fail:
    (void)rs_urma_delete_jfce(jetty_cb->jfce);
    jetty_cb->jfce = NULL;
init_jfce_fail:
    return ret;
}

STATIC void rs_ping_common_deinit_local_jetty(struct rs_cb *rscb, struct RsPingCtxCb *ping_cb,
    struct rs_ping_local_jetty_cb *jetty_cb)
{
    (void)rs_urma_delete_jetty(jetty_cb->jetty);
    jetty_cb->jetty = NULL;

    (void)rs_urma_delete_jfr(jetty_cb->jfr);
    jetty_cb->jfr = NULL;

    (void)rs_urma_delete_jfc(jetty_cb->recv_jfc.jfc);
    jetty_cb->recv_jfc.jfc = NULL;

    (void)rs_urma_delete_jfc(jetty_cb->send_jfc.jfc);
    jetty_cb->send_jfc.jfc = NULL;

    (void)RsEpollCtl(rscb->connCb.epollfd, EPOLL_CTL_DEL, jetty_cb->jfce->fd, EPOLLIN | EPOLLRDHUP);

    (void)rs_urma_delete_jfce(jetty_cb->jfce);
    jetty_cb->jfce = NULL;

    return;
}

STATIC int rs_ping_common_init_seg_cb(struct rs_cb *rscb, struct RsPingCtxCb *ping_cb, struct rs_ping_seg_cb *seg_cb)
{
    unsigned long flag = 0;
    uint32_t idx = 0;
    int ret;

    urma_reg_seg_flag_t seg_flag = {
        .bs.token_policy = URMA_TOKEN_PLAIN_TEXT,
        .bs.cacheable = URMA_NON_CACHEABLE,
        .bs.access = PINGMESH_DEF_ACCESS,
        .bs.token_id_valid = URMA_TOKEN_ID_INVALID,
        .bs.reserved = 0
    };
    urma_seg_cfg_t seg_cfg = {
        .va = 0,
        .len = seg_cb->len,
        .token_value = seg_cb->token_value,
        .flag = seg_flag,
        .user_ctx = (uintptr_t)NULL,
        .iova = 0
    };

    hccp_info("payload_offset:%u len:0x%llx sge_num:%u grp_id:%u",
        seg_cb->payload_offset, seg_cb->len, seg_cb->sge_num, rscb->grpId);

    ret = pthread_mutex_init(&seg_cb->mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread_mutex_init seg_cb mutex failed, ret:%d", ret), ret);

    flag = ((unsigned long)ping_cb->logicDevid << BUFF_FLAGS_DEVID_OFFSET) | BUFF_SP_SVM;
    ret = DlHalBuffAllocAlignEx(seg_cb->len, RA_RS_PING_BUFFER_ALIGN_4K_PAGE_SIZE,
        flag, (int)rscb->grpId, (void **)&seg_cb->addr);
    if (ret != 0) {
        hccp_err("DlHalBuffAllocAlignEx failed, length:0x%llx, dev_id:0x%x, flag:0x%lx, grp_id:%u, ret:%d",
            seg_cb->len, ping_cb->logicDevid, flag, rscb->grpId, ret);
        goto alloc_fail;
    }

    seg_cfg.va = seg_cb->addr;
    seg_cb->segment = rs_urma_register_seg(ping_cb->udev_cb.urma_ctx, &seg_cfg);
    if (seg_cb->segment == NULL) {
        ret = -errno;
        hccp_err("urma_register_seg failed, ret:%d addr:0x%llx len:0x%llx", ret, seg_cb->addr, seg_cb->len);
        goto segment_reg_fail;
    }

    // init sge list
    seg_cb->sge_list = calloc(seg_cb->sge_num, sizeof(urma_sge_t));
    if (seg_cb->sge_list == NULL) {
        ret = -errno;
        hccp_err("calloc failed, ret:%d sge_num:%u", ret, seg_cb->sge_num);
        goto calloc_fail;
    }

    for (idx = 0; idx < seg_cb->sge_num; idx++) {
        seg_cb->sge_list[idx].tseg = seg_cb->segment;
        seg_cb->sge_list[idx].len = seg_cb->payload_offset;
        if (idx == 0) {
            seg_cb->sge_list[idx].addr = seg_cb->addr;
        } else {
            seg_cb->sge_list[idx].addr = seg_cb->sge_list[idx - 1].addr + seg_cb->payload_offset;
        }
    }
    seg_cb->sge_idx = 0;

    hccp_info("eid:%016llx:%016llx segment register success, addr:0x%llx len:%u",
        ping_cb->udev_cb.eid_info.eid.in6.subnet_prefix, ping_cb->udev_cb.eid_info.eid.in6.interface_id,
        seg_cb->addr, seg_cb->len);

    return 0;

calloc_fail:
    (void)rs_urma_unregister_seg(seg_cb->segment);
    seg_cb->segment = NULL;
segment_reg_fail:
    (void)DlHalBuffFree((void *)(uintptr_t)seg_cb->addr);
alloc_fail:
    (void)pthread_mutex_destroy(&seg_cb->mutex);
    return ret;
}

STATIC void rs_ping_common_deinit_seg_cb(struct rs_ping_seg_cb *seg_cb)
{
    hccp_dbg("addr:0x%llx len:%llu", seg_cb->addr, seg_cb->len);

    free(seg_cb->sge_list);
    seg_cb->sge_list = NULL;

    (void)rs_urma_unregister_seg(seg_cb->segment);
    seg_cb->segment = NULL;

    (void)DlHalBuffFree((void *)(uintptr_t)seg_cb->addr);

    (void)pthread_mutex_destroy(&seg_cb->mutex);
}

STATIC int rs_ping_pong_init_local_jetty_buffer(struct rs_cb *rscb, struct PingInitAttr *attr,
    struct PingInitInfo *info, struct RsPingCtxCb *ping_cb)
{
    int ret;

    // prepare ping_jetty send segment
    ping_cb->ping_jetty.send_seg_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->ping_jetty.send_seg_cb.len =
        attr->client.ub.qp_attr.cap.maxSendWr * ping_cb->ping_jetty.send_seg_cb.payload_offset;
    ping_cb->ping_jetty.send_seg_cb.sge_num = attr->client.ub.qp_attr.cap.maxSendWr;
    ping_cb->ping_jetty.send_seg_cb.token_value.token = attr->client.ub.seg_attr.token_value;
    ret = rs_ping_common_init_seg_cb(rscb, ping_cb, &ping_cb->ping_jetty.send_seg_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ping_common_init_seg_cb ping_jetty send_seg_cb failed, ret %d", ret), ret);

    // prepare ping_jetty recv segment
    ping_cb->ping_jetty.recv_seg_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->ping_jetty.recv_seg_cb.len =
        attr->client.ub.qp_attr.cap.maxRecvWr * ping_cb->ping_jetty.recv_seg_cb.payload_offset;
    ping_cb->ping_jetty.recv_seg_cb.sge_num = attr->client.ub.qp_attr.cap.maxRecvWr;
    ping_cb->ping_jetty.recv_seg_cb.token_value.token = attr->client.ub.seg_attr.token_value;
    ret = rs_ping_common_init_seg_cb(rscb, ping_cb, &ping_cb->ping_jetty.recv_seg_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_seg_cb ping_jetty recv_seg_cb failed, ret %d", ret);
        goto init_ping_jetty_recv_seg_fail;
    }

    // prepare pong_jetty send segment
    ping_cb->pong_jetty.send_seg_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->pong_jetty.send_seg_cb.len =
        attr->server.ub.qp_attr.cap.maxSendWr * ping_cb->pong_jetty.send_seg_cb.payload_offset;
    ping_cb->pong_jetty.send_seg_cb.sge_num = attr->server.ub.qp_attr.cap.maxSendWr;
    ping_cb->pong_jetty.send_seg_cb.token_value.token = attr->server.ub.seg_attr.token_value;
    ret = rs_ping_common_init_seg_cb(rscb, ping_cb, &ping_cb->pong_jetty.send_seg_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_seg_cb pong_jetty send_seg_cb failed, ret %d", ret);
        goto init_pong_jetty_send_seg_fail;
    }
    // prepare pong_jetty recv segment
    ping_cb->pong_jetty.recv_seg_cb.payload_offset = PING_TOTAL_PAYLOAD_MAX_SIZE;
    ping_cb->pong_jetty.recv_seg_cb.len = attr->bufferSize;
    ping_cb->pong_jetty.recv_seg_cb.sge_num = attr->bufferSize / ping_cb->pong_jetty.recv_seg_cb.payload_offset;
    ping_cb->pong_jetty.recv_seg_cb.token_value.token = attr->server.ub.seg_attr.token_value;
    ret = rs_ping_common_init_seg_cb(rscb, ping_cb, &ping_cb->pong_jetty.recv_seg_cb);
    if (ret != 0) {
        hccp_err("rs_ping_common_init_seg_cb pong_jetty recv_seg_cb failed, ret %d", ret);
        goto init_pong_jetty_recv_seg_fail;
    }
    info->result.bufferVa = ping_cb->pong_jetty.recv_seg_cb.addr;
    info->result.bufferSize = attr->bufferSize;
    info->result.payloadOffset = ping_cb->pong_jetty.recv_seg_cb.payload_offset;
    info->result.headerSize = RS_PING_PAYLOAD_HEADER_RESV_CUSTOM;
    return 0;

init_pong_jetty_recv_seg_fail:
    rs_ping_common_deinit_seg_cb(&ping_cb->pong_jetty.send_seg_cb);
init_pong_jetty_send_seg_fail:
    rs_ping_common_deinit_seg_cb(&ping_cb->ping_jetty.recv_seg_cb);
init_ping_jetty_recv_seg_fail:
    rs_ping_common_deinit_seg_cb(&ping_cb->ping_jetty.send_seg_cb);
    return ret;
}

STATIC void rs_ping_common_deinit_local_jetty_buffer(struct RsPingCtxCb *ping_cb)
{
    rs_ping_common_deinit_seg_cb(&ping_cb->pong_jetty.recv_seg_cb);
    rs_ping_common_deinit_seg_cb(&ping_cb->pong_jetty.send_seg_cb);
    rs_ping_common_deinit_seg_cb(&ping_cb->ping_jetty.recv_seg_cb);
    rs_ping_common_deinit_seg_cb(&ping_cb->ping_jetty.send_seg_cb);
}

STATIC int rs_ping_common_jfr_post_recv(struct rs_ping_local_jetty_cb *jetty_cb)
{
    urma_jfr_wr_t *jfr_bad_wr = NULL;
    urma_jfr_wr_t jfr_wr = {0};
    urma_sge_t list = {0};
    uint32_t sge_idx;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&jetty_cb->recv_seg_cb.mutex);
    sge_idx = jetty_cb->recv_seg_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(urma_sge_t), &jetty_cb->recv_seg_cb.sge_list[sge_idx], sizeof(urma_sge_t));
    jetty_cb->recv_seg_cb.sge_idx = (sge_idx + 1) % jetty_cb->recv_seg_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&jetty_cb->recv_seg_cb.mutex);

    jfr_wr.user_ctx = (uintptr_t)sge_idx;
    jfr_wr.next = NULL;
    jfr_wr.src.sge = &list;
    jfr_wr.src.num_sge = 1;

    ret = rs_urma_post_jetty_recv_wr(jetty_cb->jetty, &jfr_wr, &jfr_bad_wr);
    if (ret != 0) {
        hccp_err("urma_post_jetty_recv_wr failed, ret:%d", ret);
        return ret;
    }

    return 0;
}

STATIC int rs_ping_common_init_jetty_post_recv_all(struct rs_ping_local_jetty_cb *jetty_cb)
{
    int ret = 0;
    uint32_t i;

    // reset recv jfc notify
    (void)rs_urma_rearm_jfc(jetty_cb->recv_jfc.jfc, false);

    // prepare jfr wqe
    for (i = jetty_cb->recv_seg_cb.sge_idx;
        i < jetty_cb->recv_seg_cb.sge_num && i < jetty_cb->jfr->jfr_cfg.depth; i++) {
        ret = rs_ping_common_jfr_post_recv(jetty_cb);
        if (ret != 0) {
            hccp_err("rs_ping_common_jfr_post_recv %u-th rqe failed, ret:%d", i, ret);
            break;
        }
    }

    return ret;
}

STATIC int rs_ping_pong_init_local_ub_resources(struct rs_cb *rscb, struct PingInitAttr *attr,
    struct PingInitInfo *info, struct RsPingCtxCb *ping_cb)
{
    urma_jetty_id_t jetty_key = {0};
    int ret;

    ret = rs_ping_common_init_local_jetty(rscb, ping_cb, &attr->client, &ping_cb->ping_jetty);
    CHK_PRT_RETURN(ret != 0, hccp_err("init ping_jetty failed, ret:%d", ret), ret);
    info->client.version = 0;
    info->client.ub.token_value = attr->client.ub.qp_attr.token_value;
    info->client.ub.size = (uint8_t)sizeof(urma_jetty_id_t);
    jetty_key = ping_cb->ping_jetty.jetty->jetty_id;

    ret = memcpy_s(info->client.ub.key, sizeof(info->client.ub.key), &jetty_key, sizeof(urma_jetty_id_t));
    if (ret != 0) {
        hccp_err("memcpy_s urma_jetty_id_t to PingQpInfo.ub.key failed, ret:%d", ret);
        goto init_pong_jetty_fail;
    }

    ret = rs_ping_common_init_local_jetty(rscb, ping_cb, &attr->server, &ping_cb->pong_jetty);
    if (ret != 0) {
        hccp_err("init pong_jetty failed, ret:%d", ret);
        goto init_pong_jetty_fail;
    }
    info->server.version = 0;
    info->server.ub.token_value = attr->server.ub.qp_attr.token_value;
    info->server.ub.size = (uint8_t)sizeof(urma_jetty_id_t);
    jetty_key = ping_cb->pong_jetty.jetty->jetty_id;
    (void)memcpy_s(info->server.ub.key, sizeof(info->server.ub.key), &jetty_key, sizeof(urma_jetty_id_t));

    ret = rs_ping_pong_init_local_jetty_buffer(rscb, attr, info, ping_cb);
    if (ret != 0) {
        hccp_err("init jetty buffer failed, ret:%d", ret);
        goto init_buffer_fail;
    }

    ret = rs_ping_common_init_jetty_post_recv_all(&ping_cb->ping_jetty);
    if (ret != 0) {
        hccp_err("ping_jetty post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }
    ret = rs_ping_common_init_jetty_post_recv_all(&ping_cb->pong_jetty);
    if (ret != 0) {
        hccp_err("pong_jetty post recv failed, ret:%d", ret);
        goto post_recv_fail;
    }
    return 0;

post_recv_fail:
    rs_ping_common_deinit_local_jetty_buffer(ping_cb);
init_buffer_fail:
    rs_ping_common_deinit_local_jetty(rscb, ping_cb, &ping_cb->pong_jetty);
init_pong_jetty_fail:
    rs_ping_common_deinit_local_jetty(rscb, ping_cb, &ping_cb->ping_jetty);
    return ret;
}

STATIC int rs_ping_urma_ping_cb_init(unsigned int phy_id, struct PingInitAttr *attr, struct PingInitInfo *info,
    unsigned int *dev_index, struct RsPingCtxCb *ping_cb)
{
    struct rs_cb *rscb = NULL;
    union urma_eid eid;
    int ret;

    ret = RsGetRsCb(phy_id, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("RsGetRsCb failed, phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    // prepare input attr
    ping_cb->udev_cb.eid_info.eid_index = attr->dev.ub.eid_index;
    ping_cb->udev_cb.eid_info.eid = attr->dev.ub.eid;
    (void)memcpy_s(&ping_cb->commInfo, sizeof(struct PingLocalCommInfo), &attr->commInfo,
        sizeof(struct PingLocalCommInfo));

    (void)memcpy_s(eid.raw, sizeof(eid.raw), attr->dev.ub.eid.raw, sizeof(attr->dev.ub.eid.raw));
    ping_cb->udev_cb.urma_dev = rs_urma_get_device_by_eid(eid, URMA_TRANSPORT_UB);
    if (ping_cb->udev_cb.urma_dev == NULL) {
        hccp_err("urma_get_device_by_eid failed, urma_dev is NULL, errno:%d eid:%016llx:%016llx", errno,
            eid.in6.subnet_prefix, eid.in6.interface_id);
        ret = -ENODEV;
        goto get_urma_dev_fail;
    }

    ret = rs_ping_cb_get_urma_context_and_index(ping_cb, attr);
    if (ret != 0) {
        hccp_err("rs_ping_cb_get_urma_context_and_index failed, ret:%d", ret);
        goto get_urma_dev_fail;
    }

    info->version = 0;
    ret = rs_ping_pong_init_local_ub_resources(rscb, attr, info, ping_cb);
    if (ret != 0) {
        hccp_err("rs_ping_pong_init_local_ub_resources failed, ret:%d phy_id:%u", ret, phy_id);
        goto init_local_resources_fail;
    }
    *dev_index = ping_cb->devIndex;
    return 0;

init_local_resources_fail:
    (void)rs_urma_delete_context(ping_cb->udev_cb.urma_ctx);
    ping_cb->udev_cb.urma_ctx = NULL;
get_urma_dev_fail:
    (void)pthread_mutex_destroy(&ping_cb->pingMutex);
    (void)pthread_mutex_destroy(&ping_cb->pongMutex);
    return ret;
}

STATIC int rs_ping_urma_find_target_node(struct RsPingCtxCb *ping_cb, struct PingQpInfo *target,
    struct RsPingTargetInfo **node)
{
    struct RsPingTargetInfo *target_next = NULL;
    struct RsPingTargetInfo *target_curr = NULL;
    urma_jetty_id_t target_jetty_id = {0};
    urma_eid_t target_eid = {0};

    rs_get_jetty_info(target, &target_jetty_id, &target_eid);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pingMutex);
    RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &ping_cb->pingList, list, struct RsPingTargetInfo);
    for (; (&target_curr->list) != &ping_cb->pingList;
        target_curr = target_next, target_next = list_entry(target_next->list.next, struct RsPingTargetInfo, list)) {
        if (rs_ping_common_compare_ub_info(&target_curr->qpInfo, target)) {
            *node = target_curr;
            RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pingMutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pingMutex);

    hccp_info("ping target node for jetty_id:%u eid:%016llx:%016llx not found", target_jetty_id.id,
        target_eid.in6.subnet_prefix, target_eid.in6.interface_id);
    return -ENODEV;
}

STATIC int rs_ping_common_import_jetty(urma_context_t *urma_ctx, struct PingQpInfo *target,
    urma_target_jetty_t **import_tjetty)
{
    urma_token_t token_value = {0};
    urma_eid_t remote_eid = {0};
    urma_rjetty_t rjetty = {0};

    rs_get_jetty_info(target, &rjetty.jetty_id, &remote_eid);

    token_value.token = target->ub.token_value;
    rjetty.trans_mode = URMA_TM_UM;
    rjetty.type = URMA_JETTY;
    rjetty.flag.bs.order_type = 0;
    rjetty.tp_type = URMA_UTP;

    *import_tjetty = rs_urma_import_jetty(urma_ctx, &rjetty, &token_value);
    if (*import_tjetty == NULL) {
        hccp_err("urma_import_jetty failed, errno:%d remote eid:%016llx:%016llx", errno,
            remote_eid.in6.subnet_prefix, remote_eid.in6.interface_id);
        return -EOPENSRC;
    }
    return 0;
}

STATIC int rs_ping_urma_alloc_target_node(struct RsPingCtxCb *ping_cb, struct PingTargetInfo *target,
    struct RsPingTargetInfo **node)
{
    struct RsPingTargetInfo *target_info = NULL;
    int ret;

    target_info = (struct RsPingTargetInfo *)calloc(1, sizeof(struct RsPingTargetInfo));
    CHK_PRT_RETURN(target_info == NULL, hccp_err("calloc target_info failed! errno:%d", errno), -ENOMEM);

    ret = pthread_mutex_init(&target_info->tripMutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init tripMutex failed, ret:%d", ret);
        goto free_target_info;
    }

    target_info->payloadSize = target->payload.size;
    if (target->payload.size > 0) {
        target_info->payloadBuffer = (char *)calloc(1, target->payload.size);
        if (target_info->payloadBuffer == NULL) {
            hccp_err("calloc payloadBuffer failed! size:%u errno:%d", target->payload.size, errno);
            ret = -ENOMEM;
            goto free_trip_mutex;
        }
        (void)memcpy_s(target_info->payloadBuffer, target->payload.size, target->payload.buffer, target->payload.size);
    }

    (void)memcpy_s(&target_info->qpInfo, sizeof(struct PingQpInfo),
        &target->remoteInfo.qpInfo, sizeof(struct PingQpInfo));

    ret = rs_ping_common_import_jetty(ping_cb->udev_cb.urma_ctx, &target->remoteInfo.qpInfo,
        &target_info->import_tjetty);
    if (ret != 0) {
        hccp_err("rs_ping_import_jetty failed, ret:%d", ret);
        goto free_payload_buffer;
    }

    target_info->resultSummary.rttMin = ~0;
    target_info->state = RS_PING_PONG_TARGET_READY;
    *node = target_info;
    return 0;

free_payload_buffer:
    if (target->payload.size > 0 && target_info->payloadBuffer != NULL) {
        free(target_info->payloadBuffer);
        target_info->payloadBuffer = NULL;
    }
free_trip_mutex:
    (void)pthread_mutex_destroy(&target_info->tripMutex);
free_target_info:
    free(target_info);
    target_info = NULL;
    return ret;
}

STATIC void rs_ping_urma_reset_recv_buffer(struct RsPingCtxCb *ping_cb)
{
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_jetty.recv_seg_cb.mutex);
    (void)memset_s((void *)(uintptr_t)ping_cb->pong_jetty.recv_seg_cb.addr, ping_cb->pong_jetty.recv_seg_cb.len,
        0, ping_cb->pong_jetty.recv_seg_cb.len);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_jetty.recv_seg_cb.mutex);
}

STATIC void rs_ping_fill_send_header(struct RsPingPayloadHeader *header, urma_jetty_id_t *server_jetty_key,
    struct rs_ping_local_jetty_cb *pong_jetty, struct RsPingTargetInfo *target)
{
    header->type = RS_PING_TYPE_URMA_DETECT;
    (void)memcpy_s(header->server.ub.key, sizeof(header->server.ub.key), server_jetty_key, sizeof(urma_jetty_id_t));
    header->server.ub.size = sizeof(urma_jetty_id_t);
    header->server.ub.token_value = pong_jetty->token_value;
    (void)memcpy_s(&header->target, sizeof(struct PingQpInfo), &target->qpInfo, sizeof(struct PingQpInfo));
}

STATIC void rs_ping_jetty_build_up_wr(struct RsPingCtxCb *ping_cb, struct RsPingTargetInfo *target,
    urma_sge_t *list, urma_jfs_wr_t *wr)
{
    wr->opcode = URMA_OPC_SEND;
    wr->flag.bs.complete_enable = 1;
    wr->tjetty = target->import_tjetty;
    wr->user_ctx = target->uuid;
    wr->send.src.sge = list;
    wr->send.src.num_sge = 1;
    wr->send.imm_data = 0;
    wr->next = NULL;
}

STATIC int rs_ping_urma_post_send(struct RsPingCtxCb *ping_cb, struct RsPingTargetInfo *target)
{
    urma_jetty_id_t server_jetty_key = {0};
    struct RsPingPayloadHeader *header = NULL;
    urma_jetty_id_t target_jetty_id = {0};
    struct timeval timestamp = {0};
    urma_jfs_wr_t *bad_wr = NULL;
    urma_eid_t target_eid = {0};
    urma_jfs_wr_t wr = {0};
    urma_sge_t list = {0};
    uint32_t sge_idx;
    int ret = 0;

    rs_get_jetty_info(&target->qpInfo, &target_jetty_id, &target_eid);
    hccp_dbg("target uuid:0x%llx state:%d payload_size:%u jetty_id:%u eid:%016llx:%016llx", target->uuid, target->state,
        target->payloadSize, target_jetty_id.id, target_eid.in6.subnet_prefix, target_eid.in6.interface_id);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_jetty.send_seg_cb.mutex);
    sge_idx = ping_cb->ping_jetty.send_seg_cb.sge_idx;
    (void)memcpy_s(&list, sizeof(urma_sge_t), &ping_cb->ping_jetty.send_seg_cb.sge_list[sge_idx], sizeof(urma_sge_t));
    ping_cb->ping_jetty.send_seg_cb.sge_idx = (sge_idx + 1) % ping_cb->ping_jetty.send_seg_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_jetty.send_seg_cb.mutex);

    // prepare ping_jetty send buffer
    server_jetty_key = ping_cb->pong_jetty.jetty->jetty_id;
    (void)memset_s((void *)(uintptr_t)list.addr, list.len, 0, list.len);
    header = (struct RsPingPayloadHeader *)(uintptr_t)list.addr;
    rs_ping_fill_send_header(header, &server_jetty_key, &ping_cb->pong_jetty, target);

    if (target->payloadSize > 0) {
        ret = memcpy_s((void *)(uintptr_t)(list.addr + RS_PING_PAYLOAD_HEADER_RESV_CUSTOM),
            (list.len - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM), (void *)target->payloadBuffer, target->payloadSize);
        CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer payload_size:%u list.len:%u failed, ret:%d",
            target->payloadSize, (list.len - RS_PING_PAYLOAD_HEADER_RESV_CUSTOM), ret), -ESAFEFUNC);
    }
    list.len = RS_PING_PAYLOAD_HEADER_RESV_CUSTOM + target->payloadSize;

    rs_ping_jetty_build_up_wr(ping_cb, target, &list, &wr);

    // record timestamp t1
    (void)gettimeofday(&timestamp, NULL);
    header->timestamp.tvSec1 = (uint64_t)timestamp.tv_sec;
    header->timestamp.tvUsec1 = (uint64_t)timestamp.tv_usec;
    header->taskId = ping_cb->taskId;
    header->magic = 0x55AA;

    ret = rs_urma_post_jetty_send_wr(ping_cb->ping_jetty.jetty, &wr, &bad_wr);
    if (ret != 0) {
        hccp_err("rs_urma_post_jetty_send_wr jetty_id:%u failed, ret:%d", server_jetty_key.id, ret);
        RS_PTHREAD_MUTEX_LOCK(&target->tripMutex);
        target->state = RS_PING_PONG_TARGET_ERROR;
        RS_PTHREAD_MUTEX_ULOCK(&target->tripMutex);
    }
    return ret;
}

STATIC int rs_ping_urma_poll_scq(struct RsPingCtxCb *ping_cb, struct RsPingTargetInfo *target)
{
    urma_cr_t send_cr = {0};
    int polled_cnt;

    polled_cnt = rs_urma_poll_jfc(ping_cb->ping_jetty.send_jfc.jfc, 1, &send_cr);
    if (polled_cnt != 1) {
        hccp_err("uuid:0x%llx rs_urma_poll_jfc polled_cnt:%d", target->uuid, polled_cnt);
        target->state = RS_PING_PONG_TARGET_ERROR;
        return -ENODATA;
    }
    if (send_cr.status != URMA_CR_SUCCESS) {
        target->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("wr_id:0x%llx error cqe cr_status(%d)", send_cr.user_ctx, send_cr.status);
        return -EOPENSRC;
    }
    return 0;
}

STATIC int rs_ping_urma_poll_rcq(struct RsPingCtxCb *ping_cb, int *polled_cnt, struct timeval *timestamp2)
{
    urma_jfc_t *ev_jfc = NULL;
    uint32_t ack_cnt = 1;
    int wait_cnt;

    // record timestamp t2
    (void)gettimeofday(timestamp2, NULL);

    wait_cnt = rs_urma_wait_jfc(ping_cb->ping_jetty.jfce, 1, 0, &ev_jfc);
    if (wait_cnt == 0) {
        return -EAGAIN;
    }
    if (wait_cnt != 1) {
        hccp_err("urma_wait_jfc failed, ret:%d", wait_cnt);
        return -EOPENSRC;
    }
    rs_urma_ack_jfc((urma_jfc_t **)&ev_jfc, &ack_cnt, 1);

    if (ev_jfc != ping_cb->ping_jetty.recv_jfc.jfc) {
        hccp_err("urma_wait_jfc returned unknown jfc");
        return -EOPENSRC;
    }
    ping_cb->ping_jetty.recv_jfc.num_events++;

    *polled_cnt = rs_urma_poll_jfc(ev_jfc, ping_cb->ping_jetty.recv_jfc.max_recv_wc_num, g_ping_jetty_recv_cr);
    CHK_PRT_RETURN(*polled_cnt > ping_cb->ping_jetty.recv_jfc.max_recv_wc_num || *polled_cnt < 0,
        hccp_err("urma_poll_jfc failed, ret:%d", *polled_cnt), -EOPENSRC);

    return 0;
}

STATIC int rs_ping_common_poll_send_jfc(struct rs_ping_local_jetty_cb *jetty_cb)
{
    urma_cr_t cr = {0};
    int polled_cnt;

    polled_cnt = rs_urma_poll_jfc(jetty_cb->send_jfc.jfc, 1, &cr);
    if (polled_cnt < 0) {
        hccp_warn("urma_poll_jfc unsuccessful, polled_cnt:%d", polled_cnt);
    } else if (polled_cnt > 0) {
        if (cr.status != URMA_CR_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe status(%d)", cr.user_ctx, cr.status);
            return -EOPENSRC;
        }
    }

    return 0;
}

STATIC int rs_pong_jetty_find_target_node(struct RsPingCtxCb *ping_cb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    struct RsPongTargetInfo *target_next = NULL;
    struct RsPongTargetInfo *target_curr = NULL;
    urma_jetty_id_t target_jetty_id = {0};
    urma_eid_t target_eid = {0};

    rs_get_jetty_info(target, &target_jetty_id, &target_eid);

    RS_CHECK_POINTER_NULL_WITH_RET(ping_cb);
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pongMutex);
    RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &ping_cb->pongList, list, struct RsPongTargetInfo);
    for (; (&target_curr->list) != &ping_cb->pongList;
        target_curr = target_next, target_next = list_entry(target_next->list.next, struct RsPongTargetInfo, list)) {
        if (rs_ping_common_compare_ub_info(&target_curr->qpInfo, target)) {
            *node = target_curr;
            RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pongMutex);
            return 0;
        }
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pongMutex);

    hccp_info("pong target node for jetty_id:%u eid:%016llX:%016llX not found", target_jetty_id.id,
        target_eid.in6.subnet_prefix, target_eid.in6.interface_id);
    return -ENODEV;
}

STATIC int rs_pong_jetty_find_alloc_target_node(struct RsPingCtxCb *ping_cb, struct PingQpInfo *target,
    struct RsPongTargetInfo **node)
{
    struct RsPongTargetInfo *target_info = NULL;
    int ret;

    ret = rs_pong_jetty_find_target_node(ping_cb, target, node);
    if (ret == 0 && (*node)->state == RS_PING_PONG_TARGET_READY) {
        return 0;
    } else if (ret == 0) {
        target_info = *node;
        hccp_info("delete pong target uuid:0x%llx state:%d, realloc again", target_info->uuid, target_info->state);
        RsListDel(&target_info->list);
        if (target_info->import_tjetty != NULL) {
            (void)rs_urma_unimport_jetty(target_info->import_tjetty);
            target_info->import_tjetty = NULL;
        }
        free(target_info);
        target_info = NULL;
    }

    target_info = (struct RsPongTargetInfo *)calloc(1, sizeof(struct RsPongTargetInfo));
    CHK_PRT_RETURN(target_info == NULL, hccp_err("calloc target_info failed! errno:%d", errno), -ENOMEM);

    (void)memcpy_s(&target_info->qpInfo, sizeof(struct PingQpInfo), target, sizeof(struct PingQpInfo));

    ret = rs_ping_common_import_jetty(ping_cb->udev_cb.urma_ctx, target, &target_info->import_tjetty);
    if (ret != 0) {
        hccp_err("rs_pong_import_jetty failed, ret:%d", ret);
        goto free_target_info;
    }

    target_info->state = RS_PING_PONG_TARGET_READY;
    *node = target_info;

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pongMutex);
    target_info->uuid = (uint64_t)ping_cb->pongNum << 32U;
    RsListAddTail(&target_info->list, &ping_cb->pongList);
    ping_cb->pongNum++;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pongMutex);

    return 0;

free_target_info:
    free(target_info);
    return ret;
}

STATIC int rs_pong_jetty_post_send(struct RsPingCtxCb *ping_cb, urma_cr_t *cr, struct timeval *timestamp2)
{
    struct RsPongTargetInfo *target_info = NULL;
    struct RsPingPayloadHeader *header = NULL;
    struct timeval timestamp3 = {0};
    urma_sge_t recv_list = {0};
    urma_sge_t send_list = {0};
    urma_jfs_wr_t *bad_wr = NULL;
    urma_jfs_wr_t wr = {0};
    uint32_t recv_sge_idx;
    uint32_t send_sge_idx;
    int ret = 0;

    // poll send jfc
    (void)rs_ping_common_poll_send_jfc(&ping_cb->pong_jetty);

    // handle detect packet & send response packet
    recv_sge_idx = (uint32_t)cr->user_ctx;
    if (recv_sge_idx > ping_cb->ping_jetty.recv_seg_cb.sge_num) {
        hccp_err("param err recv_sge_idx:%u > sge_num:%u", recv_sge_idx, ping_cb->ping_jetty.recv_seg_cb.sge_num);
        return -EIO;
    }
    (void)memcpy_s(&recv_list, sizeof(urma_sge_t),
        &ping_cb->ping_jetty.recv_seg_cb.sge_list[recv_sge_idx], sizeof(urma_sge_t));

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_jetty.send_seg_cb.mutex);
    send_sge_idx = ping_cb->pong_jetty.send_seg_cb.sge_idx;
    (void)memcpy_s(&send_list, sizeof(urma_sge_t),
        &ping_cb->pong_jetty.send_seg_cb.sge_list[send_sge_idx], sizeof(urma_sge_t));
    ping_cb->pong_jetty.send_seg_cb.sge_idx = (send_sge_idx + 1) % ping_cb->pong_jetty.send_seg_cb.sge_num;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_jetty.send_seg_cb.mutex);

    ret = memcpy_s((void *)(uintptr_t)send_list.addr, send_list.len,
        (void *)(uintptr_t)recv_list.addr, cr->completion_len);
    CHK_PRT_RETURN(ret != 0, hccp_err("memcpy_s buffer cr->completion_len:%u send_list.length:%u failed, ret:%d",
        cr->completion_len, send_list.len, ret), -ESAFEFUNC);
    send_list.len = cr->completion_len;
    header = (struct RsPingPayloadHeader *)(uintptr_t)send_list.addr;
    header->type = RS_PING_TYPE_URMA_RESPONSE;

    ret = rs_pong_jetty_find_alloc_target_node(ping_cb, &header->server, &target_info);
    if (ret != 0) {
        hccp_err("rs_pong_jetty_find_alloc_target_node failed, ret:%d", ret);
        return ret;
    }

    wr.opcode = URMA_OPC_SEND;
    wr.flag.bs.complete_enable = 1;
    wr.tjetty = target_info->import_tjetty;
    wr.user_ctx = target_info->uuid;
    wr.send.src.sge = &send_list;
    wr.send.src.num_sge = 1;
    wr.send.imm_data = 0;
    wr.next = NULL;

    // record timestamp t3
    (void)gettimeofday(&timestamp3, NULL);
    header->timestamp.tvSec2 = (uint64_t)timestamp2->tv_sec;
    header->timestamp.tvUsec2 = (uint64_t)timestamp2->tv_usec;
    header->timestamp.tvSec3 = (uint64_t)timestamp3.tv_sec;
    header->timestamp.tvUsec3 = (uint64_t)timestamp3.tv_usec;
    header->magic = 0xAA55;

    ret = rs_urma_post_jetty_send_wr(ping_cb->pong_jetty.jetty, &wr, &bad_wr);
    if (ret != 0) {
        target_info->state = RS_PING_PONG_TARGET_ERROR;
        hccp_err("urma_post_jetty_send_wr failed, ret:%d", ret);
        return ret;
    }

    return ret;
}

STATIC void rs_pong_urma_handle_send(struct RsPingCtxCb *ping_cb, int polled_cnt, struct timeval *timestamp2)
{
    urma_cr_t *cr = NULL;
    int ret, i;

    cr = g_ping_jetty_recv_cr;
    for (i = 0; i < polled_cnt; i++) {
        if (cr[i].status != URMA_CR_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe status(%d)", cr[i].user_ctx, cr[i].status);
            continue;
        }

        ret = rs_pong_jetty_post_send(ping_cb, &cr[i], timestamp2);
        if (ret != 0) {
            hccp_err("rs_pong_jetty_post_send failed, wr_id:0x%llx", cr[i].user_ctx);
            continue;
        }

        ret = rs_ping_common_jfr_post_recv(&ping_cb->ping_jetty);
        if (ret != 0) {
            hccp_err("rs_ping_common_jfr_post_recv failed, ret:%d", ret);
            continue;
        }
    }

    ret = rs_urma_rearm_jfc(ping_cb->ping_jetty.recv_jfc.jfc, false);
    if (ret != 0) {
        hccp_err("urma_rearm_jfc failed, ret:%d", ret);
    }

    return;
}

STATIC int rs_pong_jetty_resolve_response_packet(struct RsPingCtxCb *ping_cb, uint32_t sge_idx, struct timeval *timestamp4)
{
    struct RsPingTargetInfo *target_info = NULL;
    struct RsPingPayloadHeader *header = NULL;
    urma_jetty_id_t target_jetty_id = {0};
    urma_eid_t target_eid = {0};
    urma_sge_t *recv_list = NULL;
    uint32_t rtt;
    int ret;

    recv_list = &ping_cb->pong_jetty.recv_seg_cb.sge_list[sge_idx];
    header = (struct RsPingPayloadHeader *)(uintptr_t)(recv_list->addr);
    if (header->taskId != ping_cb->taskId) {
        hccp_warn("drop received packet, recv_taskId:%u, curr_taskId:%u", header->taskId, ping_cb->taskId);
        return 0;
    }
    rs_get_jetty_info(&header->target, &target_jetty_id, &target_eid);

    header->timestamp.tvSec4 = (uint64_t)timestamp4->tv_sec;
    header->timestamp.tvUsec4 = (uint64_t)timestamp4->tv_usec;
    rtt = RsPingGetTripTime(&header->timestamp);
    ret = rs_ping_urma_find_target_node(ping_cb, &header->target, &target_info);
    if (ret != 0) {
        hccp_err("rs_ping_urma_find_target_node failed, ret:%d jetty_id:%u eid:%016llX:%016llX rtt:%u", ret,
            target_jetty_id.id, target_eid.in6.subnet_prefix, target_eid.in6.interface_id, rtt);
        return ret;
    }

    (void)memset_s((void *)header, RS_PING_PAYLOAD_HEADER_MASK_SIZE, 0, RS_PING_PAYLOAD_HEADER_MASK_SIZE);
    RS_PTHREAD_MUTEX_LOCK(&target_info->tripMutex);
    target_info->resultSummary.recvCnt++;
    target_info->resultSummary.taskId = header->taskId;
    // rtt timeout, increase timeoutCnt
    if ((target_info->resultSummary.taskAttr.timeoutInterval * RS_PING_MSEC_TO_USEC) < rtt) {
        target_info->resultSummary.timeoutCnt++;
        hccp_dbg("recvCnt:%u timeoutInterval:%u rtt:%u timeoutCnt:%u", target_info->resultSummary.recvCnt,
            target_info->resultSummary.taskAttr.timeoutInterval, rtt, target_info->resultSummary.timeoutCnt);
        RS_PTHREAD_MUTEX_ULOCK(&target_info->tripMutex);
        return 0;
    }

    // handle rtt_min, rtt_max, rtt_avg
    if (target_info->resultSummary.rttMin > rtt) {
        target_info->resultSummary.rttMin = rtt;
    }
    if (target_info->resultSummary.rttMax < rtt) {
        target_info->resultSummary.rttMax = rtt;
    }
    if (target_info->resultSummary.rttAvg == 0) {
        target_info->resultSummary.rttAvg = rtt;
    }
    target_info->resultSummary.rttAvg = (target_info->resultSummary.rttAvg + rtt) / 2U;
    RS_PTHREAD_MUTEX_ULOCK(&target_info->tripMutex);
    return 0;
}

STATIC void rs_pong_urma_poll_rcq(struct RsPingCtxCb *ping_cb)
{
    struct timeval timestamp = {0};
    urma_jfc_t *ev_jfc = NULL;
    uint32_t recv_sge_idx;
    urma_cr_t *cr = NULL;
    uint32_t ack_cnt = 1;
    int polled_cnt, i;
    int wait_cnt;
    int ret;

    // record timestamp t4
    (void)gettimeofday(&timestamp, NULL);

    wait_cnt = rs_urma_wait_jfc(ping_cb->pong_jetty.jfce, 1, 0, &ev_jfc);
    if (wait_cnt == 0) {
        return;
    }
    if (wait_cnt != 1) {
        hccp_err("urma_wait_jfc failed, ret:%d", wait_cnt);
        return;
    }
    rs_urma_ack_jfc((urma_jfc_t **)&ev_jfc, &ack_cnt, 1);

    if (ev_jfc != ping_cb->pong_jetty.recv_jfc.jfc) {
        hccp_err("urma_wait_jfc returned unknown jfc");
        return;
    }
    ping_cb->pong_jetty.recv_jfc.num_events++;

    polled_cnt = rs_urma_poll_jfc(ev_jfc, ping_cb->pong_jetty.recv_jfc.max_recv_wc_num, g_pong_jetty_recv_cr);
    if (polled_cnt > ping_cb->pong_jetty.recv_jfc.max_recv_wc_num || polled_cnt < 0) {
        hccp_err("urma_poll_jfc failed, ret:%d", polled_cnt);
        goto rearm_jfc;
    }

    cr = g_pong_jetty_recv_cr;
    for (i = 0; i < polled_cnt; i++) {
        if (cr[i].status != URMA_CR_SUCCESS) {
            hccp_err("wr_id:0x%llx error cqe status(%d)", cr[i].user_ctx, cr[i].status);
            continue;
        }
        recv_sge_idx = (uint32_t)cr[i].user_ctx;
        if (recv_sge_idx >= ping_cb->pong_jetty.recv_seg_cb.sge_num) {
            hccp_err("param err recv_sge_idx:%u > sge_num:%u", recv_sge_idx, ping_cb->pong_jetty.recv_seg_cb.sge_num);
            continue;
        }

        // handle response packet result
        ret = rs_pong_jetty_resolve_response_packet(ping_cb, recv_sge_idx, &timestamp);
        if (ret != 0) {
            continue;
        }

        ret = rs_ping_common_jfr_post_recv(&ping_cb->pong_jetty);
        if (ret != 0) {
            continue;
        }
    }

rearm_jfc:
    ret = rs_urma_rearm_jfc(ev_jfc, false);
    if (ret != 0) {
        hccp_err("urma_rearm_jfc failed, ret:%d", ret);
    }

    return;
}

STATIC int rs_ping_urma_get_target_result(struct RsPingCtxCb *ping_cb, struct PingTargetCommInfo *target,
    struct PingResultInfo *result)
{
    struct RsPingTargetInfo *target_info = NULL;
    urma_jetty_id_t target_jetty_id = {0};
    urma_eid_t target_eid = {0};
    int ret;

    rs_get_jetty_info(&target->qpInfo, &target_jetty_id, &target_eid);

    ret = rs_ping_urma_find_target_node(ping_cb, &target->qpInfo, &target_info);
    if (ret != 0) {
        hccp_err("rs_ping_urma_find_target_node failed, ret:%d jetty_id:%u eid:%016llx:%016llx", ret,
            target_jetty_id.id, target_eid.in6.subnet_prefix, target_eid.in6.interface_id);
        return ret;
    }

    (void)memcpy_s(&result->summary, sizeof(struct PingResultSummary), &target_info->resultSummary,
        sizeof(struct PingResultSummary));
    if (target_info->state == RS_PING_PONG_TARGET_FINISH) {
        result->state = PING_RESULT_STATE_VALID;
    } else {
        result->state = PING_RESULT_STATE_INVALID;
    }
    hccp_dbg("eid:%016llx:%016llx jetty_id:%u, state:%d send_cnt:%u recv_cnt:%u timeout_cnt:%u rtt_min:%u rtt_max:%u "
        "rtt_avg:%u", target_eid.in6.subnet_prefix, target_eid.in6.interface_id, target_jetty_id.id, result->state,
        result->summary.sendCnt, result->summary.recvCnt, result->summary.timeoutCnt, result->summary.rttMin,
        result->summary.rttMax, result->summary.rttAvg);

    return 0;
}

STATIC void rs_ping_urma_free_target_node(struct RsPingCtxCb *ping_cb, struct RsPingTargetInfo *target_info)
{
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pingMutex);
    RsListDel(&target_info->list);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pingMutex);

    if (target_info->payloadSize > 0 && target_info->payloadBuffer != NULL) {
        free(target_info->payloadBuffer);
        target_info->payloadBuffer = NULL;
    }

    if (target_info->import_tjetty != NULL) {
        (void)rs_urma_unimport_jetty(target_info->import_tjetty);
    }
    return;
}

STATIC void rs_ping_pong_jetty_del_target_list(struct RsPingCtxCb *ping_cb)
{
    struct RsPongTargetInfo *pong_next = NULL;
    struct RsPingTargetInfo *ping_next = NULL;
    struct RsPongTargetInfo *pong_curr = NULL;
    struct RsPingTargetInfo *ping_curr = NULL;

    // del ping_list
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pingMutex);
    RS_LIST_GET_HEAD_ENTRY(ping_curr, ping_next, &ping_cb->pingList, list, struct RsPingTargetInfo);
    for (; (&ping_curr->list) != &ping_cb->pingList;
        ping_curr = ping_next, ping_next = list_entry(ping_next->list.next, struct RsPingTargetInfo, list)) {
        RsListDel(&ping_curr->list);
        if (ping_curr->payloadSize > 0 && ping_curr->payloadBuffer != NULL) {
            free(ping_curr->payloadBuffer);
            ping_curr->payloadBuffer = NULL;
        }
        if (ping_curr->import_tjetty != NULL) {
            (void)rs_urma_unimport_jetty(ping_curr->import_tjetty);
        }
        (void)pthread_mutex_destroy(&ping_curr->tripMutex);
        free(ping_curr);
        ping_curr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pingMutex);

    // del pong_list
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pongMutex);
    RS_LIST_GET_HEAD_ENTRY(pong_curr, pong_next, &ping_cb->pongList, list, struct RsPongTargetInfo);
    for (; (&pong_curr->list) != &ping_cb->pongList;
        pong_curr = pong_next, pong_next = list_entry(pong_next->list.next, struct RsPongTargetInfo, list)) {
        RsListDel(&pong_curr->list);
        if (pong_curr->import_tjetty != NULL) {
            (void)rs_urma_unimport_jetty(pong_curr->import_tjetty);
        }
        free(pong_curr);
        pong_curr = NULL;
    }
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pongMutex);
}

STATIC void rs_ping_urma_ping_cb_deinit(unsigned int phy_id, struct RsPingCtxCb *ping_cb)
{
    struct rs_cb *rscb = NULL;
    int ret;

    ret = RsGetRsCb(phy_id, &rscb);
    if (ret != 0) {
        hccp_err("RsGetRsCb failed, phy_id[%u] invalid, ret %d", phy_id, ret);
        return;
    }

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pingMutex);
    ping_cb->taskStatus = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pingMutex);

    rs_ping_pong_jetty_del_target_list(ping_cb);

    rs_ping_common_deinit_local_jetty_buffer(ping_cb);
    rs_ping_common_deinit_local_jetty(rscb, ping_cb, &ping_cb->pong_jetty);
    rs_ping_common_deinit_local_jetty(rscb, ping_cb, &ping_cb->ping_jetty);

    (void)rs_urma_delete_context(ping_cb->udev_cb.urma_ctx);
    ping_cb->udev_cb.urma_ctx = NULL;
}

// ping_pong_dfx function
STATIC void rs_ping_urma_add_target_success(struct PingTargetInfo *target, struct RsPingTargetInfo *target_info)
{
    urma_jetty_id_t jetty_id = {0};
    rs_get_jetty_info(&target_info->qpInfo, &jetty_id, NULL);
    hccp_info("target eid:%016llx:%016llx payload_size:%u add success, jetty_id:%u uuid:0x%llx",
        target->remoteInfo.eid.in6.subnet_prefix, target->remoteInfo.eid.in6.interface_id,
        target->payload.size, jetty_id.id, target_info->uuid);
}

STATIC void rs_ping_urma_ping_cb_init_success(unsigned int phy_id, struct PingInitAttr *attr, unsigned int rdev_index)
{
    hccp_run_info("ping_cb init success, phy_id:%u, eid:%016llx:%016llx, rdev_index:%u",
        phy_id, attr->dev.ub.eid.in6.subnet_prefix, attr->dev.ub.eid.in6.interface_id, rdev_index);
}

STATIC void rs_ping_urma_cannot_find_target_node(unsigned int i, int ret, struct PingTargetCommInfo target,
    unsigned int phy_id)
{
    urma_jetty_id_t jetty_id = {0};
    rs_get_jetty_info(&target.qpInfo, &jetty_id, NULL);

    hccp_err("rs_ping_urma_find_target_node i:%u failed, ret:%d eid:%016llx:%016llx jetty_id:%u phy_id:%u",i, ret,
        target.eid.in6.subnet_prefix, target.eid.in6.interface_id, jetty_id.id, phy_id);
}

struct RsPingPongOps g_rs_ping_urma_ops = {
    .checkPingFd          = rs_ping_urma_check_fd,
    .checkPongFd          = rs_pong_urma_check_fd,
    .initPingCb           = rs_ping_urma_ping_cb_init,
    .pingFindTargetNode  = rs_ping_urma_find_target_node,
    .pingAllocTargetNode = rs_ping_urma_alloc_target_node,
    .resetRecvBuffer      = rs_ping_urma_reset_recv_buffer,
    .pingPostSend         = rs_ping_urma_post_send,
    .pingPollScq          = rs_ping_urma_poll_scq,
    .pingPollRcq          = rs_ping_urma_poll_rcq,
    .pongHandleSend       = rs_pong_urma_handle_send,
    .pongPollRcq          = rs_pong_urma_poll_rcq,
    .getTargetResult      = rs_ping_urma_get_target_result,
    .pingFreeTargetNode  = rs_ping_urma_free_target_node,
    .deinitPingCb         = rs_ping_urma_ping_cb_deinit,
};

struct RsPingPongDfx g_rs_ping_urma_dfx = {
    .addTargetSuccess           = rs_ping_urma_add_target_success,
    .initPingCbSuccess         = rs_ping_urma_ping_cb_init_success,
    .pingCannotFindTargetNode = rs_ping_urma_cannot_find_target_node,
};

struct RsPingPongOps *rs_ping_urma_get_ops(void) {
    return &g_rs_ping_urma_ops;
}

struct RsPingPongDfx *rs_ping_urma_get_dfx(void) {
    return &g_rs_ping_urma_dfx;
}
