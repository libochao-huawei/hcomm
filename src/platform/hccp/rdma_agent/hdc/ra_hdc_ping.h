/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_HDC_PING_H
#define RA_HDC_PING_H

#include "hccp_ping.h"
#include "ra_ping.h"
#include "ra_rs_comm.h"
#include "ra_hdc.h"

#define RA_MAX_PING_TARGET_NUM 16

union op_ping_init_data {
    struct {
        struct ping_init_attr attr;
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int dev_index;
        struct ping_init_info info;
        uint32_t reserved[RA_RSVD_NUM_4];
    } rx_data;
};

union op_ping_add_data {
    struct {
        struct ra_rs_dev_info rdev;
        struct ping_target_info target;
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        uint32_t reserved[RA_RSVD_NUM_64];
    } rx_data;
};

union op_ping_start_data {
    struct {
        struct ra_rs_dev_info rdev;
        struct ping_task_attr attr;
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        uint32_t reserved[RA_RSVD_NUM_64];
    } rx_data;
};

union op_ping_results_data {
    struct {
        struct ra_rs_dev_info rdev;
        unsigned int num;
        struct ping_target_comm_info target[RA_MAX_PING_TARGET_NUM];
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int num;
        struct ping_result_info target[RA_MAX_PING_TARGET_NUM];
        uint32_t reserved[RA_RSVD_NUM_4];
    } rx_data;
};

union op_ping_del_data {
    struct {
        struct ra_rs_dev_info rdev;
        unsigned int num;
        struct ping_target_comm_info target[RA_MAX_PING_TARGET_NUM];
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        unsigned int num;
        uint32_t reserved[RA_RSVD_NUM_16];
    } rx_data;
};

union op_ping_stop_data {
    struct {
        struct ra_rs_dev_info rdev;
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        uint32_t reserved[RA_RSVD_NUM_64];
    } rx_data;
};

union op_ping_deinit_data {
    struct {
        struct ra_rs_dev_info rdev;
        uint32_t reserved[RA_RSVD_NUM_4];
    } tx_data;

    struct {
        uint32_t reserved[RA_RSVD_NUM_4];
    } rx_data;
};

int RaHdcPingInit(struct ra_ping_handle *pingHandle, struct ping_init_attr *initAttr,
    struct ping_init_info *initInfo);
int RaHdcPingTargetAdd(struct ra_ping_handle *pingHandle, struct ping_target_info target[], uint32_t num);
int RaHdcPingTaskStart(struct ra_ping_handle *pingHandle, struct ping_task_attr *attr);
int RaHdcPingGetResults(struct ra_ping_handle *pingHandle, struct ping_target_result target[], uint32_t *num);
int RaHdcPingTargetDel(struct ra_ping_handle *pingHandle, struct ping_target_comm_info target[], uint32_t num);
int RaHdcPingTaskStop(struct ra_ping_handle *pingHandle);
int RaHdcPingDeinit(struct ra_ping_handle *pingHandle);
#endif // RA_HDC_PING_H
