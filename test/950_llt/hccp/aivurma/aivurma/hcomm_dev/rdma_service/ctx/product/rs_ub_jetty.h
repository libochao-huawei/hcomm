/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: rdma_server ctx ub jetty internal interface declaration
 * Create: 2025-12-20
 */

#ifndef RS_JETTY_H
#define RS_JETTY_H

#include "urma_types.h"
#include "udma_u_ctl.h"
#include "dl_hal_function.h"
#include "rs_ctx_inner.h"

#define WQE_BB_SIZE 64ULL
#define PAGE_4K 0x1000
#define ALIGN_DOWN(x, a) ((x) & (~((a) - 1)))

struct jetty_va_info {
    unsigned int resType;
    int pid;
    uint64_t va;
    uint64_t len;
};

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg);
void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jettyCb);
void rs_ub_stars_va_munmap_batch(struct rs_ctx_jetty_cb **jettyCbArr, unsigned int num);
#endif // RS_JETTY_H
