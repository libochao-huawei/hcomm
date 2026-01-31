/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma_server esched interface declaration
 * Create: 2025-05-14
 */

#ifndef RS_ESCHED_H
#define RS_ESCHED_H

#include "rs_inner.h"

#define ESCHED_GRP_TS_HCCP 0
#define ESCHED_THREAD_ID_TS_HCCP 0
#define ESCHED_THREAD_TRY_TIME 100
#define ESCHED_THREAD_USLEEP_TIME 10000

struct rs_esched_info {
    unsigned int thread_status;
};

int rs_esched_init(struct rs_cb *rscb);
void rs_esched_deinit(enum ProtocolTypeT protocol);
#endif // RS_ESCHED_H
