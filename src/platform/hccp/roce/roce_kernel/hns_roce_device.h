/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_DEVICE_H
#define _HNS_ROCE_DEVICE_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/version.h>

#include <rdma/ib_verbs.h>
#include "hns_roce_notify.h"
#include "roce_k_compat.h"
#include "hns_roce_dev.h"
#include "hns_roce_com.h"

#define PCI_REVISION_ID_HIP08			0x21

struct hns_roce_hw {
    int (*reset)(struct hns_roce_dev *hr_dev, bool enable);
    int (*cmq_init)(struct hns_roce_dev *hr_dev);
    void (*cmq_exit)(struct hns_roce_dev *hr_dev);
    int (*hw_profile)(struct hns_roce_dev *hr_dev);
    int (*hw_rst_atu)(struct hns_roce_dev *hr_dev);
    int (*hw_init)(struct hns_roce_dev *hr_dev);
    void (*hw_exit)(struct hns_roce_dev *hr_dev);
    int (*post_mbox)(struct hns_roce_dev *hr_dev, struct hns_roce_mbox_post_params mbox_pst_params);
    int (*chk_mbox)(struct hns_roce_dev *hr_dev, unsigned long timeout);
    int (*rst_prc_mbox)(struct hns_roce_dev *hr_dev);
    int (*set_gid)(struct hns_roce_dev *hr_dev, u8 port, int gid_index,
                   const union ib_gid *gid, const struct ib_gid_attr *attr);
    int (*set_mac)(struct hns_roce_dev *hr_dev, u8 phy_port, u8 *addr);
    void (*set_mtu)(struct hns_roce_dev *hr_dev, u8 phy_port,
                    enum ib_mtu mtu);
    int (*write_mtpt)(void *mb_buf, struct hns_roce_mr *mr,
                      unsigned long mtpt_idx);
    int (*rereg_write_mtpt)(struct hns_roce_dev *hr_dev,
                            struct hns_roce_mr *mr,
                            struct rreg_wrt_mtpt_info rreg_info,
                            void *mb_buf);
    int (*frmr_write_mtpt)(void *mb_buf, struct hns_roce_mr *mr);
    int (*mw_write_mtpt)(void *mb_buf, struct hns_roce_mw *mw);
    void (*write_cqc)(struct hns_roce_dev *hr_dev,
                      struct hns_roce_cq *hr_cq, void *mb_buf, u64 *mtts,
                      struct hns_roce_wrtcqc_info wrt_cqc_info);
    int (*set_hem)(struct hns_roce_dev *hr_dev,
                   struct hns_roce_hem_table *table, int obj, int step_idx);
    void (*clear_hem)(struct hns_roce_dev *hr_dev,
                      struct hns_roce_hem_table *table, int obj,
                      int step_idx);
    int (*query_qp)(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr,
                    int qp_attr_mask, struct ib_qp_init_attr *qp_init_attr);
    int (*modify_qp)(struct ib_qp *ibqp, const struct ib_qp_attr *attr,
                     int attr_mask, enum ib_qp_state cur_state,
                     enum ib_qp_state new_state);
    int (*destroy_qp)(struct ib_qp *ibqp);
    int (*qp_flow_control_init)(struct hns_roce_dev *hr_dev,
                                struct hns_roce_qp *hr_qp);
    int (*post_send)(struct ib_qp *ibqp, const struct ib_send_wr *wr,
                     const struct ib_send_wr **bad_wr);
    int (*post_recv)(struct ib_qp *qp, const struct ib_recv_wr *recv_wr,
                     const struct ib_recv_wr **bad_recv_wr);
    int (*req_notify_cq)(struct ib_cq *ibcq, enum ib_cq_notify_flags flags);
    int (*poll_cq)(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc);
    int (*dereg_mr)(struct hns_roce_dev *hr_dev, struct hns_roce_mr *mr);
    int (*destroy_cq)(struct ib_cq *ibcq);
    int (*modify_cq)(struct ib_cq *cq, u16 cq_count, u16 cq_period);
    int (*init_eq)(struct hns_roce_dev *hr_dev);
    void (*cleanup_eq)(struct hns_roce_dev *hr_dev);
    void (*write_srqc)(struct hns_roce_dev *hr_dev,
                       struct hns_roce_srq *srq, struct hns_roce_wrt_srqc_info srqc_info,
                       u64 *mtts_idx, void *mb_buf);
    int (*modify_srq)(struct ib_srq *ibsrq, struct ib_srq_attr *srq_attr,
                      enum ib_srq_attr_mask srq_attr_mask,
                      struct ib_udata *udata);
    int (*query_srq)(struct ib_srq *ibsrq, struct ib_srq_attr *attr);
    int (*post_srq_recv)(struct ib_srq *ibsrq, const struct ib_recv_wr *wr,
                         const struct ib_recv_wr **bad_wr);
    const struct ib_device_ops *hns_roce_dev_ops;
    const struct ib_device_ops *hns_roce_dev_srq_ops;
};

struct roce_cdev_info {
    int dev_major;
    struct cdev cdev;
    struct class *cdev_class;
    struct device *cdev_device;
};

enum hns_roce_eq_dfx {
    HNS_ROCE_DFX_AEQE,
    HNS_ROCE_DFX_CEQE,
    HNS_ROCE_DFX_TOTAL
};

struct hns_roce_dev {
    struct ib_device    ib_dev;
    struct platform_device  *pdev;
    struct pci_dev      *pci_dev;
    struct device       *dev;
    struct hns_roce_uar     priv_uar;
    const char      *irq_names[HNS_ROCE_MAX_IRQ_NUM];
    spinlock_t      sm_lock;
    spinlock_t      bt_cmd_lock;
    bool            active;
    bool            is_reset;
    bool            dis_db;
    unsigned long       reset_cnt;
    struct hns_roce_ib_iboe iboe;
    enum hns_roce_device_state state;
    struct list_head    qp_list;
    spinlock_t      reset_lock;

    struct list_head        pgdir_list;
    struct mutex            pgdir_mutex;
    int         irq[HNS_ROCE_MAX_IRQ_NUM];
    u8 __iomem      *reg_base;
    struct hns_roce_caps    caps;
    struct radix_tree_root  qp_table_tree;

    unsigned char   dev_addr[HNS_ROCE_MAX_PORTS][ETH_ALEN];
    u64         sys_image_guid;
    u32                     vendor_id;
    u32                     vendor_part_id;
    u32                     hw_rev;
    void __iomem            *priv_addr;

    struct hns_roce_cmdq    cmd;
    struct hns_roce_bitmap    pd_bitmap;
    struct hns_roce_bitmap    xrcd_bitmap;
    struct hns_roce_uar_table uar_table;
    struct hns_roce_mr_table  mr_table;
    struct hns_roce_cq_table  cq_table;
    struct hns_roce_srq_table srq_table;
    struct hns_roce_qp_table  qp_table;
    struct hns_roce_eq_table  eq_table;
    struct hns_roce_qpc_timer_table  qpc_timer_table;
    struct hns_roce_cqc_timer_table  cqc_timer_table;

    struct hns_roce_bitmap    atu_bitmap;
    void        *peer_mem_private_data;
    char        *peer_mem_name;
    // gdr share buffer
    void                *share_mem_priv;
    int         cmd_mod;
    int         loop_idc;
    u32         sdb_offset;
    u32         odb_offset;
    dma_addr_t      uar2_dma_addr;
    u32         uar2_size;
    const struct hns_roce_hw *hw;
    const struct hns_roce_dfx_hw *dfx;
    void            *priv;

    void                *notify_priv;
    // dfx_command
    void            *dfx_priv;
    struct workqueue_struct *irq_workq;
    struct hns_roce_stat    hr_stat;
    struct mutex hr_stat_mutex;
    u32         func_num;
    u32         mac_id;
    struct workqueue_struct *flush_workq;
    struct mutex sm_mutex;
    struct roce_cdev_info cdev;
    u32 port_id;
    u32 port_num;
    u32 qp_cnt;
    u32 max_sq_num;
    u32 sq_depth;
    u32 temp_depth;

    u64 dfx_cnt[HNS_ROCE_DFX_TOTAL];
};

struct hns_roce_dfx_hw {
    int (*query_cqc_info)(struct hns_roce_dev *hr_dev, u32 cqn,
                          int *buffer);
    int (*query_qpc_info)(struct hns_roce_dev *hr_dev, u32 qpn,
                          int *buffer);
    int (*query_mpt_info)(struct hns_roce_dev *hr_dev, u32 key,
                          int *buffer);
    int (*query_cqc_stat)(struct hns_roce_dev *hr_dev,
                          char *buf, int *desc);
    int (*query_cmd_stat)(struct hns_roce_dev *hr_dev,
                          char *buf, int *desc);
    int (*query_qpc_stat)(struct hns_roce_dev *hr_dev,
                          char *buf, int *desc);
    int (*query_aeqc_stat)(struct hns_roce_dev *hr_dev,
                           char *buf, int *desc);
    int (*query_srqc_stat)(struct hns_roce_dev *hr_dev,
                           char *buf, int *desc);
    int (*query_pkt_stat)(struct hns_roce_dev *hr_dev,
                          char *buf, int *desc);
    int (*query_mpt_stat)(struct hns_roce_dev *hr_dev,
                          char *buf, int *desc);
    int (*query_ceqc_stat)(struct hns_roce_dev *hr_dev,
                           char *buf, int *desc);
    int (*modify_eq)(struct hns_roce_dev *hr_dev,
                     u16 eq_count, u16 eq_period, u16 type);
};

typedef int (*init_hem_handle)(struct hns_roce_dev *hr_dev);
typedef void (*free_hem_handle)(struct hns_roce_dev *hr_dev);

struct hns_roce_hem_init_ops {
    init_hem_handle hns_roce_hem_init_op;
    free_hem_handle hns_roce_hem_free_op;
};

typedef int (*hns_roce_init_handle)(struct hns_roce_dev *hr_dev);
typedef void (*hns_roce_clean_handle)(struct hns_roce_dev *hr_dev);

struct hns_roce_init_ops {
    hns_roce_init_handle hns_roce_init_op;
    hns_roce_clean_handle hns_roce_clean_op;
};

typedef int (*init_table_handle)(struct hns_roce_dev *hr_dev);
typedef void (*free_table_handle)(struct hns_roce_dev *hr_dev);

struct hns_roce_hca_ops {
    init_table_handle hns_roce_init_op;
    free_table_handle hns_roce_free_op;
};

static inline struct hns_roce_dev *to_hr_dev(struct ib_device *ib_dev)
{
    return container_of(ib_dev, struct hns_roce_dev, ib_dev);
}

static inline struct hns_roce_ucontext *to_hr_ucontext(struct ib_ucontext *ibucontext)
{
    return container_of(ibucontext, struct hns_roce_ucontext, ibucontext);
}

static inline struct hns_roce_pd *to_hr_pd(struct ib_pd *ibpd)
{
    return container_of(ibpd, struct hns_roce_pd, ibpd);
}

static inline struct hns_roce_xrcd *to_hr_xrcd(struct ib_xrcd *ibxrcd)
{
    return container_of(ibxrcd, struct hns_roce_xrcd, ibxrcd);
}

static inline struct hns_roce_ah *to_hr_ah(struct ib_ah *ibah)
{
    return container_of(ibah, struct hns_roce_ah, ibah);
}

static inline struct hns_roce_mr *to_hr_mr(struct ib_mr *ibmr)
{
    return container_of(ibmr, struct hns_roce_mr, ibmr);
}

static inline struct hns_roce_mw *to_hr_mw(struct ib_mw *ibmw)
{
    return container_of(ibmw, struct hns_roce_mw, ibmw);
}

static inline struct hns_roce_qp *to_hr_qp(struct ib_qp *ibqp)
{
    return container_of(ibqp, struct hns_roce_qp, ibqp);
}

static inline struct hns_roce_cq *to_hr_cq(struct ib_cq *ib_cq)
{
    return container_of(ib_cq, struct hns_roce_cq, ib_cq);
}

static inline struct hns_roce_srq *to_hr_srq(struct ib_srq *ibsrq)
{
    return container_of(ibsrq, struct hns_roce_srq, ibsrq);
}

static inline struct hns_roce_sqp *hr_to_hr_sqp(struct hns_roce_qp *hr_qp)
{
    return container_of(hr_qp, struct hns_roce_sqp, hr_qp);
}

static inline void hns_roce_write64_k(__le32 val[2], unsigned int length, void __iomem *dest)
{
    mb();
    __raw_writeq(*(u64 *) val, dest);
}

static inline struct hns_roce_qp *__hns_roce_qp_lookup(struct hns_roce_dev *hr_dev, u32 qpn)
{
    return radix_tree_lookup(&hr_dev->qp_table_tree,
                             qpn & ((unsigned int)hr_dev->caps.num_qps - 1));
}

static inline void *hns_roce_buf_offset(struct hns_roce_buf *buf, int offset)
{
    u32 page_size = 1 << (unsigned int)buf->page_shift;

    if (buf->nbufs == 1) {
        return (char *)(buf->direct.buf) + offset;
    } else {
        return (char *)(buf->page_list[(unsigned int)offset >> (unsigned int)buf->page_shift].buf) +
               ((unsigned int)offset & (page_size - 1));
    }
}

int hns_roce_init_uar_table(struct hns_roce_dev *dev);
int hns_roce_uar_alloc(struct hns_roce_dev *dev, struct hns_roce_uar *uar);
void hns_roce_uar_free(struct hns_roce_dev *dev, struct hns_roce_uar *uar);
void hns_roce_cleanup_uar_table(struct hns_roce_dev *dev);

int hns_roce_cmd_init(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_cleanup(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_event(struct hns_roce_dev *hr_dev, u16 token, u8 status, u64 out_param);
int hns_roce_cmd_use_events(struct hns_roce_dev *hr_dev);
void hns_roce_cmd_use_polling(struct hns_roce_dev *hr_dev);

int hns_roce_mtt_init(struct hns_roce_dev *hr_dev, u32 npages, int page_shift,
                      struct hns_roce_mtt *mtt);
void hns_roce_mtt_cleanup(struct hns_roce_dev *hr_dev,
                          struct hns_roce_mtt *mtt);
int hns_roce_buf_write_mtt(struct hns_roce_dev *hr_dev,
                           struct hns_roce_mtt *mtt, struct hns_roce_buf *buf);

int hns_roce_init_pd_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_xrcd_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_mr_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_eq_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_cq_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_qp_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_srq_table(struct hns_roce_dev *hr_dev);
int hns_roce_init_atu_table(struct hns_roce_dev *hr_dev);

void hns_roce_cleanup_pd_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_xrcd_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_mr_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_eq_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_cq_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_qp_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_srq_table(struct hns_roce_dev *hr_dev);
void hns_roce_cleanup_atu_table(struct hns_roce_dev *hr_dev);

int hns_roce_bitmap_alloc(struct hns_roce_bitmap *bitmap, unsigned long *obj);
void hns_roce_bitmap_free(struct hns_roce_bitmap *bitmap, unsigned long obj,
    int rr);
int hns_roce_bitmap_init(struct hns_roce_bitmap *bitmap, u32 num, u32 mask,
    u32 reserved_bot, u32 resetrved_top);
void hns_roce_bitmap_cleanup(struct hns_roce_bitmap *bitmap);
void hns_roce_cleanup_bitmap(struct hns_roce_dev *hr_dev);
int hns_roce_bitmap_alloc_range(struct hns_roce_bitmap *bitmap, int cnt,
    int align, unsigned long *obj);
void hns_roce_bitmap_free_range(struct hns_roce_bitmap *bitmap,
    unsigned long obj, int cnt,
    int rr);

int hns_roce_create_ah(struct ib_ah *ibah, struct rdma_ah_init_attr *init_attr, struct ib_udata *udata);
int hns_roce_query_ah(struct ib_ah *ibah, struct rdma_ah_attr *ah_attr);
int hns_roce_destroy_ah(struct ib_ah *ah, u32 flags);

int hns_roce_alloc_pd(struct ib_pd *ibpd, struct ib_udata *udata);
int hns_roce_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata);

int hns_roce_ib_alloc_xrcd(struct ib_xrcd *ib_xrcd, struct ib_udata *udata);
int hns_roce_ib_dealloc_xrcd(struct ib_xrcd *xrcd, struct ib_udata *udata);

struct ib_mr *hns_roce_get_dma_mr(struct ib_pd *pd, int acc);
struct ib_mr *hns_roce_reg_user_mr(struct ib_pd *pd, u64 start, u64 length,
                                   u64 virt_addr, int access_flags,
                                   struct ib_udata *udata);
struct ib_mr *hns_roce_alloc_mr(struct ib_pd *pd, enum ib_mr_type mr_type,
    u32 max_num_sg);
int hns_roce_map_mr_sg(struct ib_mr *ibmr, struct scatterlist *sg, int sg_nents,
    unsigned int *sg_offset);
int hns_roce_dereg_mr(struct ib_mr *ibmr, struct ib_udata *udata);
int hns_roce_alloc_mw(struct ib_mw *ibmw, struct ib_udata *udata);
int hns_roce_dealloc_mw(struct ib_mw *ibmw);

void hns_roce_buf_free(struct hns_roce_dev *hr_dev, u32 size,
    struct hns_roce_buf *buf);
int hns_roce_buf_alloc(struct hns_roce_dev *hr_dev, u32 size, u32 max_direct,
    struct hns_roce_buf *buf, u32 page_shift);

int hns_roce_gdr_write_mtt(struct hns_roce_dev *hr_dev,
                           struct hns_roce_mtt *mtt, struct ib_umem *umem,
                           struct hns_roce_share_sq *share_sq);

int hns_roce_ib_umem_write_mtt(struct hns_roce_dev *hr_dev,
    struct hns_roce_mtt *mtt, struct ib_umem *umem);

int hns_roce_create_srq(struct ib_srq *ib_srq, struct ib_srq_init_attr *srq_init_attr,
                        struct ib_udata *udata);
int hns_roce_modify_srq(struct ib_srq *ibsrq, struct ib_srq_attr *srq_attr,
                        enum ib_srq_attr_mask srq_attr_mask,
                        struct ib_udata *udata);
int hns_roce_destroy_srq(struct ib_srq *ibsrq, struct ib_udata *udata);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
void hns_roce_inc_rdma_hw_stats(struct ib_device *dev, int stats);
int hns_roce_get_hw_stats_num_counters(struct ib_device *dev);
struct rdma_hw_stats *hns_roce_get_net_hw_stats(struct ib_device *dev);

struct ib_mr *hns_roce_rereg_user_mr(struct ib_mr *mr, int flags, u64 start, u64 length,
                           u64 virt_addr, int mr_access_flags, struct ib_pd *pd,
                           struct ib_udata *udata);
int hns_roce_create_qp(struct ib_qp *qp, struct ib_qp_init_attr *init_attr,
		       struct ib_udata *udata);
#else
static inline void hns_roce_inc_rdma_hw_stats(struct ib_device *dev, int stats)
{
    if (dev->hw_stats) {
        dev->hw_stats->value[stats]++;
    }
}

int hns_roce_rereg_user_mr(struct ib_mr *mr, int flags, u64 start, u64 length,
                           u64 virt_addr, int mr_access_flags, struct ib_pd *pd,
                           struct ib_udata *udata);
struct ib_qp *hns_roce_create_qp(struct ib_pd *ib_pd,
                                 struct ib_qp_init_attr *init_attr,
                                 struct ib_udata *udata);
#endif
int hns_roce_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
    int attr_mask, struct ib_udata *udata);
void init_flush_work(struct hns_roce_dev *hr_dev, struct hns_roce_qp *qp);
void *get_recv_wqe(struct hns_roce_qp *hr_qp, int n);
void *get_send_wqe(struct hns_roce_qp *hr_qp, unsigned int n);
void *get_send_extend_sge(struct hns_roce_qp *hr_qp, int n);
bool hns_roce_wq_overflow(struct hns_roce_wq *hr_wq, int nreq,
    struct ib_cq *ib_cq);

void hns_roce_lock_cqs(struct hns_roce_cq *send_cq,
    struct hns_roce_cq *recv_cq);
void hns_roce_unlock_cqs(struct hns_roce_cq *send_cq,
    struct hns_roce_cq *recv_cq);
void hns_roce_qp_remove(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp);
void hns_roce_qp_free(struct hns_roce_dev *hr_dev, struct hns_roce_qp *hr_qp);
void hns_roce_release_range_qp(struct hns_roce_dev *hr_dev, int base_qpn,
    int cnt);
__be32 send_ieth(struct ib_send_wr *wr);
int to_hr_qp_type(int qp_type);
int hns_roce_ib_create_cq(struct ib_cq *ib_cq, const struct ib_cq_init_attr *attr,
    struct ib_udata *udata);

int hns_roce_ib_destroy_cq(struct ib_cq *ib_cq, struct ib_udata *udata);

int hns_roce_db_map_user(struct hns_roce_ucontext *context, unsigned long virt,
    struct hns_roce_db *db);
void hns_roce_db_unmap_user(struct hns_roce_ucontext *context,
    struct hns_roce_db *db);
int hns_roce_alloc_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db,
    int order);
void hns_roce_free_db(struct hns_roce_dev *hr_dev, struct hns_roce_db *db);

int hns_roce_get_notify_base_addr(struct hns_roce_dev *hr_dev, u64 *phy_addr, u64 *size);

void hns_roce_cq_completion(struct hns_roce_dev *hr_dev, u32 cqn);
void hns_roce_cq_event(struct hns_roce_dev *hr_dev, u32 cqn, int event_type);
void hns_roce_qp_event(struct hns_roce_dev *hr_dev, u32 qpn, int event_type);
void hns_roce_srq_event(struct hns_roce_dev *hr_dev, u32 srqn, int event_type);
int hns_get_gid_index(struct hns_roce_dev *hr_dev, u8 port, int gid_index);
int hns_roce_init(struct hns_roce_dev *hr_dev);
void hns_roce_uninit(struct hns_roce_dev *hr_dev);
void hns_roce_exit(struct hns_roce_dev *hr_dev);

int hns_roce_fill_res_entry(struct sk_buff *msg,
    struct rdma_restrack_entry *res);

int hns_roce_register_sysfs(struct hns_roce_dev *hr_dev);
void hns_roce_unregister_sysfs(struct hns_roce_dev *hr_dev);

enum hns_phy_state {
    HNS_ROCE_PHY_SLEEP      = 1,
    HNS_ROCE_PHY_POLLING        = 2,
    HNS_ROCE_PHY_DISABLED       = 3,
    HNS_ROCE_PHY_TRAINING       = 4,
    HNS_ROCE_PHY_LINKUP     = 5,
    HNS_ROCE_PHY_LINKERR        = 6,
    HNS_ROCE_PHY_TEST       = 7
};

enum hns_roce_hw_stats {
    HW_STATS_PD_ALLOC,
    HW_STATS_PD_DEALLOC,
    HW_STATS_MR_ALLOC,
    HW_STATS_MR_DEALLOC,
    HW_STATS_CQ_ALLOC,
    HW_STATS_CQ_DEALLOC,
    HW_STATS_QP_ALLOC,
    HW_STATS_QP_DEALLOC,
    HW_STATS_PD_ACTIVE,
    HW_STATS_MR_ACTIVE,
    HW_STATS_CQ_ACTIVE,
    HW_STATS_QP_ACTIVE,
    HW_STATS_AEQE,
    HW_STATS_CEQE,
    HW_STATS_TOTAL,
};

#endif /* _HNS_ROCE_DEVICE_H */
