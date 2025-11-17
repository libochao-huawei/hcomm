/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RA_ADP_POOL_H
#define RA_ADP_POOL_H

#include <pthread.h>

#define SHUTDOWN_SIGNAL 1U

typedef int (*task_func_t)(unsigned int chip_id, void *recv_buf, unsigned int recv_len);

struct ra_hdc_task {
    task_func_t func;
    struct {
        unsigned int chip_id;
        void *recv_buf;
        unsigned int recv_len;
    } args;
};

struct ra_hdc_thread_pool {
    struct ra_hdc_task *task_queue;
    unsigned int queue_size;

    unsigned int task_num;
    unsigned int queue_pi;
    unsigned int queue_ci;

    pthread_t *worker_threads;
    unsigned int thread_num;
    pthread_mutex_t pool_mutex;
    pthread_cond_t condition;

    unsigned int shutdown;
};

struct ra_hdc_thread_pool *ra_hdc_pool_create(unsigned int queue_size, unsigned int thread_num);
int ra_hdc_pool_destroy(struct ra_hdc_thread_pool *pool);
void ra_hdc_pool_add_task(struct ra_hdc_thread_pool *pool, task_func_t func, unsigned int chip_id, void *recv_buf,
    unsigned int recv_len);
#endif // RA_ADP_POOL_H
