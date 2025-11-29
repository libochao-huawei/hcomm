/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <errno.h>
#include <sys/prctl.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "hccp_common.h"
#include "ra_rs_err.h"
#include "rs.h"
#include "ra_rs_err.h"
#include "rs_inner.h"
#include "rs_epoll.h"
#include "rs_socket.h"
#include "rs_ping_inner.h"
#include "rs_ping_roce.h"
#ifndef HNS_ROCE_LLT
#include "dlog_pub.h"
#endif
#include "rs_ping.h"

struct rs_pthread_info gPingThreadInfo = {0};

int RsEpollEventPingHandle(struct rs_cb *rsCb, int fd)
{
    struct rs_ping_ctx_cb *pingCb = &rsCb->ping_cb;
    struct timeval timestamp2 = {0};
    int polledCnt = 0;
    int ret = -ENODEV;

    // thread not running, no need to handle ping
    if (pingCb->thread_status != RS_PING_THREAD_RUNNING || pingCb->ping_pong_ops == NULL) {
        return ret;
    }

    // ping rq: receive detect packet
    if (pingCb->ping_pong_ops->check_ping_fd(pingCb, fd)) {
        RS_PTHREAD_MUTEX_LOCK(&rsCb->ping_cb.dev_mutex);
        if (pingCb->init_cnt == 0) {
            goto free_dev_mutex;
        }
        ret = pingCb->ping_pong_ops->ping_poll_rcq(pingCb, &polledCnt, &timestamp2);
        if (ret != 0) {
            hccp_err("ping_poll_rcq failed, polledCnt:%d", polledCnt);
            goto free_dev_mutex;
        }
        pingCb->ping_pong_ops->pong_handle_send(pingCb, polledCnt, &timestamp2);
        goto free_dev_mutex;
    }

    // pong rq: receive response packet
    if (pingCb->ping_pong_ops->check_pong_fd(pingCb, fd)) {
        RS_PTHREAD_MUTEX_LOCK(&rsCb->ping_cb.dev_mutex);
        if (pingCb->init_cnt == 0) {
            goto free_dev_mutex;
        }
        pingCb->ping_pong_ops->pong_poll_rcq(pingCb);
        ret = 0;
        goto free_dev_mutex;
    }

    return ret;

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->ping_cb.dev_mutex);
    return ret;
}

STATIC void *RsPingHandle(void *arg)
{
    struct rs_ping_target_info *targetNext = NULL;
    struct rs_ping_target_info *targetCurr = NULL;
    struct rs_cb *rsCb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_NULL(arg);

    hccp_info("<PING> thread begin! thread_id:%lu, pid:%d, ppid:%d", pthread_self(), getpid(), getppid());
    CHK_PRT_RETURN(pthread_detach(pthread_self()) != 0, hccp_err("pthread_detach failed! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ping");

    rsCb = (struct rs_cb *)arg;

    RsGetCurTime(&gPingThreadInfo.last_check_time);
    ret = strncpy_s((char *)gPingThreadInfo.pthread_name, sizeof(gPingThreadInfo.pthread_name),
        "ping_pthread", strlen("ping_pthread"));
    CHK_PRT_RETURN(ret != 0, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", gPingThreadInfo.pthread_name);
    while (1) {
        if (rsCb->ping_cb.thread_status != RS_PING_THREAD_RUNNING) {
            break;
        }

        RsHeartbeatAlivePrint(&gPingThreadInfo);
        if (rsCb->ping_cb.task_status != RS_PING_TASK_RUNNING || rsCb->ping_cb.task_attr.packet_cnt == 0) {
            usleep(RS_PING_PERIOD_TIME_USEC);
            continue;
        }
        if (RsListEmpty(&rsCb->ping_cb.ping_list)) {
            usleep(RS_PING_PERIOD_TIME_USEC);
            continue;
        }

        RS_LIST_GET_HEAD_ENTRY(targetCurr, targetNext, &rsCb->ping_cb.ping_list, list, struct rs_ping_target_info);
        for (; rsCb->ping_cb.task_status == RS_PING_TASK_RUNNING && (&targetCurr->list) != &rsCb->ping_cb.ping_list;
            targetCurr = targetNext,
            targetNext = list_entry(targetNext->list.next, struct rs_ping_target_info, list)) {
            if (targetCurr->state != RS_PING_PONG_TARGET_READY) {
                usleep(rsCb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);
                continue;
            }

            ret = rsCb->ping_cb.ping_pong_ops->ping_post_send(&rsCb->ping_cb, targetCurr); 
            if (ret != 0) {
                hccp_warn("ping_post_send unsuccessful, ret:%d", ret);
                usleep(rsCb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);
                continue;
            }

            if (rsCb->ping_cb.task_attr.packet_cnt == 1 && targetCurr->state == RS_PING_PONG_TARGET_READY) {
                targetCurr->state = RS_PING_PONG_TARGET_FINISH;
            }
            // make sure thread will exit
            usleep(rsCb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);

            // ping poll scq
            ret = rsCb->ping_cb.ping_pong_ops->ping_poll_scq(&rsCb->ping_cb, targetCurr);
            if (ret != 0) {
                continue;
            }
            targetCurr->result_summary.send_cnt++;
        }

        // update task attr & status
        rsCb->ping_cb.task_attr.packet_cnt--;
        if (rsCb->ping_cb.task_attr.packet_cnt == 0) {
            rsCb->ping_cb.task_status = RS_PING_TASK_RESET;
        }
    }

    RS_PTHREAD_MUTEX_LOCK(&rsCb->ping_cb.ping_mutex);
    rsCb->ping_cb.thread_status = RS_PING_THREAD_FINISH;
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->ping_cb.ping_mutex);
    hccp_info("<PING> QUIT thread_id:%lu, pid:%d", pthread_self(), getpid());
    return NULL;
}

STATIC int RsPingCbInitMutex(struct rs_ping_ctx_cb *pingCb)
{
    int ret;

    ret = pthread_mutex_init(&pingCb->ping_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init ping_mutex failed ret %d", ret);
        goto ping_mutex_init_failed;
    }
    ret = pthread_mutex_init(&pingCb->pong_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init pong_mutex failed ret %d", ret);
        goto pong_mutex_init_failed;
    }
    ret = pthread_mutex_init(&pingCb->dev_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init dev_mutex failed ret %d", ret);
        goto dev_mutex_init_failed;
    }

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    RS_INIT_LIST_HEAD(&pingCb->ping_list);
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);
    RS_PTHREAD_MUTEX_LOCK(&pingCb->pong_mutex);
    RS_INIT_LIST_HEAD(&pingCb->pong_list);
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->pong_mutex);

    return 0;

dev_mutex_init_failed:
    (void)pthread_mutex_destroy(&pingCb->pong_mutex);
pong_mutex_init_failed:
    (void)pthread_mutex_destroy(&pingCb->ping_mutex);
ping_mutex_init_failed:
    return -ESYSFUNC;
}

RS_ATTRI_VISI_DEF int RsPingHandleInit(unsigned int chipId, int hdcType, unsigned int whiteListStatus)
{
    struct rs_cb *rsCb = NULL;
    int ret;

    if (hdcType != HDC_SERVICE_TYPE_RDMA_V2 && whiteListStatus != WHITE_LIST_DISABLE) {
        return 0;
    }

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rs_cb failed, ret:%d, chipId:%u", ret, chipId), -ENODEV);

    ret = RsPingCbInitMutex(&rsCb->ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ping_cb_init_mutex failed, ret %d", ret), ret);

    rsCb->ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    ret = pthread_create(&rsCb->ping_cb.tid, NULL, (void *)RsPingHandle, (void *)rsCb);
    if (ret != 0) {
        hccp_err("Create pthread failed, ret(%d) ", ret);
        rsCb->ping_cb.thread_status = RS_PING_THREAD_RESET;
        (void)pthread_mutex_destroy(&rsCb->ping_cb.ping_mutex);
        (void)pthread_mutex_destroy(&rsCb->ping_cb.pong_mutex);
        return -ESYSFUNC;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int RsPingHandleDeinit(unsigned int chipId)
{
#define THREAD_STATUS_CHANGE_TIMEOUT 100
    struct rs_cb *rsCb = NULL;
    int ret;
    int i;

    ret = RsDev2rscb(chipId, &rsCb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rs_cb failed, ret:%d, chipId:%u", ret, chipId), -ENODEV);

    if (rsCb->ping_cb.thread_status != RS_PING_THREAD_RUNNING) {
        return 0;
    }

    RS_PTHREAD_MUTEX_LOCK(&rsCb->ping_cb.ping_mutex);
    rsCb->ping_cb.thread_status = RS_PING_THREAD_RESET;
    rsCb->ping_cb.task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&rsCb->ping_cb.ping_mutex);

    // wait thread change to finish running status, wait 100 times(total cost: 1s) until timeout
    for (i = 0; i < THREAD_STATUS_CHANGE_TIMEOUT && rsCb->ping_cb.thread_status != RS_PING_THREAD_FINISH; i++) {
        usleep(RS_PING_PERIOD_TIME_USEC);
    }

    // thread not in finish running status, report timeout
    if (rsCb->ping_cb.thread_status != RS_PING_THREAD_FINISH) {
        hccp_run_info("<PING> wait thread tid:%lu finish running timeout, thread status:%d",
            rsCb->ping_cb.tid, rsCb->ping_cb.thread_status);
    }

    (void)pthread_mutex_destroy(&rsCb->ping_cb.ping_mutex);
    (void)pthread_mutex_destroy(&rsCb->ping_cb.pong_mutex);
    (void)pthread_mutex_destroy(&rsCb->ping_cb.dev_mutex);
    return 0;
}

STATIC int RsPingInitProtocolOps(struct rs_ping_ctx_cb *pingCb, enum protocol_type protocol)
{
    pingCb->protocol = protocol;

    switch (protocol) {
        case PROTOCOL_RDMA:
            pingCb->ping_pong_ops = RsPingRoceGetOps();
            pingCb->ping_pong_dfx = RsPingRoceGetDfx();
            break;
        default:
            hccp_err("unsupported protocol:%u", protocol);
            return -EINVAL;
    }

    if (pingCb->ping_pong_ops == NULL || pingCb->ping_pong_ops->init_ping_cb == NULL || pingCb->ping_pong_dfx == NULL) {
        hccp_err("ping_cb->ping_pong_ops or init_ping_cb or ping_cb->ping_pong_dfx is NULL, protocol:%u", protocol);
        return -ENOTSUPP;
    }
    return 0;
}

RS_ATTRI_VISI_DEF int RsPingInit(struct ping_init_attr *attr, struct ping_init_info *info, unsigned int *devIndex)
{
    struct rs_ping_ctx_cb *pingCb = NULL;
    struct rs_cb *rscb = NULL;
    unsigned int phyId;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL || devIndex == NULL,
        hccp_err("param error, attr or info or devIndex is NULL"), -EINVAL);

    phyId = (attr->protocol == PROTOCOL_RDMA) ? attr->dev.rdma.phy_id : UINT_MAX;
    ret = RsGetRsCb(phyId, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId[%u] invalid, ret %d", phyId, ret), ret);

    pingCb = &rscb->ping_cb;
    RS_PTHREAD_MUTEX_LOCK(&pingCb->dev_mutex);
    if (rscb->ping_cb.init_cnt != 0) {
        hccp_err("init_cnt:%u != 0", rscb->ping_cb.init_cnt);
        ret = -EEXIST;
        goto free_dev_mutex;
    }

    ret = rsGetLocalDevIDByHostDevID(phyId, &pingCb->logic_devid);
    if (ret != 0) {
        hccp_err("rsGetLocalDevIDByHostDevID failed, phyId(%u), ret(%d)", phyId, ret);
        goto free_dev_mutex;
    }

#ifdef CUSTOM_INTERFACE
    // setup sharemem for pingmesh
    ret = RsSetupSharemem(rscb, false, phyId);
    if (ret != 0) {
        hccp_err("rs_setup_sharemem failed, phyId(%u), ret(%d)", phyId, ret);
        goto free_dev_mutex;
    }
#endif

    ret = RsPingInitProtocolOps(pingCb, attr->protocol);
    if (ret != 0) {
        hccp_err("rs_ping_init_protocol_ops failed, phyId:%u ret:%d", phyId, ret);
        goto free_dev_mutex;
    }

    ret = pingCb->ping_pong_ops->init_ping_cb(phyId, attr, info, devIndex, pingCb);
    if (ret != 0) {
        hccp_err("init_ping_cb failed, phyId:%u ret:%d", phyId, ret);
        goto free_dev_mutex;
    }

    pingCb->init_cnt++;
    pingCb->ping_pong_dfx->init_ping_cb_success(phyId, attr, *devIndex);

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->dev_mutex);
    return ret;
}

STATIC int RsGetPingCb(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **pingCb)
{
    unsigned int phyId = rdev->phy_id;
    struct rs_cb *rsCb = NULL;
    int ret;

    ret = RsGetRsCb(phyId, &rsCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phyId[%u] invalid, ret %d", phyId, ret), ret);

    CHK_PRT_RETURN(rdev->dev_index != rsCb->ping_cb.dev_index,
        hccp_err("param error, devIndex:%u != pingCb.dev_index:%u", rdev->dev_index, rsCb->ping_cb.dev_index),
        -ENODEV);

    CHK_PRT_RETURN(rsCb->ping_cb.thread_status != RS_PING_THREAD_RUNNING,
        hccp_err("thread_status:%d is not running", rsCb->ping_cb.thread_status), -ESRCH);

    *pingCb = &rsCb->ping_cb;

    return 0;
}

RS_ATTRI_VISI_DEF int RsPingTargetAdd(struct ra_rs_dev_info *rdev, struct ping_target_info *target)
{
    struct rs_ping_target_info *targetInfo = NULL;
    struct rs_ping_ctx_cb *pingCb = NULL;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || target == NULL, hccp_err("param error, rdev is NULL or target is NULL"), -EINVAL);
    CHK_PRT_RETURN(target->payload.size > PING_USER_PAYLOAD_MAX_SIZE,
        hccp_err("param error, size:%u > max_size:%u", target->payload.size, PING_USER_PAYLOAD_MAX_SIZE), -EINVAL);

    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    if (pingCb->task_status != RS_PING_TASK_RESET) {
        hccp_err("task_status:%d disallow to add target phy_id:%u", pingCb->task_status, rdev->phy_id);
        return -EEXIST;
    }

    ret = pingCb->ping_pong_ops->ping_find_target_node(pingCb, &target->remote_info.qp_info, &targetInfo);
    if (ret == 0) {
        hccp_info("target node exist! phy_id:%u", rdev->phy_id);
        ret = -EEXIST;
        goto out;
    }

    ret = pingCb->ping_pong_ops->ping_alloc_target_node(pingCb, target, &targetInfo);
    if (ret != 0) {
        hccp_err("rs_ping_alloc_target_node failed, ret:%d phyId:%u", ret, rdev->phy_id);
        return ret;
    }

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    targetInfo->uuid = (uint64_t)pingCb->ping_num;
    RsListAddTail(&targetInfo->list, &pingCb->ping_list);
    pingCb->ping_num++;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

out:
    pingCb->ping_pong_dfx->add_target_success(target, targetInfo);
    return ret;
}

RS_ATTRI_VISI_DEF int RsPingTaskStart(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr)
{
    struct rs_ping_target_info *targetNext = NULL;
    struct rs_ping_target_info *targetCurr = NULL;
    struct rs_ping_ctx_cb *pingCb = NULL;
    unsigned int targetCnt = 0;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || attr == NULL, hccp_err("param error, rdev is NULL or attr is NULL"), -EINVAL);
    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    if (pingCb->task_status != RS_PING_TASK_RESET) {
        hccp_warn("task_status:%d disallow to start ping task, phyId:%u", pingCb->task_status, rdev->phy_id);
        return -EEXIST;
    }
    CHK_PRT_RETURN(attr->packet_cnt == 0 || attr->packet_interval == 0 || attr->timeout_interval == 0,
        hccp_err("param error, packet_cnt:%u or packet_interval:%u or timeout_interval:%u is 0",
        attr->packet_cnt, attr->packet_interval, attr->timeout_interval), -EINVAL);

    pingCb->ping_pong_ops->reset_recv_buffer(pingCb);

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    pingCb->task_id++;
    (void)memcpy_s(&pingCb->task_attr, sizeof(struct ping_task_attr), attr, sizeof(struct ping_task_attr));
    RS_LIST_GET_HEAD_ENTRY(targetCurr, targetNext, &pingCb->ping_list, list, struct rs_ping_target_info);
    for(; (&targetCurr->list) != &pingCb->ping_list;
        targetCurr = targetNext, targetNext = list_entry(targetNext->list.next, struct rs_ping_target_info, list)) {
        (void)memset_s(&targetCurr->result_summary, sizeof(struct ping_result_summary),
            0, sizeof(struct ping_result_summary));
        (void)memcpy_s(&targetCurr->result_summary.task_attr, sizeof(struct ping_task_attr), attr,
            sizeof(struct ping_task_attr));
        targetCurr->result_summary.rtt_min = ~0;
        targetCurr->result_summary.task_id = pingCb->task_id;
        targetCurr->state = RS_PING_PONG_TARGET_READY;
        targetCnt++;
    }

    pingCb->task_status = RS_PING_TASK_RUNNING;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    hccp_info("target_cnt:%u packet_cnt:%u packet_interval:%u timeout_interval:%u task_id:%u start success", targetCnt,
        attr->packet_cnt, attr->packet_interval, attr->timeout_interval, pingCb->task_id);
    return 0;
}

RS_ATTRI_VISI_DEF int RsPingGetResults(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num, struct ping_result_info result[])
{
    struct rs_ping_ctx_cb *pingCb = NULL;
    unsigned int expectedNum;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || num == NULL, hccp_err("param error, rdev is NULL or num is NULL"), -EINVAL);
    expectedNum = *num;
    *num = 0;
    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    // caller needs to retry, degrade log level
    if (pingCb->task_status == RS_PING_TASK_RUNNING) {
        hccp_warn("task_status:%d disallow to get ping results phy_id:%u", pingCb->task_status, rdev->phy_id);
        return -EAGAIN;
    }

    for (i = 0; i < expectedNum; i++) {
        ret = pingCb->ping_pong_ops->get_target_result(pingCb, &target[i], &result[i]);
        if (ret != 0) {
            hccp_err("rs_ping_get_target_result node i:%d failed phy_id:%u", i, rdev->phy_id);
            i = (i > 0) ? (i - 1U) : 0;
            goto out;
        }
    }

out:
    *num = i;
    return ret;
}

RS_ATTRI_VISI_DEF int RsPingTaskStop(struct ra_rs_dev_info *rdev)
{
    struct rs_ping_ctx_cb *pingCb = NULL;
    int ret;

    CHK_PRT_RETURN(rdev == NULL, hccp_err("param error, rdev is NULL"), -EINVAL);
    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    hccp_info("task_status:%d modify to %d, phyId:%u", pingCb->task_status, RS_PING_TASK_RESET, rdev->phy_id);

    RS_PTHREAD_MUTEX_LOCK(&pingCb->ping_mutex);
    pingCb->task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->ping_mutex);

    return 0;
}

RS_ATTRI_VISI_DEF int RsPingTargetDel(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num)
{
    struct rs_ping_target_info *targetInfo = NULL;
    struct rs_ping_ctx_cb *pingCb = NULL;
    unsigned int expectedNum;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || target == NULL || num == NULL,
        hccp_err("param error, rdev or target or num is NULL"), -EINVAL);
    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    if (pingCb->task_status != RS_PING_TASK_RESET) {
        hccp_err("task_status:%d disallow to delete target phy_id:%u", pingCb->task_status, rdev->phy_id);
        return -EEXIST;
    }

    expectedNum = *num;
    for (i = 0; i < expectedNum; i++) {
        ret = pingCb->ping_pong_ops->ping_find_target_node(pingCb, &target[i].qp_info, &targetInfo);
        if (ret != 0) {
            pingCb->ping_pong_dfx->ping_cannot_find_target_node(i, ret, target[i], rdev->phy_id);
            goto out;
        }

        pingCb->ping_pong_ops->ping_free_target_node(pingCb, targetInfo);
        (void)pthread_mutex_destroy(&targetInfo->trip_mutex);
        free(targetInfo);
        targetInfo = NULL;
    }

out:
    *num = i;
    return ret;
}

RS_ATTRI_VISI_DEF int RsPingDeinit(struct ra_rs_dev_info *rdev)
{
    struct rs_ping_ctx_cb *pingCb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(rdev == NULL, hccp_err("param error, rdev is NULL"), -EINVAL);
    ret = RsGetPingCb(rdev, &pingCb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phyId:%u", ret, rdev->phy_id), ret);

    RS_PTHREAD_MUTEX_LOCK(&pingCb->dev_mutex);
    if (pingCb->init_cnt == 0) {
        hccp_err("init_cnt is 0");
        ret = -ENODEV;
        goto free_dev_mutex;
    }

    pingCb->ping_pong_ops->deinit_ping_cb(rdev->phy_id, pingCb);
    pingCb->init_cnt--;
    hccp_run_info("ping_cb deinit success, phyId:%u, devIndex:%u", rdev->phy_id, rdev->dev_index);

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&pingCb->dev_mutex);
    return ret;
}
