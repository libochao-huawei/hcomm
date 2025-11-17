/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_WQE_H
#define _HNS_ROCE_HW_V2_WQE_H

#include "hnae3.h"

struct hns_roce_cfg_sgid_tb {
    __le32  table_idx_rsv;
    __le32  vf_sgid_l;
    __le32  vf_sgid_ml;
    __le32  vf_sgid_mh;
    __le32  vf_sgid_h;
    __le32  vf_sgid_type_rsv;
};
#define CFG_SGID_TB_TABLE_IDX_S 0
#define CFG_SGID_TB_TABLE_IDX_M GENMASK(7, 0)

#define CFG_SGID_TB_VF_SGID_TYPE_S 0
#define CFG_SGID_TB_VF_SGID_TYPE_M GENMASK(1, 0)

#define ROCE_CFGATU_RSVLEN    3
struct hns_roce_cfg_atu_tb {
    __le32  win_cfg;
    __le32  win_start_addr_l;
    __le32  win_trans_addr;
    __le32  rsv[ROCE_CFGATU_RSVLEN];
};
#define CFG_ATU_TB_WIN_EN_S 0

#define CFG_ATU_TB_WIN_SIZE_S 1
#define CFG_ATU_TB_WIN_SIZE_M GENMASK(6, 1)

#define CFG_ATU_TB_CMD_TYPE_S 12
#define CFG_ATU_TB_CMD_TYPE_M GENMASK(14, 12)

#define CFG_ATU_TB_WIN_IDX_S 16
#define CFG_ATU_TB_WIN_IDX_M GENMASK(19, 16)

#define CFG_ATU_TB_RST_EN_S 20

#define CFG_ATU_TB_WIN_START_ADDR_H_S 0
#define CFG_ATU_TB_WIN_START_ADDR_H_M GENMASK(7, 0)

#define CFG_ATU_TB_WIN_TRANS_ADDR_S 8
#define CFG_ATU_TB_WIN_TRANS_ADDR_M GENMASK(32, 8)

#define ROCE_CFGSMAC_RSVLEN    3
struct hns_roce_cfg_smac_tb {
    __le32  tb_idx_rsv;
    __le32  vf_smac_l;
    __le32  vf_smac_h_rsv;
    __le32  rsv[ROCE_CFGSMAC_RSVLEN];
};
#define CFG_SMAC_TB_IDX_S 0
#define CFG_SMAC_TB_IDX_M GENMASK(7, 0)

#define CFG_SMAC_TB_VF_SMAC_H_S 0
#define CFG_SMAC_TB_VF_SMAC_H_M GENMASK(15, 0)
#define CMQ_DESC_DATANUM        6
struct hns_roce_cmq_desc {
    __le16 opcode;
    __le16 flag;
    __le16 retval;
    __le16 rsv;
    __le32 data[CMQ_DESC_DATANUM];
};

#define HNS_ROCE_V2_GO_BIT_TIMEOUT_MSECS    10000

#define HNS_ROCE_HW_RUN_BIT_SHIFT   31
#define HNS_ROCE_HW_MB_STATUS_MASK  0xFF

#define HNS_ROCE_MB_TAG_S       8
#define HNS_ROCE_MB_EVENT_EN_S      16

struct hns_roce_v2_cmq_ring {
    dma_addr_t desc_dma_addr;
    struct hns_roce_cmq_desc *desc;
    u32 head;
    u32 tail;
    u16 buf_size;
    u16 desc_num;
    int next_to_use;
    int next_to_clean;
    u8 flag;
    spinlock_t lock; /* command queue lock */
};

struct hns_roce_v2_cmq {
    struct hns_roce_v2_cmq_ring csq;
    struct hns_roce_v2_cmq_ring crq;
    u16 tx_timeout;
    u16 last_status;
};

enum hns_roce_link_table_type {
    TSQ_LINK_TABLE,
    TPQ_LINK_TABLE,
};

struct hns_roce_link_table {
    struct hns_roce_buf_list table;
    struct hns_roce_buf_list *pg_list;
    u32 npages;
    u32 pg_sz;
};

struct hns_roce_link_table_entry {
    u32 blk_ba0; /* Aligned with 4KB regardless of kernel page size */
    u32 blk_ba1_nxt_ptr;
};
#define HNS_ROCE_LINK_TABLE_BA1_S 0
#define HNS_ROCE_LINK_TABLE_BA1_M GENMASK(19, 0)

#define HNS_ROCE_LINK_TABLE_NXT_PTR_S 20
#define HNS_ROCE_LINK_TABLE_NXT_PTR_M GENMASK(31, 20)

#define HNS_ROCE_V2_UAR_BUF_SIZE    4096

struct hns_roce_v2_uar {
    u32 dis_db;
};

struct hns_roce_v2_priv {
    struct hnae3_handle *handle;
    struct hns_roce_v2_cmq cmq;
    struct hns_roce_link_table tsq;
    struct hns_roce_link_table tpq;
    struct hns_roce_buf_list uar;
};

#define ROCE_EQCTX_RSVLEN    5
struct hns_roce_eq_context {
    __le32  byte_4;
    __le32  byte_8;
    __le32  byte_12;
    __le32  eqe_report_timer;
    __le32  eqe_ba0;
    __le32  eqe_ba1;
    __le32  byte_28;
    __le32  byte_32;
    __le32  byte_36;
    __le32  nxt_eqe_ba0;
    __le32  nxt_eqe_ba1;
    __le32  rsv[ROCE_EQCTX_RSVLEN];
};

#define HNS_ROCE_AEQ_DEFAULT_BURST_NUM  0x0
#define HNS_ROCE_AEQ_DEFAULT_INTERVAL   0x0
#define HNS_ROCE_CEQ_DEFAULT_BURST_NUM  0x0
#define HNS_ROCE_CEQ_DEFAULT_INTERVAL   0x0

#define HNS_ROCE_V2_EQ_STATE_INVALID        0
#define HNS_ROCE_V2_EQ_STATE_VALID      1
#define HNS_ROCE_V2_EQ_STATE_OVERFLOW       2
#define HNS_ROCE_V2_EQ_STATE_FAILURE        3

#define HNS_ROCE_V2_EQ_OVER_IGNORE_0        0
#define HNS_ROCE_V2_EQ_OVER_IGNORE_1        1

#define HNS_ROCE_V2_EQ_COALESCE_0       0
#define HNS_ROCE_V2_EQ_COALESCE_1       1

#define HNS_ROCE_V2_EQ_FIRED            0
#define HNS_ROCE_V2_EQ_ARMED            1
#define HNS_ROCE_V2_EQ_ALWAYS_ARMED     3

#define HNS_ROCE_V2_EQ_DEFAULT_INTERVAL     0x10
#define HNS_ROCE_V2_EQ_DEFAULT_BURST_NUM    0x10

#define HNS_ROCE_EQ_INIT_EQE_CNT        0
#define HNS_ROCE_EQ_INIT_PROD_IDX       0
#define HNS_ROCE_EQ_INIT_REPORT_TIMER       0
#define HNS_ROCE_EQ_INIT_MSI_IDX        0
#define HNS_ROCE_EQ_INIT_CONS_IDX       0
#define HNS_ROCE_EQ_INIT_NXT_EQE_BA     0

#define HNS_ROCE_V2_CEQ_CEQE_OWNER_S        31
#define HNS_ROCE_V2_AEQ_AEQE_OWNER_S        31

#define HNS_ROCE_V2_COMP_EQE_NUM        0x400000
#define HNS_ROCE_V2_ASYNC_EQE_NUM       0x400000

#define HNS_ROCE_V2_VF_INT_ST_AEQ_OVERFLOW_S    0
#define HNS_ROCE_V2_VF_INT_ST_BUS_ERR_S     1
#define HNS_ROCE_V2_VF_INT_ST_OTHER_ERR_S   2

#define HNS_ROCE_EQ_DB_CMD_AEQ          0x0
#define HNS_ROCE_EQ_DB_CMD_AEQ_ARMED        0x1
#define HNS_ROCE_EQ_DB_CMD_CEQ          0x2
#define HNS_ROCE_EQ_DB_CMD_CEQ_ARMED        0x3

#define EQ_ENABLE               1
#define EQ_DISABLE              0

#define EQ_REG_OFFSET               0x4

#define HNS_ROCE_INT_NAME_LEN           32
#define HNS_ROCE_V2_EQN_M GENMASK(23, 0)

#define HNS_ROCE_V2_CONS_IDX_M GENMASK(23, 0)

#define HNS_ROCE_V2_VF_ABN_INT_EN_S 0
#define HNS_ROCE_V2_VF_ABN_INT_EN_M GENMASK(0, 0)
#define HNS_ROCE_V2_VF_ABN_INT_ST_M GENMASK(2, 0)
#define HNS_ROCE_V2_VF_ABN_INT_CFG_M GENMASK(2, 0)
#define HNS_ROCE_V2_VF_EVENT_INT_EN_M GENMASK(0, 0)

/* WORD0 */
#define HNS_ROCE_EQC_EQ_ST_S 0
#define HNS_ROCE_EQC_EQ_ST_M GENMASK(1, 0)

#define HNS_ROCE_EQC_HOP_NUM_S 2
#define HNS_ROCE_EQC_HOP_NUM_M GENMASK(3, 2)

#define HNS_ROCE_EQC_OVER_IGNORE_S 4
#define HNS_ROCE_EQC_OVER_IGNORE_M GENMASK(4, 4)

#define HNS_ROCE_EQC_COALESCE_S 5
#define HNS_ROCE_EQC_COALESCE_M GENMASK(5, 5)

#define HNS_ROCE_EQC_ARM_ST_S 6
#define HNS_ROCE_EQC_ARM_ST_M GENMASK(7, 6)

#define HNS_ROCE_EQC_EQN_S 8
#define HNS_ROCE_EQC_EQN_M GENMASK(15, 8)

#define HNS_ROCE_EQC_EQE_CNT_S 16
#define HNS_ROCE_EQC_EQE_CNT_M GENMASK(31, 16)

/* WORD1 */
#define HNS_ROCE_EQC_BA_PG_SZ_S 0
#define HNS_ROCE_EQC_BA_PG_SZ_M GENMASK(3, 0)

#define HNS_ROCE_EQC_BUF_PG_SZ_S 4
#define HNS_ROCE_EQC_BUF_PG_SZ_M GENMASK(7, 4)

#define HNS_ROCE_EQC_PROD_INDX_S 8
#define HNS_ROCE_EQC_PROD_INDX_M GENMASK(31, 8)

/* WORD2 */
#define HNS_ROCE_EQC_MAX_CNT_S 0
#define HNS_ROCE_EQC_MAX_CNT_M GENMASK(15, 0)

#define HNS_ROCE_EQC_PERIOD_S 16
#define HNS_ROCE_EQC_PERIOD_M GENMASK(31, 16)

/* WORD3 */
#define HNS_ROCE_EQC_REPORT_TIMER_S 0
#define HNS_ROCE_EQC_REPORT_TIMER_M GENMASK(31, 0)

/* WORD4 */
#define HNS_ROCE_EQC_EQE_BA_L_S 0
#define HNS_ROCE_EQC_EQE_BA_L_M GENMASK(31, 0)

/* WORD5 */
#define HNS_ROCE_EQC_EQE_BA_H_S 0
#define HNS_ROCE_EQC_EQE_BA_H_M GENMASK(28, 0)

/* WORD6 */
#define HNS_ROCE_EQC_SHIFT_S 0
#define HNS_ROCE_EQC_SHIFT_M GENMASK(7, 0)

#define HNS_ROCE_EQC_MSI_INDX_S 8
#define HNS_ROCE_EQC_MSI_INDX_M GENMASK(15, 8)

#define HNS_ROCE_EQC_CUR_EQE_BA_L_S 16
#define HNS_ROCE_EQC_CUR_EQE_BA_L_M GENMASK(31, 16)

/* WORD7 */
#define HNS_ROCE_EQC_CUR_EQE_BA_M_S 0
#define HNS_ROCE_EQC_CUR_EQE_BA_M_M GENMASK(31, 0)

/* WORD8 */
#define HNS_ROCE_EQC_CUR_EQE_BA_H_S 0
#define HNS_ROCE_EQC_CUR_EQE_BA_H_M GENMASK(3, 0)

#define HNS_ROCE_EQC_CONS_INDX_S 8
#define HNS_ROCE_EQC_CONS_INDX_M GENMASK(31, 8)

/* WORD9 */
#define HNS_ROCE_EQC_NXT_EQE_BA_L_S 0
#define HNS_ROCE_EQC_NXT_EQE_BA_L_M GENMASK(31, 0)

/* WORD10 */
#define HNS_ROCE_EQC_NXT_EQE_BA_H_S 0
#define HNS_ROCE_EQC_NXT_EQE_BA_H_M GENMASK(19, 0)

#define HNS_ROCE_V2_CEQE_COMP_CQN_S 0
#define HNS_ROCE_V2_CEQE_COMP_CQN_M GENMASK(23, 0)

#define HNS_ROCE_V2_AEQE_EVENT_TYPE_S 0
#define HNS_ROCE_V2_AEQE_EVENT_TYPE_M GENMASK(7, 0)

#define HNS_ROCE_V2_AEQE_SUB_TYPE_S 8
#define HNS_ROCE_V2_AEQE_SUB_TYPE_M GENMASK(15, 8)

#define HNS_ROCE_V2_EQ_DB_CMD_S 16
#define HNS_ROCE_V2_EQ_DB_CMD_M GENMASK(17, 16)

#define HNS_ROCE_V2_EQ_DB_TAG_S 0
#define HNS_ROCE_V2_EQ_DB_TAG_M GENMASK(7, 0)

#define HNS_ROCE_V2_EQ_DB_PARA_S 0
#define HNS_ROCE_V2_EQ_DB_PARA_M GENMASK(23, 0)

#define HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_S 0
#define HNS_ROCE_V2_AEQE_EVENT_QUEUE_NUM_M GENMASK(23, 0)

struct hns_roce_wqe_atomic_seg {
    __le64          fetchadd_swap_data;
    __le64          cmp_data;
};

struct hns_roce_query_mbdb_cnt {
    __le32 mailbox_issue_cnt;
    __le32 mailbox_exe_cnt;
    __le32 doorbell_issue_cnt;
    __le32 doorbell_exe_cnt;
    __le32 eq_doorbell_issue_cnt;
    __le32 eq_doorbell_exe_cnt;
};

#define RDFX_CNTSNAP_RSVLEN    5
struct rdfx_cnt_snap {
    __le32 data_0;
    __le32 rsv[RDFX_CNTSNAP_RSVLEN];
};

struct rdfx_query_pkt_cnt {
    __le32 rc_pkt_num;
    __le32 uc_pkt_num;
    __le32 ud_pkt_num;
    __le32 xrc_pkt_num;
    __le32 total_pkt_num;
    __le32 error_pkt_num;
};

#define RDFX_QUERYCQECNT_RSVLEN    2
struct rdfx_query_cqe_cnt {
    __le32 port0_cqe;
    __le32 port1_cqe;
    __le32 port2_cqe;
    __le32 port3_cqe;
    __le32 rsv[RDFX_QUERYCQECNT_RSVLEN];
};

#define RDFX_QUERYCNPRXCNT_RSVLEN    2
struct rdfx_query_cnp_rx_cnt {
    __le32 port0_cnp_rx;
    __le32 port1_cnp_rx;
    __le32 port2_cnp_rx;
    __le32 port3_cnp_rx;
    __le32 rsv[RDFX_QUERYCNPRXCNT_RSVLEN];
};

#define RDFX_QUERYCNPTXCNT_RSVLEN    2
struct rdfx_query_cnp_tx_cnt {
    __le32 port0_cnp_tx;
    __le32 port1_cnp_tx;
    __le32 port2_cnp_tx;
    __le32 port3_cnp_tx;
    __le32 rsv[RDFX_QUERYCNPTXCNT_RSVLEN];
};

#define hns_roce_v2_sysfs_print_err(ret, cur) do { \
    if (ret < 0) { \
        dev_err(hr_dev->dev, "snprintf err ret[%d]\n", ret); \
    } else { \
        cur += ret; \
    } \
} while (0)

#define HNS_ROCE_V2_SYSFS_BUF_MAX_SIZE      1000U
#define hns_roce_v2_sysfs_print(out, cur, fmt, ...) do {\
    if (cur < HNS_ROCE_V2_SYSFS_BUF_MAX_SIZE) { \
        int  ret_v; \
        ret_v = snprintf_s(out + cur, HNS_ROCE_V2_SYSFS_BUF_MAX_SIZE - cur, HNS_ROCE_V2_SYSFS_BUF_MAX_SIZE - cur -1,\
            fmt, ##__VA_ARGS__); \
        hns_roce_v2_sysfs_print_err(ret_v, cur); \
    } \
} while (0)

#define HNS_ROCE_SYSFS_SRQC_PRINT(srqc) do { \
    hns_roce_v2_sysfs_print(out, cur_len, "SRQC(0x%x) BT0: 0x%llx\n", srqn, bt0_ba); \
    hns_roce_v2_sysfs_print(out, cur_len, "SRQC(0x%x) BT1: 0x%llx\n", srqn, bt1_ba); \
    for (i = 0; i < (sizeof(*srq_context) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) { \
        hns_roce_v2_sysfs_print(out, cur_len, "SRQC(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n", \
            srqn, *srqc, *(srqc + INT_PTR_OFF_1), *(srqc + INT_PTR_OFF_2), *(srqc + INT_PTR_OFF_3), \
            *(srqc + INT_PTR_OFF_4), *(srqc + INT_PTR_OFF_5), *(srqc + INT_PTR_OFF_6), *(srqc + INT_PTR_OFF_7)); \
        srqc += BYTE_UNIT_LEN; \
    } \
} while (0)

#define HNS_ROCE_SYSFS_MPT_PRINT(mpt) do { \
    hns_roce_v2_sysfs_print(out, cur_len, "MPT(0x%x) BT0: 0x%llx\n", key, bt0_ba); \
    hns_roce_v2_sysfs_print(out, cur_len, "MPT(0x%x) BT1: 0x%llx\n", key, bt1_ba); \
    for (i = 0; i < (sizeof(*mpt_ctx) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) { \
        hns_roce_v2_sysfs_print(out, cur_len, "MPT(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n", \
            key, *mpt, *(mpt + INT_PTR_OFF_1), *(mpt + INT_PTR_OFF_2), *(mpt + INT_PTR_OFF_3), *(mpt + INT_PTR_OFF_3), \
            *(mpt + INT_PTR_OFF_4), *(mpt + INT_PTR_OFF_5), *(mpt + INT_PTR_OFF_6)); \
        mpt += BYTE_UNIT_LEN; \
    } \
} while (0)

#define HNS_ROCE_SYSFS_QPC_PRINT(qpc) do { \
    hns_roce_v2_sysfs_print(out, cur_len, "QPC(0x%x) BT0: 0x%llx\n", qpn, bt0_ba); \
    hns_roce_v2_sysfs_print(out, cur_len, "QPC(0x%x) BT1: 0x%llx\n", qpn, bt1_ba); \
    for (i = 0; i < (sizeof(*qp_context) >> GET_LEN_BY_OFF); i += BYTE_UNIT_LEN) { \
        hns_roce_v2_sysfs_print(out, cur_len, "QPC(0x%x): %08x %08x %08x %08x %08x %08x %08x %08x\n", \
            qpn, *qpc, *(qpc + INT_PTR_OFF_1), *(qpc + INT_PTR_OFF_2), *(qpc + INT_PTR_OFF_3), *(qpc + INT_PTR_OFF_4), \
            *(qpc + INT_PTR_OFF_5), *(qpc + INT_PTR_OFF_6), *(qpc + INT_PTR_OFF_7)); \
        qpc += BYTE_UNIT_LEN; \
    } \
} while (0)

#define HNS_ROCE_PRINT_RX_RC_UC_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "RX RC PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->rc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->rc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->rc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->rc_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "RX UC PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->uc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->uc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->uc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->uc_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_RX_UD_XRC_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "RX UD PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->ud_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->ud_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->ud_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->ud_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "RX XRC PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->xrc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->xrc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->xrc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->xrc_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_RX_ERR_ALL_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "RX ALL PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->total_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->total_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->total_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->total_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "RX ERR PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[0]->error_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_1]->error_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_2]->error_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_3]->error_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_TX_RC_UC_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "TX RC PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->rc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->rc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->rc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->rc_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "TX UC PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->uc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->uc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->uc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->uc_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_TX_UD_XRC_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "TX UD PKT : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->ud_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->ud_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->ud_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->ud_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "TX XRC PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->xrc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->xrc_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->xrc_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->xrc_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_TX_ERR_ALL_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "TX ALL PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->total_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->total_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->total_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->total_pkt_num); \
    hns_roce_v2_sysfs_print(out, cur_len, "TX ERR PKT: 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_query[RESP_QUERY_ARRAY_IDX_4]->error_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_5]->error_pkt_num, \
        resp_query[RESP_QUERY_ARRAY_IDX_6]->error_pkt_num, resp_query[RESP_QUERY_ARRAY_IDX_7]->error_pkt_num); \
} while (0)

#define HNS_ROCE_PRINT_CX_STAT() do { \
    hns_roce_v2_sysfs_print(out, cur_len, "CQE       : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_cqe->port0_cqe, resp_cqe->port1_cqe, resp_cqe->port2_cqe, resp_cqe->port3_cqe); \
    hns_roce_v2_sysfs_print(out, cur_len, "CNP RX    : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_cnp_rx->port0_cnp_rx, resp_cnp_rx->port1_cnp_rx, resp_cnp_rx->port2_cnp_rx, resp_cnp_rx->port3_cnp_rx); \
    hns_roce_v2_sysfs_print(out, cur_len, "CNP TX    : 0x%08x  0x%08x  0x%08x  0x%08x\n", \
        resp_cnp_tx->port0_cnp_tx, resp_cnp_tx->port1_cnp_tx, resp_cnp_tx->port2_cnp_tx, resp_cnp_tx->port3_cnp_tx); \
} while (0)

#endif
