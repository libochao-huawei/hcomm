/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_SYSFS_V2_H_
#define _HNS_ROCE_HW_SYSFS_V2_H_

#ifdef DEFINE_HNS_LLT
#undef static
#define static
#endif

#define GET_LEN_BY_OFF  2
#define BYTE_UNIT_LEN       8
#define INT_PTR_OFF_1       1
#define INT_PTR_OFF_2       2
#define INT_PTR_OFF_3       3
#define INT_PTR_OFF_4       4
#define INT_PTR_OFF_5       5
#define INT_PTR_OFF_6       6
#define INT_PTR_OFF_7       7
#define RESP_QUERY_ARRAY_IDX_1  1
#define RESP_QUERY_ARRAY_IDX_2  2
#define RESP_QUERY_ARRAY_IDX_3  3
#define RESP_QUERY_ARRAY_IDX_4  4
#define RESP_QUERY_ARRAY_IDX_5  5
#define RESP_QUERY_ARRAY_IDX_6  6
#define RESP_QUERY_ARRAY_IDX_7  7

#define DECIMAL        10

struct roce_stat_info {
    unsigned long long roce_rx_rc_pkt_num;
    unsigned long long roce_rx_all_pkt_num;
    unsigned long long roce_rx_err_pkt_num;
    unsigned long long roce_tx_rc_pkt_num;
    unsigned long long roce_tx_all_pkt_num;
    unsigned long long roce_tx_err_pkt_num;
    unsigned long long roce_cqe_num;
    unsigned long long roce_tx_cnp_pkt_num;
    unsigned long long roce_rx_cnp_pkt_num;
};

enum roce_net_context_type {
    DS_TYPE_QPC = 0,
    DS_TYPE_AEQC,
    DS_TYPE_CEQC,
    DS_TYPE_CQC,
    DS_TYPE_MPT,
    DS_TYPE_MAX,
};

struct hns_rocee_reg_info {
    unsigned int rocee_twp_alm;
    unsigned int rocee_tpp_alm;
    unsigned int rocee_trp_err_flg_0;
    unsigned int rocee_trp_err_flg_1;
};

#define HNS_ROCEE_REG_INDEX_THREE  3
#define HNS_ROCEE_CMD_SEND_NUM_TWO 2
#define HNS_ROCEE_CMD_DESC_NUM_TWO 2
#define HNS_ROCEE_REG_FLAG_NEXT BIT(2)

#define HNS_GET_NETWORK_REG_DATA_CMD 0x7014
#define HIARM_ROCE_REG_ADDR  0x131000000

#define DESC_DATA_INDEX_FOUR  4
#define DESC_DATA_INDEX_TWO  2
#define OPERATE_NUMBER_32 32

enum rocee_regs_index {
    /* ERR regs */
    ROCEE_RAS_INT_CFG2_LABEL      = 1,
    ROCEE_RAS_INT_SRC1_LABEL      = 2,
    ROCEE_RAS_INT_SRC2_LABEL      = 3,
    SCC_INT_SRC_LABEL             = 4,
    ROCEE_TDP_STA_LABEL           = 5,
    ROCEE_TDP_ALM_LABEL           = 6,
    ROCEE_TWP_STA_LABEL           = 7,
    ROCEE_TWP_ALM_LABEL           = 8,
    ROCEE_TGP_STA_LABEL           = 9,
    ROCEE_TGP_ALM_LABEL           = 10,
    ROCEE_TMP_STA_LABEL           = 11,
    ROCEE_TMP_ALM_LABEL           = 12,
    ROCEE_TPP_STA_LABEL           = 13,
    ROCEE_TPP_ALM_LABEL           = 14,
    ROCEE_SSU_TC_XOFF_LABEL       = 15,
    ROCEE_TPP_TC_XOFF_LABEL       = 16,

    /* TRP regs */
    ROCEE_TRP_EMPTY_0_LABEL       = 101,
    ROCEE_TRP_EMPTY_1_LABEL       = 102,
    ROCEE_TRP_EMPTY_2_LABEL       = 103,
    ROCEE_TRP_EMPTY_3_LABEL       = 104,
    ROCEE_TRP_EMPTY_4_LABEL       = 105,
    ROCEE_TRP_FULL_0_LABEL        = 106,
    ROCEE_TRP_FULL_1_LABEL        = 107,
    ROCEE_TRP_FULL_2_LABEL        = 108,
    ROCEE_TRP_FULL_3_LABEL        = 109,
    ROCEE_TRP_FSM_LABEL           = 110,
    TRP_RX_CNP_CNT_LABEL          = 111,
    TRP_SCC_CNP_CNT_LABEL         = 112,
    TRP_INNER_STA_0_LABEL         = 113,
    TRP_INNER_STA_1_LABEL         = 114,
    TRP_INNER_STA_2_LABEL         = 115,
    TRP_INNER_STA_3_LABEL         = 116,
    TRP_INNER_STA_4_LABEL         = 117,
    TRP_INNER_STA_5_LABEL         = 118,
    TRP_INNER_STA_6_LABEL         = 119,
    TRP_INNER_STA_7_LABEL         = 120,
    TRP_INNER_STA_8_LABEL         = 121,
    TRP_RX_CQE_CNT_0_LABEL        = 122,
    TRP_RX_CQE_CNT_1_LABEL        = 123,
    TRP_RX_CQE_CNT_2_LABEL        = 124,
    TRP_RX_CQE_CNT_3_LABEL        = 125,

    /* TSP regs */
    ROCEE_TPP_STA1_LABEL          = 201,
    ROCEE_TPP_STA2_LABEL          = 202,
    ROCEE_TPP_STA3_LABEL          = 203,
    ROCEE_TPP_STA4_LABEL          = 204,
    ROCEE_TPP_STA5_LABEL          = 205,
    ROCEE_TPP_STA6_LABEL          = 206,
    ROCEE_TPP_STA7_LABEL          = 207,
    ROCEE_TWP_STA1_LABEL          = 208,
    ROCEE_TDP_STA1_LABEL          = 209,
    ROCEE_TPP_STA_RSV0_LABEL      = 210,
    ROCEE_TPP_STA_RSV1_LABEL      = 211,
    ROCEE_TPP_STA_RSV2_LABEL      = 212,
    ROCEE_TPP_STA_RSV3_LABEL      = 213,
    TWP_TC_HDR_XOFF_LABEL         = 214,
    TWP_TC_ATM_XOFF_LABEL         = 215,
    TSP_SGE_ERR_DROP_LEN_LABEL    = 216,
    TSP_SGE_AXI_CNT_LABEL         = 217,
    ROCEE_TDP_TRP_CNT_LABEL       = 218,
    ROCEE_TDP_MDB_CNT_LABEL       = 219,
    ROCEE_TDP_EXT_DEQ_CNT_LABEL   = 220,
    ROCEE_TDP_LP_CNT_LABEL        = 221,
    ROCEE_TDP_QMM_CNT_LABEL       = 222,
    ROCEE_TDP_EXT_ENQ_CNT_LABEL   = 223,
    ROCEE_TDP_TWP_CNT0_LABEL      = 224,
    ROCEE_TDP_TWP_CNT1_LABEL      = 225,
    ROCEE_TDP_TWP_CNT2_LABEL      = 226,
    ROCEE_TDP_SCC_CNT_LABEL       = 227,
    ROCEE_SCC_TDP_CNT_LABEL       = 228,
    ROCEE_TDP_TM_CNT_LABEL        = 229,
    ROCEE_TM_TDP_CNT_LABEL        = 230,

    /* CNT regs */
    CAEP_TRP_AE_CNT_I_LABEL       = 301,
    CAEP_MDB_AE_CNT_I_LABEL       = 302,
    CAEP_QMM_CE_CNT_I_LABEL       = 303,
    CAEP_QMM_CE_VLD_CNT_I_LABEL   = 304,
    CAEP_VFT_ERR_CNT_O_LABEL      = 305,
    CAEP_ACE_DISCARD_CNT_O_LABEL  = 306,
    CAEP_ACE_VLD_CNT_O_LABEL      = 307,
    ROCEE_MBX_ISSUE_CNT_LABEL     = 308,
    ROCEE_MBX_EXEC_CNT_LABEL      = 309,
    ROCEE_DB_ISSUE_CNT_LABEL      = 310,
    ROCEE_DB_EXEC_CNT_LABEL       = 311,
    ROCEE_EQDB_ISSUE_CNT_LABEL    = 312,
    ROCEE_EQDB_EXEC_CNT_LABEL     = 313,
};

struct reg_info {
    unsigned int label;
    unsigned int reg_offset;
};

struct hnae3_handle *hclge_get_hnae3_handle(int dev_id);

#endif // _HNS_ROCE_HW_SYSFS_V2_H_

