/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HCCP_CTX_H
#define HCCP_CTX_H

#include "hccp_common.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup libinit
 * rdma/ub gid/eid
 */
union hccp_eid {
    uint8_t raw[16U]; /* Network Order */
    struct {
        uint64_t reserved; /* If IPv4 mapped to IPv6, == 0 */
        uint32_t prefix;   /* If IPv4 mapped to IPv6, == 0x0000ffff */
        uint32_t addr;     /* If IPv4 mapped to IPv6, == IPv4 addr */
    } in4;
    struct {
        uint64_t subnet_prefix;
        uint64_t interface_id;
    } in6;
};

#define GET_EID_BY_IP_MAX_NUM 32
#define DEV_EID_INFO_MAX_NAME 64

struct dev_eid_info {
    char name[DEV_EID_INFO_MAX_NAME];
    uint32_t type;
    uint32_t eid_index;
    union hccp_eid eid;
    uint32_t die_id;
    uint32_t chip_id;
    uint32_t func_id;
    uint32_t resv;
};

struct ctx_init_cfg {
    int mode; /**< refer to enum NetworkMode */
    union {
        struct {
            bool disabled_lite_thread; /**< true will not start lite thread */
        } rdma;
    };
};

struct ctx_init_attr {
    unsigned int phy_id; /**< physical device id */
    union {
        struct {
            uint32_t notify_type; /**< refer to enum notify_type */
            int family; /**< AF_INET(ipv4) or AF_INET6(ipv6) */
            union HccpIpAddr local_ip;
        } rdma;

        struct {
            uint32_t eid_index;
            union hccp_eid eid;
        } ub;
    };
    uint32_t resv[16U];
};

#define MEM_KEY_SIZE 128

struct mem_key {
    // RDMA: 4Bytes for uint32_t rkey
    // UB: 52Bytes for urma_seg_t seg
    uint8_t value[MEM_KEY_SIZE];
    uint8_t size;
};

struct dev_notify_info {
    uint64_t va;
    uint64_t size;
    struct mem_key key;
    uint32_t resv[4U];
};

struct dev_base_attr {
    uint32_t sq_max_depth;
    uint32_t rq_max_depth;
    uint32_t sq_max_sge;
    uint32_t rq_max_sge;
    union {
        struct {
            struct dev_notify_info global_notify_info;
        } rdma;

        struct {
            uint32_t max_jfs_inline_len;
            uint32_t max_jfs_rsge;
            uint32_t die_id;
            uint32_t chip_id;
            uint32_t func_id;
        } ub;
    };

    uint32_t resv[16U];
};

struct mem_info {
    uint64_t addr;
    uint64_t size;
};

enum mem_seg_token_policy {
    MEM_SEG_TOKEN_NONE = 0,
    MEM_SEG_TOKEN_PLAIN_TEXT = 1,
    MEM_SEG_TOKEN_SIGNED = 2,
    MEM_SEG_TOKEN_ALL_ENCRYPTED = 3,
    MEM_SEG_TOKEN_RESERVED = 4
};

enum mem_seg_access_flags {
    MEM_SEG_ACCESS_LOCAL_ONLY         = 1,
    MEM_SEG_ACCESS_READ               = (1 << 1U),
    MEM_SEG_ACCESS_WRITE              = (1 << 2U),
    MEM_SEG_ACCESS_ATOMIC             = (1 << 3U),
};

union reg_seg_flag {
    struct {
        uint32_t token_policy   : 3;  /**< refer to enum mem_seg_token_policy */
        uint32_t cacheable      : 1;  /* 0: URMA_NON_CACHEABLE.
                                         1: URMA_CACHEABLE. */
        uint32_t dsva           : 1;
        uint32_t access         : 6;  /**< refer to enum mem_seg_access_flags */
        uint32_t non_pin        : 1;  /* 0: segment pages pinned.
                                         1: segment pages non-pinned. */
        uint32_t user_iova      : 1;  /* 0: segment without user iova addr.
                                         1: segment with user iova addr. */
        uint32_t token_id_valid : 1;  /* 0: token id in cfg is invalid.
                                         1: token id in cfg is valid. */
        uint32_t reserved       : 18;
    } bs;
    uint32_t value;
};

struct mem_reg_attr {
    struct mem_info mem;
    union {
        struct {
            int access; /**< refer to enum mem_mr_access_flags */
        } rdma;

        struct {
            union reg_seg_flag flags;
            uint32_t token_value; /**< refer to urma_token_t */
            void *token_id_handle; /**< NULL means unspecified, valid if flags.token_id_valid been set */
        } ub;
    };
    uint32_t resv[8U];
};

struct mem_reg_info {
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

struct mr_reg_info_t {
    struct mem_reg_attr in;
    struct mem_reg_info out;
};

union import_seg_flag {
    struct {
        uint32_t cacheable      : 1;  /* 0: URMA_NON_CACHEABLE.
                                         1: URMA_CACHEABLE. */
        uint32_t access         : 6;  /**< refer to enum mem_seg_access_flags */
        uint32_t mapping        : 1;  /* 0: URMA_SEG_NOMAP/
                                         1: URMA_SEG_MAPPED. */
        uint32_t reserved       : 24;
    } bs;
    uint32_t value;
};

struct mem_import_attr {
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

struct mem_import_info {
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

struct mr_import_info_t {
    struct mem_import_attr in;
    struct mem_import_info out;
};

struct hccp_token_id {
    uint32_t token_id;
};

enum jfc_mode {
    JFC_MODE_NORMAL = 0,      /* Corresponding jetty mode：JETTY_MODE_URMA_NORMAL and JETTY_MODE_USER_CTL_NORMAL */
    JFC_MODE_STARS_POLL = 1,  /* Corresponding jetty mode：JETTY_MODE_CACHE_LOCK_DWQE and JETTY_MODE_USER_CTL_NORMAL */
    JFC_MODE_CCU_POLL = 2,    /* Corresponding jetty mode: JETTY_MODE_CCU */
    JFC_MODE_USER_CTL_NORMAL = 3,    /* Corresponding jetty mode: JETTY_MODE_USER_CTL_NORMAL */
    JFC_MODE_MAX
};

union jfc_flag {
    struct {
        uint32_t lock_free         : 1;
        uint32_t jfc_inline        : 1;
        uint32_t reserved          : 30;
    } bs;
    uint32_t value;
};

struct cq_create_attr {
    void *chan_handle;
    uint32_t depth;
    union {
        struct {
            uint64_t cq_context;
            uint32_t mode; /**< refer to enum HCCP_RDMA_NOR_MODE etc. */
            uint32_t comp_vector;
        } rdma;

        struct {
            uint64_t user_ctx;
            enum jfc_mode mode;
            uint32_t ceqn;
            union jfc_flag flag; /**< refer to urma_jfc_flag_t */
            struct {
                bool valid;
                uint32_t cqe_flag; /* Indicates whether the jfc is handling the current die or cross-die CCU CQE */
            } ccu_ex_cfg;
        } ub;
    };
};

struct cq_create_info {
    uint64_t va; /**< refer to struct urma_jfc*, struct ibv_cq* */
    uint32_t id; /**< jfc id */
    uint32_t cqe_size;
    uint64_t buf_addr;
    uint64_t swdb_addr;
};

struct cq_info_t {
    struct cq_create_attr in;
    struct cq_create_info out;
};

union data_plane_cstm_flag {
    struct {
        uint32_t poll_cq_cstm : 1; // 0: hccp poll cq; 1: caller poll cq
        uint32_t reserved : 31;
    } bs;
    uint32_t value;
};

struct chan_info_t {
    struct {
        union data_plane_cstm_flag data_plane_flag;
    } in;
    struct {
        int fd;
    } out;
};

enum jetty_mode {
    JETTY_MODE_URMA_NORMAL = 0,      /* jetty_id belongs to [0, 1023] */
    JETTY_MODE_CACHE_LOCK_DWQE = 1,  /* jetty_id belongs to [1216, 5311] */
    JETTY_MODE_CCU = 2,              /* jetty_id belongs to [1024, 1151] */
    JETTY_MODE_USER_CTL_NORMAL = 3,  /* jetty_id belongs to [5312, 9407] */
    JETTY_MODE_CCU_TA_CACHE = 4,     /* jetty_id belongs to [1024, 1151] */
    JETTY_MODE_MAX
};

enum transport_mode_t {
    CONN_RM = 1, /**< only for UB, Reliable Message */
    CONN_RC = 2, /**< Reliable Connection */
};

union jetty_flag {
    struct {
        uint32_t share_jfr       : 1;  /* 0: URMA_NO_SHARE_JFR.
                                          1: URMA_SHARE_JFR.   */
        uint32_t reserved       : 31;
    } bs;
    uint32_t value;
};

union jfs_flag {
    struct {
        uint32_t lock_free      : 1;  /* default as 0, lock protected */
        uint32_t error_suspend  : 1;  /* 0: error continue; 1: error suspend */
        uint32_t outorder_comp  : 1;  /* 0: not support; 1: support out-of-order completion */
        uint32_t order_type     : 8;  /* (0x0): default, auto config by driver */
                                      /* (0x1): OT, target ordering */
                                      /* (0x2): OI, initiator ordering */
                                      /* (0x3): OL, low layer ordering */
                                      /* (0x4): UNO, unreliable non ordering */
        uint32_t multi_path     : 1;  /* 1: multi-path, 0: single path, for ubagg only. */
        uint32_t reserved       : 20;
    } bs;
    uint32_t value;
};
 
struct jetty_que_cfg_ex {
    uint32_t buff_size;
    uint64_t buff_va;
};

union cstm_jfs_flag {
    struct {
        uint32_t sq_cstm        : 1; /**< valid in jetty mode: JETTY_MODE_CCU */
        uint32_t db_cstm        : 1;
        uint32_t db_ctl_cstm    : 1;
        uint32_t reserved       : 29;
    } bs;
    uint32_t value;
};

struct qp_create_attr {
    void *scq_handle;
    void *rcq_handle;
    void *srq_handle;

    uint32_t sq_depth;
    uint32_t rq_depth;

    enum transport_mode_t transport_mode;

    union {
        struct {
            uint32_t mode; /**< refer to enum HCCP_RDMA_NOR_MODE etc. */
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
            void *token_id_handle; /**< NULL means unspecified */
            uint32_t token_value; /**< refer to urma_token_t; jfr_cfg->token_value */
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
                    bool lock_flag;       /**< false: unlocked, true: locked by buffer. forced: true. */
                    uint32_t sqe_buf_idx; /* base sqe index */
                } ta_cache_mode;
            };
        } ub;
    };

    uint32_t resv[16U];
};

#define DEV_QP_KEY_SIZE 64

struct qp_key {
    // ROCE: qpn(4), psn(4), gid_idx(4), gid(16), tc(4), sl(4), retry_cnt(4), timeout(4) etc
    // UB: jetty_id(24), trans_mode(4)
    uint8_t value[DEV_QP_KEY_SIZE];
    uint8_t size;
};

struct qp_create_info {
    struct qp_key key; /**< for modify qp or import & bind jetty*/
    union {
        struct {
            uint32_t qpn;
        } rdma;

        struct {
            uint32_t uasid;
            uint32_t id; /**< jetty id */
            uint64_t sq_buff_va; /**< valid in jetty mode：JETTY_MODE_CACHE_LOCK_DWQE and JETTY_MODE_USER_CTL_NORMAL */
            uint64_t wqebb_size; /**< valid in jetty mode: JETTY_MODE_CACHE_LOCK_DWQE and JETTY_MODE_USER_CTL_NORMAL */
            uint64_t db_addr;
            uint32_t db_token_id;
            uint64_t ci_addr;
        } ub;
    };
    uint64_t va; /**< refer to struct urma_jetty*, struct ibv_qp* */
    uint32_t resv[16U];
};

enum jetty_grp_policy {
    JETTY_GRP_POLICY_RR = 0,
    JETTY_GRP_POLICY_HASH_HINT = 1,
    JETTY_GRP_POLICY_MAX
};

enum target_type {
    TARGET_TYPE_JFR = 0,
    TARGET_TYPE_JETTY = 1,
    TARGET_TYPE_JETTY_GROUP = 2,
    TARGET_TYPE_MAX
};

enum {
    TOKEN_POLICY_NONE = 0,
    TOKEN_POLICY_PLAIN_TEXT = 1,
    TOKEN_POLICY_SIGNED = 2,
    TOKEN_POLICY_ALL_ENCRYPTED = 3,
    TOKEN_POLICY_RESERVED
};

union import_jetty_flag {
    struct {
        uint32_t token_policy   : 3;
        uint32_t order_type     : 8;  /* (0x0): default, auto config by driver */
                                      /* (0x1): OT, target ordering */
                                      /* (0x2): OI, initiator ordering */
                                      /* (0x3): OL, low layer ordering */
                                      /* (0x4): UNO, unreliable non ordering */
        uint32_t share_tp       : 1;  /* 1: shared tp; 0: non-shared tp. When rc mode is not ta dst ordering,
                                         this flag can only be set to 0. */
        uint32_t reserved       : 20;
    } bs;
    uint32_t value;
};

enum jetty_import_mode {
    JETTY_IMPORT_MODE_NORMAL = 0,
    JETTY_IMPORT_MODE_EXP = 1,
    JETTY_IMPORT_MODE_MAX
};

#define HCCP_MAX_TPID_INFO_NUM 128
 
union get_tp_cfg_flag {
    struct {
        uint32_t ctp : 1;
        uint32_t rtp : 1;
        uint32_t utp : 1;
        uint32_t uboe : 1;
        uint32_t pre_defined : 1;
        uint32_t dynamic_defined : 1;
        uint32_t reserved : 26;
    } bs;
    uint32_t value;
};
 


struct tp_info {
    uint64_t tp_handle;
    uint32_t resv;
};

struct jetty_import_exp_cfg {
    uint64_t tp_handle;
    uint64_t peer_tp_handle;
    uint64_t tag;
    uint32_t tx_psn;
    uint32_t rx_psn;
    uint32_t rsv[16U];
};

struct qp_import_attr {
    struct qp_key key; /**< for RDMA, save key on rem_qp_handle for bind to modify qp */
    union {
        struct {
            enum jetty_import_mode mode;
            uint32_t token_value; /**< refer to urma_token_t */
            enum jetty_grp_policy policy; /**< refer to urma_jetty_grp_policy_t */
            enum target_type type; /**< refer to urma_target_type */
            union import_jetty_flag flag; /**< refer to urma_import_jetty_flag_t */
            struct jetty_import_exp_cfg exp_import_cfg; /**< only valid on mode JETTY_IMPORT_MODE_EXP */
            uint32_t tp_type; /**< refer to urma_tp_type_t */
        } ub;
    };
    uint32_t resv[7U];
};

struct qp_import_info {
    union {
        struct {
            uint64_t tjetty_handle; /**< refer to urma_target_jetty_t *tjetty */
            uint32_t tpn; /**< refer to urma_tp_t tp */
        } ub;
    };
    uint32_t resv[8U];
};

struct qp_import_info_t {
    struct qp_import_attr in;
    struct qp_import_info out;
};

struct wr_sge_list {
    uint64_t addr;
    uint32_t len;
    void *lmem_handle;
};

struct wr_notify_info {
    uint64_t notify_data; /**< notify data */
    uint64_t notify_addr; /**< remote notify addr */
    void *notify_handle; /**< remote notify handle */
};

struct wr_reduce_info {
    bool reduce_en;
    uint8_t reduce_opcode;
    uint8_t reduce_data_type;
};

enum ra_ub_opcode {
    RA_UB_OPC_WRITE               = 0x00,
    RA_UB_OPC_WRITE_NOTIFY        = 0x02,
    RA_UB_OPC_READ                = 0x10,
    RA_UB_OPC_NOP                 = 0x51,
    RA_UB_OPC_LAST
};

union jfs_wr_flag {
    struct {
        uint32_t place_order : 2;       /* 0: There is no order with other WR.
                                           1: relax order.
                                           2: strong order.
                                           3: reserve. see urma_order_type_t */
        uint32_t comp_order : 1;        /* 0: There is no completion order with other WR.
                                           1: Completion order with previous WR. */
        uint32_t fence : 1;             /* 0: There is no fence.
                                           1: Fence with previous read and atomic WR. */
        uint32_t solicited_enable : 1;  /* 0: not solicited.
                                           1: Solicited. */
        uint32_t complete_enable : 1;   /* 0: DO not Generate CR for this WR.
                                           1: Generate CR for this WR after the WR is completed. */
        uint32_t inline_flag : 1;       /* 0: Nodata.
                                           1: Inline data. */
        uint32_t reserved : 25;
    } bs;
    uint32_t value;
};

struct send_wr_data {
    struct wr_sge_list *sges;
    uint32_t num_sge; /**< size of segs, not exceeds to MAX_SGE_NUM */

    uint8_t *inline_data;
    uint32_t inline_size; /**< size of inline_data, see struct dev_base_attr */

    uint64_t remote_addr;
    void *rmem_handle;

    union {
        struct {
            uint64_t wr_id;
            enum RaWrOpcode opcode;
            unsigned int flags; /**< reference to ra_send_flags */
            struct WrAuxInfo aux; /**< aux info */
        } rdma;

        struct {
            uint64_t user_ctx;
            enum ra_ub_opcode opcode; /**< refer to urma_opcode_t */
            union jfs_wr_flag flags; /**< refer to urma_jfs_wr_flag_t */
            void *rem_qp_handle; /**< resv for RM use */
            struct wr_notify_info notify_info; /**< required for opcode RA_UB_OPC_WRITE_NOTIFY */
            struct wr_reduce_info reduce_info; /**<reduce is enabled when reduce_en is set to true */
        } ub;
    };

    uint32_t imm_data;

    uint32_t resv[16U];
};

struct ub_post_info {
    uint16_t funcId : 7;
    uint16_t dieId : 1;
    uint16_t rsv : 8;
    uint16_t jettyId;
    // doorbell value
    uint16_t piVal;
    // direct wqe
    uint8_t dwqe[128U];
    uint16_t dwqe_size; /**< size of dwqe calc by piVal, 64 or 128 */
};

struct send_wr_resp {
    union {
        struct WqeInfoT wqe_tmp; /**< wqe template info used for V80 offload */
        struct DbInfo db; /**< doorbell info used for V71 and V80 opbase */
        struct ub_post_info doorbell_info; /**< doorbell info used for UB */
        uint8_t resv[384U]; /**< resv for write value doorbell info */
    };
};

#define CUSTOM_CHAN_DATA_MAX_SIZE 2048

struct custom_chan_info_in {
    char data[CUSTOM_CHAN_DATA_MAX_SIZE];
    unsigned int offset_start;
    unsigned int op;
};

struct custom_chan_info_out {
    char data[CUSTOM_CHAN_DATA_MAX_SIZE];
    unsigned int offset_next;
    int op_ret;
};

enum jetty_attr_mask {
    JETTY_ATTR_RX_THRESHOLD = 0x1,
    JETTY_ATTR_STATE = 0x1 << 1
};

enum jetty_state {
    JETTY_STATE_RESET = 0,
    JETTY_STATE_READY,
    JETTY_STATE_SUSPENDED,
    JETTY_STATE_ERROR
};

struct jetty_attr {
    uint32_t mask; // mask value refer to enum jetty_attr_mask
    uint32_t rx_threshold;
    enum jetty_state state;
    uint32_t resv[2U];
};

#define HCCP_MAX_QP_QUERY_NUM 128U
#define HCCP_MAX_QP_DESTROY_BATCH_NUM 768U

enum aux_info_in_type {
    AUX_INFO_IN_TYPE_CQE = 0,
    AUX_INFO_IN_TYPE_AE = 1,
    AUX_INFO_IN_TYPE_MAX,
};

struct aux_info_in {
    enum aux_info_in_type type;
    union {
        struct {
            uint32_t status;
            uint8_t s_r;
        } cqe;
        struct {
            uint32_t event_type;
        } ae;
    };
    uint8_t resv[7U];
};

#define AUX_INFO_NUM_MAX 256U

struct aux_info_out {
    uint32_t aux_info_type[AUX_INFO_NUM_MAX];
    uint32_t aux_info_value[AUX_INFO_NUM_MAX];
    uint32_t aux_info_num;
};

#define CR_ERR_INFO_MAX_NUM 96U

struct CrErrInfo {
    uint32_t status;
    uint32_t jetty_id;
    struct timeval time;
    uint32_t resv[2U];
};

struct async_event {
    uint32_t res_id;
    uint32_t event_type;
};

#define ASYNC_EVENT_MAX_NUM 128U

/**
 * @ingroup libudma
 * @brief get total dev eid info num
 * @param info [IN] see RaInfo
 * @param num [OUT] num of dev eid info
 * @see ra_get_dev_eid_info_list
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_dev_eid_info_num(struct RaInfo info, unsigned int *num);

/**
 * @ingroup libudma
 * @brief get all dev info list
 * @param info [IN] see RaInfo
 * @param info_list [IN/OUT] dev eid info list
 * @param num [IN/OUT] num of dev eid info list
 * @see ra_get_dev_eid_info_num
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_dev_eid_info_list(struct RaInfo info, struct dev_eid_info info_list[],
    unsigned int *num);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief ctx initialization will start lite thread by default
 * @param cfg [IN] ctx init cfg
 * @param attr [IN] ctx init attr
 * @param ctx_handle [OUT] ctx handle
 * @see ra_ctx_deinit
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_init(struct ctx_init_cfg *cfg, struct ctx_init_attr *attr, void **ctx_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief get dev base attr
 * @param ctx_handle [IN] ctx handle
 * @param attr [OUT] dev base attr
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_dev_base_attr(void *ctx_handle, struct dev_base_attr *attr);

/**
 * @ingroup libudma
 * @brief get async event
 * @param ctx_handle [IN] ctx handle
 * @param events [IN/OUT] see struct async_event
 * @param num [IN/OUT] num of events
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_get_async_events(void *ctx_handle, struct async_event events[], unsigned int *num);

/**
 * @ingroup libudma
 * @brief get corresponding eid by ip
 * @param ctx_handle [IN] ctx handle
 * @param ip [IN] ip array, see struct IpInfo
 * @param eid [IN/OUT] eid array, see union hccp_eid
 * @param num [IN/OUT] num of ip and eid array, max num is GET_EID_BY_IP_MAX_NUM
 * @see ra_ctx_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_get_eid_by_ip(void *ctx_handle, struct IpInfo ip[], union hccp_eid eid[],
    unsigned int *num);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief ctx deinitialization
 * @param ctx_handle [IN] ctx handle
 * @see ra_ctx_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_deinit(void *ctx_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief alloc token id
 * @param ctx_handle [IN] ctx handle
 * @param info [OUT] see struct hccp_token_id
 * @param token_id_handle [OUT] token id handle
 * @see ra_ctx_token_id_free
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_token_id_alloc(void *ctx_handle, struct hccp_token_id *info, void **token_id_handle);
 
/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief free token id
 * @param ctx_handle [IN] ctx handle
 * @param token_id_handle [IN] token id handle
 * @see ra_ctx_token_id_alloc
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_token_id_free(void *ctx_handle, void *token_id_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief register local mem
 * @param ctx_handle [IN] ctx handle
 * @param lmem_info [IN/OUT] lmem reg info
 * @param lmem_handle [OUT] lmem handle
 * @see ra_ctx_lmem_unregister
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_register(void *ctx_handle, struct mr_reg_info_t *lmem_info, void **lmem_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unregister local mem
 * @param ctx_handle [IN] ctx handle
 * @param lmem_handle [IN] lmem handle
 * @see ra_ctx_lmem_register
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_lmem_unregister(void *ctx_handle, void *lmem_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief import remote mem
 * @param ctx_handle [IN] ctx handle
 * @param rmem_info [IN/OUT] rmem info
 * @param rmem_handle [OUT] rmem handle, key as rkey for send wr
 * @see ra_ctx_rmem_unimport
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_rmem_import(void *ctx_handle, struct mr_import_info_t *rmem_info, void **rmem_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unimport remote mem
 * @param ctx_handle [IN] ctx handle
 * @param rmem_handle [IN] rmem handle
 * @see ra_ctx_rmem_import
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_rmem_unimport(void *ctx_handle, void *rmem_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief  create comp channel
 * @param ctx_handle [IN] ctx handle
 * @param chan_info [IN/OUT] see chan_info_t
 * @param chan_handle [OUT] comp chan handle
 * @see ra_ctx_chan_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_chan_create(void *ctx_handle, struct chan_info_t *chan_info, void **chan_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief  destroy comp channel
 * @param ctx_handle [IN] ctx handle
 * @param chan_handle [IN] comp chan handle
 * @see ra_ctx_chan_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_chan_destroy(void *ctx_handle, void *chan_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief create jfc/cq
 * @param ctx_handle [IN] ctx handle
 * @param info [IN/OUT] cq info
 * @param cq_handle [OUT] cq handle
 * @see ra_ctx_cq_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_cq_create(void *ctx_handle, struct cq_info_t *info, void **cq_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief destroy jfc/cq
 * @param ctx_handle [IN] ctx handle
 * @param cq_handle [IN] cq handle
 * @see ra_ctx_cq_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_cq_destroy(void *ctx_handle, void *cq_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief create jetty/qp
 * @param ctx_handle [IN] ctx handle
 * @param attr [IN] qp attr
 * @param info [OUT] qp info
 * @param qp_handle [OUT] qp handle
 * @see ra_ctx_qp_destroy
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_create(void *ctx_handle, struct qp_create_attr *attr, struct qp_create_info *info,
    void **qp_handle);

/**
 * @ingroup libudma
 * @brief batch query jetty attr
 * @param qp_handle [IN] qp handle
 * @param attr [IN/OUT] see struct jetty_attr
 * @param num [IN/OUT] size of qp_handle and attr array, max num is HCCP_MAX_QP_QUERY_NUM
 * @see ra_ctx_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_query_batch(void *qp_handle[], struct jetty_attr attr[], unsigned int *num);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief destroy jetty/qp
 * @param qp_handle [IN] qp handle
 * @see ra_ctx_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_destroy(void *qp_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief import jetty/prepare rem_qp_handle for modify qp
 * @param ctx_handle [IN] ctx handle
 * @param qp_info [IN/OUT] qp import info
 * @param rem_qp_handle [OUT] remote qp handle
 * @see ra_ctx_qp_unimport
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_import(void *ctx_handle, struct qp_import_info_t *qp_info, void **rem_qp_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unimport jetty
 * @param ctx_handle [IN] ctx handle
 * @param rem_qp_handle [IN] qp handle
 * @see ra_ctx_qp_import
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unimport(void *ctx_handle, void *rem_qp_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief bind jetty/modify qp
 * @param qp_handle [IN] qp handle
 * @param rem_qp_handle [IN] rem qp handle
 * @see ra_ctx_qp_unbind
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_bind(void *qp_handle, void *rem_qp_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief unbind jetty
 * @param qp_handle [IN] qp handle
 * @see ra_ctx_qp_bind
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_qp_unbind(void *qp_handle);

/**
 * @ingroup librdma
 * @ingroup libudma
 * @brief batch post send wr
 * @param qp_handle [IN] qp handle
 * @param send_wr_data [IN] send wr data
 * @param op_resp [IN/OUT] send wr resp
 * @param num [IN] size of wr_list & op_resp
 * @param complete_num [OUT] number of wr been post send successfully
 * @see ra_ctx_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_batch_send_wr(void *qp_handle, struct send_wr_data wr_list[], struct send_wr_resp op_resp[],
    unsigned int num, unsigned int *complete_num);

/**
 * @ingroup libudma
 * @brief custom channel
 * @param info [IN] see RaInfo
 * @param in [IN] see custom_chan_info_in
 * @param out [OUT] see custom_chan_info_out
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_custom_channel(struct RaInfo info, struct custom_chan_info_in *in,
    struct custom_chan_info_out *out);

/**
 * @ingroup libudma
 * @brief update ci
 * @param qp_handle [IN] qp handle
 * @param ci [IN] ci
 * @see ra_ctx_qp_create
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_update_ci(void *qp_handle, uint16_t ci);

/**
 * @ingroup libudma
 * @brief get aux info
 * @param ctx_handle [IN] ctx handle
 * @param in [IN] see struct aux_info_in
 * @param out [OUT] see struct aux_info_out
 * @see ra_ctx_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_get_aux_info(void *ctx_handle, struct aux_info_in *in, struct aux_info_out *out);

/**
 * @ingroup libudma
 * @brief get cr err info by ctx_handle
 * @param ctx_handle [IN] ctx_handle
 * @param info_list [IN/OUT] cr err info
 * @param num [IN/OUT] num of cr err info, max num of input is CR_ERR_INFO_MAX_NUM
 * @see ra_ctx_init
 * @retval #zero Success
 * @retval #non-zero Failure
*/
HCCP_ATTRI_VISI_DEF int ra_ctx_get_cr_err_info_list(void *ctx_handle, struct CrErrInfo *info_list,
    unsigned int *num);
#ifdef __cplusplus
}
#endif

#endif // HCCP_CTX_H
