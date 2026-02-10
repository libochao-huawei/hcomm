/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/prctl.h>
#include <pthread.h>
#include "securec.h"
#include "user_log.h"
#include "dl_hal_function.h"
#include "hccp_msg.h"
#include "hccp_common.h"
#include "ra_rs_err.h"
#include "rs_ub.h"
#include "rs_ccu.h"
#include "rs_esched.h"

struct rs_esched_info g_rs_esched_info = {0};

STATIC void rs_esched_jetty_destroy(struct rs_cb *rscb, ts_ub_task_report_t *task_info)
{
    unsigned int die_id, func_id, ue_info;
    int ret, i;

    for (i = 0; i < task_info->num; i++) {
        die_id = task_info->array[i].udie_id;
        func_id = task_info->array[i].function_id;
        ue_info = rs_generate_ue_info(die_id, func_id);
        ret = rs_ub_ctx_jetty_free(rscb, ue_info, task_info->array[i].jetty_id);
        if (ret != 0) {
            hccp_run_warn("rs_ub_ctx_jetty_free unsuccessful, ret[%d] task_index[%d] logicId[%u] die_id[%u] "
                "func_id[%u] jetty_id[%u]", ret, i, rscb->logicId, die_id, func_id, task_info->array[i].jetty_id);
            continue;
        }

        hccp_info("jetty destroy task success, task_index[%d] logicId[%u] die_id[%u] func_id[%u] jetty_id[%u]",
            i, rscb->logicId, die_id, func_id, task_info->array[i].jetty_id);
    }
    return;
}

STATIC void rs_esched_ccu_mission_kill(unsigned int logicId, ts_ccu_task_report_t *task_info)
{
    int ret, i;

    for (i = 0; i < task_info->num; i++) {
        ret = rs_ctx_ccu_mission_kill(task_info->array[i].udie_id);
        if (ret != 0) {
            hccp_run_warn("ccu set task_kill unsuccessful, ret[%d] task_index[%d] logicId[%u] udie_id[%u]",
                ret, i, logicId, task_info->array[i].udie_id);
            continue;
        }
        hccp_info("ccu set task_kill success, task_index[%d] logicId[%u] udie_id[%u]", i, logicId,
            task_info->array[i].udie_id);
    }
    return;
}

STATIC int rs_esched_exec_by_cmd_type(struct rs_cb *rscb, struct tag_ts_hccp_msg *msg)
{
    int ret = 0;

    switch (msg->cmd_type) {
        case 0: // UB force kill
            rs_esched_jetty_destroy(rscb, &msg->u.ub_task_info);
            break;
        case 1: // CCU force kill
            rs_esched_ccu_mission_kill(rscb->logicId, &msg->u.ccu_task_info);
            break;
        default:
            hccp_run_warn("tag_ts_hccp_msg unsupported cmd type[%u]", msg->cmd_type);
            ret = -EINVAL;
            break;
    }
    return ret;
}

STATIC void rs_esched_clean_all_resource(struct rs_cb *rscb)
{
    struct rs_ub_dev_cb *dev_cb_curr = NULL;
    struct rs_ub_dev_cb *dev_cb_next = NULL;
    int ret;

    RS_PTHREAD_MUTEX_LOCK(&rscb->mutex);

    RS_LIST_GET_HEAD_ENTRY(dev_cb_curr, dev_cb_next, &rscb->rdevList, list, struct rs_ub_dev_cb);
    for (; (&dev_cb_curr->list) != &rscb->rdevList;
         dev_cb_curr = dev_cb_next,
         dev_cb_next = list_entry(dev_cb_next->list.next, struct rs_ub_dev_cb, list)) {
        hccp_info("logicId[%u] devIndex[%u] start clean", rscb->logicId, dev_cb_curr->index);
        rs_ub_free_jetty_cb_list(dev_cb_curr, &dev_cb_curr->jetty_list, &dev_cb_curr->rjetty_list);

        ret = rs_ctx_ccu_mission_kill(dev_cb_curr->dev_attr.ub.die_id);
        if (ret != 0) {
            hccp_run_warn("ccu set task_kill unsuccessful, ret[%d] devIndex[%u]", ret, dev_cb_curr->index);
            continue;
        }

        ret = rs_ctx_ccu_mission_done(dev_cb_curr->dev_attr.ub.die_id);
        if (ret != 0) {
            hccp_run_warn("ccu clean task_kill status unsuccessful, ret[%d] devIndex[%u]", ret, dev_cb_curr->index);
        }
    }

    RS_PTHREAD_MUTEX_ULOCK(&rscb->mutex);
    return;
}

STATIC int rs_esched_process_event(struct rs_cb *rscb, struct event_info *event_data)
{
    unsigned int subevent_id = event_data->comm.subevent_id;
    struct tag_ts_hccp_msg *msg;
    uint16_t is_app_exit;
    int ret = 0;

    CHK_PRT_RETURN(event_data->priv.msg_len != sizeof(struct tag_ts_hccp_msg),
        hccp_err("event invalid, msg_len[%u] != [%u], event_id[%d] subevent_id[%u]",
        event_data->priv.msg_len, sizeof(struct tag_ts_hccp_msg), event_data->comm.event_id, subevent_id), -EINVAL);

    msg = (struct tag_ts_hccp_msg *)event_data->priv.msg;
    is_app_exit = msg->is_app_exit;
    switch (is_app_exit) {
        case 0: // host app alive, exec by cmd_type
            ret = rs_esched_exec_by_cmd_type(rscb, msg);
            break;
        case 1: // host app exit, clean all resource
            rs_esched_clean_all_resource(rscb);
            break;
        default:
            hccp_run_warn("tag_ts_hccp_msg unsupported is_app_exit status[%u]", is_app_exit);
            ret = -EINVAL;
            break;
    }

    return ret;
}

STATIC void rs_esched_ack_event(struct rs_cb *rscb, struct event_info *event_data)
{
    struct event_summary ack_event = {0};
    int ret = 0;

    ack_event.pid = event_data->comm.pid;
    ack_event.grp_id = event_data->comm.grp_id;
    ack_event.event_id = EVENT_HCCP_MSG;
    ack_event.subevent_id = TOPIC_KILL_DONE_MSG;
    ack_event.msg_len = event_data->priv.msg_len;
    ack_event.msg = event_data->priv.msg;
    ack_event.dst_engine = CCPU_DEVICE;
    ack_event.policy = ONLY;
    ret = DlHalEschedSubmitEvent(rscb->logicId, &ack_event);
    if (ret != 0) {
        hccp_run_warn("DlHalEschedSubmitEvent unsuccessful, ret[%d] logicId[%u]", ret, rscb->logicId);
    }

    return;
}

STATIC void rs_esched_handle_event(struct rs_cb *rscb)
{
    struct event_info event = {0};
    int ret;

    ret = DlHalEschedWaitEvent(rscb->logicId, ESCHED_GRP_TS_HCCP, ESCHED_THREAD_ID_TS_HCCP, 0, &event);
    if (ret == DRV_ERROR_SCHED_WAIT_TIMEOUT || ret == DRV_ERROR_NO_EVENT) {
        return;
    }

    if (ret != DRV_ERROR_NONE) {
        hccp_run_warn("DlHalEschedWaitEvent unsuccessful, ret[%d] logicId[%u]", ret, rscb->logicId);
        return;
    }

    hccp_info("wait event success, event_id[%d] subevent_id[%u]", event.comm.event_id, event.comm.subevent_id);
    ret = rs_esched_process_event(rscb, &event);
    if (ret != 0) {
        hccp_run_warn("rs_esched_process_event unsuccessful, ret[%d] logicId[%u]", ret, rscb->logicId);
    }

    rs_esched_ack_event(rscb, &event);
}

STATIC void *rs_esched_handle(void *arg)
{
    struct rs_cb *rscb = (struct rs_cb *)arg;
    int ret;

    ret = pthread_detach(pthread_self());
    CHK_PRT_RETURN(ret, hccp_err("pthread detach failed ret %d", ret), NULL);
    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_rs_esched");
    g_rs_esched_info.thread_status = THREAD_RUNNING;

    while (1) {
        if (g_rs_esched_info.thread_status == THREAD_DESTROYING) {
            break;
        }

        rs_esched_handle_event(rscb);
        usleep(ESCHED_THREAD_USLEEP_TIME);
    }

    hccp_run_info("rs esched handle thread exit success, logic_devid[%u]", rscb->logicId);
    g_rs_esched_info.thread_status = THREAD_HALT;
    return NULL;
}

int rs_esched_init(struct rs_cb *rscb)
{
    pthread_t rs_esched_tid;
    int ret = 0;

    if (rscb->protocol != PROTOCOL_UDMA) {
        return 0;
    }

    ret = DlHalEschedAttachDevice(rscb->logicId);
    CHK_PRT_RETURN(ret != 0, hccp_err("halEschedSubscribeEvent failed, ret[%d] logicId[%u]",
        ret, rscb->logicId), ret);

    ret = DlHalEschedCreateGrp(rscb->logicId, ESCHED_GRP_TS_HCCP, GRP_TYPE_BIND_CP_CPU);
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalEschedCreateGrp failed, ret[%d] logicId[%u]",
        ret, rscb->logicId), ret);

    ret = DlHalEschedSubscribeEvent(rscb->logicId, ESCHED_GRP_TS_HCCP, ESCHED_THREAD_ID_TS_HCCP,
        (1UL << EVENT_HCCP_MSG));
    CHK_PRT_RETURN(ret != 0, hccp_err("DlHalEschedSubscribeEvent failed, ret[%d] logicId[%u]",
        ret, rscb->logicId), ret);

    ret = pthread_create(&rs_esched_tid, NULL, rs_esched_handle, (void *)rscb);
    CHK_PRT_RETURN(ret != 0, hccp_err("pthread create failed, ret[%d] logicId[%u]", ret, rscb->logicId), -ESYSFUNC);

    return 0;
}

void rs_esched_deinit(enum ProtocolTypeT protocol)
{
    int try_again;

    if (protocol != PROTOCOL_UDMA) {
        return;
    }

    if (g_rs_esched_info.thread_status == THREAD_HALT) {
        return;
    }

    // not need to use mutex, because rs_esched thread can only be created once when rs_init exec in hccp process
    g_rs_esched_info.thread_status = THREAD_DESTROYING;

    try_again = ESCHED_THREAD_TRY_TIME;
    while ((g_rs_esched_info.thread_status != THREAD_HALT) && try_again != 0) {
        usleep(ESCHED_THREAD_USLEEP_TIME);
        try_again--;
    }

    if (try_again <= 0) {
        hccp_warn("rs_esched_handle thread quit timeout");
    }
    return;
}
