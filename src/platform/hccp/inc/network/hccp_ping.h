/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _HCCP_PING_H
#define _HCCP_PING_H

#include "hccp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qp_cap {
    uint32_t max_send_wr;
    uint32_t max_recv_wr;
    uint32_t max_send_sge;
    uint32_t max_recv_sge;
    uint32_t max_inline_data;
};

union ping_qp_attr {
    struct {
        struct cq_ext_attr cq_attr;
        struct {
            struct qp_cap cap;
            uint32_t udp_sport;
            uint32_t reserved[4U];
        } qp_attr;
        uint32_t reserved[4U];
    } rdma;
};

struct ping_local_comm_info {
    int version;
    union {
        struct {
            uint32_t flow_label;
            uint8_t hop_limit;
            struct qos_attr qos_attr;
            uint32_t udp_sport;
            uint32_t reserved[7U];
        } rdma;
    };
};

union ping_dev {
    struct rdev rdma;
};

struct ping_init_attr {
    int version;
    int mode;
    union ping_dev dev;
    struct ping_local_comm_info comm_info;
    union ping_qp_attr client;
    union ping_qp_attr server;
    uint32_t buffer_size;
    enum protocol_type protocol;
    union {
        struct {
            uint32_t reserved[31U];
        } rdma;
    };
};

struct ping_qp_info {
    int version;
    union {
        struct {
            union hccp_gid gid;
            uint32_t qpn;
            uint32_t qkey;
            uint32_t reserved[4U];
        } rdma;
    };
};

struct ping_buffer_info {
    // all result buffer
    uint64_t buffer_va;
    uint32_t buffer_size;
    // each payload offset & header size
    uint32_t payload_offset;
    uint32_t header_size;
};

struct ping_init_info {
    int version;
    struct ping_qp_info client;
    struct ping_qp_info server;
    struct ping_buffer_info result;
    uint32_t reserved[32U];
};

struct ping_task_attr {
    uint32_t packet_cnt;
    uint32_t packet_interval;
    uint32_t timeout_interval;
};

#define PING_TOTAL_PAYLOAD_MAX_SIZE 2048U
#define PING_USER_PAYLOAD_MAX_SIZE 1500U

struct ping_payload_info {
    char buffer[PING_USER_PAYLOAD_MAX_SIZE];
    uint32_t size;
};

struct ping_target_comm_info {
    union {
        union hccp_ip_addr ip;
    };
    struct ping_qp_info qp_info;
};

struct ping_target_info {
    int version;
    struct ping_local_comm_info local_info;
    struct ping_target_comm_info remote_info;
    struct ping_payload_info payload;
    uint32_t reserved[16U];
};

enum ping_result_state {
    PING_RESULT_STATE_NOT_FOUND = 0,
    PING_RESULT_STATE_INVALID = 1,
    PING_RESULT_STATE_VALID = 2,
    PING_RESULT_STATE_MAX
};

struct ping_result_summary {
    int version;
    struct ping_task_attr task_attr;

    uint32_t rtt_min; /**< tv_usec */
    uint32_t rtt_max; /**< tv_usec */
    uint32_t rtt_avg; /**< tv_usec */

    uint32_t send_cnt;
    uint32_t recv_cnt;
    uint32_t timeout_cnt;

    uint32_t task_id;
    uint32_t reserved[31U];
};

struct ping_result_info {
    enum ping_result_state state;
    struct ping_result_summary summary;
};

struct ping_target_result {
    struct ping_target_comm_info remote_info;
    struct ping_result_info result;
};

/**
 * @ingroup libinit
 * @brief Rdma_agent ping initialization
 * @param init_attr [IN] init attr
 * @param init_info [OUT] init info
 * @param ping_handle [OUT] ping handle info
 * @see ra_ping_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingInit(struct ping_init_attr *initAttr, struct ping_init_info *initInfo,
    void **pingHandle);

/**
 * @ingroup librdma
 * @brief Rdma_agent add target to list
 * @param ping_handle [IN] ping handle info
 * @param target [IN] ping target info
 * @param num [IN] num of target
 * @see ra_ping_target_del
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingTargetAdd(void *pingHandle, struct ping_target_info target[], uint32_t num);

/**
 * @ingroup librdma
 * @brief Rdma_agent trigger ping task start
 * @param ping_handle [IN] ping handle info
 * @param attr [IN] ping task attr
 * @see ra_ping_task_stop
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingTaskStart(void *pingHandle, struct ping_task_attr *attr);

/**
 * @ingroup librdma
 * @brief Rdma_agent ping get results
 * @param ping_handle [IN] ping handle info
 * @param target [IN/OUT] ping result info
 * @param num [IN/OUT] num of target & num of results got
 * @see ra_ping_target_del
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingGetResults(void *pingHandle, struct ping_target_result target[], uint32_t *num);

/**
 * @ingroup librdma
 * @brief Rdma_agent del target from list
 * @param ping_handle [IN] ping handle info
 * @param target [IN] ping target info
 * @param num [IN] num of target
 * @see ra_ping_target_add
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingTargetDel(void *pingHandle, struct ping_target_comm_info target[], uint32_t num);

/**
 * @ingroup librdma
 * @brief Rdma_agent trigger ping task stop
 * @param ping_handle [IN] ping handle info
 * @see ra_ping_task_start
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingTaskStop(void *pingHandle);

/**
 * @ingroup libinit
 * @brief Rdma_agent ping deinitialization
 * @param ping_handle [IN] ping handle info
 * @see ra_ping_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int RaPingDeinit(void *pingHandle);
#ifdef __cplusplus
}
#endif
#endif // _HCCP_PING_H