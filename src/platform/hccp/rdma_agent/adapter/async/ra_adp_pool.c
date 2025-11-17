/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <sys/prctl.h>
#include <pthread.h>
#include "securec.h"
#include "user_log.h"
#include "ra_hdc.h"
#include "ra_rs_err.h"
#include "ra_adp_pool.h"

STATIC void *ra_hdc_worker_thread(void *arg)
{
    struct ra_hdc_thread_pool *pool = (struct ra_hdc_thread_pool *)arg;
    pthread_t tidp = pthread_self();
    struct ra_hdc_task task = {0};

    (void)prctl(PR_SET_NAME, (unsigned long)"hccp_rs_work");

    while (1) {
        RA_PTHREAD_MUTEX_LOCK(&pool->pool_mutex);
        // block thread until task received
        while (pool->task_num == 0 && pool->shutdown != SHUTDOWN_SIGNAL) {
            pthread_cond_wait(&pool->condition, &pool->pool_mutex);
        }
        if (pool->shutdown == SHUTDOWN_SIGNAL) {
            pool->thread_num--;
            RA_PTHREAD_MUTEX_UNLOCK(&pool->pool_mutex);
            break;
        }

        // consume a task
        task.func = pool->task_queue[pool->queue_ci].func;
        task.args.chip_id = pool->task_queue[pool->queue_ci].args.chip_id;
        task.args.recv_buf = pool->task_queue[pool->queue_ci].args.recv_buf;
        task.args.recv_len = pool->task_queue[pool->queue_ci].args.recv_len;
        pool->queue_ci = (pool->queue_ci + 1) % pool->queue_size;
        pool->task_num--;

        // notify manager to produce
        pthread_cond_signal(&pool->condition);
        RA_PTHREAD_MUTEX_UNLOCK(&pool->pool_mutex);

        // do task
        task.func(task.args.chip_id, task.args.recv_buf, task.args.recv_len);
        free(task.args.recv_buf);
        task.args.recv_buf = NULL;
    }

    hccp_run_info("tidp:%ld exit", tidp);
    return NULL;
}

STATIC int ra_hdc_pool_mutex_cond_init(struct ra_hdc_thread_pool *pool)
{
    int ret;

    ret = pthread_mutex_init(&pool->pool_mutex, NULL);
    CHK_PRT_RETURN(ret != 0, hccp_err("pool_mutex mutex_init failed ret %d", ret), -ESYSFUNC);

    ret = pthread_cond_init(&pool->condition, NULL);
    if (ret != 0) {
        hccp_err("condition cond_init failed ret %d", ret);
        ret = -ESYSFUNC;
        goto deinit_pool_mutex;
    }

    return 0;

deinit_pool_mutex:
    pthread_mutex_destroy(&pool->pool_mutex);
    return ret;
}

STATIC void ra_hdc_pool_mutex_cond_deinit(struct ra_hdc_thread_pool *pool)
{
    pthread_cond_destroy(&pool->condition);
    pthread_mutex_destroy(&pool->pool_mutex);
}

STATIC void ra_hdc_pool_free_workers(struct ra_hdc_thread_pool *pool)
{
    int timeout = RA_THREAD_TRY_TIME;
    unsigned int i;

    RA_PTHREAD_MUTEX_LOCK(&pool->pool_mutex);
    pool->shutdown = SHUTDOWN_SIGNAL;
    for (i = 0; i < pool->thread_num; i++) {
        pthread_cond_signal(&pool->condition);
    }
    RA_PTHREAD_MUTEX_UNLOCK(&pool->pool_mutex);

    // wait for all threads exit until time out: RA_THREAD_TRY_TIME * RA_THREAD_SLEEP_TIME us
    while (pool->thread_num > 0 && timeout > 0) {
        usleep(RA_THREAD_SLEEP_TIME);
        timeout--;
    }
    if (pool->thread_num > 0 && timeout <= 0) {
        hccp_warn("destroy thread pool timeout, thread_num:%u > 0 and timeout:%d <= 0", pool->thread_num, timeout);
    }
}

struct ra_hdc_thread_pool *ra_hdc_pool_create(unsigned int queue_size, unsigned int thread_num)
{
    struct ra_hdc_thread_pool *pool = NULL;
    unsigned int i;
    int ret;

    pool = (struct ra_hdc_thread_pool *)calloc(1, sizeof(struct ra_hdc_thread_pool));
    CHK_PRT_RETURN(pool == NULL, hccp_err("calloc pool failed"), NULL);
    pool->task_queue = (struct ra_hdc_task *)calloc(queue_size, sizeof(struct ra_hdc_task));
    if (pool->task_queue == NULL) {
        hccp_err("calloc task_queue failed, queue_size:%u", queue_size);
        goto free_pool;
    }
    pool->queue_size = queue_size;

    ret = ra_hdc_pool_mutex_cond_init(pool);
    if (ret != 0) {
        hccp_err("ra_hdc_pool_mutex_cond_init failed, ret:%d", ret);
        goto free_queue;
    }

    pool->worker_threads = (pthread_t *)calloc(thread_num, sizeof(pthread_t));
    if (pool->worker_threads == NULL) {
        hccp_err("calloc worker_threads failed, thread_num:%u", thread_num);
        goto free_cond;
    }
    for (i = 0; i < thread_num; i++) {
        ret = pthread_create(&pool->worker_threads[i], NULL, (void *)ra_hdc_worker_thread, pool);
        if (ret != 0) {
            hccp_err("Create pthread i:%u failed, ret:%d", i, ret);
            pool->thread_num = i;
            goto free_thread;
        }
    }
    pool->thread_num = thread_num;

    return pool;
free_thread:
    ra_hdc_pool_free_workers(pool);
    free(pool->worker_threads);
    pool->worker_threads = NULL;
free_cond:
    ra_hdc_pool_mutex_cond_deinit(pool);
free_queue:
    free(pool->task_queue);
    pool->task_queue = NULL;
free_pool:
    free(pool);
    pool = NULL;
    return NULL;
}

void ra_hdc_pool_add_task(struct ra_hdc_thread_pool *pool, task_func_t func, unsigned int chip_id, void *recv_buf,
    unsigned int recv_len)
{
    RA_PTHREAD_MUTEX_LOCK(&pool->pool_mutex);
    // block until task can be received
    while (pool->task_num == pool->queue_size) {
        pthread_cond_wait(&pool->condition, &pool->pool_mutex);
    }

    // produce a task
    pool->task_queue[pool->queue_pi].func = func;
    pool->task_queue[pool->queue_pi].args.chip_id = chip_id;
    pool->task_queue[pool->queue_pi].args.recv_buf = recv_buf;
    pool->task_queue[pool->queue_pi].args.recv_len = recv_len;
    pool->queue_pi = (pool->queue_pi + 1) % pool->queue_size;
    pool->task_num++;

    // notify worker to consume
    pthread_cond_signal(&pool->condition);
    RA_PTHREAD_MUTEX_UNLOCK(&pool->pool_mutex);
}

int ra_hdc_pool_destroy(struct ra_hdc_thread_pool *pool)
{
    CHK_PRT_RETURN(pool == NULL, hccp_err("param invalid, pool is NULL"), -EINVAL);

    ra_hdc_pool_free_workers(pool);
    if (pool->task_queue != NULL) {
        free(pool->task_queue);
        pool->task_queue = NULL;
    }
    if (pool->worker_threads) {
        free(pool->worker_threads);
        pool->worker_threads = NULL;
    }
    ra_hdc_pool_mutex_cond_deinit(pool);
    free(pool);
    pool = NULL;
    return 0;
}
