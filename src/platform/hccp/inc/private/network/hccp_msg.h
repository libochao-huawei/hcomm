/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TS_HCCP_MSG_H
#define TS_HCCP_MSG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#pragma pack(push)
#pragma pack (1)
typedef struct tag_ts_ccu_task_info {
    uint8_t udie_id;
    uint8_t rsv;
    uint16_t mission_id;
} ts_ccu_task_info_t;

typedef struct tag_ts_ub_task_info {
    uint8_t udie_id;
    uint8_t function_id;
    uint16_t jetty_id;
} ts_ub_task_info_t;

typedef struct tag_ts_ccu_task_report {
    uint8_t num;
    uint8_t rsv1[3];
    ts_ccu_task_info_t array[8];
    uint8_t rsv2[60]; // rsv 60B
} ts_ccu_task_report_t;

typedef struct tag_ts_ub_task_report {
    uint8_t num;
    uint8_t rsv1[3];
    ts_ub_task_info_t array[4];
    uint8_t rsv2[76]; // rsv 76B
} ts_ub_task_report_t;

typedef struct tag_ts_hccp_msg {
    // head 32B
    int32_t hccp_pid;  // apm_query_slave_tgid_by_master
    uint32_t host_pid;
    uint8_t cmd_type;   // 0:ub force kill; 1:ccu force kill
    uint8_t vf_id;
    uint16_t sq_id;
    uint16_t is_app_exit;
    uint16_t sqe_type;
    uint8_t app_flag;
    uint8_t intr_type;
    uint16_t model_id;
    uint8_t res2[12];
    // info 96B
    union {
        ts_ub_task_report_t ub_task_info;
        ts_ccu_task_report_t ccu_task_info;
    } u;
} ts_hccp_msg;

typedef enum tag_ts_hccp_sub_event_id {
    TOPIC_SEND_KILL_MSG = 1U,
    TOPIC_KILL_DONE_MSG,
} ts_hccp_sub_event_id;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TS_HCCP_MSG_H */
