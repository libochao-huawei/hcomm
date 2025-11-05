/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_DEV_H
#define _HNS_ROCE_DEV_H

#define DRV_NAME "hns_roce"

/* hip08 is a pci device, it includes two version according pci version id */
#define PCI_REVISION_ID_HIP08_A         0x20
#define PCI_REVISION_ID_HIP08_B         0x21

#define HNS_ROCE_HW_VER1    ('h' << 24 | 'i' << 16 | '0' << 8 | '6')

#define HNS_ROCE_MAX_MSG_LEN            0x80000000

#define HNS_ROCE_ALOGN_UP(a, b) ((((a) + (b) - 1) / (b)) * (b))

#define HNS_ROCE_IB_MIN_SQ_STRIDE       6

#define HNS_ROCE_BA_SIZE            (32 * 4096)

#define BA_BYTE_LEN             8

#define BITS_PER_BYTE               8

/* Hardware specification only for v1 engine */
#define HNS_ROCE_MIN_CQE_NUM            0x40
#define HNS_ROCE_MIN_WQE_NUM            0x20

/* Hardware specification only for v1 engine */
#define HNS_ROCE_MAX_INNER_MTPT_NUM     0x7
#define HNS_ROCE_MAX_MTPT_PBL_NUM       0x100000
#define HNS_ROCE_MAX_SGE_NUM            2


#define HNS_ROCE_EACH_FREE_CQ_WAIT_MSECS    20
#define HNS_ROCE_MAX_FREE_CQ_WAIT_CNT   \
    (5000 / HNS_ROCE_EACH_FREE_CQ_WAIT_MSECS)
#define HNS_ROCE_CQE_WCMD_EMPTY_BIT     0x2
#define HNS_ROCE_MIN_CQE_CNT            16

#define HNS_ROCE_MAX_IRQ_NUM            128

#define HNS_ROCE_SGE_IN_WQE         2
#define HNS_ROCE_SGE_SHIFT          4

#define EQ_ENABLE               1
#define EQ_DISABLE              0

#define HNS_ROCE_CEQ                0
#define HNS_ROCE_AEQ                1

#define HNS_ROCE_CEQ_ENTRY_SIZE         0x4
#define HNS_ROCE_AEQ_ENTRY_SIZE         0x10

#define HNS_ROCE_SL_SHIFT           28
#define HNS_ROCE_TCLASS_SHIFT           20
#define HNS_ROCE_FLOW_LABEL_MASK        0xfffff

#define HNS_ROCE_MAX_PORTS          6
#define HNS_ROCE_MAX_GID_NUM            16
#define HNS_ROCE_GID_SIZE           16

#define HNS_ROCE_SGE_SIZE           16

#define HNS_ROCE_HOP_NUM_0          0xff

#define BITMAP_NO_RR                0
#define BITMAP_RR               1

#define PKEY_ID                 0xffff
#define GUID_LEN                8
#define NODE_DESC_SIZE              64
#define DB_REG_OFFSET               0x1000

#define HNS_ROCE_CEQ_MAX_BURST_NUM      0xffff
#define HNS_ROCE_CEQ_MAX_INTERVAL       0xffff
#define HNS_ROCE_EQ_MAXCNT_MASK         1
#define HNS_ROCE_EQ_PERIOD_MASK         2

#define SERV_TYPE_RC                0
#define SERV_TYPE_RD                2
#define SERV_TYPE_UC                1
#define SERV_TYPE_UD                3
#define SERV_TYPE_XRC               5
/* Configure to HW for PAGE_SIZE larger than 4KB */
#define PG_SHIFT_OFFSET             (PAGE_SHIFT - 12)

#define PAGES_SHIFT_8               8
#define PAGES_SHIFT_16              16
#define PAGES_SHIFT_24              24
#define PAGES_SHIFT_32              32

#define HNS_ROCE_PCI_BAR_NR         2

#define HNS_ROCE_IDX_QUE_ENTRY_SZ       4

#define HNS_ROCE_FRMR_MAX_PA            512

#define SRQ_DB_REG              0x230

#define ROCE_DEV_STATE_CHECK_GOTO_OUT(state, bad_wr, wr, ret, nreq) \
{ if ((state) >= HNS_ROCE_DEVICE_STATE_RST_DOWN) {(*(bad_wr)) = (wr);(ret) = -EIO;(nreq) = 0;goto out;}}

enum {
    MR_TYPE_MR = 0,
    MR_TYPE_FRMR = 1,
    MR_TYPE_DMA = 3,
};

enum {
    HNS_ROCE_SUPPORT_RQ_RECORD_DB = 1 << 0,
    HNS_ROCE_SUPPORT_SQ_RECORD_DB = 1 << 1,
};

enum {
    HNS_ROCE_SUPPORT_CQ_RECORD_DB = 1 << 0,
};

enum hns_roce_qp_state {
    HNS_ROCE_QP_STATE_RST,
    HNS_ROCE_QP_STATE_INIT,
    HNS_ROCE_QP_STATE_RTR,
    HNS_ROCE_QP_STATE_RTS,
    HNS_ROCE_QP_STATE_SQD,
    HNS_ROCE_QP_STATE_ERR,
    HNS_ROCE_QP_NUM_STATE,
};

enum queue_type {
    HNS_ROCE_SQ,
    HNS_ROCE_RQ,
    HNS_ROCE_CQ,
};

enum hns_roce_event {
    HNS_ROCE_EVENT_TYPE_PATH_MIG                  = 0x01,
    HNS_ROCE_EVENT_TYPE_PATH_MIG_FAILED           = 0x02,
    HNS_ROCE_EVENT_TYPE_COMM_EST                  = 0x03,
    /* sqd comes from rts by modifying mb, after then hardware will clear the pending wqe and
    not handle incoming wqe any more, which triggers this event */
    HNS_ROCE_EVENT_TYPE_SQ_DRAINED                = 0x04,
    HNS_ROCE_EVENT_TYPE_WQ_CATAS_ERROR            = 0x05,   /* invalid wqe  filed */
    HNS_ROCE_EVENT_TYPE_INV_REQ_LOCAL_WQ_ERROR    = 0x06,   /* invalid opcode */
    HNS_ROCE_EVENT_TYPE_LOCAL_WQ_ACCESS_ERROR     = 0x07,   /* illegal lkey */
    HNS_ROCE_EVENT_TYPE_SRQ_LIMIT_REACH           = 0x08,
    HNS_ROCE_EVENT_TYPE_SRQ_LAST_WQE_REACH        = 0x09,
    HNS_ROCE_EVENT_TYPE_SRQ_CATAS_ERROR           = 0x0a,
    HNS_ROCE_EVENT_TYPE_CQ_ACCESS_ERROR           = 0x0b,    /* not used */
    HNS_ROCE_EVENT_TYPE_CQ_OVERFLOW               = 0x0c,    /* cq overflow */
    HNS_ROCE_EVENT_TYPE_CQ_ID_INVALID             = 0x0d,    /* not used, deledte */
    HNS_ROCE_EVENT_TYPE_PORT_CHANGE               = 0x0f,    /* not used, deleted */
    /* 0x10 and 0x11 is unused in currently application case */
    HNS_ROCE_EVENT_TYPE_DB_OVERFLOW               = 0x12,    /* db overflow */
    HNS_ROCE_EVENT_TYPE_MB                        = 0x13,
    HNS_ROCE_EVENT_TYPE_CEQ_OVERFLOW              = 0x14,
    HNS_ROCE_EVENT_TYPE_FLR               = 0x15,
};

/* Local Work Queue Catastrophic Error,SUBTYPE 0x5 */
enum {
    HNS_ROCE_LWQCE_QPC_ERROR        = 1,
    HNS_ROCE_LWQCE_MTU_ERROR        = 2,
    HNS_ROCE_LWQCE_WQE_BA_ADDR_ERROR    = 3,
    HNS_ROCE_LWQCE_WQE_ADDR_ERROR       = 4,
    HNS_ROCE_LWQCE_SQ_WQE_SHIFT_ERROR   = 5,
    HNS_ROCE_LWQCE_SL_ERROR         = 6,
    HNS_ROCE_LWQCE_PORT_ERROR       = 7,
};

/* Local Access Violation Work Queue Error,SUBTYPE 0x7 */
enum {
    HNS_ROCE_LAVWQE_R_KEY_VIOLATION     = 1,
    HNS_ROCE_LAVWQE_LENGTH_ERROR        = 2,
    HNS_ROCE_LAVWQE_VA_ERROR        = 3,
    HNS_ROCE_LAVWQE_PD_ERROR        = 4,
    HNS_ROCE_LAVWQE_RW_ACC_ERROR        = 5,
    HNS_ROCE_LAVWQE_KEY_STATE_ERROR     = 6,
    HNS_ROCE_LAVWQE_MR_OPERATION_ERROR  = 7,
};

/* DOORBELL overflow subtype */
enum {
    HNS_ROCE_DB_SUBTYPE_SDB_OVF     = 1,
    HNS_ROCE_DB_SUBTYPE_SDB_ALM_OVF     = 2,
    HNS_ROCE_DB_SUBTYPE_ODB_OVF     = 3,
    HNS_ROCE_DB_SUBTYPE_ODB_ALM_OVF     = 4,
    HNS_ROCE_DB_SUBTYPE_SDB_ALM_EMP     = 5,
    HNS_ROCE_DB_SUBTYPE_ODB_ALM_EMP     = 6,
};

/* RQ&SRQ related operations */
enum {
    HNS_ROCE_OPCODE_SEND_DATA_RECEIVE   = 0x06,
    HNS_ROCE_OPCODE_RDMA_WITH_IMM_RECEIVE   = 0x07,
};

enum {
    HNS_ROCE_CAP_FLAG_REREG_MR      = BIT(0),
    HNS_ROCE_CAP_FLAG_ROCE_V1_V2        = BIT(1),
    HNS_ROCE_CAP_FLAG_RQ_INLINE     = BIT(2),
    HNS_ROCE_CAP_FLAG_RECORD_DB     = BIT(3),
    HNS_ROCE_CAP_FLAG_SQ_RECORD_DB      = BIT(4),
    HNS_ROCE_CAP_FLAG_XRC           = BIT(6),
    HNS_ROCE_CAP_FLAG_SRQ           = BIT(5),
    HNS_ROCE_CAP_FLAG_MW            = BIT(7),
    HNS_ROCE_CAP_FLAG_FRMR          = BIT(8),
    HNS_ROCE_CAP_FLAG_QP_FLOW_CTRL      = BIT(9),
    HNS_ROCE_CAP_FLAG_ATOMIC        = BIT(10),
};

enum hns_roce_mtt_type {
    MTT_TYPE_WQE,
    MTT_TYPE_CQE,
    MTT_TYPE_SRQWQE,
    MTT_TYPE_IDX
};

#define HNS_ROCE_DB_TYPE_COUNT          2
#define HNS_ROCE_DB_UNIT_SIZE           4

enum {
    HNS_ROCE_DB_PER_PAGE = PAGE_SIZE / 4
};

enum hns_roce_reset_stage {
    HNS_ROCE_STATE_NON_RST,
    HNS_ROCE_STATE_RST_BEF_DOWN,
    HNS_ROCE_STATE_RST_DOWN,
    HNS_ROCE_STATE_RST_UNINIT,
    HNS_ROCE_STATE_RST_INIT,
    HNS_ROCE_STATE_RST_INITED,
};

enum hns_roce_instance_state {
    HNS_ROCE_STATE_NON_INIT,
    HNS_ROCE_STATE_INIT,
    HNS_ROCE_STATE_INITED,
    HNS_ROCE_STATE_UNINIT,
};

enum {
    HNS_ROCE_RST_DIRECT_RETURN      = 0,
};

enum {
    CMD_RST_PRC_OTHERS,
    CMD_RST_PRC_SUCCESS,
    CMD_RST_PRC_EBUSY,
};

#define HNS_ROCE_CMD_SUCCESS            1

#define HNS_ROCE_PORT_DOWN          0
#define HNS_ROCE_PORT_UP            1

#define HNS_ROCE_MTT_ENTRY_PER_SEG      8

#define PAGE_ADDR_SHIFT             12

#define HNS_ROCE_DISABLE_DB         1

struct hns_roce_uar {
    u64     pfn;
    unsigned long   index;
    unsigned long   logic_idx;
};

struct hns_roce_vma_data {
    struct list_head list;
    struct vm_area_struct *vma;
    struct mutex *vma_list_mutex;
};

struct hns_roce_ucontext {
    struct ib_ucontext  ibucontext;
    struct hns_roce_uar uar;
    struct list_head    page_list;
    struct mutex        page_mutex;
    struct list_head    vma_list;
    struct mutex        vma_list_mutex;

    struct hns_roce_notify_node notify_node;
};

struct hns_roce_pd {
    struct ib_pd        ibpd;
    unsigned long       pdn;
};

struct hns_roce_xrcd {
    struct ib_xrcd      ibxrcd;
    unsigned long       xrcdn;
    struct ib_pd           *pd;
    struct ib_cq           *cq;
};

struct hns_roce_bitmap {
    unsigned long       last;
    unsigned long       top;
    unsigned long       max;
    unsigned long       reserved_top;
    unsigned long       mask;
    spinlock_t      lock;
    unsigned long       *table;
};

struct rreg_wrt_mtpt_info {
    int flags;
    u32 pdn;
    int mr_access_flags;
    u64 iova;
    u64 size;
};

struct hns_roce_wrtcqc_info {
    dma_addr_t dma_handle;
    int nent;
    u32 vector;
};

struct hns_roce_abs_filed {
    int attr_mask;
    enum ib_qp_state cur_state;
    enum ib_qp_state new_state;
};

struct hns_roce_srq_info {
    u32 pdn;
    u32 cqn;
    u16 xrcd;
};

struct hns_roce_wrt_srqc_info {
    u32 pdn;
    u16 xrcd;
    u32 cqn;
    dma_addr_t dma_handle_wqe;
    dma_addr_t dma_handle_idx;
};

/* Order bitmap length -- bit num compute formula: 1 << (max_order - order)
 * Order = 0: bitmap is biggest, order = max bitmap is least (only a bit)
 * Every bit repesent to a partner free/used status in bitmap
 *
 * Initial, bits of other bitmap are all 0 except that a bit of max_order is 1
 * Bit = 1 represent to idle and available; bit = 0: not available
 */
struct hns_roce_buddy {
    unsigned long **bits;
    u32            *num_free;
    int             max_order;
    spinlock_t      lock;
};

/* For Hardware Entry Memory */
struct hns_roce_hem_table {
    u32     type; /* HEM type: 0 = qpc, 1 = mtt, 2 = cqc, 3 = srq, 4 = other */
    unsigned long   num_hem;
    unsigned long   num_obj;
    unsigned long   obj_size;
    unsigned long   table_chunk_size;
    int     lowmem;
    struct mutex    mutex;
    struct hns_roce_hem **hem;
    u64     **bt_l1;
    dma_addr_t  *bt_l1_dma_addr;
    u64     **bt_l0;
    dma_addr_t  *bt_l0_dma_addr;
};

struct hns_roce_mtt {
    unsigned long       first_seg;
    int         order;
    int         page_shift;
    enum hns_roce_mtt_type  mtt_type;
};

struct hns_roce_mw {
    struct ib_mw        ibmw;
    u32         pdn;
    u32         rkey;
    int         enabled; /* MW's active status */
    u32         pbl_buf_pg_sz;
    u32         pbl_ba_pg_sz;
    u32         pbl_hop_num;
};

/* Only support 4K page size for mr register */
#define MR_SIZE_4K 0

#define PROCESS_SIGN_LENGTH 49
#define PROCESS_RESV_LENGTH 4

struct process_sign {
    u32 tgid;
    u32 devid; /* chip_id */
    u32 vfid;
    char sign[PROCESS_SIGN_LENGTH];
    char resv[PROCESS_RESV_LENGTH];
};

struct hns_roce_umem {
    struct ib_peer_memory_client *peer_mem_client;
    struct invalidation_ctx *invalidation_ctx;
    struct process_sign psign;
    void    *peer_mem_client_context;
    unsigned int page_shift;
    unsigned int npages;
};

struct hns_roce_mr_alloc_para {
    u32 pd;
    u64 iova;
    u64 size;
    u32 access;
};

struct hns_roce_mr {
    struct ib_mr        ibmr;
    struct ib_umem      *umem;
    struct hns_roce_umem    hr_umem;
    u64         iova; /* MR's virtual orignal addr */
    u64         size; /* Address range of MR */
    u32         key; /* Key of MR */
    u32         pd;   /* PD num of MR */
    u32         access; /* Access permission of MR */
    u32         npages;
    int         enabled; /* MR's active status */
    int         type;   /* MR's register type */
    u64         *pbl_buf; /* MR's PBL space */
    dma_addr_t      pbl_dma_addr;   /* MR's PBL space PA */
    u32         pbl_size; /* PA number in the PBL */
    u64         pbl_ba; /* page table address */
    u32         l0_chunk_last_num; /* L0 last number */
    u32         l1_chunk_last_num; /* L1 last number */
    u64         **pbl_bt_l2; /* PBL BT L2 */
    u64         **pbl_bt_l1; /* PBL BT L1 */
    u64         *pbl_bt_l0; /* PBL BT L0 */
    dma_addr_t      *pbl_l2_dma_addr; /* PBL BT L2 dma addr */
    dma_addr_t      *pbl_l1_dma_addr; /* PBL BT L1 dma addr */
    dma_addr_t      pbl_l0_dma_addr; /* PBL BT L0 dma addr */
    u32         pbl_ba_pg_sz; /* BT chunk page size */
    u32         pbl_buf_pg_sz; /* buf chunk page size */
    u32         pbl_hop_num; /* multi-hop number */
};

struct hns_roce_mr_table {
    struct hns_roce_bitmap      mtpt_bitmap;
    struct hns_roce_buddy       mtt_buddy;
    struct hns_roce_hem_table   mtt_table;
    struct hns_roce_hem_table   mtpt_table;
    struct hns_roce_buddy       mtt_cqe_buddy;
    struct hns_roce_hem_table   mtt_cqe_table;
    struct hns_roce_buddy       mtt_srqwqe_buddy;
    struct hns_roce_hem_table   mtt_srqwqe_table;
    struct hns_roce_buddy       mtt_idx_buddy;
    struct hns_roce_hem_table   mtt_idx_table;
};

struct hns_roce_wq {
    u64     *wrid;     /* Work request ID */
    spinlock_t  lock;
    int     wqe_cnt;  /* WQE num */
    u32     max_post;
    int     max_gs;
    int     offset;
    int     wqe_shift; /* WQE size */
    u32     head;
    u32     tail;
    void __iomem    *db_reg_l;
};

struct hns_roce_sge {
    int     sge_cnt;  /* SGE num */
    int     offset;
    int     sge_shift; /* SGE size */
};

struct hns_roce_buf_list {
    void        *buf;
    dma_addr_t  map;
};

struct hns_roce_buf {
    struct hns_roce_buf_list    direct;
    struct hns_roce_buf_list    *page_list;
    int             nbufs;
    u32             npages;
    int             page_shift;
};

struct hns_roce_db_pgdir {
    struct list_head    list;
    DECLARE_BITMAP(order0, HNS_ROCE_DB_PER_PAGE);
    DECLARE_BITMAP(order1, HNS_ROCE_DB_PER_PAGE / HNS_ROCE_DB_TYPE_COUNT);
    unsigned long       *bits[HNS_ROCE_DB_TYPE_COUNT];
    u32         *page;
    dma_addr_t      db_dma;
};

struct hns_roce_user_db_page {
    struct list_head    list;
    struct ib_umem      *umem;
    unsigned long       user_virt;
    refcount_t      refcount;
};

struct hns_roce_db {
    u32     *db_record;
    union {
        struct hns_roce_db_pgdir *pgdir;
        struct hns_roce_user_db_page *user_page;
    } u;
    dma_addr_t  dma;
    void        *virt_addr;
    int     index;
    int     order;
};

struct hns_roce_cq_buf {
    struct hns_roce_buf hr_buf;
    struct hns_roce_mtt hr_mtt;
};

struct hns_roce_cq {
    struct ib_cq            ib_cq;
    struct hns_roce_cq_buf      hr_buf;
    struct hns_roce_db      db;
    u8              db_en;
    spinlock_t          lock;
    struct ib_umem          *umem;
    void (*comp)(struct hns_roce_cq *cq);
    void (*event)(struct hns_roce_cq *cq, enum hns_roce_event event_type);

    struct hns_roce_uar     *uar;
    u32             cq_depth;
    u32             cons_index;
    u32             *set_ci_db;
    void __iomem            *cq_db_l;
    u16             *tptr_addr;
    int             arm_sn;
    unsigned long           cqn;
    u32             vector;
    atomic_t            refcount;
    struct completion       free;
    struct list_head        list_sq;
    struct list_head        list_rq;
    int             reset_has_notifyed;
    struct list_head        reset_notify;
    struct workqueue_struct     *workq;
};

#endif
