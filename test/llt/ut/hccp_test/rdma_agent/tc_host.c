/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra peer unit test.
 * Author:
 * Create: 2020-07-21
 */
#include "hccp.h"
#include "securec.h"
#include "ut_dispatch.h"
#include "ra_client_host.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"

extern struct ra_socket_ops g_ra_peer_socket_ops;
extern int hdc_send_recv_pkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);
extern int ra_inet_pton(int family, union hccp_ip_addr ip, char net_addr[], unsigned int len);
extern int ra_hdc_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id, unsigned long long **notify_va);
extern int ra_rdev_init_check(int mode, struct rdev rdev_info, char local_ip[], unsigned int num, void *rdma_handle);
extern int ra_rdev_init_check_ip(int mode, struct rdev rdev_info, char local_ip[]);
extern void rs_set_ctx(unsigned int phy_id);
extern int rs_socket_get_client_socket_err_info(struct socket_connect_info conn[], struct socket_err_info err[],
    unsigned int num);
extern int rs_socket_get_server_socket_err_info(struct socket_listen_info conn[], struct server_socket_err_info err[],
    unsigned int num);
extern int rs_socket_accept_credit_add(struct socket_listen_info conn[], uint32_t num, unsigned int credit_limit);
extern int ra_peer_socket_accept_credit_add(unsigned int phy_id, struct SocketListenInfoT conn[], unsigned int num,
    unsigned int credit_limit);

unsigned int g_interface_version;

int ra_hdc_get_ifaddrs_stub(unsigned int phy_id, struct ifaddr_info ifaddr_infos[], unsigned int *num)
{
    *num = 4;
    return 0;
}

int ra_get_interface_version_stub(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

void tc_ifaddr()
{
    int ret;

    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 100);
    g_interface_version = 0;
    unsigned int ifaddr_num = 4;
    struct interface_info interface_infos[4] = {0};
    bool is_all = false;

    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 1;
    ifaddr_num = 4;
    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    mocker(ra_hdc_get_ifaddrs, 100 , 0);
    g_interface_version = 1;
    ifaddr_num = 4;
    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    g_interface_version = 2;
    ifaddr_num = 4;
    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 3;
    ifaddr_num = 4;
    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);

    g_interface_version = 1;
    ifaddr_num = 4;
    mocker(calloc, 10 , NULL);
    ret = ra_ifaddr_info_converter(0, is_all, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(-22, ret);
    mocker_clean();

    return;
}

int stub_ra_hdc_send_wr_v2(struct ra_qp_handle *qp_hdc, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp)
{
    return 0;
}

void tc_host()
{
    dl_hal_init();
    int ret = 0;
    struct RdevInitInfo init_info = {0};
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct mr_info mr_info;
    mocker((stub_fn_t)hdc_send_recv_pkt, 200, 0);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 200);
    g_interface_version = 2;

    ret = ra_rdev_init_with_backup(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct ra_rdma_handle *rdma_handle_bakcup = NULL;
    ret = ra_rdev_init_with_backup(&init_info, &rdev_info, &rdev_info, &rdma_handle_bakcup);
    EXPECT_INT_NE(0, ret);

    ra_rdev_deinit(NULL, NOTIFY);
    struct ra_rdma_handle *rdma_handle = NULL;
    struct ra_rdma_handle *rdma_handle2 = NULL;
    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328008, ret);

    mocker(ra_rdev_init_check_ip, 10, 0);
    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_rdev_get_handle(rdev_info.phy_id, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);
    mocker_clean();

    mocker((stub_fn_t)hdc_send_recv_pkt, 200, 0);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 200);
    struct ra_socket_handle *socket_handle = NULL;
    ret = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_qp_create(NULL, 0, 1, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create_with_attrs(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    ra_get_ifnum(NULL, NULL);

    int ifnum = 0;
    struct ra_init_config config = { 0 };
    struct ra_get_ifattr ifattr_config = { 0 };
	ifattr_config.nic_position = NETWORK_PEER_ONLINE;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    ifattr_config.phy_id = 0;
    ifattr_config.nic_position = NETWORK_OFFLINE;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    ifattr_config.is_all = true;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_NE(0, ret);
    ifattr_config.is_all = false;

    ifattr_config.nic_position = 5;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(228304, ret);

    ifattr_config.phy_id = 129;
    ifattr_config.nic_position = NETWORK_OFFLINE;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

    ifattr_config.nic_position = 5;
    ret = ra_get_ifnum(&ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

    ra_get_ifaddrs(NULL, NULL, NULL);
    ra_socket_white_list_add(socket_handle, NULL, 0);
    ra_socket_white_list_del(socket_handle, NULL, 0);

    ret = ra_socket_set_white_list_status(0);
    EXPECT_INT_EQ(0, ret);
    ret = ra_socket_set_white_list_status(3);
    EXPECT_INT_EQ(128203, ret);
    ret = ra_socket_get_white_list_status(NULL);
    EXPECT_INT_EQ(128203, ret);
    unsigned int enable;
    ret = ra_socket_get_white_list_status(&enable);
    EXPECT_INT_EQ(0, ret);

    ra_socket_listen_start(NULL, 0);
    ra_socket_listen_stop(NULL, 0);
    ra_get_sockets(0, NULL, 0, NULL);
    ra_socket_send(NULL, NULL, 0, NULL);
    ra_socket_recv(NULL, NULL, 0, NULL);
    ra_get_qp_status(NULL, NULL);
    ra_mr_reg(NULL, NULL);
    ra_mr_dereg(NULL, NULL);
    ra_send_wr(NULL, NULL, NULL);
    ra_send_wrlist(NULL, NULL, NULL, 0, NULL);
    ra_get_notify_base_addr(NULL, NULL, NULL);
    ra_get_notify_mr_info(NULL, NULL);
    ra_qp_destroy(NULL);
    ra_qp_connect_async(NULL, NULL);
    ra_register_mr(NULL, NULL, NULL);
    ra_deregister_mr(NULL, NULL);
    ra_get_cqe_err_info(0, NULL);
    ra_get_qp_attr(NULL, NULL);

    ret = ra_send_wr_v2(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct ra_qp_handle ra_qp_handle_v2 = {0};
    struct send_wr_v2 wr_v2 = {0};
    struct send_wr_rsp op_rsp_v2 = {0};
    struct sg_list list_v2 = {0};
    list_v2.len = 0xFFFFFFFF;
    wr_v2.buf_list = &list_v2;
    ret = ra_send_wr_v2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    list_v2.len = 0x1;
    ret = ra_send_wr_v2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    struct ra_rdma_ops rdma_ops_v2 = {0};
    rdma_ops_v2.ra_send_wr_v2 = stub_ra_hdc_send_wr_v2;
    ra_qp_handle_v2.rdma_ops = &rdma_ops_v2;
    ret = ra_send_wr_v2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_EQ(0, ret);

    int devid = 0;
    int ra_case_other = 2;
    unsigned int remote_ip[1] = {0};
    struct socket_wlist_info_t white_list[1];
    int flag = 0;
    int port = 0;
    int timeout = 0;
    void *addr = malloc(1);
    void *data = malloc(1);
    unsigned long long size = 1;
    int access = 1;

    struct send_wr *wr = calloc(1, sizeof(struct send_wr));
    struct sg_list list[2];
    list[0].len = 1;
    list[1].len = 2147483649;
    int wqe_index = 0;
    wr->buf_list = list;
    int index = 0;
    unsigned long pa = 1;
    unsigned long va = 1;
    int sock_fd = 1;
    struct ra_rdma_ops rdma_ops;
 
    struct socket_hdc_info *hdc_socket_handle = calloc(1, sizeof(struct socket_hdc_info));
    struct SocketConnectInfoT conn[1];
    conn[0].socket_handle = NULL;
    ra_socket_batch_connect(&conn, 1);
    ra_socket_batch_close(&conn, 1);

    conn[0].socket_handle = socket_handle;
    struct SocketInfoT conn_tmp = {0};
    struct SocketCloseInfoT close[1] = {0};
    struct SocketListenInfoT listen[1];

    listen[0].socket_handle = NULL;
    ra_socket_listen_start(&listen, 1);
    ra_socket_listen_stop(&listen, 1);

    listen[0].socket_handle = socket_handle;
    close[0].socket_handle = socket_handle;
    close[0].fd_handle = hdc_socket_handle;

    struct SocketInfoT socket_info[1] = {0};

	struct ra_init_config offline_config = {
		.phy_id = 0,
		.nic_position = 1,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
	};
    struct ra_get_ifattr offline_ifattr_config = {
		.phy_id = 0,
		.nic_position = 1,
        .is_all = 0,
	};
    int qp_status = 0;
    int server = 0;
    int client = 1;
    struct send_wr_rsp op_rsp = {0};

    struct send_wrlist_data wrlist_send[1];
    struct send_wr_rsp wrlist_rsp[1];
	unsigned int ifaddr_num = 4;
	struct interface_info interface_infos[4] = {0};
	ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(0, ret);

    ifaddr_num = 0;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

	ifaddr_num = 9;
	ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

    ifaddr_num = 4;
    offline_ifattr_config.phy_id = 128;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    offline_ifattr_config.phy_id = 0;
    offline_ifattr_config.nic_position = NETWORK_PEER_ONLINE;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    offline_ifattr_config.nic_position = 5;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(228304, ret);

    offline_ifattr_config.is_all = true;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_NE(0, ret);
    offline_ifattr_config.is_all = false;

    mocker_invoke(ra_hdc_get_ifaddrs, ra_hdc_get_ifaddrs_stub, 10);
    ifaddr_num = 4;
    ret = ra_ifaddr_info_converter(0, 0, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    mocker_clean();
    mocker((stub_fn_t)hdc_send_recv_pkt, 200, 0);
    offline_config.nic_position = 1;
    unsigned int interface_version = 0;
    ret  = ra_get_interface_version (0, 0, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = ra_get_interface_version (0, 12, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = ra_get_interface_version (0, 24, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = ra_get_interface_version (0, 25, NULL);
    EXPECT_INT_EQ(128303, ret);

    ret  = conver_return_code (0, 100001);
    EXPECT_INT_EQ(100001, ret);

    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 200);
    g_interface_version = 2;
    ret = ra_socket_white_list_add(socket_handle, white_list, 1);
    EXPECT_INT_EQ(0, ret);
    ret = ra_socket_white_list_del(socket_handle, white_list, 1);
    EXPECT_INT_EQ(0, ret);

    ret = ra_socket_batch_connect(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = ra_socket_batch_connect(&conn, 1);
    EXPECT_INT_EQ(0, ret);

    unsigned long long sent_size = 0;
    ret = ra_socket_send(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_EQ(128203, ret);

    unsigned long long received_size = 0;
    ret = ra_socket_recv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_EQ(128203, ret);

    ret = ra_socket_batch_close(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = ra_socket_batch_close(&close, 1);
    EXPECT_INT_EQ(0, ret);

    hdc_socket_handle = calloc(1, sizeof(struct socket_hdc_info));

    socket_info[0].socket_handle = NULL;
    unsigned int connected_num = 0;
    ra_get_sockets(0, &socket_info, 1, &connected_num);

    socket_info[0].fd_handle = hdc_socket_handle;
    socket_info[0].socket_handle = socket_handle;
    ret = ra_get_sockets(0, &socket_info, 1, &connected_num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_socket_listen_start(&listen, 1);
    EXPECT_INT_EQ(0, ret);

    socket_handle->rdev_info.family = AF_INET;
    socket_handle->rdev_info.phy_id = 0;
    ret = ra_socket_listen_stop(&listen, 1);
    EXPECT_INT_EQ(0, ret);

    socket_handle->rdev_info.phy_id = 8;
    ret = ra_socket_send(hdc_socket_handle, data, 1, &sent_size);

    ret = ra_socket_recv(hdc_socket_handle, data, 1, &received_size);

    mocker(ra_hdc_socket_send, 10 , 1);
    ret = ra_socket_send(hdc_socket_handle, data, 1, &sent_size);

    mocker(ra_hdc_socket_recv, 10 , 1);
    ret = ra_socket_recv(hdc_socket_handle, data, 1, &received_size);

    socket_handle->rdev_info.phy_id = 129;
    ret = ra_socket_white_list_add(socket_handle, white_list, 1);
    EXPECT_INT_NE(0, ret);
    ret = ra_socket_white_list_del(socket_handle, white_list, 1);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_batch_connect(&conn, 1);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_send(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_recv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_batch_close(&close, 1);
    EXPECT_INT_NE(0, ret);

    ra_get_sockets(0, &socket_info, 1, &connected_num);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_listen_start(&listen, 1);
    EXPECT_INT_NE(0, ret);
    socket_handle->rdev_info.family = AF_INET;
    ret = ra_socket_listen_stop(&listen, 1);
    EXPECT_INT_NE(0, ret);

    ret = ra_qp_create(rdma_handle, 3, 0, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create_with_attrs(rdma_handle, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(rdma_handle, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct ra_qp_handle *qp_handle = NULL;
    struct ra_qp_handle *qp_handle_with_attr = NULL;
    struct ra_qp_handle *typical_qp_handle = NULL;
    struct ra_qp_handle *ai_qp_handle = NULL;
    struct AiQpInfo info;
    ret = ra_qp_create_with_attrs(rdma_handle, NULL, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(rdma_handle, NULL, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    struct QpExtAttrs ext_attrs;
    ext_attrs.version = 0;
    ret = ra_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(rdma_handle, &ext_attrs, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    ext_attrs.version = QP_CREATE_WITH_ATTR_VERSION;
    ext_attrs.qp_mode = -1;
    ret = ra_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(rdma_handle, &ext_attrs, &info, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ext_attrs.qp_mode = RA_RS_GDR_TMPL_QP_MODE;
    ret = ra_qp_create(rdma_handle, 1, 0, &qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create(rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = ra_ai_qp_create(rdma_handle, &ext_attrs, &info, &ai_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_qp_destroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = ra_qp_destroy(ai_qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_batch_modify(NULL, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);
 
    struct ra_qp_handle *batch_modify_qp_hdc[1];
    batch_modify_qp_hdc[0] = qp_handle;
 
    ret = ra_qp_batch_modify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_EQ(0, ret);
 
    ret = ra_qp_batch_modify(rdma_handle, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_EQ(0, ret);

    struct ra_rdma_handle rdma_handle_err = {0};
    rdma_handle_err = *rdma_handle;
    (&rdma_handle_err)->rdev_info.phy_id = 128;
 
    ret = ra_qp_batch_modify(&rdma_handle_err, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_NE(0, ret);

    batch_modify_qp_hdc[0] = NULL;
    ret = ra_qp_batch_modify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_NE(0, ret);

	unsigned int temp_depth, qp_num;
	ret = ra_get_tsqp_depth(NULL, &temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = ra_get_tsqp_depth(rdma_handle, NULL, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = ra_get_tsqp_depth(rdma_handle, &temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

	ret = ra_set_tsqp_depth(NULL, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = ra_set_tsqp_depth(rdma_handle, temp_depth, NULL);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 1;
	ret = ra_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 8;
	ret = ra_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

    struct ra_qp_handle *qp_handle_wrlist = NULL;
    ret = ra_qp_create(rdma_handle, 0, 0, &qp_handle_wrlist);
    EXPECT_INT_EQ(0, ret);
    qp_handle_wrlist->rdma_ops = NULL;
    unsigned int complete_num = 1;
    wrlist_send[0].mem_list.len = 1;
    ret = ra_send_wrlist(qp_handle_wrlist, wrlist_send, wrlist_rsp, 1, &complete_num);
    qp_handle_wrlist->rdma_ops = rdma_handle->rdma_ops;
    ret = ra_qp_destroy(qp_handle_wrlist);
    EXPECT_INT_EQ(0, ret);

    mr_info.addr = addr;
    mr_info.access = access;
    mr_info.lkey = 0;
    mr_info.size = size;
    ret = ra_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_notify_base_addr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
    struct mr_info mr_info2;
    ret = ra_get_notify_mr_info(rdma_handle, &mr_info2);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_qp_status(qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    ret = ra_send_wr(qp_handle, wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_connect_async(qp_handle, hdc_socket_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_mr_dereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_destroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    // typical qp create
    struct typical_qp qp_info = {0};
    ret = ra_typical_qp_create(NULL, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, NULL, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 3, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, -1, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_EQ(0, ret);

    // typical modify qp
    struct typical_qp remote_qp_info = {0};
    ret = ra_typical_qp_modify(typical_qp_handle, NULL, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_modify(NULL, &qp_info, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_modify(typical_qp_handle, &qp_info, &remote_qp_info);
    EXPECT_INT_EQ(0, ret);

    // typical send wr
    ret = ra_typical_send_wr(NULL, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_send_wr(typical_qp_handle, wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_destroy(typical_qp_handle);
    EXPECT_INT_EQ(0, ret);   

    struct ra_qp_handle *qp_handle1 = NULL;
    struct ra_qp_handle *qp_handle1_with_attr = NULL;
    struct ra_qp_handle *typical_qp_handle1 = NULL;
    ret = ra_qp_create(rdma_handle, 0, 5, &qp_handle1);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_qp_attr(qp_handle1, &qp_num);
    EXPECT_INT_NE(0, ret);

    struct qp_attr attr;
    struct ra_qp_handle qp_handle_tmp;
    ret = ra_get_qp_attr(&qp_handle_tmp, &attr);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->rdev_info.phy_id =129;
    ret = ra_qp_create(rdma_handle, 0, 0, &qp_handle1);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle1_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle1);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_tsqp_depth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(128103, ret);
    ret = ra_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdma_ops->ra_qp_create = NULL;
    rdma_handle->rdma_ops->ra_qp_create_with_attrs = NULL;
    rdma_handle->rdma_ops->ra_typical_qp_create = NULL;
    ret = ra_qp_create(rdma_handle, 0, 0, &qp_handle1);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle1_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle1);
    EXPECT_INT_NE(0, ret);

    struct ra_qp_handle qp_handle2 = {0};
    qp_handle = &qp_handle2;
    qp_handle->rdma_ops = NULL;
    mr_info.addr = addr;
    mr_info.access = access;
    mr_info.lkey = 0;
    mr_info.size = size;
    ret = ra_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_NE(0, ret);

    rdma_handle->rdev_info.phy_id = 0;
    ret = ra_get_notify_base_addr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
	ret = ra_get_notify_mr_info(rdma_handle, &mr_info2);
	EXPECT_INT_EQ(0, ret);

    ret = ra_get_qp_status(qp_handle, &qp_status);
    EXPECT_INT_NE(0, ret);

    ret = ra_send_wr(qp_handle, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);

    ret = ra_qp_connect_async(qp_handle, hdc_socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_mr_dereg(qp_handle, &mr_info);
    EXPECT_INT_NE(0, ret);

    ret = ra_qp_destroy(qp_handle);
    EXPECT_INT_NE(0, ret);

    socket_handle->rdev_info.phy_id = 128;
    ret = ra_socket_deinit(socket_handle);
    EXPECT_INT_EQ(128003, ret);

    mocker(ra_hdc_socket_deinit, 1, -1);
    socket_handle->rdev_info.phy_id = 0;
    ret = ra_socket_deinit(socket_handle);
    EXPECT_INT_EQ(128000, ret);

    rdma_handle->rdev_info.phy_id = 128;
    ret = ra_rdev_deinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(128003, ret);

    rdma_handle->rdev_info.phy_id = 0;
    ret = ra_rdev_deinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);

    mocker_clean();

    ret = ra_init(NULL);
    EXPECT_INT_EQ(128003, ret);
    ret = ra_deinit(NULL);
    EXPECT_INT_EQ(128003, ret);
    //dev id > MAX_DEV_NUM
    struct ra_init_config erro_device_config = {
        .phy_id = 129,
        .nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    ret = ra_init(&erro_device_config);
    EXPECT_INT_EQ(128003, ret);

    ret = ra_deinit(&erro_device_config);
    EXPECT_INT_EQ(128003, ret);

    struct ra_init_config device_config = {
        .phy_id = 0,
        .nic_position = 0,
        .hdc_type = 0,
    };
    ret = ra_init(&device_config);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_hdc_init, 10 ,0);
    ret = ra_init(&offline_config);
    mocker_clean();

    mocker(ra_hdc_init, 10 ,-1);
    ret = ra_init(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    //mocker(ra_hdc_get_interface_version, 10, -1);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 10);
    ret  = ra_get_interface_version (0, 24, &interface_version);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    struct ra_init_config online_config = {
        .phy_id = 0,
        .nic_position = 2,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    ret = ra_init(&online_config);
    EXPECT_INT_EQ(228004, ret);

    mocker(drvGetProcessSign, 10 ,-1);
    ret = ra_init(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(strcpy_s, 10 ,-1);
    ret = ra_init(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    ret = ra_deinit(&device_config);
    EXPECT_INT_EQ(0, ret);

    device_config.nic_position = 5;
    ret = ra_deinit(&device_config);
    EXPECT_INT_NE(0, ret);

    mocker(ra_hdc_deinit, 10 ,0);
    ret = ra_deinit(&offline_config);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(ra_hdc_deinit, 10 ,-1);
    ret = ra_deinit(&offline_config);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_inet_pton, 10, 0);
    ret =  ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(ra_inet_pton, 10 , -22);
    ret = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 10 ,-1);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 10);
    ret  = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(calloc, 10 , 0);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 10);
    ret = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 100);
    rdev_info.phy_id = 129;
    ret = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_deinit(NULL);
    EXPECT_INT_NE(0, ret);

    rdev_info.phy_id = 0;
    ret = ra_socket_init(5, rdev_info, &socket_handle);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_NE(0, ret);

    //mocker(ra_rdev_init_check, 5 , 0);
    //mocker(ra_hdc_notify_base_addr_init, 10 , -1);
    //ret = ra_rdev_init(5, NOTIFY, rdev_info, &rdma_handle);
    //EXPECT_INT_EQ(128000, ret);
    //mocker_clean();

    mocker(ra_rdev_init_check, 2 , 0);
    mocker(ra_hdc_notify_base_addr_init, 5 , 0);
    mocker(calloc, 10 , NULL);
    ret = ra_rdev_init(5, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328000, ret);
    mocker_clean();

    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    char local_ip[1];
    mocker(ra_get_ifaddrs, 10, 0);
    mocker(ra_inet_pton, 10, -1);
    ra_rdev_init_check_ip(NETWORK_OFFLINE, rdev_info, local_ip);
    mocker_clean();

    mocker(ra_rdev_init_check, 2 , 0);
    mocker(ra_hdc_notify_base_addr_init, 5 , 0);
    ret = ra_rdev_init(10, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(128003, ret);
    mocker_clean();

    mocker(ra_rdev_init_check, 2 , 0);
    mocker(ra_peer_notify_base_addr_init, 5 , 0);
    mocker(memcpy_s, 10 , -1);
    ret = ra_rdev_init(NETWORK_PEER_ONLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328006, ret);
    mocker_clean();

    mocker(ra_rdev_init_check, 2 , 0);
    mocker(ra_hdc_notify_base_addr_init, 5 , 0);
    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(128003, ret);
    mocker_clean();

    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 7;
    mocker(ra_get_ifaddrs, 10 , 0);
    ret = ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(328008, ret);
    mocker_clean();

	unsigned long long *notify_va = NULL;
    mocker(drvDeviceGetIndexByPhyId, 10 , -1);
    ret = ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(halNotifyGetInfo, 10 , -1);
    ret = ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(halMemAlloc, 10 , -1);
    ret = ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(ra_hdc_notify_cfg_set, 5 , -1);
    mocker(halMemFree, 10 , -1);
    ret = ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
	
	struct ra_rdma_handle rdma_handle_tmp = {0};
	struct ra_rdma_ops ops = {0};
	rdma_handle_tmp.rdev_info.phy_id = 0;
	rdma_handle_tmp.rdev_info.family = AF_INET;
	rdma_handle_tmp.rdev_info.family = 0;
	rdma_handle_tmp.rdma_ops = &ops;
	mocker(ra_inet_pton, 1, -1);
	ret = ra_rdev_deinit(&rdma_handle_tmp, NOTIFY);
	EXPECT_INT_EQ(128000, ret);
	mocker_clean();
	
	mocker(ra_inet_pton, 1, 0);
	ret = ra_rdev_deinit(&rdma_handle_tmp, NOTIFY);
	EXPECT_INT_EQ(128003, ret);
	mocker_clean();


    free(addr);
    free(data);
    free(wr);
    //free(qp_handle);
    free(hdc_socket_handle);
    dl_hal_deinit();
    return;
}

int ra_recv_wrlist_stub(struct ra_qp_handle *handle, struct recv_wrlist_data *wr, unsigned int recv_num,
        unsigned int *complete_num)
{
    return 0;
}
void tc_ra_recv_wrlist(void)
{
    int ret;
    struct ra_qp_handle qp_handle = {0};
    struct recv_wrlist_data wr = {0};
    unsigned int recv_num = 1;
    unsigned int complete_num = 0;

    ret = ra_recv_wrlist(NULL, NULL, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.mem_list.len = 0xffffffff;
    ret = ra_recv_wrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.mem_list.len = 100;
    qp_handle.rdma_ops = NULL;
    ret = ra_recv_wrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    struct ra_rdma_ops rdma_ops = {0};
    rdma_ops.ra_recv_wrlist = ra_recv_wrlist_stub;
    qp_handle.rdma_ops = &rdma_ops;
    ret = ra_recv_wrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_host_ra_send_wrlist_ext()
{
    struct ra_qp_handle qp_handle;
    qp_handle.rdma_ops = NULL;

    struct send_wrlist_data_ext wr[1];
    struct send_wr_rsp op_rsp[1];

    unsigned int complete_num;

    ra_send_wrlist_ext(&qp_handle, wr, op_rsp, 1, &complete_num);
}

int ra_send_normal_wrlist_stub(void *qp_handle, struct wr_info wr[], struct send_wr_rsp op_rsp[], unsigned int send_num,
    unsigned int *complete_num)
{
    return 0;
}

void tc_host_ra_send_normal_wrlist()
{
    struct ra_rdma_ops rdma_ops = {0};
    struct ra_qp_handle qp_handle;
    struct send_wr_rsp op_rsp[1];
    qp_handle.rdma_ops = NULL;
    unsigned int complete_num;
    struct wr_info wr[1];
    int ret = 0;

    ret = ra_send_normal_wrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    rdma_ops.ra_send_normal_wrlist = ra_send_normal_wrlist_stub;
    qp_handle.rdma_ops = &rdma_ops;
    wr[0].mem_list.len = MAX_SG_LIST_LEN_MAX + 1;
    ret = ra_send_normal_wrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr[0].mem_list.len = 1;
    ret = ra_send_normal_wrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(0, ret);

    return;
}


int ra_set_qp_attr_qos_stub(struct ra_qp_handle *qp_stub, struct qos_attr *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_qos()
{
    int ret;
    struct qos_attr attr = {0};
    struct ra_qp_handle qp_handle;
    qp_handle.rdma_ops = NULL;

    ret = ra_set_qp_attr_qos(NULL, &attr);
    EXPECT_INT_EQ(128103, ret);

    ret = ra_set_qp_attr_qos(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 256;
    attr.sl = 8;
    ret = ra_set_qp_attr_qos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.sl = 4;
    ret = ra_set_qp_attr_qos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 33 * 4;
    ret = ra_set_qp_attr_qos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    struct ra_rdma_ops rdma_ops = {0};
    rdma_ops.ra_set_qp_attr_qos = ra_set_qp_attr_qos_stub;
    qp_handle.rdma_ops = &rdma_ops;
    ret = ra_set_qp_attr_qos(&qp_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_timeout_stub(struct ra_qp_handle *qp_stub, unsigned int *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_timeout()
{
   int ret;
    unsigned int timeout = 0;
    struct ra_qp_handle qp_handle;
    qp_handle.rdma_ops = NULL;

    ret = ra_set_qp_attr_timeout(NULL, &timeout);
    EXPECT_INT_EQ(128103, ret);

    ret = ra_set_qp_attr_timeout(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = ra_set_qp_attr_timeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = ra_set_qp_attr_timeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 23;
    ret = ra_set_qp_attr_timeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    struct ra_rdma_ops rdma_ops = {0};
    rdma_ops.ra_set_qp_attr_timeout = ra_set_qp_attr_timeout_stub;
    qp_handle.rdma_ops = &rdma_ops;
    ret = ra_set_qp_attr_timeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_retry_cnt_stub(struct ra_qp_handle *qp_stub, unsigned int *retry_cnt)
{
    return 0;
}

void tc_ra_set_qp_attr_retry_cnt()
{
    int ret;
    unsigned int retry_cnt = 0;
    struct ra_qp_handle qp_handle;
    qp_handle.rdma_ops = NULL;

    ret = ra_set_qp_attr_retry_cnt(NULL, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    ret = ra_set_qp_attr_retry_cnt(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 8;
    ret = ra_set_qp_attr_retry_cnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 4;
    ret = ra_set_qp_attr_retry_cnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 7;
    ret = ra_set_qp_attr_retry_cnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    struct ra_rdma_ops rdma_ops = {0};
    rdma_ops.ra_set_qp_attr_retry_cnt = ra_set_qp_attr_retry_cnt_stub;
    qp_handle.rdma_ops = &rdma_ops;
    ret = ra_set_qp_attr_retry_cnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_create_event_handle(void)
{
    int ret;
    int fd;

    ret = ra_create_event_handle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_create_event_handle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_peer_create_event_handle, 1024, -EINVAL);
    ret = ra_create_event_handle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_ctl_event_handle(void)
{
    int ret;
    int fd = 0;
    int fd_handle;

    ret = ra_ctl_event_handle(-1, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctl_event_handle(fd, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctl_event_handle(fd, &fd_handle, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctl_event_handle(fd, &fd_handle, 1, 100);
    EXPECT_INT_NE(0, ret);

    ret = ra_ctl_event_handle(fd, &fd_handle, 1, 0);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_peer_ctl_event_handle, 1024, -EINVAL);
    ret = ra_ctl_event_handle(fd, &fd_handle, 1, 0);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_wait_event_handle(void)
{
    int ret;
    int fd = 0;
    unsigned int events_num = 0;
    struct socket_event_info event_info = {};

    ret = ra_wait_event_handle(-1, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_wait_event_handle(fd, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_wait_event_handle(fd, &event_info, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_wait_event_handle(fd, &event_info, 0, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_wait_event_handle(fd, &event_info, 0, 1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_wait_event_handle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_peer_wait_event_handle, 1024, -EINVAL);
    ret = ra_wait_event_handle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_destroy_event_handle(void)
{
    int ret;
    int fd;

    ret = ra_destroy_event_handle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_destroy_event_handle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_peer_destroy_event_handle, 1024, -EINVAL);
    ret = ra_destroy_event_handle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

int ra_hdc_poll_cq_stub(struct ra_qp_handle *qp_hdc, bool is_send_cq, unsigned int num_entries, void *wc)
{
    if (is_send_cq) {
        return -1;
    }
    return 0;
}

void tc_ra_poll_cq(void)
{
    int ret;
    struct ra_qp_handle qp_handle = {0};
    struct ra_rdma_ops rdma_ops = {0};
    struct rdma_lite_wc_v2 lite_wc = {0};

    ret = ra_poll_cq(NULL, true, 0, NULL);
    EXPECT_INT_NE(0, ret);

    qp_handle.rdma_ops = &rdma_ops;
    rdma_ops.ra_poll_cq = NULL;
    ret = ra_poll_cq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    rdma_ops.ra_poll_cq = ra_hdc_poll_cq_stub;
    ret = ra_poll_cq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    ret = ra_poll_cq(&qp_handle, false, 1, &lite_wc);
    EXPECT_INT_EQ(0, ret);
}

void tc_get_vnic_ip_infos(void)
{
    int ret;
    unsigned int ids[1] = {0};
    unsigned int infos[1] = {0};

    ret = ra_socket_get_vnic_ip_infos(0, PHY_ID_VNIC_IP, NULL, 0, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_get_vnic_ip_infos(0xFFFF, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_get_vnic_ip_infos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = ra_socket_get_vnic_ip_infos(0, 0xFFFF, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    g_interface_version = 0;
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 100);
    ret = ra_socket_get_vnic_ip_infos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

int ra_socket_batch_abort_stub(unsigned int phy_id, struct SocketConnectInfoT conn[], unsigned int num)
{
    return 0;
}

void tc_ra_socket_batch_abort(void)
{
    int ret;
    unsigned int phy_id;
    struct SocketConnectInfoT conn = {0};
    struct ra_socket_handle socket_handle = {0};
    struct ra_socket_ops socket_ops = {0};

    ret = ra_socket_batch_abort(NULL, 0);
    EXPECT_INT_EQ(128203, ret);

    conn.socket_handle = NULL;
    ret = ra_socket_batch_abort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);

    socket_ops.ra_socket_batch_abort = ra_socket_batch_abort_stub;
    socket_handle.socket_ops = &socket_ops;
    socket_handle.rdev_info.phy_id = 16;
    conn.socket_handle = &socket_handle;
    ret = ra_socket_batch_abort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);

    socket_handle.rdev_info.phy_id = 0;
    mocker(ra_inet_pton, 5, -1);
    ret = ra_socket_batch_abort(&conn, 1);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_inet_pton, 5, 0);
    ret = ra_socket_batch_abort(&conn, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_get_client_socket_err_info(void)
{
    int ret = 0;
    struct SocketConnectInfoT conn[10] = {0};
    struct socket_err_info err[10] = {0};
    unsigned int num = 1;
    struct ra_socket_handle *socket_handle = NULL;
    struct rdev rdev_info = {0};

    socket_handle = malloc(sizeof(struct ra_socket_handle));
    socket_handle->rdev_info = rdev_info;
    mocker_clean();

    conn[0].socket_handle = NULL;
    ret = ra_get_client_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(128203, ret);

    conn[0].socket_handle = socket_handle;
    socket_handle->socket_ops = &g_ra_peer_socket_ops;
    rdev_info.phy_id = 0;
    mocker(ra_inet_pton, 5, 0);
    mocker(ra_get_socket_connect_info, 1, 0);
    mocker(rs_set_ctx, 1, 0);
    mocker(rs_socket_get_client_socket_err_info, 1, 1);
    ret = ra_get_client_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();

    mocker(ra_inet_pton, 5, 0);
    mocker(ra_get_socket_connect_info, 1, 0);
    mocker(rs_set_ctx, 1, 0);
    mocker(rs_socket_get_client_socket_err_info, 1, 0);
    ret = ra_get_client_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    free(socket_handle);
    socket_handle = NULL;
}

void tc_ra_get_server_socket_err_info(void)
{
    int ret = 0;
    struct SocketListenInfoT conn[10] = {0};
    struct server_socket_err_info err[10] = {0};
    unsigned int num = 1;
    struct ra_socket_handle *socket_handle = NULL;
    struct rdev rdev_info = {0};

    socket_handle = malloc(sizeof(struct ra_socket_handle));
    socket_handle->rdev_info = rdev_info;
    mocker_clean();

    conn[0].socket_handle = NULL;
    ret = ra_get_server_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(128203, ret);

    conn[0].socket_handle = socket_handle;
    socket_handle->socket_ops = &g_ra_peer_socket_ops;
    rdev_info.phy_id = 0;
    mocker(ra_inet_pton, 5, 0);
    mocker(ra_get_socket_listen_info, 1, 0);
    mocker(rs_set_ctx, 1, 0);
    mocker(rs_socket_get_server_socket_err_info, 1, 1);
    ret = ra_get_server_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();

    mocker(ra_inet_pton, 5, 0);
    mocker(ra_get_socket_listen_info, 1, 0);
    mocker(rs_set_ctx, 1, 0);
    mocker(rs_socket_get_server_socket_err_info, 1, 0);
    ret = ra_get_server_socket_err_info(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    free(socket_handle);
    socket_handle = NULL;
}

void tc_ra_socket_accept_credit_add(void)
{
    struct SocketListenInfoT conn[10] = {0};
    struct ra_socket_handle socket_handle = {0};
    struct ra_socket_ops socket_ops = {0};
    int ret = 0;

    conn[0].socket_handle = &socket_handle;
    socket_handle.socket_ops = &socket_ops;
    socket_handle.socket_ops->ra_socket_accept_credit_add = ra_peer_socket_accept_credit_add;
    mocker(ra_inet_pton, 1, 0);
    mocker(ra_get_socket_listen_info, 1, 0);
    mocker(rs_set_ctx, 1, 0);
    mocker(ra_peer_mutex_lock, 1, 0);
    mocker(ra_peer_mutex_unlock, 1, 0);
    mocker(rs_socket_accept_credit_add, 1, 0);
    ret = ra_socket_accept_credit_add(conn, 1, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_remap_mr(void)
{
    struct ra_rdma_handle rdma_handle = {0};
    struct mem_remap_info info[1] = {0};
    struct ra_rdma_ops rdma_ops = {0};
    int ret = 0;

    mocker_clean();
    rdma_handle.rdma_ops = &rdma_ops;
    rdma_handle.rdma_ops->ra_remap_mr = ra_hdc_remap_mr;
    mocker(ra_hdc_process_msg, 1, 0);
    ret = ra_remap_mr((void *)&rdma_handle, info, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_register_mr(void)
{
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_mr_handle *mr_handle = NULL;
    struct ra_rdma_ops rdma_ops = {0};
    struct mr_info info = {0};
    int ret = 0;

    mocker_clean();
    mocker(ra_hdc_process_msg, 2, 0);
    rdma_handle.rdma_ops = &rdma_ops;
    rdma_handle.rdma_ops->ra_register_mr = ra_hdc_typical_mr_reg;
    rdma_handle.rdma_ops->ra_deregister_mr = ra_hdc_typical_mr_dereg;
    ret = ra_register_mr((void *)&rdma_handle, &info, (void **)&mr_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_deregister_mr((void *)&rdma_handle, (void *)mr_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}