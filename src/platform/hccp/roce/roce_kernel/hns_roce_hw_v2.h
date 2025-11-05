/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_HW_V2_H
#define _HNS_ROCE_HW_V2_H

#include <linux/bitops.h>
#include "hns_roce_hw_v2_dev.h"
#include "hns_roce_hw_v2_ctx.h"
#include "hns_roce_hw_v2_com.h"
#include "hns_roce_hw_v2_wqe.h"

extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

int hns_roce_v2_query_mpt_stat(struct hns_roce_dev *hr_dev,
                               char *buf, int *desc);
int hns_roce_v2_query_srqc_stat(struct hns_roce_dev *hr_dev,
                                char *buf, int *desc);
int hns_roce_v2_query_qpc_stat(struct hns_roce_dev *hr_dev,
                               char *buf, int *desc);
int hns_roce_v2_query_aeqc_stat(struct hns_roce_dev *hr_dev,
                                char *buf, int *desc);
int hns_roce_v2_query_pkt_stat(struct hns_roce_dev *hr_dev,
                               char *buf, int *buff_size);
int hns_roce_v2_query_ceqc_stat(struct hns_roce_dev *hr_dev,
                                char *buf, int *desc);
int hns_roce_v2_query_cmd_stat(struct hns_roce_dev *hr_dev,
                               char *buf, int *desc);
int hns_roce_v2_query_cqc_stat(struct hns_roce_dev *hr_dev,
                               char *buf, int *desc);
int hns_roce_v2_modify_eq(struct hns_roce_dev *hr_dev,
                          u16 eq_count, u16 eq_period, u16 type);

int hns_roce_v2_query_cqc_info(struct hns_roce_dev *hr_dev, u32 cqn,
                               int *buffer);
int hns_roce_v2_query_qpc_info(struct hns_roce_dev *hr_dev, u32 qpn,
                               int *buffer);
int hns_roce_v2_query_mpt_info(struct hns_roce_dev *hr_dev, u32 key,
                               int *buffer);
void hns_roce_cmq_setup_basic_desc(struct hns_roce_cmq_desc *desc,
                                   enum hns_roce_opcode_type opcode,
                                   bool is_read);
int hns_roce_cmq_send(struct hns_roce_dev *hr_dev,
                      struct hns_roce_cmq_desc *desc, int num);

#define HNS_SDI_MODE_DEV_ID     0xA125
#define HNS_SDI_100G_DEV_ID     0xD800
#define HNS_PEER_MEM_DEV_ID     0xA126
#ifdef CONFIG_INFINIBAND_HNS_DFX
void rdfx_cp_sq_wqe_buf(struct hns_roce_dev *hr_dev, struct hns_roce_qp *qp,
                        unsigned int ind, void *wqe,
                        struct hns_roce_v2_rc_send_wqe *rc_sq_wqe,
                        const struct ib_send_wr *wr);

void rdfx_set_cqe_info(struct hns_roce_dev *hr_dev, struct hns_roce_cq *hr_cq,
                       struct hns_roce_v2_cqe *cqe);
#else
#define rdfx_set_cqe_info(hr_dev, hr_cq, cqe)
#define rdfx_cp_sq_wqe_buf(hr_dev, qp, ind, wqe, rc_sq_wqe, wr)
#endif

#define HNS_ROCE_V2_SCC_CTX_DONE_S 0
#define ROCE_SCCCTXCLR_RSVLEN    5
struct hns_roce_scc_ctx_clr {
    __le32 rocee_scc_ctx_clr_qpn;
    __le32 rsv[ROCE_SCCCTXCLR_RSVLEN];
};

#define ROCE_SCCCTXCLRDONE_RSVLEN    5
struct hns_roce_scc_ctx_clr_done {
    __le32 rocee_scc_ctx_clr_done;
    __le32 rsv[ROCE_SCCCTXCLRDONE_RSVLEN];
};

#ifndef CONFIG_INFINIBAND_HNS_TEST
#define v2_resv_pds 0
#define v2_resv_qps 8
#define v2_resv_mrws    0
#define v2_resv_cqs 0
#define v2_resv_srqs    0
#endif
static inline void hns_roce_write64(struct hns_roce_dev *hr_dev, __le32 val[2], unsigned int length,
                                    void __iomem *dest)
{
    struct hns_roce_v2_priv *priv = (struct hns_roce_v2_priv *)hr_dev->priv;
    struct hnae3_handle *handle = priv->handle;
    const struct hnae3_ae_ops *ops = handle->ae_algo->ops;
    if (!hr_dev->dis_db && !ops->get_hw_reset_stat(handle)) {
        hns_roce_write64_k(val, length, dest);
    }
}

#define PBLBA_ALIGN_OFFSET  3
#define MPT_BA_PGSIZE       3
#define PAGESZ_OFFSET       6
#define MPT_BUF_PAGESZ      2
#define LOWBITS_WIDTH       32
#define MTT_BA_PAGESZ       6
#define CQE_BA_PAGESZ       6
#define EQE_BA_PAGESZ       3
#define EQE_BUF_PGSZ        2
#define DOORBELL_NUM        2
#define WC_BYTE_LEN         8
#define FSTLAST_PG          1
#define SNDLAST_PG          2
#define SEVICE_NEEDPG_NUM   2
#define SLNUM_BASE          4
#define PAGE_4K             12
#define LOWBITS_MASK        0xffffffff
#define PA_BITS_NUM         44
#define SMAC_INDEX4         4
#define SMAC_INDEX5         5
#define ALIGNED_16M_OFFSET  24
#define PBL_LEVEL           1
#define PBL_2LEVELS         2
#define STEP_IDXONE         1
#define WSIZE_HIGHBITS_NUM  23
#define CONTEXT_NUM         2
#define ADDR_LOWBITS_OFFSET 24
#define ADDR_HIGHBITS_OFFSET 32
#define MASK_8BITS           0xff
#define MASK_24BITS          0xffffff
#define SHORT_MASK          0xffff
#define SQ_PSN_VAL          4
#define WQE_SGE_BA_OFFSET   3
#define TRRL_OFFSET         4
#define TRRL_BA_OFFSET      16
#define RAQ_OFFSET          32
#define MAC_INDEX_ZERO      0
#define MAC_INDEX_FST       1
#define MAC_INDEX_SND       2
#define MAC_INDEX_TRD       3
#define MAC_INDEX_FORTH     4
#define MAC_INDEX_FIFTH     5
#define IRRLBA_OFFSET       6
#define DESC_DATA_ZERO      0
#define DESC_DATA_FST       1
#define DESC_DATA_SND       2
#define PCI_BAR_NUMBER      4
#define DMAC_INDEX_FORTH    4
#define IRRL_IDX_VAL        3
#define TRAFFIC_CLASS_OFFSET 2
#define TIMEOUT_MAX         31
#define CMD_QPC_NUM         2
#define DOORBELL_NUM        2
#define CIMAX(eq_entries)   (2 * (eq_entries) - 1)
#define EQBA_LOWBITS_OFFSET 3
#define EQBA_HIGBITS_OFFSET 35
#define CUREQBA_OFFSET      12
#define CUREQBA_HIGH_OFFSET 28
#define CUREQBA_HUGE_OFFSET 60
#define NEXTEQE_BA_OFFSET   12
#define NEXTEQE_HIGH_OFFSET 44
#define QPC_BA_PGSZ         4
#define QPC_BUF_PGSZ        3
#define SRQC_BA_PGSZ        3
#define SRQC_BUF_PGSZ       2
#define CQC_BA_PGSZ         3
#define CQC_BUF_PGSZ        2
#define QUEUE_NUM           30
#define QUEUE_NAME_CNT      29
#define DMA_HANDLE_OFFSET   3
#define DMA_HANDLE_HIGHOFF  35
#define SLEEP_TIME          20
#define SLEEP_LONGTIME      100
#define VF_SGIDL            0
#define VF_SGIDML           4
#define VF_SGIDMH           8
#define VF_SGIDH            12
#define REG_SMACH           4

#define HNS_ROCE_HW_NUM_TWO             2
#define HNS_ROCE_HW_THIRTY_THREE        3
#define HNS_ROCE_HW_NUM_FOUR            4

int hns_roce_get_g_is_d(void);
void *get_cqe_v2(struct hns_roce_cq *hr_cq, int n);
void *get_sw_cqe_v2(struct hns_roce_cq *hr_cq, int n);
void hns_roce_free_srq_wqe(struct hns_roce_srq *srq, int wqe_index);
int hns_roce_v2_get_roce_qpc_stat(struct hns_roce_dev *hr_dev,
                                  const char *index, char *buf, int len);

#endif
