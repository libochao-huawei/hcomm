/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma service ping over URMA
 * Create: 2025-09-22
 */

#ifndef RS_PING_URMA_H
#define RS_PING_URMA_H

#include "rs_ping_inner.h"

#define PING_URMA_DEV_CNT 1
#define RS_PING_URMA_RECV_WC_NUM 16
#define PINGMESH_DEF_ACCESS (URMA_ACCESS_READ | URMA_ACCESS_WRITE | URMA_ACCESS_ATOMIC)

struct RsPingPongOps *rs_ping_urma_get_ops(void);
struct RsPingPongDfx *rs_ping_urma_get_dfx(void);

#endif // RS_PING_URMA_H
