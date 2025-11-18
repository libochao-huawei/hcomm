/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ra async unit test.
 * Create: 2025-04-12
 */

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "ut_dispatch.h"
#include "dl_hal_function.h"
#include "ra_rs_comm.h"
#include "hccp_common.h"
#include "ra_async.h"
#include "ra.h"
#include "ra_hdc.h"
#include "ra_hdc_ctx.h"
#include "ra_hdc_async.h"
#include "ra_hdc_async_ctx.h"
#include "ra_hdc_async_socket.h"
#include "ra_client_host.h"
#include "ra_comm.h"
#include "tc_ra_async.h"

extern int ra_hdc_async_init_session(struct ra_init_config *cfg);
extern int ra_hdc_async_mutex_init(unsigned int phy_id);
extern void ra_hw_async_set_connect_status(unsigned int phy_id, unsigned int connect_status);

void tc_ra_ctx_lmem_register_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct mr_reg_info_t lmem_info = {0};
    struct ra_lmem_handle *lmem_handle = NULL;
    struct ra_request_handle *req_handle = NULL;
    union op_lmem_reg_info_data async_data = {0};
    struct mem_reg_info info = {0};
    int ret;

    mocker_clean();
    ctx_handle.protocol = 1;

    mocker(ra_hdc_ctx_lmem_register_async, 10, -1);
    ret = ra_ctx_lmem_register_async(&ctx_handle, &lmem_info, &lmem_handle, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(ra_hdc_send_msg_async, 10, 0);
    ret = ra_ctx_lmem_register_async(&ctx_handle, &lmem_info, &lmem_handle, &req_handle);
    EXPECT_INT_EQ(ret, 0);

    req_handle->recv_buf = &async_data;
    req_handle->priv_data = &info;
    ra_hdc_async_handle_lmem_register(req_handle);
    free(lmem_handle);
    mocker_clean();
    free(req_handle);
}

void tc_ra_ctx_lmem_unregister_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct ra_lmem_handle *lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    void *req_handle = NULL;
 
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
    free(req_handle);

    mocker(ra_hdc_send_msg_async, 1, 0);
    lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
    free(req_handle);

    mocker(ra_hdc_ctx_lmem_unregister_async, 1, -1);
    lmem_handle = malloc(sizeof(struct ra_lmem_handle));
    ra_ctx_lmem_unregister_async(&ctx_handle, lmem_handle, &req_handle);
    mocker_clean();
}
 
void tc_ra_ctx_qp_create_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_create_attr qp_attr = {0};
    struct qp_info_t *qp_info = NULL;
    struct ra_ctx_qp_handle *qp_handle = NULL;
    struct ra_request_handle *req_handle = NULL;
    struct ra_cq_handle scq_handle = {0};
    struct ra_cq_handle rcq_handle = {0};
 
    qp_attr.scq_handle = (void*)&scq_handle;
    qp_attr.rcq_handle = (void*)&rcq_handle;
    ctx_handle.protocol = 1;
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_qp_create_async(&ctx_handle, &qp_attr, &qp_info, &qp_handle, &req_handle);
    free(qp_handle);
    free(req_handle);
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_qp_create_async(&ctx_handle, &qp_attr, &qp_info, &qp_handle, &req_handle);
    mocker_clean();
    free(qp_handle);
    free(req_handle);

    mocker(ra_hdc_ctx_qp_create_async, 1, -1);
    ra_ctx_qp_create_async(&ctx_handle, &qp_attr, &qp_info, &qp_handle, &req_handle);
    mocker_clean();
}
 
void tc_ra_ctx_qp_destroy_async()
{
    struct ra_ctx_qp_handle *qp_handle = malloc(sizeof(struct ra_ctx_qp_handle));
    void *req_handle = NULL;
 
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_qp_destroy_async(qp_handle, &req_handle);
    mocker_clean();

    free(req_handle);

    qp_handle = malloc(sizeof(struct ra_ctx_qp_handle));
    mocker(ra_hdc_ctx_qp_destroy_async, 1, -1);
    ra_ctx_qp_destroy_async(qp_handle, &req_handle);
    mocker_clean();
}
 
void tc_ra_ctx_qp_import_async()
{
    struct ra_ctx_handle ctx_handle = {0};
    struct qp_import_info_t info = {0};
    struct ra_ctx_rem_qp_handle *rem_qp_handle = NULL;
    struct ra_request_handle *req_handle = NULL;
 
    ctx_handle.protocol = 1;
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_qp_import_async(&ctx_handle, &info, &rem_qp_handle, &req_handle);
    free(rem_qp_handle);
    mocker_clean();
 
    mocker(ra_hdc_ctx_qp_import_async, 1, -1);
    ra_ctx_qp_import_async(&ctx_handle, &info, &rem_qp_handle, &req_handle);
    mocker_clean();
    free(req_handle);
}
 
void tc_ra_ctx_qp_unimport_async()
{
    struct ra_ctx_rem_qp_handle *rem_qp_handle = malloc(sizeof(struct ra_ctx_rem_qp_handle));
    void *req_handle = NULL;
    mocker_clean();
 
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_ctx_qp_unimport_async(rem_qp_handle, &req_handle);
    mocker_clean();

    free(req_handle);

    rem_qp_handle = malloc(sizeof(struct ra_ctx_rem_qp_handle));
    mocker(ra_hdc_ctx_qp_unimport_async, 1, -1);
    ra_ctx_qp_unimport_async(rem_qp_handle, &req_handle);
    mocker_clean();
}
 
void tc_ra_socket_send_async()
{
    struct socket_hdc_info fd_handle = {0};
    struct ra_request_handle *req_handle = NULL;
    unsigned long long sent_size = 0;

    mocker_clean();
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_socket_send_async(&fd_handle,"a", 1, &sent_size, &req_handle);
    mocker_clean();

    free(req_handle);
}
 
void tc_ra_socket_recv_async()
{
    struct socket_hdc_info fd_handle = {0};
    struct ra_request_handle *req_handle = NULL;
    unsigned long long received_size = 0;
    char data = 0;
 
    mocker_clean();
    mocker(ra_hdc_send_msg_async, 1, 0);
    ra_socket_recv_async(&fd_handle, &data, 1, &received_size, &req_handle);
    free(req_handle->priv_data);
    mocker_clean();
 
    free(req_handle);
}
 
void tc_ra_get_async_req_result()
{
    struct ra_request_handle *req_handle = malloc(sizeof(struct ra_request_handle));
    struct ra_async_op_handle op_handle = {0};
    int req_result = 0;
 
    req_handle->is_done = 1;
    req_handle->recv_len = 1;
    req_handle->op_handle = &op_handle;
    RA_INIT_LIST_HEAD(&req_handle->list);
    req_handle->recv_buf = malloc(1);
    req_handle->priv_handle = NULL;
    mocker_clean();
 
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ra_get_async_req_result(req_handle, &req_result);
    mocker_clean();
}

void tc_ra_socket_batch_connect_async_normal()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_connect_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);

    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_connect_info, 10, 0);

    ret = ra_socket_batch_connect_async(conn, 1, &req_handle);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_connect_async_fail()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_connect_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);

    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_connect_info, 10, 0);

    ret = ra_socket_batch_connect_async(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_connect_async()
{
    tc_ra_socket_batch_connect_async_normal();
    tc_ra_socket_batch_connect_async_fail();
}

void tc_ra_socket_listen_start_async_normal()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_listen_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_listen_info, 10, 0);

    ret = ra_socket_listen_start_async(conn, 1, &req_handle);
    free(req_handle->priv_data);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_start_async_fail()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_listen_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_listen_info, 10, 0);

    ret = ra_socket_listen_start_async(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_start_async()
{
    tc_ra_socket_listen_start_async_normal();
    tc_ra_socket_listen_start_async_fail();
}

void tc_ra_socket_listen_stop_async_normal()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_listen_info_t conn[1] = {0};

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_listen_info, 10, 0);

    (void)ra_socket_listen_stop_async(conn, 1, &req_handle);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_stop_async_fail()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_listen_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_listen_info, 10, 0);

    ret = ra_socket_listen_stop_async(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
    return;
}

void tc_ra_socket_listen_stop_async()
{
    tc_ra_socket_listen_stop_async_normal();
    tc_ra_socket_listen_stop_async_fail();
}

void tc_ra_socket_batch_close_async_normal()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_hdc_info fd_handle = {0};
    struct socket_close_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;
    conn[0].fd_handle = (void *)&fd_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_connect_info, 10, 0);

    ret = ra_socket_batch_close_async(conn, 1, &req_handle);
    free(req_handle->priv_data);
    free(req_handle);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_close_async_fail()
{
    struct ra_request_handle *req_handle = NULL;
    struct ra_socket_handle socket_handle = {0};
    struct socket_hdc_info fd_handle = {0};
    struct socket_close_info_t conn[1] = {0};
    int ret = 0;

    conn[0].socket_handle = (void *)&socket_handle;
    conn[0].fd_handle = (void *)&fd_handle;

    mocker_clean();
    mocker(ra_inet_pton, 10, 0);
    mocker(ra_hdc_send_msg_async, 10, 0);
    mocker(ra_get_socket_connect_info, 10, 0);

    ret = ra_socket_batch_close_async(NULL, 1, &req_handle);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
    return;
}

void tc_ra_socket_batch_close_async()
{
    tc_ra_socket_batch_close_async_normal();
    tc_ra_socket_batch_close_async_fail();
}

void tc_ra_hdc_async_init_session()
{
    unsigned int connect_status = HDC_CONNECTED;
    struct ra_init_config cfg = {0};
    unsigned int phy_id = 0;
    int ret = 0;

    mocker(pthread_create, 2, 0);
    mocker(dl_drv_device_get_bare_tgid, 1, 0);
    mocker(ra_hdc_async_mutex_init, 1, -1);
    ra_hw_async_set_connect_status(phy_id, connect_status);
    ret = ra_hdc_async_init_session(&cfg);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_hdc_session_accept()
{
    unsigned int host_tgid = 0;
    HDC_SESSION session = { 0 };
    int ret = 0;

    mocker(dl_drv_hdc_session_accept, 1 , 0);
    mocker(dl_drv_hdc_set_session_reference, 1 , 0);
    mocker(dl_hal_hdc_get_session_attr, 1 , 0);
    ret = ra_hdc_session_accept(0, &session, &host_tgid);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}
