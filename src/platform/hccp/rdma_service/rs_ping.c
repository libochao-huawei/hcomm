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

struct rs_pthread_info g_ping_thread_info = {0};

int rs_epoll_event_ping_handle(struct rs_cb *rs_cb, int fd)
{
    struct rs_ping_ctx_cb *ping_cb = &rs_cb->ping_cb;
    struct timeval timestamp2 = {0};
    int polled_cnt = 0;
    int ret = -ENODEV;

    // thread not running, no need to handle ping
    if (ping_cb->thread_status != RS_PING_THREAD_RUNNING || ping_cb->ping_pong_ops == NULL) {
        return ret;
    }

    // ping rq: receive detect packet
    if (ping_cb->ping_pong_ops->check_ping_fd(ping_cb, fd)) {
        RS_PTHREAD_MUTEX_LOCK(&rs_cb->ping_cb.dev_mutex);
        if (ping_cb->init_cnt == 0) {
            goto free_dev_mutex;
        }
        ret = ping_cb->ping_pong_ops->ping_poll_rcq(ping_cb, &polled_cnt, &timestamp2);
        if (ret != 0) {
            hccp_err("ping_poll_rcq failed, polled_cnt:%d", polled_cnt);
            goto free_dev_mutex;
        }
        ping_cb->ping_pong_ops->pong_handle_send(ping_cb, polled_cnt, &timestamp2);
        goto free_dev_mutex;
    }

    // pong rq: receive response packet
    if (ping_cb->ping_pong_ops->check_pong_fd(ping_cb, fd)) {
        RS_PTHREAD_MUTEX_LOCK(&rs_cb->ping_cb.dev_mutex);
        if (ping_cb->init_cnt == 0) {
            goto free_dev_mutex;
        }
        ping_cb->ping_pong_ops->pong_poll_rcq(ping_cb);
        ret = 0;
        goto free_dev_mutex;
    }

    return ret;

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->ping_cb.dev_mutex);
    return ret;
}

STATIC void *rs_ping_handle(void *arg)
{
    struct rs_ping_target_info *target_next = NULL;
    struct rs_ping_target_info *target_curr = NULL;
    struct rs_cb *rs_cb = NULL;
    int ret;

    RS_CHECK_POINTER_NULL_RETURN_NULL(arg);

    hccp_info("<PING> thread begin! thread_id:%lu, pid:%d, ppid:%d", pthread_self(), getpid(), getppid());
    CHK_PRT_RETURN(pthread_detach(pthread_self()) != 0, hccp_err("pthread_detach failed! thread_id:%lu, errno:%d",
        pthread_self(), errno), NULL);

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_ping");

    rs_cb = (struct rs_cb *)arg;

    rs_get_cur_time(&g_ping_thread_info.last_check_time);
    ret = strncpy_s((char *)g_ping_thread_info.pthread_name, sizeof(g_ping_thread_info.pthread_name),
        "ping_pthread", strlen("ping_pthread"));
    CHK_PRT_RETURN(ret != 0, hccp_err("strncpy_s pthread name failed, ret[%d]", ret), NULL);

    hccp_run_info("pthread[%s] is alive!", g_ping_thread_info.pthread_name);
    while (1) {
        if (rs_cb->ping_cb.thread_status != RS_PING_THREAD_RUNNING) {
            break;
        }

        rs_heartbeat_alive_print(&g_ping_thread_info);
        if (rs_cb->ping_cb.task_status != RS_PING_TASK_RUNNING || rs_cb->ping_cb.task_attr.packet_cnt == 0) {
            usleep(RS_PING_PERIOD_TIME_USEC);
            continue;
        }
        if (rs_list_empty(&rs_cb->ping_cb.ping_list)) {
            usleep(RS_PING_PERIOD_TIME_USEC);
            continue;
        }

        RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &rs_cb->ping_cb.ping_list, list, struct rs_ping_target_info);
        for (; rs_cb->ping_cb.task_status == RS_PING_TASK_RUNNING && (&target_curr->list) != &rs_cb->ping_cb.ping_list;
            target_curr = target_next,
            target_next = list_entry(target_next->list.next, struct rs_ping_target_info, list)) {
            if (target_curr->state != RS_PING_PONG_TARGET_READY) {
                usleep(rs_cb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);
                continue;
            }

            ret = rs_cb->ping_cb.ping_pong_ops->ping_post_send(&rs_cb->ping_cb, target_curr); 
            if (ret != 0) {
                hccp_warn("ping_post_send unsuccessful, ret:%d", ret);
                usleep(rs_cb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);
                continue;
            }

            if (rs_cb->ping_cb.task_attr.packet_cnt == 1 && target_curr->state == RS_PING_PONG_TARGET_READY) {
                target_curr->state = RS_PING_PONG_TARGET_FINISH;
            }
            // make sure thread will exit
            usleep(rs_cb->ping_cb.task_attr.packet_interval * RS_PING_MSEC_TO_USEC);

            // ping poll scq
            ret = rs_cb->ping_cb.ping_pong_ops->ping_poll_scq(&rs_cb->ping_cb, target_curr);
            if (ret != 0) {
                continue;
            }
            target_curr->result_summary.send_cnt++;
        }

        // update task attr & status
        rs_cb->ping_cb.task_attr.packet_cnt--;
        if (rs_cb->ping_cb.task_attr.packet_cnt == 0) {
            rs_cb->ping_cb.task_status = RS_PING_TASK_RESET;
        }
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->ping_cb.ping_mutex);
    rs_cb->ping_cb.thread_status = RS_PING_THREAD_FINISH;
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->ping_cb.ping_mutex);
    hccp_info("<PING> QUIT thread_id:%lu, pid:%d", pthread_self(), getpid());
    return NULL;
}

STATIC int rs_ping_cb_init_mutex(struct rs_ping_ctx_cb *ping_cb)
{
    int ret;

    ret = pthread_mutex_init(&ping_cb->ping_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init ping_mutex failed ret %d", ret);
        goto ping_mutex_init_failed;
    }
    ret = pthread_mutex_init(&ping_cb->pong_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init pong_mutex failed ret %d", ret);
        goto pong_mutex_init_failed;
    }
    ret = pthread_mutex_init(&ping_cb->dev_mutex, NULL);
    if (ret != 0) {
        hccp_err("pthread_mutex_init dev_mutex failed ret %d", ret);
        goto dev_mutex_init_failed;
    }

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    RS_INIT_LIST_HEAD(&ping_cb->ping_list);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->pong_mutex);
    RS_INIT_LIST_HEAD(&ping_cb->pong_list);
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->pong_mutex);

    return 0;

dev_mutex_init_failed:
    (void)pthread_mutex_destroy(&ping_cb->pong_mutex);
pong_mutex_init_failed:
    (void)pthread_mutex_destroy(&ping_cb->ping_mutex);
ping_mutex_init_failed:
    return -ESYSFUNC;
}

RS_ATTRI_VISI_DEF int rs_ping_handle_init(unsigned int chip_id, int hdc_type, unsigned int white_list_status)
{
    struct rs_cb *rs_cb = NULL;
    int ret;

    if (hdc_type != HDC_SERVICE_TYPE_RDMA_V2 && white_list_status != WHITE_LIST_DISABLE) {
        return 0;
    }

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rs_cb failed, ret:%d, chip_id:%u", ret, chip_id), -ENODEV);

    ret = rs_ping_cb_init_mutex(&rs_cb->ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_ping_cb_init_mutex failed, ret %d", ret), ret);

    rs_cb->ping_cb.thread_status = RS_PING_THREAD_RUNNING;
    ret = pthread_create(&rs_cb->ping_cb.tid, NULL, (void *)rs_ping_handle, (void *)rs_cb);
    if (ret != 0) {
        hccp_err("Create pthread failed, ret(%d) ", ret);
        rs_cb->ping_cb.thread_status = RS_PING_THREAD_RESET;
        (void)pthread_mutex_destroy(&rs_cb->ping_cb.ping_mutex);
        (void)pthread_mutex_destroy(&rs_cb->ping_cb.pong_mutex);
        return -ESYSFUNC;
    }

    return 0;
}

RS_ATTRI_VISI_DEF int rs_ping_handle_deinit(unsigned int chip_id)
{
#define THREAD_STATUS_CHANGE_TIMEOUT 100
    struct rs_cb *rs_cb = NULL;
    int ret;
    int i;

    ret = rs_dev2rscb(chip_id, &rs_cb, false);
    CHK_PRT_RETURN(ret != 0, hccp_err("get rs_cb failed, ret:%d, chip_id:%u", ret, chip_id), -ENODEV);

    if (rs_cb->ping_cb.thread_status != RS_PING_THREAD_RUNNING) {
        return 0;
    }

    RS_PTHREAD_MUTEX_LOCK(&rs_cb->ping_cb.ping_mutex);
    rs_cb->ping_cb.thread_status = RS_PING_THREAD_RESET;
    rs_cb->ping_cb.task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&rs_cb->ping_cb.ping_mutex);

    // wait thread change to finish running status, wait 100 times(total cost: 1s) until timeout
    for (i = 0; i < THREAD_STATUS_CHANGE_TIMEOUT && rs_cb->ping_cb.thread_status != RS_PING_THREAD_FINISH; i++) {
        usleep(RS_PING_PERIOD_TIME_USEC);
    }

    // thread not in finish running status, report timeout
    if (rs_cb->ping_cb.thread_status != RS_PING_THREAD_FINISH) {
        hccp_run_info("<PING> wait thread tid:%lu finish running timeout, thread status:%d",
            rs_cb->ping_cb.tid, rs_cb->ping_cb.thread_status);
    }

    (void)pthread_mutex_destroy(&rs_cb->ping_cb.ping_mutex);
    (void)pthread_mutex_destroy(&rs_cb->ping_cb.pong_mutex);
    (void)pthread_mutex_destroy(&rs_cb->ping_cb.dev_mutex);
    return 0;
}

STATIC int rs_ping_init_protocol_ops(struct rs_ping_ctx_cb *ping_cb, enum protocol_type protocol)
{
    ping_cb->protocol = protocol;

    switch (protocol) {
        case PROTOCOL_RDMA:
            ping_cb->ping_pong_ops = rs_ping_roce_get_ops();
            ping_cb->ping_pong_dfx = rs_ping_roce_get_dfx();
            break;
        default:
            hccp_err("unsupported protocol:%u", protocol);
            return -EINVAL;
    }

    if (ping_cb->ping_pong_ops == NULL || ping_cb->ping_pong_ops->init_ping_cb == NULL || ping_cb->ping_pong_dfx == NULL) {
        hccp_err("ping_cb->ping_pong_ops or init_ping_cb or ping_cb->ping_pong_dfx is NULL, protocol:%u", protocol);
        return -ENOTSUPP;
    }
    return 0;
}

RS_ATTRI_VISI_DEF int rs_ping_init(struct ping_init_attr *attr, struct ping_init_info *info, unsigned int *dev_index)
{
    struct rs_ping_ctx_cb *ping_cb = NULL;
    struct rs_cb *rscb = NULL;
    unsigned int phy_id;
    int ret = 0;

    CHK_PRT_RETURN(attr == NULL || info == NULL || dev_index == NULL,
        hccp_err("param error, attr or info or dev_index is NULL"), -EINVAL);

    phy_id = (attr->protocol == PROTOCOL_RDMA) ? attr->dev.rdma.phy_id : UINT_MAX;
    ret = rs_get_rs_cb(phy_id, &rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    ping_cb = &rscb->ping_cb;
    RS_PTHREAD_MUTEX_LOCK(&ping_cb->dev_mutex);
    if (rscb->ping_cb.init_cnt != 0) {
        hccp_err("init_cnt:%u != 0", rscb->ping_cb.init_cnt);
        ret = -EEXIST;
        goto free_dev_mutex;
    }

    ret = rsGetLocalDevIDByHostDevID(phy_id, &ping_cb->logic_devid);
    if (ret != 0) {
        hccp_err("rsGetLocalDevIDByHostDevID failed, phy_id(%u), ret(%d)", phy_id, ret);
        goto free_dev_mutex;
    }

#ifdef CUSTOM_INTERFACE
    // setup sharemem for pingmesh
    ret = rs_setup_sharemem(rscb, false, phy_id);
    if (ret != 0) {
        hccp_err("rs_setup_sharemem failed, phy_id(%u), ret(%d)", phy_id, ret);
        goto free_dev_mutex;
    }
#endif

    ret = rs_ping_init_protocol_ops(ping_cb, attr->protocol);
    if (ret != 0) {
        hccp_err("rs_ping_init_protocol_ops failed, phy_id:%u ret:%d", phy_id, ret);
        goto free_dev_mutex;
    }

    ret = ping_cb->ping_pong_ops->init_ping_cb(phy_id, attr, info, dev_index, ping_cb);
    if (ret != 0) {
        hccp_err("init_ping_cb failed, phy_id:%u ret:%d", phy_id, ret);
        goto free_dev_mutex;
    }

    ping_cb->init_cnt++;
    ping_cb->ping_pong_dfx->init_ping_cb_success(phy_id, attr, *dev_index);

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->dev_mutex);
    return ret;
}

STATIC int rs_get_ping_cb(struct ra_rs_dev_info *rdev, struct rs_ping_ctx_cb **ping_cb)
{
    unsigned int phy_id = rdev->phy_id;
    struct rs_cb *rs_cb = NULL;
    int ret;

    ret = rs_get_rs_cb(phy_id, &rs_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_rs_cb failed, phy_id[%u] invalid, ret %d", phy_id, ret), ret);

    CHK_PRT_RETURN(rdev->dev_index != rs_cb->ping_cb.dev_index,
        hccp_err("param error, dev_index:%u != ping_cb.dev_index:%u", rdev->dev_index, rs_cb->ping_cb.dev_index),
        -ENODEV);

    CHK_PRT_RETURN(rs_cb->ping_cb.thread_status != RS_PING_THREAD_RUNNING,
        hccp_err("thread_status:%d is not running", rs_cb->ping_cb.thread_status), -ESRCH);

    *ping_cb = &rs_cb->ping_cb;

    return 0;
}

RS_ATTRI_VISI_DEF int rs_ping_target_add(struct ra_rs_dev_info *rdev, struct ping_target_info *target)
{
    struct rs_ping_target_info *target_info = NULL;
    struct rs_ping_ctx_cb *ping_cb = NULL;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || target == NULL, hccp_err("param error, rdev is NULL or target is NULL"), -EINVAL);
    CHK_PRT_RETURN(target->payload.size > PING_USER_PAYLOAD_MAX_SIZE,
        hccp_err("param error, size:%u > max_size:%u", target->payload.size, PING_USER_PAYLOAD_MAX_SIZE), -EINVAL);

    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    if (ping_cb->task_status != RS_PING_TASK_RESET) {
        hccp_err("task_status:%d disallow to add target phy_id:%u", ping_cb->task_status, rdev->phy_id);
        return -EEXIST;
    }

    ret = ping_cb->ping_pong_ops->ping_find_target_node(ping_cb, &target->remote_info.qp_info, &target_info);
    if (ret == 0) {
        hccp_info("target node exist! phy_id:%u", rdev->phy_id);
        ret = -EEXIST;
        goto out;
    }

    ret = ping_cb->ping_pong_ops->ping_alloc_target_node(ping_cb, target, &target_info);
    if (ret != 0) {
        hccp_err("rs_ping_alloc_target_node failed, ret:%d phy_id:%u", ret, rdev->phy_id);
        return ret;
    }

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    target_info->uuid = (uint64_t)ping_cb->ping_num;
    rs_list_add_tail(&target_info->list, &ping_cb->ping_list);
    ping_cb->ping_num++;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

out:
    ping_cb->ping_pong_dfx->add_target_success(target, target_info);
    return ret;
}

RS_ATTRI_VISI_DEF int rs_ping_task_start(struct ra_rs_dev_info *rdev, struct ping_task_attr *attr)
{
    struct rs_ping_target_info *target_next = NULL;
    struct rs_ping_target_info *target_curr = NULL;
    struct rs_ping_ctx_cb *ping_cb = NULL;
    unsigned int target_cnt = 0;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || attr == NULL, hccp_err("param error, rdev is NULL or attr is NULL"), -EINVAL);
    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    if (ping_cb->task_status != RS_PING_TASK_RESET) {
        hccp_warn("task_status:%d disallow to start ping task, phy_id:%u", ping_cb->task_status, rdev->phy_id);
        return -EEXIST;
    }
    CHK_PRT_RETURN(attr->packet_cnt == 0 || attr->packet_interval == 0 || attr->timeout_interval == 0,
        hccp_err("param error, packet_cnt:%u or packet_interval:%u or timeout_interval:%u is 0",
        attr->packet_cnt, attr->packet_interval, attr->timeout_interval), -EINVAL);

    ping_cb->ping_pong_ops->reset_recv_buffer(ping_cb);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    ping_cb->task_id++;
    (void)memcpy_s(&ping_cb->task_attr, sizeof(struct ping_task_attr), attr, sizeof(struct ping_task_attr));
    RS_LIST_GET_HEAD_ENTRY(target_curr, target_next, &ping_cb->ping_list, list, struct rs_ping_target_info);
    for(; (&target_curr->list) != &ping_cb->ping_list;
        target_curr = target_next, target_next = list_entry(target_next->list.next, struct rs_ping_target_info, list)) {
        (void)memset_s(&target_curr->result_summary, sizeof(struct ping_result_summary),
            0, sizeof(struct ping_result_summary));
        (void)memcpy_s(&target_curr->result_summary.task_attr, sizeof(struct ping_task_attr), attr,
            sizeof(struct ping_task_attr));
        target_curr->result_summary.rtt_min = ~0;
        target_curr->result_summary.task_id = ping_cb->task_id;
        target_curr->state = RS_PING_PONG_TARGET_READY;
        target_cnt++;
    }

    ping_cb->task_status = RS_PING_TASK_RUNNING;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    hccp_info("target_cnt:%u packet_cnt:%u packet_interval:%u timeout_interval:%u task_id:%u start success", target_cnt,
        attr->packet_cnt, attr->packet_interval, attr->timeout_interval, ping_cb->task_id);
    return 0;
}

RS_ATTRI_VISI_DEF int rs_ping_get_results(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num, struct ping_result_info result[])
{
    struct rs_ping_ctx_cb *ping_cb = NULL;
    unsigned int expected_num;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || num == NULL, hccp_err("param error, rdev is NULL or num is NULL"), -EINVAL);
    expected_num = *num;
    *num = 0;
    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    // caller needs to retry, degrade log level
    if (ping_cb->task_status == RS_PING_TASK_RUNNING) {
        hccp_warn("task_status:%d disallow to get ping results phy_id:%u", ping_cb->task_status, rdev->phy_id);
        return -EAGAIN;
    }

    for (i = 0; i < expected_num; i++) {
        ret = ping_cb->ping_pong_ops->get_target_result(ping_cb, &target[i], &result[i]);
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

RS_ATTRI_VISI_DEF int rs_ping_task_stop(struct ra_rs_dev_info *rdev)
{
    struct rs_ping_ctx_cb *ping_cb = NULL;
    int ret;

    CHK_PRT_RETURN(rdev == NULL, hccp_err("param error, rdev is NULL"), -EINVAL);
    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    hccp_info("task_status:%d modify to %d, phy_id:%u", ping_cb->task_status, RS_PING_TASK_RESET, rdev->phy_id);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->ping_mutex);
    ping_cb->task_status = RS_PING_TASK_RESET;
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->ping_mutex);

    return 0;
}

RS_ATTRI_VISI_DEF int rs_ping_target_del(struct ra_rs_dev_info *rdev, struct ping_target_comm_info target[],
    unsigned int *num)
{
    struct rs_ping_target_info *target_info = NULL;
    struct rs_ping_ctx_cb *ping_cb = NULL;
    unsigned int expected_num;
    unsigned int i;
    int ret;

    CHK_PRT_RETURN(rdev == NULL || target == NULL || num == NULL,
        hccp_err("param error, rdev or target or num is NULL"), -EINVAL);
    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    if (ping_cb->task_status != RS_PING_TASK_RESET) {
        hccp_err("task_status:%d disallow to delete target phy_id:%u", ping_cb->task_status, rdev->phy_id);
        return -EEXIST;
    }

    expected_num = *num;
    for (i = 0; i < expected_num; i++) {
        ret = ping_cb->ping_pong_ops->ping_find_target_node(ping_cb, &target[i].qp_info, &target_info);
        if (ret != 0) {
            ping_cb->ping_pong_dfx->ping_cannot_find_target_node(i, ret, target[i], rdev->phy_id);
            goto out;
        }

        ping_cb->ping_pong_ops->ping_free_target_node(ping_cb, target_info);
        (void)pthread_mutex_destroy(&target_info->trip_mutex);
        free(target_info);
        target_info = NULL;
    }

out:
    *num = i;
    return ret;
}

RS_ATTRI_VISI_DEF int rs_ping_deinit(struct ra_rs_dev_info *rdev)
{
    struct rs_ping_ctx_cb *ping_cb = NULL;
    int ret = 0;

    CHK_PRT_RETURN(rdev == NULL, hccp_err("param error, rdev is NULL"), -EINVAL);
    ret = rs_get_ping_cb(rdev, &ping_cb);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_get_ping_cb failed, ret=%d phy_id:%u", ret, rdev->phy_id), ret);

    RS_PTHREAD_MUTEX_LOCK(&ping_cb->dev_mutex);
    if (ping_cb->init_cnt == 0) {
        hccp_err("init_cnt is 0");
        ret = -ENODEV;
        goto free_dev_mutex;
    }

    ping_cb->ping_pong_ops->deinit_ping_cb(rdev->phy_id, ping_cb);
    ping_cb->init_cnt--;
    hccp_run_info("ping_cb deinit success, phy_id:%u, dev_index:%u", rdev->phy_id, rdev->dev_index);

free_dev_mutex:
    RS_PTHREAD_MUTEX_ULOCK(&ping_cb->dev_mutex);
    return ret;
}
