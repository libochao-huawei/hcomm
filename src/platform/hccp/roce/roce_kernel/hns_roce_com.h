/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_COM_H
#define _HNS_ROCE_COM_H

struct hns_roce_idx_que {
    struct hns_roce_buf     idx_buf;
    int             entry_sz;
    u32             buf_size;
    struct ib_umem          *umem;
    struct hns_roce_mtt     mtt;
    unsigned long           *bitmap;
    u32 bitmap_len;
};

struct hns_roce_srq {
    struct ib_srq       ibsrq;
    void (*event)(struct hns_roce_srq *srq, enum hns_roce_event event);
    unsigned long       srqn;
    unsigned int         max;
    int         max_gs;
    int         wqe_shift;
    void __iomem        *db_reg_l;

    refcount_t      refcount;
    struct completion   free;

    struct hns_roce_buf buf;
    u64            *wrid;
    struct ib_umem         *umem;
    struct hns_roce_mtt mtt;
    struct hns_roce_idx_que idx_que;
    spinlock_t      lock;
    unsigned int         head;
    unsigned int         tail;
    u16         wqe_ctr;
    struct mutex        mutex;
};

struct hns_roce_uar_table {
    struct hns_roce_bitmap bitmap;
};

struct hns_roce_qp_table {
    struct hns_roce_bitmap      bitmap;
    spinlock_t          lock;
    struct hns_roce_hem_table   qp_table;
    struct hns_roce_hem_table   irrl_table;
    struct hns_roce_hem_table   trrl_table;
    struct hns_roce_hem_table   scc_ctx_table;
};

struct hns_roce_qpc_timer_table {
    struct hns_roce_bitmap      bitmap;
    spinlock_t          lock;
    struct radix_tree_root      tree;
    struct hns_roce_hem_table   table;
};

struct hns_roce_cq_table {
    struct hns_roce_bitmap      bitmap;
    spinlock_t          lock;
    struct radix_tree_root      tree;
    struct hns_roce_hem_table   table;
};

struct hns_roce_cqc_timer_table {
    struct hns_roce_bitmap      bitmap;
    spinlock_t          lock;
    struct radix_tree_root      tree;
    struct hns_roce_hem_table   table;
};

struct hns_roce_srq_table {
    struct hns_roce_bitmap      bitmap;
    spinlock_t          lock;
    struct radix_tree_root      tree;
    struct hns_roce_hem_table   table;
};

struct hns_roce_raq_table {
    struct hns_roce_buf_list    *e_raq_buf;
};

struct hns_roce_av {
    __le32      port;
    u8          gid_index;
    u8          stat_rate;
    u8          hop_limit;
    u32         flowlabel;
    u16         udp_sport;
    u8          sl;
    u8          tclass;
    u8          dgid[HNS_ROCE_GID_SIZE];
    u8          mac[ETH_ALEN];
    __le16      vlan;
    bool        vlan_en;
};

struct hns_roce_ah {
    struct ib_ah        ibah;
    struct hns_roce_av  av;
    u16 vlan_tag;
    u8 vlan_en;
};

struct hns_roce_cmd_context {
    struct completion   done;
    int         result;
    int         next;
    u64         out_param;
    u16         token;
};

struct hns_roce_cmdq {
    struct dma_pool     *pool;
    struct mutex        hcr_mutex;
    struct semaphore    poll_sem;
    /*
     * Event mode: cmd register mutex protection,
     * ensure to not exceed max_cmds and user use limit region
     */
    struct semaphore    event_sem;
    int         max_cmds;
    spinlock_t      context_lock;
    int         free_head;
    struct hns_roce_cmd_context *context;
    /*
     * Result of get integer part
     * which max_comds compute according a power of 2
     */
    u16         token_mask;
    /*
     * Process whether use event mode, init default non-zero
     * After the event queue of cmd event ready,
     * can switch into event mode
     * close device, switch into poll mode(non event mode)
     */
    u8          use_events;
    u8          toggle;
};

struct hns_roce_cmd_mailbox {
    void               *buf;
    dma_addr_t      dma;
};

struct hns_roce_dev;

struct hns_roce_rinl_sge {
    void            *addr;
    u32         len;
};

struct hns_roce_rinl_wqe {
    struct hns_roce_rinl_sge *sg_list;
    u32          sge_cnt;
};

struct hns_roce_rinl_buf {
    struct hns_roce_rinl_wqe *wqe_list;
    u32          wqe_cnt;
};


// gdr
struct hns_roce_share_sq {
    u32 index;
    u64 sq_addr;
    u64 temp_addr;
    u64 db_addr;
    u64 dfx_addr;
    u32 max_sq_depth;
    u32 max_temp_depth;
    u32 max_db_depth;
    u32 sq_buf_len;
    u32 max_sq_num;
};

struct hns_roce_qp {
    struct ib_qp        ibqp;
    struct hns_roce_buf hr_buf;
    struct hns_roce_wq  rq;
    struct hns_roce_db  rdb;
    struct hns_roce_db  sdb;
    u8          rdb_en;
    u8          sdb_en;
    u32         doorbell_qpn;
    __le32          sq_signal_bits;
    u32         sq_next_wqe;
    struct hns_roce_wq  sq;

    struct ib_umem      *umem;
    struct hns_roce_mtt mtt;
    u32         buff_size;
    struct mutex        mutex;
    u16         xrcdn;
    u8          port;
    u8          phy_port;
    u8          sl;
    u8          resp_depth;
    u8          state;
    u8          next_state; /* record for flush cqe */
    int         attr_mask;  /* record for flush cqe */
    u32         access_flags;
    u32                     atomic_rd_en;
    u32         pkey_index;
    u32         qkey;
    void (*event)(struct hns_roce_qp *qp,
                  enum hns_roce_event event_type);
    unsigned long       qpn;

    atomic_t        refcount;
    struct completion   free;

    struct hns_roce_sge sge;
    u32         next_sge;

    u32         gdr_enable;
    struct hns_roce_share_sq share_sq;

    struct hns_roce_rinl_buf rq_inl_buf;
    struct list_head    qps_list;
    struct list_head    rcq_list;
    struct list_head    scq_list;
};

struct hns_roce_sqp {
    struct hns_roce_qp  hr_qp;
};

struct hns_roce_ib_iboe {
    spinlock_t      lock;
    struct net_device      *netdevs[HNS_ROCE_MAX_PORTS];
    struct notifier_block   nb;
    u8          phy_port[HNS_ROCE_MAX_PORTS];
};

enum {
    HNS_ROCE_EQ_STAT_INVALID  = 0,
    HNS_ROCE_EQ_STAT_VALID    = 1,
};

struct hns_roce_ceqe {
    u32         comp;
};

struct hns_roce_aeqe {
    __le32 asyn;
    union {
        struct {
            __le32 qp;
            u32 rsv0;
            u32 rsv1;
        } qp_event;

        struct {
            __le32 srq;
            u32 rsv0;
            u32 rsv1;
        } srq_event;

        struct {
            __le32 cq;
            u32 rsv0;
            u32 rsv1;
        } cq_event;

        struct {
            __le32 ceqe;
            u32 rsv0;
            u32 rsv1;
        } ce_event;

        struct {
            __le64  out_param;
            __le16  token;
            u8  status;
            u8  rsv0;
        } __packed cmd;
    } event;
};

struct hns_roce_eq {
    struct hns_roce_dev     *hr_dev;
    void __iomem            *doorbell;

    int             type_flag; /* Aeq:1 ceq:0 */
    int             eqn;
    u32             entries;
    int             log_entries;
    int             eqe_size;
    int             irq;
    int             log_page_size;
    int             cons_index;
    struct hns_roce_buf_list    *buf_list;
    int             over_ignore;
    int             coalesce;
    int             arm_st;
    u64             eqe_ba;
    int             eqe_ba_pg_sz;
    int             eqe_buf_pg_sz;
    int             hop_num;
    u64             *bt_l0; /* Base address table for L0 */
    u64             **bt_l1; /* Base address table for L1 */
    u64             **buf;
    dma_addr_t          l0_dma;
    dma_addr_t          *l1_dma;
    dma_addr_t          *buf_dma;
    u32             l0_last_num; /* L0 last chunk num */
    u32             l1_last_num; /* L1 last chunk num */
    int             eq_max_cnt;
    int             eq_period;
    int             shift;
    dma_addr_t          cur_eqe_ba;
    dma_addr_t          nxt_eqe_ba;
    int             event_type;
    int             sub_type;
};

struct hns_roce_eq_table {
    struct hns_roce_eq  *eq;
    void __iomem        **eqc_base; /* only for hw v1 */
};

struct hns_roce_caps {
    u64     fw_ver;
    u8      num_ports;
    int     gid_table_len[HNS_ROCE_MAX_PORTS];
    int     pkey_table_len[HNS_ROCE_MAX_PORTS];
    int     local_ca_ack_delay;
    int     num_uars;
    u32     phy_num_uars;
    u32     max_sq_sg;
    u32     max_sq_inline;  /* 32 */
    u32     max_rq_sg;
    u32     max_extend_sg;
    int     num_qps;
    int             reserved_qps;
    int     num_qpc_timer;
    int     num_cqc_timer;
    u32     max_srq_sg;
    int     num_srqs;
    u32     max_wqes;
    u32     max_srqs;
    u32     max_srq_wrs;
    u32     max_srq_sges;
    u32     max_sq_desc_sz;
    u32     max_rq_desc_sz;
    u32     max_srq_desc_sz;
    int     max_qp_init_rdma;
    int     max_qp_dest_rdma;
    int     num_cqs;
    int     max_cqes;
    int     min_cqes;
    u32     min_wqes;
    int     reserved_cqs;
    int     reserved_srqs;
    u32     max_srqwqes;
    int     num_aeq_vectors;
    int     num_comp_vectors;
    int     num_other_vectors;
    int     num_mtpts;
    u32     num_mtt_segs;
    u32     num_cqe_segs;
    u32     num_srqwqe_segs;
    u32     num_idx_segs;
    int     reserved_mrws;
    int     reserved_uars;
    int     num_pds;
    int     reserved_pds;
    int     num_xrcds;
    int     reserved_xrcds;
    u32     mtt_entry_sz;
    u32     cq_entry_sz;
    u32     page_size_cap;
    u32     reserved_lkey;
    int     mtpt_entry_sz;
    int     qpc_entry_sz;
    int     irrl_entry_sz;
    int     trrl_entry_sz;
    int     cqc_entry_sz;
    int     srqc_entry_sz;
    int     idx_entry_sz;
    int     scc_ctx_entry_sz;
    int     qpc_timer_entry_sz;
    int     cqc_timer_entry_sz;
    u32     pbl_ba_pg_sz;
    u32     pbl_buf_pg_sz;
    u32     pbl_hop_num;
    int     aeqe_depth;
    int     ceqe_depth;
    enum ib_mtu max_mtu;
    u32     qpc_bt_num;
    u32     qpc_timer_bt_num;
    u32     srqc_bt_num;
    u32     cqc_bt_num;
    u32     cqc_timer_bt_num;
    u32     mpt_bt_num;
    u32     scc_ctx_bt_num;
    u32     qpc_ba_pg_sz;
    u32     qpc_buf_pg_sz;
    u32     qpc_hop_num;
    u32     srqc_ba_pg_sz;
    u32     srqc_buf_pg_sz;
    u32     srqc_hop_num;
    u32     cqc_ba_pg_sz;
    u32     cqc_buf_pg_sz;
    u32     cqc_hop_num;
    u32     mpt_ba_pg_sz;
    u32     mpt_buf_pg_sz;
    u32     mpt_hop_num;
    u32     mtt_ba_pg_sz;
    u32     mtt_buf_pg_sz;
    u32     mtt_hop_num;
    u32     scc_ctx_ba_pg_sz;
    u32     scc_ctx_buf_pg_sz;
    u32     scc_ctx_hop_num;
    u32     qpc_timer_ba_pg_sz;
    u32     qpc_timer_buf_pg_sz;
    u32     qpc_timer_hop_num;
    u32     cqc_timer_ba_pg_sz;
    u32     cqc_timer_buf_pg_sz;
    u32     cqc_timer_hop_num;
    u32     cqe_ba_pg_sz;
    u32     cqe_buf_pg_sz;
    u32     cqe_hop_num;
    u32     srqwqe_ba_pg_sz;
    u32     srqwqe_buf_pg_sz;
    u32     srqwqe_hop_num;
    u32     idx_ba_pg_sz;
    u32     idx_buf_pg_sz;
    u32     idx_hop_num;
    u32     eqe_ba_pg_sz;
    u32     eqe_buf_pg_sz;
    u32     eqe_hop_num;
    u32     sl_num;
    u32     tsq_buf_pg_sz;
    u32     tpq_buf_pg_sz;
    u32     chunk_sz;   /* chunk size in non multihop mode */
    u64     flags;
};

struct hns_roce_work {
    struct hns_roce_dev *hr_dev;
    struct work_struct work;
    u32 qpn;
    u32 cqn;
    int event_type;
    int sub_type;
};

struct hns_roce_flush_work {
    struct hns_roce_dev *hr_dev;
    struct work_struct work;
    struct hns_roce_qp *hr_qp;
};

struct hns_roce_stat {
    int cqn;
    int srqn;
    int ceqn;
    int qpn;
    int aeqn;
    int key;
};

enum {
    HNS_ROCE_CMDQ_NORMAL,
    HNS_ROCE_CMDQ_TIMEOUT,
};

enum hns_roce_device_state {
    HNS_ROCE_DEVICE_STATE_INITED,
    HNS_ROCE_DEVICE_STATE_RST_DOWN,
    HNS_ROCE_DEVICE_STATE_UNINIT,
};

struct hns_roce_mbox_post_params {
    u64 in_param;
    u64 out_param;
    u32 in_modifier;
    u8 op_modifier;
    u16 op;
    u16 token;
    int event;
};

struct hns_roce_user_mr_info {
    u64 start;
    u64 length;
    u64 virt_addr;
    int access_flags;
    int flags;
    u32 pdn;
    int npages;
};

#endif
