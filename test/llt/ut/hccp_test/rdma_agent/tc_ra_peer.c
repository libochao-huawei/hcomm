/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra peer unit test.
 * Author:
 * Create: 2020-07-21
 */
#include "ra_peer.h"
#include "ra_rs_err.h"
#include <errno.h>
#include "securec.h"
#include "ut_dispatch.h"
#include "rs.h"
extern int ra_peer_set_conn_param(struct socket_info_t conn[],
    struct socket_fd_data rs_conn[], unsigned int i, int buf_size);
extern int ra_rdev_init_check_ip(int mode, struct rdev rdev_info, char local_ip[]);
extern int ra_peer_loopback_qp_create(struct ra_rdma_handle *rdma_handle, struct loopback_qp_pair *qp_pair,
    void **qp_handle);
extern int ra_peer_loopback_single_qp_create(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle **qp_handle,
    struct ibv_qp **qp);
extern int ra_peer_loopback_qp_modify(struct ra_qp_handle *qp_handle0, struct ra_qp_handle *qp_handle1);

int ra_peer_loopback_single_qp_create_stub(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle **qp_handle,
    struct ibv_qp **qp)
{
    static int call_num = 0;
    int ret = 0;
    call_num++;

    if (call_num == 1) {
        return ra_peer_loopback_single_qp_create(rdma_handle, qp_handle, qp);
    } else {
        return -1;
    }
    return ret;
}

void *calloc_stub(size_t __nmemb, size_t __size)
{
    static int call_num = 0;
    call_num++;
    if (call_num == 1) {
        return calloc(__nmemb, __size);
    } else {
        return NULL;
    }
    return NULL;
}

void tc_peer()
{
    int ret;
    int dev_id = 0;
    int flag = 0;
    int port = 0;
    int timeout = 100;
    void *addr = NULL;
    void *data = NULL;
    int size = 0;
    int max_size = 2050;
    int access = 0;
    struct send_wr *wr = NULL;
    int wqe_index = 0;
    int index = 0;
    unsigned long pa = 0;
    unsigned long va = 0;
    struct qp_peer_info *qp_info = NULL;
    struct socket_connect_info_t conn[1];
    struct socket_listen_info_t listen[1];
    struct socket_info_t info[1];
    struct socket_close_info_t close[1] = {0};
    int sock_fd = 1;
    void *qp_handle;
    void *qp_handle_with_attr;
    int status = 0;
    struct ra_init_config config = {
        .phy_id = dev_id,
        .nic_position = 1,
        .hdc_type = 0,
    };
    config.phy_id = 0;
    int ip_addr;
    unsigned int host_tgid = 0;
    int qp_mode = 0;
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct ra_rdma_handle rdma_handle_tmp = {
        .rdev_info = rdev_info,
        .rdev_index = 0,
    };
    struct ra_socket_handle socket_handle_tmp ={
        .rdev_info = rdev_info,
    };
    struct ra_rdma_handle *rdma_handle = &rdma_handle_tmp;
    struct ra_socket_handle *socket_handle = &socket_handle_tmp;
    struct socket_fd_data rs_conn[] = {0};

    listen[0].socket_handle = socket_handle;
    conn[0].socket_handle = socket_handle;
    close[0].socket_handle = socket_handle;
    info[0].socket_handle = socket_handle;
    struct qp_ext_attrs ext_attrs;
    ext_attrs.version = QP_CREATE_WITH_ATTR_VERSION;
    ext_attrs.qp_mode = RA_RS_NOR_QP_MODE;

    ret = ra_peer_init(&config, 1);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_init(&config, 1);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_socket_batch_connect(0, conn, 1);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_socket_listen_start(0, listen, 1);

    ret = ra_peer_socket_listen_stop(0, listen, 1);

    ret = ra_peer_get_sockets(0, 0, info, 1);
    EXPECT_INT_EQ(1, ret);
    
    info[0].socket_handle = socket_handle;
    info[0].status = 1;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = ra_peer_get_sockets(0, 0, info, 1);
    EXPECT_INT_EQ(-12, ret);
    mocker_clean();


    info[0].fd_handle = calloc(1, sizeof(struct socket_peer_info));

    ret = ra_peer_socket_send(0, info[0].fd_handle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = ra_peer_socket_send(0, info[0].fd_handle, data, max_size);
    EXPECT_INT_EQ(max_size, ret);

    ret = ra_peer_socket_recv(0, info[0].fd_handle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = ra_peer_socket_recv(0, info[0].fd_handle, data, max_size);
    EXPECT_INT_EQ(max_size, ret);

   
    rs_conn[0].phy_id = 0;
    rs_conn[0].fd = 0;
    ret = ra_peer_set_conn_param(info, rs_conn, 0, 0);
    EXPECT_INT_EQ(0, ret);

    unsigned int temp_depth = 128;
    unsigned int qp_num = 0;
    ret = ra_peer_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_get_tsqp_depth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);


    ret = ra_peer_qp_create(rdma_handle, flag, qp_mode, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle);
    ret = ra_peer_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle_with_attr);

    struct qos_attr qos_attr= {0};
    qos_attr.tc = 110;
    qos_attr.sl = 3;
    ret = ra_peer_set_qp_attr_qos(qp_handle, &qos_attr);
    EXPECT_INT_EQ(0, ret);

    unsigned int rdma_timeout = 6;
    ret = ra_peer_set_qp_attr_timeout(qp_handle, &rdma_timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = ra_peer_set_qp_attr_retry_cnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_notify_base_addr_init(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_notify_base_addr_init(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = notify_base_addr_uninit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = notify_base_addr_uninit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);
#if 1

    ret = ra_peer_qp_connect_async(qp_handle, info[0].fd_handle);
    EXPECT_INT_EQ(0, size);

    close[0].fd_handle = info[0].fd_handle;
    ret = ra_peer_socket_batch_close(0, close, 1);
    EXPECT_INT_EQ(0, size);

    mocker(memset_s, 20, 1);
    ra_peer_socket_batch_close(0, close, 1);
    mocker_clean();

    ret = ra_peer_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(0, ret);

    struct mr_info mr_info;
    mr_info.addr = addr;
    mr_info.size = size;
    mr_info.access = access;

    void *mr_handle = NULL;

    ret = ra_peer_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_mr_dereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_register_mr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_deregister_mr(rdma_handle, mr_handle);
    EXPECT_INT_EQ(0, ret);

    void *comp_channel = NULL;
    ret = ra_peer_create_comp_channel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_destroy_comp_channel(comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_send_wr(qp_handle, wr, &wqe_index);
    EXPECT_INT_EQ(0, ret);

    struct srq_attr attr = {0};
    ret = ra_peer_create_srq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_destroy_srq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    struct recv_wrlist_data rev_wr = {0};
    rev_wr.wr_id = 100;
    rev_wr.mem_list.lkey = 0xff;
    rev_wr.mem_list.addr = addr;
    rev_wr.mem_list.len = size;
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    ret = ra_peer_recv_wrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(0, ret);

    unsigned long long notify_size;
    ret = ra_peer_get_notify_base_addr(qp_handle, &va, &notify_size);
    EXPECT_INT_EQ(0, ret);
#endif
    ret = ra_peer_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_peer_qp_destroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
#if 1
    ret = ra_peer_deinit(&config);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_deinit(&config);
    EXPECT_INT_EQ(0, ret);

    //mocker((stub_fn_t)drvDeviceGetPhyIdByIndex, 10, -1);
    //ret = ra_peer_get_server_devid(0, &dev_id);
    //EXPECT_INT_EQ(-ENODEV, ret);
    //mocker_clean();

    struct socket_connect_info_t connect_err_rs[1] = { 0 };
    connect_err_rs[0].socket_handle = socket_handle;
    mocker((stub_fn_t)rs_socket_batch_connect, 10, -1);
    ret = ra_peer_socket_batch_connect(0, connect_err_rs, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker((stub_fn_t)rs_socket_set_scope_id, 10, -2);
    ret = ra_peer_socket_batch_connect(0, connect_err_rs, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct socket_listen_info_t listen_err_rs[1] = {0};
    listen_err_rs[0].socket_handle = socket_handle;
    mocker((stub_fn_t)rs_socket_listen_start, 10, -1);
    ret = ra_peer_socket_listen_start(0, listen_err_rs, 1);
    EXPECT_INT_NE(0, ret);
    mocker((stub_fn_t)rs_socket_set_scope_id, 10, -2);
    ret = ra_peer_socket_listen_start(0, listen_err_rs, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct socket_listen_info_t listen_err_rs2[1];
    listen_err_rs2[0].socket_handle = socket_handle;
    listen_err_rs2[0].port = 0;
    mocker((stub_fn_t)rs_socket_listen_stop, 10, -1);
    ret = ra_peer_socket_listen_stop(0, listen_err_rs2, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct socket_info_t info_err_rs[1];
    info_err_rs[0].socket_handle = socket_handle;
    info_err_rs[0].fd_handle = NULL;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = ra_peer_get_sockets(0, 0, info_err_rs, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    mocker(ra_peer_set_conn_param, 1, 1);
    ret = ra_peer_get_sockets(0, 0, info_err_rs, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    struct socket_info_t info_err_rs2[1];
    info_err_rs2[0].socket_handle = socket_handle;
    info_err_rs2[0].fd_handle = NULL;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ret = ra_peer_get_sockets(0, 0, info_err_rs2, 1);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

    struct socket_info_t info_err_rs3[1];
    info_err_rs3[0].socket_handle = socket_handle;
    info_err_rs3[0].fd_handle = NULL;
    mocker_ret((stub_fn_t)memcpy_s, 0, 1, 1);
    ret = ra_peer_get_sockets(0, 0, info_err_rs3, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    struct socket_info_t  info_err_rs4[1];
    info_err_rs4[0].socket_handle = socket_handle;
    info_err_rs4[0].fd_handle = NULL;
    mocker((stub_fn_t)rs_get_sockets, 10, 0);
    mocker((stub_fn_t)rs_get_ssl_enable, 10, -1);
    ret = ra_peer_get_sockets(0, 0, info_err_rs4, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_set_tsqp_depth, 10, -1);
    ret = ra_peer_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_get_tsqp_depth, 10, -1);
    ret = ra_peer_get_tsqp_depth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(-1, ret);

	qp_handle = NULL;
    qp_handle_with_attr = NULL;
    mocker((stub_fn_t)calloc, 10, NULL);
    ret  = ra_peer_qp_create(rdma_handle, flag, qp_mode, &qp_handle);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle);
    ret  = ra_peer_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    mocker((stub_fn_t)rs_qp_create, 10, 1);
    mocker((stub_fn_t)rs_qp_create_with_attrs, 10, 1);
    ret = ra_peer_qp_create(rdma_handle, flag, qp_mode, &qp_handle);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle);
    ret  = ra_peer_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    ret = ra_peer_qp_create(rdma_handle, flag, qp_mode, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)rs_qp_destroy, 10, -1);
    ret = ra_peer_qp_destroy(qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    qp_mode = 2;
    ret = ra_peer_qp_create(rdma_handle, flag, qp_mode, &qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_qos(qp_handle, &qos_attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_timeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_retry_cnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_notify_base_addr_init(1000, 0);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = notify_base_addr_uninit(1000, 0);
    EXPECT_INT_EQ(-EINVAL, ret);


    mocker((stub_fn_t)rs_get_qp_status, 10, -1);
    ret = ra_peer_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_mr_reg, 10, -1);
    ret = ra_peer_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_mr_dereg, 10, -1);
    ret = ra_peer_mr_dereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_register_mr, 10, -1);
    ret = ra_peer_register_mr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_deregister_mr, 10, -1);
    ret = ra_peer_deregister_mr(rdma_handle, mr_handle);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_create_comp_channel, 10, -1);
    ret = ra_peer_create_comp_channel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_destroy_comp_channel, 10, -1);
    ret = ra_peer_destroy_comp_channel(comp_channel);
    EXPECT_INT_EQ(-1, ret);

    struct srq_attr attr_srq = {0};
    mocker((stub_fn_t)rs_create_srq, 10, -1);
    ret = ra_peer_create_srq(rdma_handle, &attr_srq);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_destroy_srq, 10, -1);
    ret = ra_peer_destroy_srq(rdma_handle, &attr_srq);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_recv_wrlist, 10, -1);
    ret = ra_peer_recv_wrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    //mocker((stub_fn_t)rs_send_wr, 10, -1);
    //ret = ra_peer_send_wr(qp_handle, wr, &wqe_index);
    //EXPECT_INT_EQ(-1, ret);
    //mocker_clean();

    struct socket_info_t info_rs[1];
    info_rs[0].socket_handle = socket_handle;

    ret = ra_peer_get_sockets(0, 0, info_rs, 1);
    EXPECT_INT_EQ(1, ret);

    info_rs[0].fd_handle = calloc(1, sizeof(struct socket_peer_info));

    mocker((stub_fn_t)rs_qp_connect_async, 10, -1);
    ret = ra_peer_qp_connect_async(qp_handle, info_rs[0].fd_handle);
    EXPECT_INT_EQ(-1, ret);


    mocker((stub_fn_t)rs_get_notify_mr_info, 10, -1);
    ret = ra_peer_get_notify_base_addr(qp_handle, &va, &notify_size);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_peer_socket_send, 10, -1);
    ret = ra_peer_socket_send(dev_id, info_rs[0].fd_handle, data, size);
    EXPECT_INT_EQ(-1, ret);

    ret = ra_peer_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    struct socket_close_info_t close_rs[1] = {0};
    close_rs[0].fd_handle = info_rs[0].fd_handle;
    close_rs[0].socket_handle = socket_handle;
    mocker((stub_fn_t)rs_socket_batch_close, 10, -1);
    ret = ra_peer_socket_batch_close(0, close_rs, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_init, 10, -1);
    ret = ra_peer_init(&config, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_deinit, 10, -11);
    ret = ra_peer_deinit(&config);
    EXPECT_INT_EQ(-11, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_deinit, 10, -1);
    ret = ra_peer_deinit(&config);
    EXPECT_INT_EQ(-1, ret);

    mocker_clean();
#endif
    return;
}

void tc_peer_fail()
{
    struct ra_socket_handle socket_handle;
    socket_handle.rdev_info.phy_id = 0;
    socket_handle.rdev_info.family = 0;
    struct socket_connect_info_t conn[1];
    conn[0].socket_handle = &socket_handle;
    conn[0].port = 0;
    struct socket_connect_info rs_conn[1] = {0};
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ra_get_socket_connect_info(conn, 1, rs_conn, 2);
    mocker_clean();

    struct socket_listen_info_t conn_listen[1];
    struct socket_listen_info rs_conn_listen[1];
    conn_listen[0].phase = 0;
    conn_listen[0].err = 0;
    conn_listen[0].socket_handle = &socket_handle;
    conn_listen[0].port = 0;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ra_get_socket_listen_info(conn_listen, 1, rs_conn_listen, 2);
    mocker_clean();

    rs_conn_listen[0].phase = 0;
    rs_conn_listen[0].err = 0;
    rs_conn_listen[0].phy_id = 0;
    rs_conn_listen[0].family = 0;
    conn_listen[0].socket_handle = &socket_handle;
    rs_conn_listen[0].port = 0;
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ra_get_socket_listen_result(rs_conn_listen, 1, conn_listen, 2);
    mocker_clean();


    struct socket_listen_info_t conn_listen_info[1] = {0};
    conn_listen_info[0].port  = 0;
    conn_listen_info[0].socket_handle = &socket_handle;
    mocker((stub_fn_t)ra_get_socket_listen_info, 10, 0);
    mocker((stub_fn_t)rs_socket_listen_start, 10, -1);
    ra_peer_socket_listen_start(0, conn_listen_info, 1);
    mocker((stub_fn_t)rs_socket_listen_stop, 10, -1);
    ra_peer_socket_listen_stop(0, conn_listen_info, 1);
    mocker_clean();

    struct socket_peer_info peer_socket_handle = {0};
    int ret;

    ret = ra_peer_socket_send(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.ssl_enable = 1;
    ret = ra_peer_socket_send(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.ssl_enable = 0;
    ret = ra_peer_socket_recv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.ssl_enable = 1;
    ret = ra_peer_socket_recv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    struct rdev rdev_info;
	rdev_info.phy_id = 0;
    struct socket_wlist_info_t white_list[1];
    mocker((stub_fn_t)inet_ntoa, 10, NULL);
    ra_peer_socket_white_list_add(rdev_info, white_list, 1);
    ra_peer_socket_white_list_del(rdev_info, white_list, 1);
    mocker_clean();

    mocker((stub_fn_t)rs_socket_deinit, 10, -1);
    ra_peer_socket_deinit(rdev_info);
    mocker_clean();

    mocker((stub_fn_t)rs_peer_get_ifnum, 10, -1);
    unsigned int num = 0;
    ra_peer_get_ifnum(0, &num);
    mocker_clean();

    struct interface_info interface_infos[1];
    mocker((stub_fn_t)rs_peer_get_ifaddrs, 10, -1);
    ra_peer_get_ifaddrs(0, interface_infos, &num);
    mocker_clean();

    return;
}

void tc_ra_peer_epoll_ctl_add()
{
    int ret;

    ret = ra_peer_epoll_ctl_add(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)rs_epoll_ctl_add, 3, -1);
    ret = ra_peer_epoll_ctl_add(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_set_tcp_recv_callback()
{
    ra_set_tcp_recv_callback(NULL, NULL);
    (void)ra_peer_set_tcp_recv_callback(0, NULL);

    struct ra_socket_handle abc = {0};
    int cb = 0;
    ra_set_tcp_recv_callback(&abc, &cb);

    abc.rdev_info.phy_id = 10000;
    ra_set_tcp_recv_callback(&abc, &cb);
    return;
}

void tc_ra_peer_epoll_ctl_mod()
{
    int ret;

    ret = ra_peer_epoll_ctl_mod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)rs_epoll_ctl_mod, 3, -1);
    ret = ra_peer_epoll_ctl_mod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_epoll_ctl_del()
{
    int ret;
    struct socket_peer_info fd_handle = {0};

    ret = ra_peer_epoll_ctl_del((const void *)&fd_handle);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)rs_epoll_ctl_del, 3, -1);
    ret = ra_peer_epoll_ctl_del((const void *)&fd_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_cq_create()
{
    int ret;
    struct ra_rdma_handle rdma_handle;
    rdma_handle.rdev_info.phy_id = 0;
    rdma_handle.rdev_index = 0;

    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    struct cq_attr attr;
    attr.ib_send_cq = &ib_send_cq;
    attr.ib_recv_cq = &ib_recv_cq;
    attr.send_cq_depth = 16384;
    attr.recv_cq_depth = 16384;
    attr.send_cq_event_id = 1;
    attr.recv_cq_event_id = 2;

    ret = ra_peer_cq_create(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_cq_destroy(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)rs_cq_create, 3, -1);
    ret = ra_peer_cq_create(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = ra_peer_cq_create(&rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)rs_cq_destroy, 3, -1);
    ret = ra_peer_cq_destroy(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
    return;
}

void tc_ra_peer_normal_qp_create()
{
    int ret;
    struct ra_qp_handle *qp_handle;
    struct ibv_qp_init_attr qp_init_attr;
    struct ra_rdma_handle rdma_handle;
    rdma_handle.rdev_info.phy_id = 0;
    rdma_handle.rdev_index = 0;
    void** qp = NULL;
    ret = ra_peer_normal_qp_create(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_normal_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)calloc, 3, NULL);
    ret = ra_peer_normal_qp_create(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_normal_qp_create, 3, -1);
    ret = ra_peer_normal_qp_create(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    ret = ra_peer_normal_qp_create(&rdma_handle, &qp_init_attr, &qp_handle, qp);
    EXPECT_INT_EQ(0, ret);
    mocker((stub_fn_t)rs_normal_qp_destroy, 3, -1);
    ret = ra_peer_normal_qp_destroy(qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
    return;
}

void tc_ra_peer_create_event_handle()
{
    int ret;
    int fd;

    ret = ra_peer_create_event_handle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_ctl_event_handle()
{
    int ret;
    int fd_handle;

    ret = ra_peer_ctl_event_handle(0, NULL, 0, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = ra_peer_ctl_event_handle(0, &fd_handle, 1, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_wait_event_handle()
{
    int ret;
    int fd;

    ret = ra_peer_wait_event_handle(0, NULL, 0, -1, 0);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_destroy_event_handle()
{
    int ret;
    int fd;

    ret = ra_peer_destroy_event_handle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_loopback_qp_create()
{
    struct ra_rdma_handle *rdma_handle2;
    struct ra_rdma_handle *rdma_handle;
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    int ret = 0;

    mocker(ra_rdev_init_check_ip, 10, 0);
    ret = ra_rdev_init(NETWORK_PEER_ONLINE, NO_USE, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_rdev_get_handle(rdev_info.phy_id, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);
    mocker_clean();

    struct loopback_qp_pair qp_pair;
    void *qp_handle = NULL;

    ret = ra_loopback_qp_create(NULL, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = ra_loopback_qp_create(rdma_handle, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = ra_loopback_qp_create(rdma_handle, &qp_pair, NULL);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdev_info.phy_id = 128;
    ret = ra_loopback_qp_create(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdev_info.phy_id = 0;
    mocker(ra_peer_loopback_qp_create, 10, -1);
    ret = ra_loopback_qp_create(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128100, ret);

    mocker_clean();
    // 创建flush qp主成功场景
    ret = ra_loopback_qp_create(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    // 销毁flush qp
    ret = ra_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_rdev_deinit(rdma_handle, NO_USE);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct ra_rdma_handle rdma_handle_tmp = {
        .rdev_info = rdev_info,
        .rdev_index = 0,
    };
    struct loopback_qp_pair qp_pair;
    void *qp_handle = NULL;
    int ret = 0;

    mocker(ra_peer_loopback_single_qp_create, 10, -1);
    ret = ra_peer_loopback_qp_create(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker_invoke(ra_peer_loopback_single_qp_create, ra_peer_loopback_single_qp_create_stub, 10);
    ret = ra_peer_loopback_qp_create(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(ra_peer_loopback_qp_modify, 10, -1);
    ret = ra_peer_loopback_qp_create(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_single_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct ra_rdma_handle rdma_handle_tmp = {
        .rdev_info = rdev_info,
        .rdev_index = 0,
    };
    struct ra_qp_handle *qp_handle = NULL;
    struct ibv_qp *qp = NULL;
    int ret = 0;

    mocker(ra_peer_cq_create, 10, -1);
    ret = ra_peer_loopback_single_qp_create(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(ra_peer_normal_qp_create, 10, -1);
    ret = ra_peer_loopback_single_qp_create(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}
