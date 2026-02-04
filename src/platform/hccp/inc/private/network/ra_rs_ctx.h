/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_RS_CTX_H
#define RA_RS_CTX_H

#include "hccp_ctx.h"
#include "ra_rs_comm.h"

#define MAX_RSGE_NUM    2
#define MAX_CTX_WR_NUM  4
#define MAX_INLINE_SIZE 64

struct mem_reg_attr_t {
    struct mem_info mem;
    union {
        struct {
            int access; /**< refer to enum mem_mr_access_flags */
        } rdma;

        struct {
            union reg_seg_flag flags;
            uint32_t token_value; /**< refer to urma_token_t */
            uint64_t token_id_addr; /**< NULL means unspecified, valid if flags.token_id_valid been set */
        } ub;
    };
    uint32_t resv[8U];
};

struct mem_reg_info_t {
    struct mem_key key;
    union {
        struct {
            uint32_t lkey;
        } rdma;

        struct {
            uint32_t token_id;
            uint64_t target_seg_handle; /**< refer to urma_target_seg_t */
        } ub;
    };
    uint32_t resv[8U];
};

struct mem_import_attr_t {
    struct mem_key key;

    union {
        struct {
            union import_seg_flag flags; /**< refer to urma_import_seg_flag_t */
            uint64_t mapping_addr; /**< addr is needed if flag mapping set value */
            uint32_t token_value; /**< refer to urma_token_t */
        } ub;
    };
    uint32_t resv[4U];
};

struct mem_import_info_t {
    union {
        struct {
            uint32_t rkey;
        } rdma;

        struct {
            uint64_t target_seg_handle; /**< refer to urma_target_seg_t */
        } ub;
    };
    uint32_t resv[4U];
};

struct ctx_cq_attr {
    uint64_t chan_addr; /**< resv for chan_cb index */
    uint32_t depth;
    union {
        struct {
            uint64_t cq_context;
            uint32_t mode; /**< refer to enum RA_RDMA_NOR_MODE etc. */
            uint32_t comp_vector;
        } rdma;

        struct {
            uint64_t user_ctx;
            enum jfc_mode mode;
            uint32_t ceqn;
            union jfc_flag flag; /**< refer to urma_jfc_flag_t */
            struct {
                bool valid;
                uint32_t cqe_flag;
            } ccu_ex_cfg;
        } ub;
    };
    uint32_t resv[4U];
};

struct ctx_cq_info {
    uint64_t addr; /**< refer to ibv_cq*, urma_jfc_t* for cq_cb index */
    struct {
        uint32_t id; /**< jfc id */
        uint32_t cqe_size;
        uint64_t buf_addr;
        uint64_t swdb_addr;
        uint32_t resv[2U]; /**< resv for stars poll cq */
    } ub;
    uint32_t resv[4U];
};

struct ctx_qp_attr {
    uint64_t scq_index;
    uint64_t rcq_index;
    uint64_t srq_index;

    uint32_t sq_depth;
    uint32_t rq_depth;

    enum transport_mode_t transport_mode;

    union {
        struct {
            uint32_t mode; /**< refer to enum RA_RDMA_NOR_MODE etc. */
            uint32_t udp_sport; /**< UDP source port */
            uint8_t traffic_class; /**< traffic class */
            uint8_t sl; /**< service level */
            uint8_t timeout; /**< local ack timeout */
            uint8_t rnr_retry; /**< RNR retry count */
            uint8_t retry_cnt; /**< retry count */
        } rdma;

        struct {
            enum jetty_mode mode;
            uint32_t jetty_id; /**< [optional] user specified jetty id, 0 means not specified */
            union jetty_flag flag; /**< refer to union urma_jetty_flag */
            union jfs_flag jfs_flag; /**< refer to union urma_jfs_flag; jfs_cfg->flag */
            uint32_t token_value; /**< refer to urma_token_t; jfr_cfg->token_value */
            uint64_t token_id_addr; /**< NULL means unspecified */
            uint8_t priority; /**< the priority of JFS. services with low delay need set high priority. Range:[0-0xf] */
            uint8_t rnr_retry; /**< the RNR retry count when receive RNR reponse; Range:[0-7] */
            uint8_t err_timeout; /**< the timeout to report error. Range: [0-31] */
            union {
                struct {
                    struct jetty_que_cfg_ex sq; /**< specify sq buffer config, required when cstm_flag.bs.sq_cstm specified */
                    bool pi_type; /**< false: op mode, true: async mode */
                    union cstm_jfs_flag cstm_flag; /**< refer to union udma_jfs_flag */
                    uint32_t sqebb_num; /**< required when cstm_flag.bs.sq_cstm specified */
                } ext_mode;
                struct {
                    bool lock_flag;
                    uint32_t sqe_buf_idx;
                } ta_cache_mode;
            };
        } ub;
    };

    uint32_t resv[16U];
};

struct ra_rs_jetty_import_attr {
    enum jetty_import_mode mode;
    uint32_t token_value;
    enum jetty_grp_policy policy;
    enum target_type type;
    union import_jetty_flag flag;
    struct jetty_import_exp_cfg exp_import_cfg; /**< only valid on mode JETTY_IMPORT_MODE_EXP */
    uint32_t tp_type;
    uint32_t resv[15U];
};

struct ra_rs_jetty_import_info {
    uint64_t tjetty_handle;
    uint32_t tpn;
    uint32_t resv[16U];
};

struct wrlist_base_info {
    unsigned int phy_id;
    unsigned int dev_index;
    unsigned int qpn;
};

struct ra_wr_sge {
    uint64_t addr;
    uint32_t len;
    uint64_t dev_lmem_handle;
};

struct hdc_wr_notify_info {
    uint64_t notify_data;
    uint64_t notify_addr;
    uint64_t notify_handle;
};

struct batch_send_wr_data {
    struct ra_wr_sge sges[MAX_SGE_NUM];
    uint32_t num_sge;

    char inline_data[MAX_INLINE_SIZE];
    uint32_t inline_size;

    uint64_t remote_addr;
    uint64_t dev_rmem_handle;

    union {
        struct {
            uint64_t wr_id;
            enum RaWrOpcode opcode;
            unsigned int flags; /**< reference to ra_send_flags */
            struct WrAuxInfo aux;
        } rdma;

        struct {
            uint64_t user_ctx;
            enum ra_ub_opcode opcode;
            union jfs_wr_flag flags;
            uint64_t rem_jetty;
            struct hdc_wr_notify_info notify_info;
            struct wr_reduce_info reduce_info;
        } ub;
    };

    uint32_t imm_data;

    uint32_t resv[16U];
};
#endif // RA_RS_CTX_H
