/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_CTX_INNER_H
#define RS_CTX_INNER_H

#include <pthread.h>
#include <urma_types.h>
#include "hccp_ctx.h"
#include "rs_inner.h"
#include "rs_list.h"

#define DEV_INDEX_DIEID_OFFSET 8U
#define DEV_INDEX_CNT_OFFSET 16U
#define DEV_INDEX_UE_INFO_MASK 0x0000FFFFUL
#define CI_ADDR_BUFFER_ALIGN_4K_PAGE_SIZE 4096U
#define WQE_BB_SIZE 64ULL

struct rs_ub_dev_cb {
    struct rs_cb *rscb;
    unsigned int phyId;
    unsigned int eid_index;
    union hccp_eid eid;
    urma_context_t *urma_ctx;
    urma_device_t *urma_dev;
    unsigned int index;
    struct dev_base_attr dev_attr;

    unsigned int cqeErrCnt;
    pthread_mutex_t cqeErrCntMutex;

    pthread_mutex_t mutex;
    unsigned int async_event_cnt;
    unsigned int jfce_cnt;
    unsigned int jfc_cnt;
    unsigned int jetty_cnt;
    unsigned int rjetty_cnt;
    unsigned int token_id_cnt;
    unsigned int lseg_cnt;
    unsigned int rseg_cnt;
    struct RsListHead async_event_list;
    struct RsListHead jfce_list;
    struct RsListHead jfc_list;
    struct RsListHead jetty_list;
    struct RsListHead rjetty_list;
    struct RsListHead token_id_list;
    struct RsListHead lseg_list;
    struct RsListHead rseg_list;
    struct RsListHead list;
};

struct rs_ctx_async_event_cb {
    struct rs_ub_dev_cb *dev_cb;
    urma_async_event_t async_event;
    unsigned int res_id;
    struct RsListHead list;
};

struct rs_ctx_jfce_cb {
    struct rs_ub_dev_cb *dev_cb;
    uint64_t jfce_addr; // urma_jfce_t *
    union data_plane_cstm_flag data_plane_flag;
    struct RsListHead list;
};

struct rs_ctx_jfc_cb {
    struct rs_ub_dev_cb *dev_cb;
    uint64_t jfc_addr;
    enum jfc_mode jfc_type;
    uint32_t depth;
    uint32_t jfc_id;
    uint64_t buf_addr;
    uint64_t swdb_addr;
    struct RsListHead list;
    struct {
        bool valid;
        uint32_t cqe_flag;
    } ccu_ex_cfg;
};

struct RsCrErrInfo {
    pthread_mutex_t mutex;
    struct CrErrInfo info;
};

struct rs_ctx_jetty_cb {
    struct rs_ub_dev_cb *dev_cb;
    urma_jetty_t *jetty;
    urma_jfr_t *jfr;
    int jetty_mode;
    uint32_t jetty_id;
    int transport_mode;
    unsigned int state;
    unsigned int tx_depth;
    unsigned int rx_depth;
    urma_jetty_flag_t flag;
    urma_jfs_flag_t jfs_flag;
    uint64_t token_id_addr; /**< NULL means unspecified */
    unsigned int token_value;
    uint8_t priority;
    uint8_t rnr_retry;
    uint8_t err_timeout;
    union {
        struct {
            struct jetty_que_cfg_ex sq;
            bool pi_type;
            union cstm_jfs_flag cstm_flag;
            uint32_t sqebb_num;
        } ext_mode;
        struct {
            bool lock_flag;
            uint32_t sqe_buf_idx;
        } ta_cache_mode;
    };
    uint64_t sq_buff_va;
    uint64_t db_addr;
    uint32_t db_token_id;
    uint64_t db_seg_handle;
    pthread_mutex_t mutex;
    uint32_t last_pi;
    struct ctx_qp_share_info *qp_share_info_addr;
    struct RsCrErrInfo cr_err_info;
    struct RsListHead list;
};

struct rs_token_id_cb {
    struct rs_ub_dev_cb *dev_cb;
    urma_token_id_t *token_id;
    struct RsListHead list;
};

struct rs_ctx_rem_jetty_cb {
    struct rs_ub_dev_cb *dev_cb;
    urma_target_jetty_t *tjetty;
    struct qp_key jetty_key;
    enum jetty_import_mode mode;
    unsigned int token_value;
    enum jetty_grp_policy policy;
    enum target_type type;
    union import_jetty_flag flag;
    uint32_t tp_type;
    struct jetty_import_exp_cfg exp_import_cfg;
    unsigned int state;
    struct RsListHead list;
};

struct rs_seg_info {
    uint64_t addr;
    uint64_t len;
    urma_seg_t seg;
};

struct rs_seg_cb {
    struct rs_ub_dev_cb *dev_cb;

    struct rs_seg_info seg_info;
    uint32_t state;

    urma_token_t token_value;
    urma_target_seg_t *segment;

    struct RsListHead list;
};

struct udma_va_info {
    enum res_addr_type resType;
    int pid;
    uint64_t va;
    uint64_t len;
};

STATIC inline uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id)
{
    return (die_id << DEV_INDEX_DIEID_OFFSET) | func_id;
}

STATIC inline uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id)
{
    return (dev_cnt << DEV_INDEX_CNT_OFFSET) | rs_generate_ue_info(die_id, func_id);
}

#endif // RS_CTX_INNER_H
