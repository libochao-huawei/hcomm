/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_COM_H
#define _HNS_ROCE_HW_V2_COM_H

struct hns_roce_v2_cq_db {
    __le32  byte_4;
    __le32  parameter;
};

#define V2_CQ_DB_BYTE_4_TAG_S 0U
#define V2_CQ_DB_BYTE_4_TAG_M GENMASK(23, 0)

#define V2_CQ_DB_BYTE_4_CMD_S 24U
#define V2_CQ_DB_BYTE_4_CMD_M GENMASK(27, 24)

#define V2_CQ_DB_PARAMETER_CONS_IDX_S 0U
#define V2_CQ_DB_PARAMETER_CONS_IDX_M GENMASK(23, 0)

#define V2_CQ_DB_PARAMETER_CMD_SN_S 25U
#define V2_CQ_DB_PARAMETER_CMD_SN_M GENMASK(26, 25)

#define V2_CQ_DB_PARAMETER_NOTIFY_S 24U

struct hns_roce_v2_ud_send_wqe {
    __le32  byte_4;
    __le32  msg_len;
    __le32  immtdata;
    __le32  byte_16;
    __le32  byte_20;
    __le32  byte_24;
    __le32  qkey;
    __le32  byte_32;
    __le32  byte_36;
    __le32  byte_40;
    __le32  dmac;
    __le32  byte_48;
    u8  dgid[GID_LEN_V2];
};

#define V2_UD_SEND_WQE_BYTE_4_OPCODE_S 0U
#define V2_UD_SEND_WQE_BYTE_4_OPCODE_M GENMASK(4, 0)

#define V2_UD_SEND_WQE_BYTE_4_OWNER_S 7U

#define V2_UD_SEND_WQE_BYTE_4_CQE_S 8U

#define V2_UD_SEND_WQE_BYTE_4_SE_S 11U

#define V2_UD_SEND_WQE_BYTE_16_PD_S 0U
#define V2_UD_SEND_WQE_BYTE_16_PD_M GENMASK(23, 0)

#define V2_UD_SEND_WQE_BYTE_16_SGE_NUM_S 24U
#define V2_UD_SEND_WQE_BYTE_16_SGE_NUM_M GENMASK(31, 24)

#define V2_UD_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_S 0U
#define V2_UD_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_M GENMASK(23, 0)

#define V2_UD_SEND_WQE_BYTE_24_UDPSPN_S 16U
#define V2_UD_SEND_WQE_BYTE_24_UDPSPN_M GENMASK(31, 16)

#define V2_UD_SEND_WQE_BYTE_32_DQPN_S 0U
#define V2_UD_SEND_WQE_BYTE_32_DQPN_M GENMASK(23, 0)

#define V2_UD_SEND_WQE_BYTE_36_VLAN_S 0U
#define V2_UD_SEND_WQE_BYTE_36_VLAN_M GENMASK(15, 0)

#define V2_UD_SEND_WQE_BYTE_36_HOPLIMIT_S 16U
#define V2_UD_SEND_WQE_BYTE_36_HOPLIMIT_M GENMASK(23, 16)

#define V2_UD_SEND_WQE_BYTE_36_TCLASS_S 24U
#define V2_UD_SEND_WQE_BYTE_36_TCLASS_M GENMASK(31, 24)

#define V2_UD_SEND_WQE_BYTE_40_FLOW_LABEL_S 0U
#define V2_UD_SEND_WQE_BYTE_40_FLOW_LABEL_M GENMASK(19, 0)

#define V2_UD_SEND_WQE_BYTE_40_SL_S 20U
#define V2_UD_SEND_WQE_BYTE_40_SL_M GENMASK(23, 20)

#define V2_UD_SEND_WQE_BYTE_40_PORTN_S 24U
#define V2_UD_SEND_WQE_BYTE_40_PORTN_M GENMASK(26, 24)

#define V2_UD_SEND_WQE_BYTE_40_UD_VLAN_EN_S 30U

#define V2_UD_SEND_WQE_BYTE_40_LBI_S 31U

#define V2_UD_SEND_WQE_DMAC_0_S 0U
#define V2_UD_SEND_WQE_DMAC_0_M GENMASK(7, 0)

#define V2_UD_SEND_WQE_DMAC_1_S 8U
#define V2_UD_SEND_WQE_DMAC_1_M GENMASK(15, 8)

#define V2_UD_SEND_WQE_DMAC_2_S 16U
#define V2_UD_SEND_WQE_DMAC_2_M GENMASK(23, 16)

#define V2_UD_SEND_WQE_DMAC_3_S 24U
#define V2_UD_SEND_WQE_DMAC_3_M GENMASK(31, 24)

#define V2_UD_SEND_WQE_BYTE_48_DMAC_4_S 0U
#define V2_UD_SEND_WQE_BYTE_48_DMAC_4_M GENMASK(7, 0)

#define V2_UD_SEND_WQE_BYTE_48_DMAC_5_S 8U
#define V2_UD_SEND_WQE_BYTE_48_DMAC_5_M GENMASK(15, 8)

#define V2_UD_SEND_WQE_BYTE_48_SGID_INDX_S 16U
#define V2_UD_SEND_WQE_BYTE_48_SGID_INDX_M GENMASK(23, 16)

#define V2_UD_SEND_WQE_BYTE_48_SMAC_INDX_S 24U
#define V2_UD_SEND_WQE_BYTE_48_SMAC_INDX_M GENMASK(31, 24)

struct hns_roce_v2_rc_send_wqe {
    __le32      byte_4;
    __le32      msg_len;
    union {
        __le32  inv_key;
        __le32  immtdata;
    };
    __le32      byte_16;
    __le32      byte_20;
    __le32      rkey;
    __le64      va;
};

#define V2_RC_SEND_WQE_BYTE_4_OPCODE_S 0U
#define V2_RC_SEND_WQE_BYTE_4_OPCODE_M GENMASK(4, 0)

#define V2_RC_SEND_WQE_BYTE_4_OWNER_S 7U

#define V2_RC_SEND_WQE_BYTE_4_CQE_S 8U

#define V2_RC_SEND_WQE_BYTE_4_FENCE_S 9U

#define V2_RC_SEND_WQE_BYTE_4_SO_S 10U

#define V2_RC_SEND_WQE_BYTE_4_SE_S 11U

#define V2_RC_SEND_WQE_BYTE_4_INLINE_S 12U

#define V2_RC_FRMR_WQE_BYTE_4_BIND_EN_S 19U

#define V2_RC_FRMR_WQE_BYTE_4_ATOMIC_S 20U

#define V2_RC_FRMR_WQE_BYTE_4_RR_S 21U

#define V2_RC_FRMR_WQE_BYTE_4_RW_S 22U

#define V2_RC_FRMR_WQE_BYTE_4_LW_S 23U

#define V2_RC_SEND_WQE_BYTE_16_XRC_SRQN_S 0U
#define V2_RC_SEND_WQE_BYTE_16_XRC_SRQN_M GENMASK(23, 0)

#define V2_RC_SEND_WQE_BYTE_16_SGE_NUM_S 24U
#define V2_RC_SEND_WQE_BYTE_16_SGE_NUM_M GENMASK(31, 24)

#define V2_RC_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_S 0U
#define V2_RC_SEND_WQE_BYTE_20_MSG_START_SGE_IDX_M GENMASK(23, 0)

struct hns_roce_wqe_frmr_seg {
    __le32  pbl_size;
    __le32  mode_buf_pg_sz;
};

#define V2_RC_FRMR_WQE_BYTE_40_PBL_BUF_PG_SZ_S  4U
#define V2_RC_FRMR_WQE_BYTE_40_PBL_BUF_PG_SZ_M  GENMASK(7, 4)

#define V2_RC_FRMR_WQE_BYTE_40_BLK_MODE_S 8U

struct hns_roce_v2_wqe_data_seg {
    __le32    len;
    __le32    lkey;
    __le64    addr;
};

struct hns_roce_v2_db {
    __le32  byte_4;
    __le32  parameter;
};

#define ROCE_QUERYVER_RSVLEN    5
struct hns_roce_query_version {
    __le16 rocee_vendor_id;
    __le16 rocee_hw_version;
    __le32 rsv[ROCE_QUERYVER_RSVLEN];
};

#define ROCE_QUERYFWINFO_RSVLEN    5
struct hns_roce_query_fw_info {
    __le32 fw_ver;
    __le32 rsv[ROCE_QUERYFWINFO_RSVLEN];
};

#define ROCE_FUNCCLEAR_RSVLEN    4
struct hns_roce_func_clear {
    __le32 rst_funcid_en;
    __le32 func_done;
    __le32 rsv[ROCE_FUNCCLEAR_RSVLEN];
};

#define ROCE_PFFUNC_RSVLEN    2
struct hns_roce_pf_func_info {
    __le32 pf_own_func_num;
    __le32 pf_own_mac_id;
    __le32 pf_own_port_num;
    __le32 pf_own_port_id;
    __le32 rsv[ROCE_PFFUNC_RSVLEN];
};

#define FUNC_CLEAR_RST_FUN_EN_S 8

#define FUNC_CLEAR_RST_FUN_DONE_S 0U

#define HNS_ROCE_V2_FUNC_CLEAR_TIMEOUT_MSECS    (512 * 100)
#define HNS_ROCE_V2_READ_FUNC_CLEAR_FLAG_INTERVAL   40
#define HNS_ROCE_V2_READ_FUNC_CLEAR_FLAG_FAIL_WAIT  20

#define QUERY_PF_RES_CMDQ_DESC_NUM          2
#define QUERY_PF_TIMER_RES_CMDQ_DESC_NUM        2
#define ALLOC_VF_RES_CMDQ_DESC_NUM          2
#define CONFIG_LLM_CMDQ_DESC_NUM            2

/* TSQ and RAQ each account for 4B */
#define QP_EX_DB_SIZE                   8
#define CQ_EX_DB_SIZE                   4
#define TIMEOUT_POLL_QUEUE_NUM              4

struct hns_roce_cfg_llm_a {
    __le32 base_addr_l;
    __le32 base_addr_h;
    __le32 depth_pgsz_init_en;
    __le32 head_ba_l;
    __le32 head_ba_h_nxtptr;
    __le32 head_ptr;
};

#define CFG_LLM_QUE_DEPTH_S 0U
#define CFG_LLM_QUE_DEPTH_M GENMASK(12, 0)

#define CFG_LLM_QUE_PGSZ_S 16U
#define CFG_LLM_QUE_PGSZ_M GENMASK(19, 16)

#define CFG_LLM_INIT_EN_S 20U
#define CFG_LLM_INIT_EN_M GENMASK(20, 20)

#define CFG_LLM_HEAD_PTR_S 0U
#define CFG_LLM_HEAD_PTR_M GENMASK(11, 0)
#define ROCE_CFGLLM_RSVLEN    3
struct hns_roce_cfg_llm_b {
    __le32 tail_ba_l;
    __le32 tail_ba_h;
    __le32 tail_ptr;
    __le32 rsv[ROCE_CFGLLM_RSVLEN];
};

#define CFG_LLM_TAIL_BA_H_S 0U
#define CFG_LLM_TAIL_BA_H_M GENMASK(19, 0)

#define CFG_LLM_TAIL_PTR_S 0U
#define CFG_LLM_TAIL_PTR_M GENMASK(11, 0)

#define ROCE_CFGGLOBAL_RSVLEN    5
struct hns_roce_cfg_global_param {
    __le32 time_cfg_udp_port;
    __le32 rsv[ROCE_CFGGLOBAL_RSVLEN];
};

#define CFG_GLOBAL_PARAM_DATA_0_ROCEE_TIME_1US_CFG_S 0
#define CFG_GLOBAL_PARAM_DATA_0_ROCEE_TIME_1US_CFG_M GENMASK(9, 0)

#define CFG_GLOBAL_PARAM_DATA_0_ROCEE_UDP_PORT_S 16
#define CFG_GLOBAL_PARAM_DATA_0_ROCEE_UDP_PORT_M GENMASK(31, 16)

struct hns_roce_pf_res_a {
    __le32  rsv;
    __le32  qpc_bt_idx_num;
    __le32  srqc_bt_idx_num;
    __le32  cqc_bt_idx_num;
    __le32  mpt_bt_idx_num;
    __le32  eqc_bt_idx_num;
};

#define PF_RES_DATA_1_PF_QPC_BT_IDX_S 0U
#define PF_RES_DATA_1_PF_QPC_BT_IDX_M GENMASK(10, 0)

#define PF_RES_DATA_1_PF_QPC_BT_NUM_S 16U
#define PF_RES_DATA_1_PF_QPC_BT_NUM_M GENMASK(27, 16)

#define PF_RES_DATA_2_PF_SRQC_BT_IDX_S 0U
#define PF_RES_DATA_2_PF_SRQC_BT_IDX_M GENMASK(8, 0)

#define PF_RES_DATA_2_PF_SRQC_BT_NUM_S 16U
#define PF_RES_DATA_2_PF_SRQC_BT_NUM_M GENMASK(25, 16)

#define PF_RES_DATA_3_PF_CQC_BT_IDX_S 0U
#define PF_RES_DATA_3_PF_CQC_BT_IDX_M GENMASK(8, 0)

#define PF_RES_DATA_3_PF_CQC_BT_NUM_S 16U
#define PF_RES_DATA_3_PF_CQC_BT_NUM_M GENMASK(25, 16)

#define PF_RES_DATA_4_PF_MPT_BT_IDX_S 0U
#define PF_RES_DATA_4_PF_MPT_BT_IDX_M GENMASK(8, 0)

#define PF_RES_DATA_4_PF_MPT_BT_NUM_S 16U
#define PF_RES_DATA_4_PF_MPT_BT_NUM_M GENMASK(25, 16)

#define PF_RES_DATA_5_PF_EQC_BT_IDX_S 0U
#define PF_RES_DATA_5_PF_EQC_BT_IDX_M GENMASK(8, 0)

#define PF_RES_DATA_5_PF_EQC_BT_NUM_S 16U
#define PF_RES_DATA_5_PF_EQC_BT_NUM_M GENMASK(25, 16)

struct hns_roce_pf_res_b {
    __le32  rsv0;
    __le32  smac_idx_num;
    __le32  sgid_idx_num;
    __le32  qid_idx_sl_num;
    __le32  scc_ctx_bt_idx_num;
    __le32  rsv;
};

#define PF_RES_DATA_1_PF_SMAC_IDX_S 0U
#define PF_RES_DATA_1_PF_SMAC_IDX_M GENMASK(7, 0)

#define PF_RES_DATA_1_PF_SMAC_NUM_S 8U
#define PF_RES_DATA_1_PF_SMAC_NUM_M GENMASK(16, 8)

#define PF_RES_DATA_2_PF_SGID_IDX_S 0U
#define PF_RES_DATA_2_PF_SGID_IDX_M GENMASK(7, 0)

#define PF_RES_DATA_2_PF_SGID_NUM_S 8U
#define PF_RES_DATA_2_PF_SGID_NUM_M GENMASK(16, 8)

#define PF_RES_DATA_3_PF_QID_IDX_S 0U
#define PF_RES_DATA_3_PF_QID_IDX_M GENMASK(9, 0)

#define PF_RES_DATA_3_PF_SL_NUM_S 16U
#define PF_RES_DATA_3_PF_SL_NUM_M GENMASK(26, 16)

#define PF_RES_DATA_4_PF_SCC_CTX_BT_IDX_S 0U
#define PF_RES_DATA_4_PF_SCC_CTX_BT_IDX_M GENMASK(8, 0)

#define PF_RES_DATA_4_PF_SCC_CTX_BT_NUM_S 9U
#define PF_RES_DATA_4_PF_SCC_CTX_BT_NUM_M GENMASK(17, 9)

#define ROCE_PFTIMERS_RSVLEN    3
struct hns_roce_pf_timer_res_a {
    __le32  rsv0;
    __le32  qpc_timer_bt_idx_num;
    __le32  cqc_timer_bt_idx_num;
    __le32  rsv[ROCE_PFTIMERS_RSVLEN];
};

#define PF_RES_DATA_1_PF_QPC_TIMER_BT_IDX_S 0U
#define PF_RES_DATA_1_PF_QPC_TIMER_BT_IDX_M GENMASK(11, 0)

#define PF_RES_DATA_1_PF_QPC_TIMER_BT_NUM_S 16U
#define PF_RES_DATA_1_PF_QPC_TIMER_BT_NUM_M GENMASK(28, 16)

#define PF_RES_DATA_2_PF_CQC_TIMER_BT_IDX_S 0U
#define PF_RES_DATA_2_PF_CQC_TIMER_BT_IDX_M GENMASK(10, 0)

#define PF_RES_DATA_2_PF_CQC_TIMER_BT_NUM_S 16U
#define PF_RES_DATA_2_PF_CQC_TIMER_BT_NUM_M GENMASK(27, 16)

struct hns_roce_vf_res_a {
    __le32 vf_id;
    __le32 vf_qpc_bt_idx_num;
    __le32 vf_srqc_bt_idx_num;
    __le32 vf_cqc_bt_idx_num;
    __le32 vf_mpt_bt_idx_num;
    __le32 vf_eqc_bt_idx_num;
};

#define VF_RES_A_DATA_1_VF_QPC_BT_IDX_S 0U
#define VF_RES_A_DATA_1_VF_QPC_BT_IDX_M GENMASK(10, 0)

#define VF_RES_A_DATA_1_VF_QPC_BT_NUM_S 16U
#define VF_RES_A_DATA_1_VF_QPC_BT_NUM_M GENMASK(27, 16)

#define VF_RES_A_DATA_2_VF_SRQC_BT_IDX_S 0U
#define VF_RES_A_DATA_2_VF_SRQC_BT_IDX_M GENMASK(8, 0)

#define VF_RES_A_DATA_2_VF_SRQC_BT_NUM_S 16U
#define VF_RES_A_DATA_2_VF_SRQC_BT_NUM_M GENMASK(25, 16)

#define VF_RES_A_DATA_3_VF_CQC_BT_IDX_S 0U
#define VF_RES_A_DATA_3_VF_CQC_BT_IDX_M GENMASK(8, 0)

#define VF_RES_A_DATA_3_VF_CQC_BT_NUM_S 16U
#define VF_RES_A_DATA_3_VF_CQC_BT_NUM_M GENMASK(25, 16)

#define VF_RES_A_DATA_4_VF_MPT_BT_IDX_S 0U
#define VF_RES_A_DATA_4_VF_MPT_BT_IDX_M GENMASK(8, 0)

#define VF_RES_A_DATA_4_VF_MPT_BT_NUM_S 16U
#define VF_RES_A_DATA_4_VF_MPT_BT_NUM_M GENMASK(25, 16)

#define VF_RES_A_DATA_5_VF_EQC_IDX_S 0U
#define VF_RES_A_DATA_5_VF_EQC_IDX_M GENMASK(8, 0)

#define VF_RES_A_DATA_5_VF_EQC_NUM_S 16U
#define VF_RES_A_DATA_5_VF_EQC_NUM_M GENMASK(25, 16)

struct hns_roce_vf_res_b {
    __le32 rsv0;
    __le32 vf_smac_idx_num;
    __le32 vf_sgid_idx_num;
    __le32 vf_qid_idx_sl_num;
    __le32 vf_sccc_idx_num;
    __le32 rsv1;
};

#define VF_RES_B_DATA_0_VF_ID_S 0U
#define VF_RES_B_DATA_0_VF_ID_M GENMASK(7, 0)

#define VF_RES_B_DATA_1_VF_SMAC_IDX_S 0U
#define VF_RES_B_DATA_1_VF_SMAC_IDX_M GENMASK(7, 0)

#define VF_RES_B_DATA_1_VF_SMAC_NUM_S 8U
#define VF_RES_B_DATA_1_VF_SMAC_NUM_M GENMASK(16, 8)

#define VF_RES_B_DATA_2_VF_SGID_IDX_S 0U
#define VF_RES_B_DATA_2_VF_SGID_IDX_M GENMASK(7, 0)

#define VF_RES_B_DATA_2_VF_SGID_NUM_S 8U
#define VF_RES_B_DATA_2_VF_SGID_NUM_M GENMASK(16, 8)

#define VF_RES_B_DATA_3_VF_QID_IDX_S 0U
#define VF_RES_B_DATA_3_VF_QID_IDX_M GENMASK(9, 0)

#define VF_RES_B_DATA_3_VF_SL_NUM_S 16U
#define VF_RES_B_DATA_3_VF_SL_NUM_M GENMASK(19, 16)

#define VF_RES_B_DATA_4_VF_SCCC_BT_IDX_S 0U
#define VF_RES_B_DATA_4_VF_SCCC_BT_IDX_M GENMASK(8, 0)

#define VF_RES_B_DATA_4_VF_SCCC_BT_NUM_S 9U
#define VF_RES_B_DATA_4_VF_SCCC_BT_NUM_M GENMASK(17, 9)

struct hns_roce_vf_switch {
    __le32 rocee_sel;
    __le32 fun_id;
    __le32 cfg;
    __le32 resv1;
    __le32 resv2;
    __le32 resv3;
};

#define VF_SWITCH_DATA_FUN_ID_VF_ID_S 3U
#define VF_SWITCH_DATA_FUN_ID_VF_ID_M GENMASK(10, 3)

#define VF_SWITCH_DATA_CFG_ALW_LPBK_S 1U
#define VF_SWITCH_DATA_CFG_ALW_LCL_LPBK_S 2U
#define VF_SWITCH_DATA_CFG_ALW_DST_OVRD_S 3U

struct hns_roce_post_mbox {
    __le32  in_param_l;
    __le32  in_param_h;
    __le32  out_param_l;
    __le32  out_param_h;
    __le32  cmd_tag;
    __le32  token_event_en;
};

#define ROCE_MBOXSTATUS_RSVLEN    5
struct hns_roce_mbox_status {
    __le32  mb_status_hw_run;
    __le32  rsv[ROCE_MBOXSTATUS_RSVLEN];
};

struct hns_roce_cfg_bt_attr {
    __le32 vf_qpc_cfg;
    __le32 vf_srqc_cfg;
    __le32 vf_cqc_cfg;
    __le32 vf_mpt_cfg;
    __le32 vf_scc_ctx_cfg;
    __le32 rsv;
};

#define CFG_BT_ATTR_DATA_0_VF_QPC_BA_PGSZ_S 0U
#define CFG_BT_ATTR_DATA_0_VF_QPC_BA_PGSZ_M GENMASK(3, 0)

#define CFG_BT_ATTR_DATA_0_VF_QPC_BUF_PGSZ_S 4U
#define CFG_BT_ATTR_DATA_0_VF_QPC_BUF_PGSZ_M GENMASK(7, 4)

#define CFG_BT_ATTR_DATA_0_VF_QPC_HOPNUM_S 8U
#define CFG_BT_ATTR_DATA_0_VF_QPC_HOPNUM_M GENMASK(9, 8)

#define CFG_BT_ATTR_DATA_1_VF_SRQC_BA_PGSZ_S 0U
#define CFG_BT_ATTR_DATA_1_VF_SRQC_BA_PGSZ_M GENMASK(3, 0)

#define CFG_BT_ATTR_DATA_1_VF_SRQC_BUF_PGSZ_S 4U
#define CFG_BT_ATTR_DATA_1_VF_SRQC_BUF_PGSZ_M GENMASK(7, 4)

#define CFG_BT_ATTR_DATA_1_VF_SRQC_HOPNUM_S 8U
#define CFG_BT_ATTR_DATA_1_VF_SRQC_HOPNUM_M GENMASK(9, 8)

#define CFG_BT_ATTR_DATA_2_VF_CQC_BA_PGSZ_S 0U
#define CFG_BT_ATTR_DATA_2_VF_CQC_BA_PGSZ_M GENMASK(3, 0)

#define CFG_BT_ATTR_DATA_2_VF_CQC_BUF_PGSZ_S 4U
#define CFG_BT_ATTR_DATA_2_VF_CQC_BUF_PGSZ_M GENMASK(7, 4)

#define CFG_BT_ATTR_DATA_2_VF_CQC_HOPNUM_S 8U
#define CFG_BT_ATTR_DATA_2_VF_CQC_HOPNUM_M GENMASK(9, 8)

#define CFG_BT_ATTR_DATA_3_VF_MPT_BA_PGSZ_S 0U
#define CFG_BT_ATTR_DATA_3_VF_MPT_BA_PGSZ_M GENMASK(3, 0)

#define CFG_BT_ATTR_DATA_3_VF_MPT_BUF_PGSZ_S 4U
#define CFG_BT_ATTR_DATA_3_VF_MPT_BUF_PGSZ_M GENMASK(7, 4)

#define CFG_BT_ATTR_DATA_3_VF_MPT_HOPNUM_S 8U
#define CFG_BT_ATTR_DATA_3_VF_MPT_HOPNUM_M GENMASK(9, 8)

#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BA_PGSZ_S 0U
#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BA_PGSZ_M GENMASK(3, 0)

#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BUF_PGSZ_S 4U
#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_BUF_PGSZ_M GENMASK(7, 4)

#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_HOPNUM_S 8U
#define CFG_BT_ATTR_DATA_4_VF_SCC_CTX_HOPNUM_M GENMASK(9, 8)
#endif
