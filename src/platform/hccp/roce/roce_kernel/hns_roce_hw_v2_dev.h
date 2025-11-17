/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_DEV_H
#define _HNS_ROCE_HW_V2_DEV_H

#define HNS_ROCE_DEVICE_CPUS 16

#if 1
#define HNS_ROCE_VF_QPC_BT_NUM(is_std) (is_std ? (256) : (8))
#define HNS_ROCE_VF_SCCC_BT_NUM(is_std) (is_std ? (64) : (16))
#define HNS_ROCE_VF_SRQC_BT_NUM(is_std) (is_std ? (64) : (4))
#define HNS_ROCE_VF_CQC_BT_NUM(is_std) (is_std ? (64) : (4))
#define HNS_ROCE_VF_MPT_BT_NUM(is_std) (is_std ? (64) : (4))
#define HNS_ROCE_VF_EQC_NUM(is_std) (is_std ? (64) : (4))
#define HNS_ROCE_VF_SMAC_NUM(is_std) (is_std ? (32) : (8))

#define HNS_ROCE_VF_SL_NUM          8
#else
#define HNS_ROCE_VF_QPC_BT_NUM(d)           16
#define HNS_ROCE_VF_SCCC_BT_NUM(d)              4
#define HNS_ROCE_VF_SRQC_BT_NUM(d)              4
#define HNS_ROCE_VF_CQC_BT_NUM(d)           4
#define HNS_ROCE_VF_MPT_BT_NUM(d)           4
#define HNS_ROCE_VF_EQC_NUM(d)              64
#define HNS_ROCE_VF_SMAC_NUM(d)         8
#define HNS_ROCE_VF_SGID_NUM(d)         8
#define HNS_ROCE_VF_SL_NUM          4
#endif

#define HNS_ROCE_V2_MAX_QP_NUM          0x100000
#define HNS_ROCE_V2_MAX_QPC_TIMER_NUM       0x200
#define HNS_ROCE_V2_MAX_WQE_NUM         0x8000
#define HNS_ROCE_V2_MAX_SRQ         0x100000
#define HNS_ROCE_V2_MAX_SRQ_WR          0x8000
#define HNS_ROCE_V2_MAX_SRQ_SGE         0x100
#define HNS_ROCE_V2_MAX_CQ_NUM          0x100000
#define HNS_ROCE_V2_MAX_CQC_TIMER_NUM       0x100
#define HNS_ROCE_V2_MAX_SRQ_NUM         0x100000
#define HNS_ROCE_V2_MAX_CQE_NUM         0x400000
#define HNS_ROCE_V2_MAX_SRQWQE_NUM      0x8000
#define HNS_ROCE_V2_MAX_RQ_SGE_NUM      0x100
#define HNS_ROCE_V2_MAX_SQ_SGE_NUM      0xff
#define HNS_ROCE_V2_MAX_SRQ_SGE_NUM     0x100
#define HNS_ROCE_V2_MAX_EXTEND_SGE_NUM      0x200000
#define HNS_ROCE_V2_MAX_SQ_INLINE       0x20
#define HNS_ROCE_V2_UAR_NUM         256
#define HNS_ROCE_V2_PHY_UAR_NUM         1
#define HNS_ROCE_V2_MAX_IRQ_NUM(is_std) (is_std ? (65) : (65))
#define HNS_ROCE_V2_COMP_VEC_NUM(is_std) (is_std ? (63) : (2))
#define HNS_ROCE_V2_VF_MAX_IRQ_NUM      33
#define HNS_ROCE_V2_VF_COMP_VEC_NUM     2
#define HNS_ROCE_V2_AEQE_VEC_NUM        1
#define HNS_ROCE_V2_ABNORMAL_VEC_NUM        1
#define HNS_ROCE_V2_MAX_MTPT_NUM        0x100000
#define HNS_ROCE_V2_MAX_MTT_SEGS        0x2000000
#define HNS_ROCE_V2_MAX_CQE_SEGS        0x2000000
#define HNS_ROCE_V2_MAX_SRQWQE_SEGS     0x1000000
#define HNS_ROCE_V2_MAX_IDX_SEGS        0x1000000
#define HNS_ROCE_V2_MAX_PD_NUM          0x200000
#define HNS_ROCE_V2_MAX_XRCD_NUM        0x1000000
#define HNS_ROCE_V2_MAX_QP_INIT_RDMA        128
#define HNS_ROCE_V2_MAX_QP_DEST_RDMA        128
#define HNS_ROCE_V2_MAX_SQ_DESC_SZ      64
#define HNS_ROCE_V2_MAX_RQ_DESC_SZ      16
#define HNS_ROCE_V2_MAX_SRQ_DESC_SZ     64
#define HNS_ROCE_V2_QPC_ENTRY_SZ        256
#define HNS_ROCE_V2_IRRL_ENTRY_SZ       64
#define HNS_ROCE_V2_TRRL_ENTRY_SZ       48
#define HNS_ROCE_V2_CQC_ENTRY_SZ        64
#define HNS_ROCE_V2_SRQC_ENTRY_SZ       64
#define HNS_ROCE_V2_MTPT_ENTRY_SZ       64
#define HNS_ROCE_V2_MTT_ENTRY_SZ        64
#define HNS_ROCE_V2_IDX_ENTRY_SZ        4
#define HNS_ROCE_V2_CQE_ENTRY_SIZE      32
#define HNS_ROCE_V2_SCC_CTX_ENTRY_SZ        32
#define HNS_ROCE_V2_QPC_TIMER_ENTRY_SZ      4096
#define HNS_ROCE_V2_CQC_TIMER_ENTRY_SZ      4096
#define HNS_ROCE_V2_PAGE_SIZE_SUPPORTED     0xFFFFF000
#define HNS_ROCE_V2_MAX_INNER_MTPT_NUM      2
#define HNS_ROCE_INVALID_LKEY           0x100
#define HNS_ROCE_CMQ_TX_TIMEOUT         30000
#define HNS_ROCE_V2_UC_RC_SGE_NUM_IN_WQE    2
#define HNS_ROCE_V2_RSV_QPS         8

/* Time out for hardware to complete reset */
#define HNS_ROCE_V2_HW_RST_TIMEOUT      1000

#define HNS_ROCE_V2_HW_RST_COMPLETION_WAIT  20

/* The longest time for software reset process in NIC subsystem, if a timeout
 * occurs, it indicates that the network subsystem has encountered a serious
 * error and cannot be recovered from the reset processing.
 */
#define HNS_ROCE_V2_RST_PRC_MAX_TIME        300000

#define HNS_ROCE_CONTEXT_HOP_NUM        1
#define HNS_ROCE_SCC_CTX_HOP_NUM        1
#define HNS_ROCE_MTT_HOP_NUM            1
#define HNS_ROCE_CQE_HOP_NUM            1
#define HNS_ROCE_SRQWQE_HOP_NUM         1
#define HNS_ROCE_PBL_HOP_NUM            2
#define HNS_ROCE_EQE_HOP_NUM            1
#define HNS_ROCE_IDX_HOP_NUM            1
#define HNS_ROCE_MEM_PAGE_SUPPORT_8K        2
#define HNS_ROCE_MEM_PAGE_SUPPORT_2T        6
#define HNS_ROCE_MEM_PAGE_SUPPORT_64G       3

#define HNS_ROCE_V2_GID_INDEX_NUM       32

#define HNS_ROCE_V2_TABLE_CHUNK_SIZE        (1 << 18)

#define HNS_ROCE_CMD_FLAG_IN_VALID_SHIFT    0
#define HNS_ROCE_CMD_FLAG_OUT_VALID_SHIFT   1
#define HNS_ROCE_CMD_FLAG_NEXT_SHIFT        2
#define HNS_ROCE_CMD_FLAG_WR_OR_RD_SHIFT    3
#define HNS_ROCE_CMD_FLAG_NO_INTR_SHIFT     4
#define HNS_ROCE_CMD_FLAG_ERR_INTR_SHIFT    5

#define HNS_ROCE_CMD_FLAG_IN        BIT(HNS_ROCE_CMD_FLAG_IN_VALID_SHIFT)
#define HNS_ROCE_CMD_FLAG_OUT       BIT(HNS_ROCE_CMD_FLAG_OUT_VALID_SHIFT)
#define HNS_ROCE_CMD_FLAG_NEXT      BIT(HNS_ROCE_CMD_FLAG_NEXT_SHIFT)
#define HNS_ROCE_CMD_FLAG_WR        BIT(HNS_ROCE_CMD_FLAG_WR_OR_RD_SHIFT)
#define HNS_ROCE_CMD_FLAG_NO_INTR   BIT(HNS_ROCE_CMD_FLAG_NO_INTR_SHIFT)
#define HNS_ROCE_CMD_FLAG_ERR_INTR  BIT(HNS_ROCE_CMD_FLAG_ERR_INTR_SHIFT)

#define HNS_ROCE_CMQ_DESC_NUM_S     3
#define HNS_ROCE_CMQ_EN_B       16
#define HNS_ROCE_CMQ_ENABLE     BIT(HNS_ROCE_CMQ_EN_B)

#define HNS_ROCE_CMQ_SCC_CLR_DONE_CNT       100

#define check_whether_last_step(hop_num, step_idx) \
    ((step_idx == 0 && hop_num == HNS_ROCE_HOP_NUM_0) || \
    (step_idx == 1 && hop_num == 1) || \
    (step_idx == 2 && hop_num == 2))

#define V2_QP_SUPPORT_STATE_HALF(cur_state, new_state) \
    ((cur_state == IB_QPS_ERR && new_state == IB_QPS_RESET) || \
    (cur_state == IB_QPS_SQD && new_state == IB_QPS_RESET) || \
    (cur_state == IB_QPS_SQE && new_state == IB_QPS_RESET) || \
    (cur_state == IB_QPS_INIT && new_state == IB_QPS_ERR) || \
    (cur_state == IB_QPS_RTR && new_state == IB_QPS_ERR) || \
    (cur_state == IB_QPS_RTS && new_state == IB_QPS_ERR) || \
    (cur_state == IB_QPS_SQD && new_state == IB_QPS_ERR) || \
    (cur_state == IB_QPS_SQE && new_state == IB_QPS_ERR) || \
    (cur_state == IB_QPS_ERR && new_state == IB_QPS_ERR))

#define V2_QP_SUPPORT_STATE(cur_state, new_state) \
    ((cur_state == IB_QPS_RTS && new_state == IB_QPS_RTS) || \
    (cur_state == IB_QPS_SQE && new_state == IB_QPS_RTS) || \
    (cur_state == IB_QPS_RTS && new_state == IB_QPS_SQD) || \
    (cur_state == IB_QPS_SQD && new_state == IB_QPS_SQD) || \
    (cur_state == IB_QPS_SQD && new_state == IB_QPS_RTS) || \
    (cur_state == IB_QPS_INIT && new_state == IB_QPS_RESET) || \
    (cur_state == IB_QPS_RTR && new_state == IB_QPS_RESET) || \
    (cur_state == IB_QPS_RTS && new_state == IB_QPS_RESET) || \
     V2_QP_SUPPORT_STATE_HALF(cur_state, new_state))

#define HNS_ICL_SWITCH_CMD_ROCEE_SEL_SHIFT  0

#define HNS_ICL_SWITCH_CMD_ROCEE_SEL    BIT(HNS_ICL_SWITCH_CMD_ROCEE_SEL_SHIFT)

#define CMD_CSQ_DESC_NUM (1024)
#define CMD_CRQ_DESC_NUM (1024)

enum {
    NO_ARMED = 0x0,
    REG_NXT_CEQE = 0x2,
    REG_NXT_SE_CEQE = 0x3
};

#define V2_CQ_DB_REQ_NOT_SOL            0
#define V2_CQ_DB_REQ_NOT            1

#define V2_CQ_STATE_VALID           1
#define V2_QKEY_VAL             0x80010000

#define GID_LEN_V2              16

#define HNS_ROCE_V2_CQE_QPN_MASK        0x3ffff

enum {
    HNS_ROCE_V2_WQE_OP_SEND             = 0x0,
    HNS_ROCE_V2_WQE_OP_SEND_WITH_INV        = 0x1,
    HNS_ROCE_V2_WQE_OP_SEND_WITH_IMM        = 0x2,
    HNS_ROCE_V2_WQE_OP_RDMA_WRITE           = 0x3,
    HNS_ROCE_V2_WQE_OP_RDMA_WRITE_WITH_IMM      = 0x4,
    HNS_ROCE_V2_WQE_OP_RDMA_READ            = 0x5,
    HNS_ROCE_V2_WQE_OP_ATOM_CMP_AND_SWAP        = 0x6,
    HNS_ROCE_V2_WQE_OP_ATOM_FETCH_AND_ADD       = 0x7,
    HNS_ROCE_V2_WQE_OP_ATOM_MSK_CMP_AND_SWAP    = 0x8,
    HNS_ROCE_V2_WQE_OP_ATOM_MSK_FETCH_AND_ADD   = 0x9,
    HNS_ROCE_V2_WQE_OP_FAST_REG_PMR         = 0xa,
    HNS_ROCE_V2_WQE_OP_LOCAL_INV            = 0xb,
    HNS_ROCE_V2_WQE_OP_BIND_MW_TYPE         = 0xc,
    HNS_ROCE_V2_WQE_OP_MASK             = 0x1f,
};

enum {
    HNS_ROCE_SQ_OPCODE_SEND = 0x0,
    HNS_ROCE_SQ_OPCODE_SEND_WITH_INV = 0x1,
    HNS_ROCE_SQ_OPCODE_SEND_WITH_IMM = 0x2,
    HNS_ROCE_SQ_OPCODE_RDMA_WRITE = 0x3,
    HNS_ROCE_SQ_OPCODE_RDMA_WRITE_WITH_IMM = 0x4,
    HNS_ROCE_SQ_OPCODE_RDMA_READ = 0x5,
    HNS_ROCE_SQ_OPCODE_ATOMIC_COMP_AND_SWAP = 0x6,
    HNS_ROCE_SQ_OPCODE_ATOMIC_FETCH_AND_ADD = 0x7,
    HNS_ROCE_SQ_OPCODE_ATOMIC_MASK_COMP_AND_SWAP = 0x8,
    HNS_ROCE_SQ_OPCODE_ATOMIC_MASK_FETCH_AND_ADD = 0x9,
    HNS_ROCE_SQ_OPCODE_FAST_REG_WR = 0xa,
    HNS_ROCE_SQ_OPCODE_LOCAL_INV = 0xb,
    HNS_ROCE_SQ_OPCODE_BIND_MW = 0xc,
};

enum {
    /* rq operations */
    HNS_ROCE_V2_OPCODE_RDMA_WRITE_IMM = 0x0,
    HNS_ROCE_V2_OPCODE_SEND = 0x1,
    HNS_ROCE_V2_OPCODE_SEND_WITH_IMM = 0x2,
    HNS_ROCE_V2_OPCODE_SEND_WITH_INV = 0x3,
};

enum {
    HNS_ROCE_V2_SQ_DB   = 0x0,
    HNS_ROCE_V2_RQ_DB   = 0x1,
    HNS_ROCE_V2_SRQ_DB  = 0x2,
    HNS_ROCE_V2_CQ_DB_PTR   = 0x3,
    HNS_ROCE_V2_CQ_DB_NTR   = 0x4,
};

enum {
    HNS_ROCE_CQE_V2_SUCCESS             = 0x00,
    HNS_ROCE_CQE_V2_LOCAL_LENGTH_ERR        = 0x01,
    HNS_ROCE_CQE_V2_LOCAL_QP_OP_ERR         = 0x02,
    HNS_ROCE_CQE_V2_LOCAL_PROT_ERR          = 0x04,
    HNS_ROCE_CQE_V2_WR_FLUSH_ERR            = 0x05,
    HNS_ROCE_CQE_V2_MW_BIND_ERR         = 0x06,
    HNS_ROCE_CQE_V2_BAD_RESP_ERR            = 0x10,
    HNS_ROCE_CQE_V2_LOCAL_ACCESS_ERR        = 0x11,
    HNS_ROCE_CQE_V2_REMOTE_INVAL_REQ_ERR        = 0x12,
    HNS_ROCE_CQE_V2_REMOTE_ACCESS_ERR       = 0x13,
    HNS_ROCE_CQE_V2_REMOTE_OP_ERR           = 0x14,
    HNS_ROCE_CQE_V2_TRANSPORT_RETRY_EXC_ERR     = 0x15,
    HNS_ROCE_CQE_V2_RNR_RETRY_EXC_ERR       = 0x16,
    HNS_ROCE_CQE_V2_REMOTE_ABORT_ERR        = 0x22,

    HNS_ROCE_V2_CQE_STATUS_MASK         = 0xff,
};

/* CMQ command */
enum hns_roce_opcode_type {
    HNS_ROCE_OPC_QUERY_HW_VER           = 0x8000,
    HNS_ROCE_OPC_CFG_GLOBAL_PARAM           = 0x8001,
    HNS_ROCE_OPC_ALLOC_PF_RES           = 0x8004,
    HNS_ROCE_OPC_STRONG_ORDER_RES           = 0x800e,
    HNS_ROCE_OPC_QUERY_PF_RES           = 0x8400,
    HNS_ROCE_OPC_ALLOC_VF_RES           = 0x8401,
    HNS_ROCE_OPC_CFG_EXT_LLM            = 0x8403,
    HNS_ROCE_OPC_CFG_TMOUT_LLM          = 0x8404,
    HNS_ROCE_OPC_QUERY_PF_TIMER_RES     = 0x8406,
    HNS_ROCE_OPC_QUERY_FUNC_INFO        = 0x8407,
    HNS_ROCE_OPC_CFG_ATU_TB             = 0x8408,   /* TEMP OP CODE */
    HNS_ROCE_OPC_CFG_SGID_TB            = 0x8500,
    HNS_ROCE_OPC_CFG_SMAC_TB            = 0x8501,
    HNS_ROCE_OPC_POST_MB                = 0x8504,
    HNS_ROCE_OPC_QUERY_MB_ST            = 0x8505,
    HNS_ROCE_OPC_CFG_BT_ATTR            = 0x8506,
    HNS_ROCE_OPC_FUNC_CLEAR             = 0x8508,
    HNS_ROCE_OPC_SCC_CTX_CLR            = 0x8509,
    HNS_ROCE_OPC_QUERY_SCC_CTX          = 0x850a,
    HNS_ROCE_OPC_RESET_SCC_CTX          = 0x850b,
    HNS_QUERY_FW_VER                = 0x0001,
    HNS_SWITCH_PARAMETER_CFG            = 0x1033,

    /* DFx command */
    HNS_ROCE_OPC_CNT_SNAP                           = 0x8006,
    HNS_ROCE_OPC_QUEYR_PKT_CNT                      = 0x8200,
    HNS_ROCE_OPC_QUEYR_CQE_CNT                      = 0x8201,
    HNS_ROCE_OPC_QUEYR_MBDB_CNT                     = 0x8202,
    HNS_ROCE_OPC_QUEYR_CNP_RX_CNT                   = 0x8203,
    HNS_ROCE_OPC_QUEYR_CNP_TX_CNT                   = 0x8204,
    HNS_ROCE_OPC_QUEYR_MDB_DFX                      = 0x8300,
    HNS_ROCE_OPC_QUEYR_TSP_DFX                      = 0x8301,
    HNS_ROCE_OPC_QUEYR_TRP_ERR_DFX                  = 0x8308,

    NETWORK_QUERY_ROCE_DFX_REG_NUM_CMD              = 0x830F,
    NETWORK_QUERY_ROCE_DFX_REG_INFO_CMD             = 0x8310,
};

enum {
    TYPE_CRQ,
    TYPE_CSQ,
};

enum hns_roce_cmd_return_status {
    CMD_EXEC_SUCCESS    = 0,
    CMD_NO_AUTH     = 1,
    CMD_NOT_EXEC        = 2,
    CMD_QUEUE_FULL      = 3,
};

enum hns_roce_sgid_type {
    GID_TYPE_FLAG_ROCE_V1 = 0,
    GID_TYPE_FLAG_ROCE_V2_IPV4,
    GID_TYPE_FLAG_ROCE_V2_IPV6,
};

struct hns_roce_v2_cq_context {
    __le32  byte_4_pg_ceqn;
    __le32  byte_8_cqn;
    __le32  cqe_cur_blk_addr;
    __le32  byte_16_hop_addr;
    __le32  cqe_nxt_blk_addr;
    __le32  byte_24_pgsz_addr;
    __le32  byte_28_cq_pi;
    __le32  byte_32_cq_ci;
    __le32  cqe_ba;
    __le32  byte_40_cqe_ba;
    __le32  byte_44_db_record;
    __le32  db_record_addr;
    __le32  byte_52_cqe_cnt;
    __le32  byte_56_cqe_period_maxcnt;
    __le32  cqe_report_timer;
    __le32  byte_64_se_cqe_idx;
};
#define HNS_ROCE_V2_CQ_DEFAULT_BURST_NUM 0x0
#define HNS_ROCE_V2_CQ_DEFAULT_INTERVAL 0x0

#define V2_CQC_BYTE_4_CQ_ST_S 0U
#define V2_CQC_BYTE_4_CQ_ST_M GENMASK(1, 0)

#define V2_CQC_BYTE_4_POLL_S 2U

#define V2_CQC_BYTE_4_SE_S 3U

#define V2_CQC_BYTE_4_OVER_IGNORE_S 4U

#define V2_CQC_BYTE_4_COALESCE_S 5U

#define V2_CQC_BYTE_4_ARM_ST_S 6U
#define V2_CQC_BYTE_4_ARM_ST_M GENMASK(7, 6)

#define V2_CQC_BYTE_4_SHIFT_S 8U
#define V2_CQC_BYTE_4_SHIFT_M GENMASK(12, 8)

#define V2_CQC_BYTE_4_CMD_SN_S 13U
#define V2_CQC_BYTE_4_CMD_SN_M GENMASK(14, 13)

#define V2_CQC_BYTE_4_CEQN_S 15U
#define V2_CQC_BYTE_4_CEQN_M GENMASK(23, 15)

#define V2_CQC_BYTE_4_NOTIFY_BASE_ADDR_0_7_S 24U
#define V2_CQC_BYTE_4_NOTIFY_BASE_ADDR_0_7_M GENMASK(31, 24)

#define V2_CQC_BYTE_4_PAGE_OFFSET_S 24U
#define V2_CQC_BYTE_4_PAGE_OFFSET_M GENMASK(31, 24)

#define V2_CQC_BYTE_8_CQN_S 0U
#define V2_CQC_BYTE_8_CQN_M GENMASK(23, 0)

#define V2_CQC_BYTE_16_NOTIFY_BASE_ADDR_8_17_S 20U
#define V2_CQC_BYTE_16_NOTIFY_BASE_ADDR_8_17_M GENMASK(29, 20)

#define V2_CQC_BYTE_16_CQE_CUR_BLK_ADDR_S 0U
#define V2_CQC_BYTE_16_CQE_CUR_BLK_ADDR_M GENMASK(19, 0)

#define V2_CQC_BYTE_16_CQE_HOP_NUM_S 30U
#define V2_CQC_BYTE_16_CQE_HOP_NUM_M GENMASK(31, 30)

#define V2_CQC_BYTE_24_NOTIFY_BASE_ADDR_18_21_S 20U
#define V2_CQC_BYTE_24_NOTIFY_BASE_ADDR_18_21_M GENMASK(23, 20)

#define V2_CQC_BYTE_24_CQE_NXT_BLK_ADDR_S 0U
#define V2_CQC_BYTE_24_CQE_NXT_BLK_ADDR_M GENMASK(19, 0)

#define V2_CQC_BYTE_24_CQE_BA_PG_SZ_S 24U
#define V2_CQC_BYTE_24_CQE_BA_PG_SZ_M GENMASK(27, 24)

#define V2_CQC_BYTE_24_CQE_BUF_PG_SZ_S 28U
#define V2_CQC_BYTE_24_CQE_BUF_PG_SZ_M GENMASK(31, 28)

#define V2_CQC_BYTE_28_NOTIFY_BASE_ADDR_22_29_S 24U
#define V2_CQC_BYTE_28_NOTIFY_BASE_ADDR_22_29_M GENMASK(31, 24)

#define V2_CQC_BYTE_28_CQ_PRODUCER_IDX_S 0U
#define V2_CQC_BYTE_28_CQ_PRODUCER_IDX_M GENMASK(23, 0)

#define V2_CQC_BYTE_32_NOTIFY_BASE_ADDR_30_37_S 24U
#define V2_CQC_BYTE_32_NOTIFY_BASE_ADDR_30_37_M GENMASK(31, 24)

#define V2_CQC_BYTE_32_CQ_CONSUMER_IDX_S 0U
#define V2_CQC_BYTE_32_CQ_CONSUMER_IDX_M GENMASK(23, 0)

#define V2_CQC_BYTE_40_RSV6_S 29U
#define V2_CQC_BYTE_40_RSV6_M GENMASK(32, 29)

#define V2_CQC_BYTE_40_CQE_BA_S 0U
#define V2_CQC_BYTE_40_CQE_BA_M GENMASK(28, 0)

#define V2_CQC_BYTE_44_DB_RECORD_EN_S 0

#define V2_CQC_BYTE_44_DB_RECORD_ADDR_S 1U
#define V2_CQC_BYTE_44_DB_RECORD_ADDR_M GENMASK(31, 1)

#define V2_CQC_BYTE_52_NOTIFY_BASE_ADDR_38_45_S 24U
#define V2_CQC_BYTE_52_NOTIFY_BASE_ADDR_38_45_M GENMASK(31, 24)

#define V2_CQC_BYTE_52_CQE_CNT_S 0U
#define V2_CQC_BYTE_52_CQE_CNT_M GENMASK(23, 0)

#define V2_CQC_BYTE_56_CQ_MAX_CNT_S 0U
#define V2_CQC_BYTE_56_CQ_MAX_CNT_M GENMASK(15, 0)

#define V2_CQC_BYTE_56_CQ_PERIOD_S 16U
#define V2_CQC_BYTE_56_CQ_PERIOD_M GENMASK(31, 16)

#define V2_CQC_BYTE_64_SE_CQE_IDX_S 0U
#define V2_CQC_BYTE_64_SE_CQE_IDX_M GENMASK(23, 0)

#define V2_CQC_BYTE_64_NOTIFY_BASE_ADDR_46_51_S 24U
#define V2_CQC_BYTE_64_NOTIFY_BASE_ADDR_46_51_M GENMASK(29, 24)

#define V2_CQC_BYTE_64_NOTIFY_EN_S 30U

#define NOTIFY_BASE_ADDR_SHIFT_7 7U
#define NOTIFY_BASE_ADDR_SHIFT_8 8U
#define NOTIFY_BASE_ADDR_SHIFT_15 15U
#define NOTIFY_BASE_ADDR_SHIFT_18 18U
#define NOTIFY_BASE_ADDR_SHIFT_22 22U
#define NOTIFY_BASE_ADDR_SHIFT_30 30U
#define NOTIFY_BASE_ADDR_SHIFT_32 32U

struct hns_roce_srq_context {
    __le32  byte_4_srqn_srqst;
    __le32  byte_8_limit_wl;
    __le32  byte_12_xrcd;
    __le32  byte_16_pi_ci;
    __le32  wqe_bt_ba; /* Aligned with 8B, so store [:3] */
    __le32  byte_24_wqe_bt_ba;
    __le32  byte_28_rqws_pd;
    __le32  idx_bt_ba; /* Aligned with 8B, so store [:3] */
    __le32  rsv_idx_bt_ba;
    __le32  idx_cur_blk_addr;
    __le32  byte_44_idxbufpgsz_addr;
    __le32  idx_nxt_blk_addr;
    __le32  rsv_idxnxtblkaddr;
    __le32  byte_56_xrc_cqn;
    __le32  db_record_addr_record_en;
    __le32  db_record_addr;
};

#define SRQC_BYTE_4_SRQ_ST_S 0U
#define SRQC_BYTE_4_SRQ_ST_M GENMASK(1, 0)

#define SRQC_BYTE_4_SRQ_WQE_HOP_NUM_S 2U
#define SRQC_BYTE_4_SRQ_WQE_HOP_NUM_M GENMASK(3, 2)

#define SRQC_BYTE_4_SRQ_SHIFT_S 4U
#define SRQC_BYTE_4_SRQ_SHIFT_M GENMASK(7, 4)

#define SRQC_BYTE_4_SRQN_S 8U
#define SRQC_BYTE_4_SRQN_M GENMASK(31, 8)

#define SRQC_BYTE_8_SRQ_LIMIT_WL_S 0U
#define SRQC_BYTE_8_SRQ_LIMIT_WL_M GENMASK(15, 0)

#define SRQC_BYTE_12_SRQ_XRCD_S 0U
#define SRQC_BYTE_12_SRQ_XRCD_M GENMASK(23, 0)

#define SRQC_BYTE_16_SRQ_PRODUCER_IDX_S 0U
#define SRQC_BYTE_16_SRQ_PRODUCER_IDX_M GENMASK(15, 0)

#define SRQC_BYTE_16_SRQ_CONSUMER_IDX_S 0U
#define SRQC_BYTE_16_SRQ_CONSUMER_IDX_M GENMASK(31, 16)

#define SRQC_BYTE_24_SRQ_WQE_BT_BA_S 0U
#define SRQC_BYTE_24_SRQ_WQE_BT_BA_M GENMASK(28, 0)

#define SRQC_BYTE_28_PD_S 0U
#define SRQC_BYTE_28_PD_M GENMASK(23, 0)

#define SRQC_BYTE_28_RQWS_S 24U
#define SRQC_BYTE_28_RQWS_M GENMASK(27, 24)

#define SRQC_BYTE_36_SRQ_IDX_BT_BA_S 0U
#define SRQC_BYTE_36_SRQ_IDX_BT_BA_M GENMASK(28, 0)

#define SRQC_BYTE_44_SRQ_IDX_CUR_BLK_ADDR_S 0U
#define SRQC_BYTE_44_SRQ_IDX_CUR_BLK_ADDR_M GENMASK(19, 0)

#define SRQC_BYTE_44_SRQ_IDX_HOP_NUM_S 22U
#define SRQC_BYTE_44_SRQ_IDX_HOP_NUM_M GENMASK(23, 22)

#define SRQC_BYTE_44_SRQ_IDX_BA_PG_SZ_S 24U
#define SRQC_BYTE_44_SRQ_IDX_BA_PG_SZ_M GENMASK(27, 24)

#define SRQC_BYTE_44_SRQ_IDX_BUF_PG_SZ_S 28U
#define SRQC_BYTE_44_SRQ_IDX_BUF_PG_SZ_M GENMASK(31, 28)

#define SRQC_BYTE_52_SRQ_IDX_NXT_BLK_ADDR_S 0U
#define SRQC_BYTE_52_SRQ_IDX_NXT_BLK_ADDR_M GENMASK(19, 0)

#define SRQC_BYTE_56_SRQ_XRC_CQN_S 0U
#define SRQC_BYTE_56_SRQ_XRC_CQN_M GENMASK(23, 0)

#define SRQC_BYTE_56_SRQ_WQE_BA_PG_SZ_S 24U
#define SRQC_BYTE_56_SRQ_WQE_BA_PG_SZ_M GENMASK(27, 24)

#define SRQC_BYTE_56_SRQ_WQE_BUF_PG_SZ_S 28U
#define SRQC_BYTE_56_SRQ_WQE_BUF_PG_SZ_M GENMASK(31, 28)

#define SRQC_BYTE_60_SRQ_RECORD_EN_S 0U

#define SRQC_BYTE_60_SRQ_DB_RECORD_ADDR_S 1U
#define SRQC_BYTE_60_SRQ_DB_RECORD_ADDR_M GENMASK(31, 1)

enum {
    V2_MPT_ST_VALID = 0x1,
    V2_MPT_ST_FREE  = 0x2,
};

enum hns_roce_v2_qp_state {
    HNS_ROCE_QP_ST_RST,
    HNS_ROCE_QP_ST_INIT,
    HNS_ROCE_QP_ST_RTR,
    HNS_ROCE_QP_ST_RTS,
    HNS_ROCE_QP_ST_SQD,
    HNS_ROCE_QP_ST_SQER,
    HNS_ROCE_QP_ST_ERR,
    HNS_ROCE_QP_ST_SQ_DRAINING,
    HNS_ROCE_QP_NUM_ST
};

struct hns_roce_v2_qp_context {
    __le32  byte_4_sqpn_tst;
    __le32  wqe_sge_ba; /* Aligned with 8B, so store [:3] */
    __le32  byte_12_sq_hop;
    __le32  byte_16_buf_ba_pg_sz;
    __le32  byte_20_smac_sgid_idx;
    __le32  byte_24_mtu_tc;
    __le32  byte_28_at_fl;
    u8  dgid[GID_LEN_V2];
    __le32  dmac;
    __le32  byte_52_udpspn_dmac;
    __le32  byte_56_dqpn_err;
    __le32  byte_60_qpst_tempid;
    __le32  qkey_xrcd;
    __le32  byte_68_rq_db;
    __le32  rq_db_record_addr;
    __le32  byte_76_srqn_op_en;
    __le32  byte_80_rnr_rx_cqn;
    __le32  byte_84_rq_ci_pi;
    __le32  rq_cur_blk_addr;
    __le32  byte_92_srq_info;
    __le32  byte_96_rx_reqmsn;
    __le32  rq_nxt_blk_addr;
    __le32  byte_104_rq_sge;
    __le32  byte_108_rx_reqepsn;
    __le32  rq_rnr_timer;
    __le32  rx_msg_len;
    __le32  rx_rkey_pkt_info;
    __le64  rx_va;
    __le32  byte_132_trrl;
    __le32  trrl_ba; /* Aligned with 64B, but store [:4] */
    __le32  byte_140_raq;
    __le32  byte_144_raq;
    __le32  byte_148_raq;
    __le32  byte_152_raq;
    __le32  byte_156_raq;
    __le32  byte_160_sq_ci_pi;
    __le32  sq_cur_blk_addr;
    __le32  byte_168_irrl_idx;
    __le32  byte_172_sq_psn;
    __le32  byte_176_msg_pktn;
    __le32  sq_cur_sge_blk_addr;
    __le32  byte_184_irrl_idx;
    __le32  cur_sge_offset;
    __le32  byte_192_ext_sge;
    __le32  byte_196_sq_psn;
    __le32  byte_200_sq_max;
    __le32  irrl_ba; /* Aligned with 64B, so store [:6] */
    __le32  byte_208_irrl;
    __le32  byte_212_lsn;
    __le32  sq_timer;
    __le32  byte_220_retry_psn_msn;
    __le32  byte_224_retry_msg;
    __le32  rx_sq_cur_blk_addr;
    __le32  byte_232_irrl_sge;
    __le32  irrl_cur_sge_offset;
    __le32  byte_240_irrl_tail;
    __le32  byte_244_rnr_rxack;
    __le32  byte_248_ack_psn;
    __le32  byte_252_err_txcqn;
    __le32  byte_256_sqflush_rqcqe;
};

#endif
