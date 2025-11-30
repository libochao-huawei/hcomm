#include "ra.h"
#include "hccp.h"
#include "hccp_ping.h"
#include "ut_dispatch.h"
#include "ra_hdc.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_tlv.h"
#include "dlfcn.h"
#include "dl.h"
#include "stdlib.h"
#include <errno.h>
#include "rs.h"
#include "rs_ping.h"
#include "ra_comm.h"
#include "ra_client_host.h"
#include "ra_peer.h"
#include "ra_tlv.h"

#define RA_QP_TX_DEPTH         32767
#define TC_TLV_HDC_MSG_SIZE    (32 * 1024) // 32KB
typedef uint32_t u32;
typedef uint16_t u16;
//typedef uint64_t u64;
typedef unsigned long long u64;
typedef signed int s32;

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern STATIC int ra_hdc_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id, unsigned long long **notify_va);
extern int ra_hdc_lite_post_send(struct ra_qp_handle *qp_hdc, struct lite_mr_info *local_mr,
    struct lite_mr_info *rem_mr, struct lite_send_wr *wr, struct send_wr_rsp *wr_rsp);
extern void ra_hdc_get_opcode_lite_support(unsigned int phy_id, unsigned int support_feature, int *support);
extern void rs_set_ctx(unsigned int phy_id);
extern int rs_socket_get_client_socket_err_info(struct socket_connect_info conn[], struct socket_err_info err[],
    unsigned int num);
extern int rs_socket_get_server_socket_err_info(struct socket_listen_info conn[], struct server_socket_err_info err[],
    unsigned int num);
extern int ra_hdc_get_tlv_recv_msg(struct tlv_msg *recv_msg, char *recv_data);
extern struct ra_socket_ops g_ra_peer_socket_ops;
extern int ra_rdev_init_check_ip(int mode, struct rdev rdev_info, char local_ip[]);
extern int hdc_send_recv_pkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);
extern int ra_hdc_get_lite_support(struct ra_rdma_handle *rdma_handle, unsigned int phy_id);
extern int ra_peer_loopback_qp_create(struct ra_rdma_handle *rdma_handle, struct loopback_qp_pair *qp_pair,
    void **qp_handle);
extern int ra_peer_loopback_single_qp_create(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle **qp_handle,
    struct ibv_qp **qp);
extern int ra_peer_loopback_qp_modify(struct ra_qp_handle *qp_handle0, struct ra_qp_handle *qp_handle1);

int sec_cpy_ret = 0;
unsigned int g_interface_version = 0;

// struct ibv_mr {
	// struct ibv_context     *context;
	// struct ibv_pd	       *pd;
	// void		       *addr;
	// size_t			length;
	// uint32_t		handle;
	// uint32_t		lkey;
	// uint32_t		rkey;
// };

extern void* dl_handle;
extern int host_notify_base_addr_init(unsigned int phy_id);
extern int host_notify_base_addr_uninit(unsigned int phy_id);
extern int ra_peer_set_conn_param(struct SocketInfoT conn[],
    struct socket_fd_data rs_conn[], unsigned int i, int buf_size);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
extern int dl_hal_mem_ctl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);
extern void ra_hdc_lite_mutex_deinit(struct ra_rdma_handle *rdma_handle);
extern void ra_rdma_lite_free_ctx(struct rdma_lite_context *lite_ctx);
extern int dl_hal_sensor_node_unregister(uint32_t devid, uint64_t handle);
extern int ra_sensor_node_register(unsigned int phy_id, struct ra_rdma_handle *rdma_handle);
extern int ra_hdc_lite_mutex_init(struct ra_rdma_handle *rdma_handle, unsigned int phy_id);
extern int ra_hdc_lite_get_cq_qp_attr(struct ra_qp_handle *qp_hdc,
    struct rdma_lite_cq_attr *lite_send_cq_attr, struct rdma_lite_cq_attr *lite_recv_cq_attr,
    struct rdma_lite_qp_attr *lite_qp_attr);
extern int ra_hdc_lite_init_mem_pool(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc,
    struct rdma_lite_cq_attr *lite_send_cq_attr, struct rdma_lite_cq_attr *lite_recv_cq_attr,
    struct rdma_lite_qp_attr *lite_qp_attr);
extern int ra_rdma_lite_destroy_cq(struct rdma_lite_cq *lite_cq);
extern void ra_hdc_lite_deinit_mem_pool(struct ra_rdma_handle *rdma_handle, struct ra_qp_handle *qp_hdc);
extern void ra_hdc_lite_qp_attr_init(struct ra_qp_handle *qp_hdc, struct rdma_lite_qp_attr *lite_qp_attr,
    struct rdma_lite_qp_cap *cap);
extern int ra_rdma_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr);
extern int ra_rdma_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx);
extern int ra_hdc_get_cqe_err_info_num(struct ra_rdma_handle *rdma_handle, unsigned int *num);
extern struct rdma_lite_context *ra_rdma_lite_alloc_ctx(u8 phy_id, struct dev_cap_info *cap);
extern struct rdma_lite_cq *ra_rdma_lite_create_cq(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_cq_attr *lite_cq_attr);
extern int ra_rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp);
extern struct rdma_lite_qp *ra_rdma_lite_create_qp(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_qp_attr *lite_qp_attr);
extern int rs_socket_accept_credit_add(struct socket_listen_info conn[], uint32_t num, unsigned int credit_limit);
extern int ra_peer_socket_accept_credit_add(unsigned int phy_id, struct SocketListenInfoT conn[], unsigned int num,
    unsigned int credit_limit);
extern hdcError_t dl_hal_hdc_recv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t dl_drv_hdc_alloc_msg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t dl_drv_hdc_free_msg(struct drvHdcMsg *msg);
extern hdcError_t dl_drv_hdc_session_close(HDC_SESSION session);
extern int rs_remap_mr(unsigned int phy_id, unsigned int rdev_index, struct mem_remap_info mem_list[],
    unsigned int mem_num);

int stub_ra_hdc_process_msg_wrlist(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 1;
    return 0;
}

int stub_ra_hdc_process_msg_wrlist_3(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 5;
    return 0;
}

int stub_ra_get_interface_version(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

void tc_abnormal()
{
	int ret;
	int dev_id = 0;
	int max_dev = 16;
	int ra_case_other = 2;
	int ra_case_online = 0;
	struct ra_init_config online_config = {
		.phy_id = 10,
		.nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
	};

	ret = ra_init(&online_config);
	return;
}

static int ra_get_interface_version_stub(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

int stub_rs_get_lite_support(unsigned int phy_id, unsigned int rdev_index, int *support_lite)
{
	return 0;
}

int stub_ra_hdc_process_rdev_init(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_rdev_init_data *rdev_init_data;
    union op_lite_support_data *lite_support_data;

    if (opcode == RA_RS_RDEV_INIT) {
        rdev_init_data = (union rdev_init_data *)data;
        rdev_init_data->rx_data.rdev_index = 0;
    } else if (opcode == RA_RS_GET_LITE_SUPPORT) {
        lite_support_data = (union op_lite_support_data *)data;
        lite_support_data->rx_data.support_lite = 1;
    }
    return 0;
}

void stub_ra_hdc_get_opcode_lite_support(unsigned int phy_id, unsigned int support_feature, int *support)
{
    if (support_feature == 1) {
        *support = 1;
        return;
    }
}


DLLEXPORT drvError_t stub_session_connect_hdc(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    static HDC_SESSION g_hdc_session = 1;
    *session = g_hdc_session;
    return 0;
}

void tc_hdc_env_init()
{
    struct ra_init_config offline_hdc_config = {
        .phy_id = 0,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
        .enable_hdc_async = false,
    };
    struct process_ra_sign p_ra_sign;
    p_ra_sign.tgid = 0;

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect_hdc, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    int ret = ra_hdc_init(&offline_hdc_config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_env_deinit()
{
    struct ra_init_config offline_hdc_config = {
        .phy_id = 0,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
        .enable_hdc_async = false,
    };

    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    int ret = ra_hdc_deinit(&offline_hdc_config);
    EXPECT_INT_EQ(ret, 0);
	mocker_clean();
}

int ra_hdc_get_lite_support_stub(struct ra_rdma_handle *rdma_handle, unsigned int phy_id)
{
    rdma_handle->support_lite = 1;
    return 0;
}

extern int ra_rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc);
int stub_ra_rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    int i = 0;
    for (i = 0; i < num_entries; i++) {
        lite_wc[i].status = 0x12;
    }

    return 2;
}

int stub_dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    *value = (1 << 8);
    return 0;
}

int stub_ra_hdc_send_wr_v2(struct ra_qp_handle *qp_hdc, struct send_wr_v2 *wr, struct send_wr_rsp *op_rsp)
{
    return 0;
}

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

void tc_normal_offline()
{
	int ret;
	int dev_id = 0;
	int qp_mode = 1;
	unsigned int host_tgid = 0;

    void *addr = malloc(1);
    void *data = malloc(1);
    unsigned long long size = 1;
    unsigned long pa = 1;
    unsigned long va = 1;
    int sock_fd = 1;
	int ip_addr = 0;
	int flag = 0;
	int port = 10000;
	int tms = 100;
	void *addr1 = malloc(size);
	void *addr2 = malloc(size);
	//char data[8192] = {};
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

	long size_notify = 0;
	struct send_wr wr;
	struct sg_list mem;
	u32 wqe_index;

	mem.addr = addr1;
	mem.len = 10;
	wr.buf_list = &mem;
	wr.dst_addr = 0x111;
	wr.buf_num = 1;
	wr.op = 0;
	wr.send_flag = 0;
    struct send_wr_rsp op_rsp;

    //struct ra_rdma_ops rdma_ops;

	struct SocketListenInfoT listen[1];
	struct SocketConnectInfoT conn[1];
	struct SocketCloseInfoT close[1] = {0};
	struct SocketInfoT socket_info[1];

	int qp_status = 0;
	int access = 1<<1;
	int server = 0;
	int client = 1;
        char *pid_sign = {"ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};

	mocker_invoke((stub_fn_t)dlopen, dlopen_stub, 1);
	mocker_invoke((stub_fn_t)dlsym, dlsym_stub, 24);
	mocker_invoke((stub_fn_t)dlclose, dlclose_stub, 1);

	dl_handle = malloc(1);
	hccp_init(dev_id, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
	sleep(2);
	ra_init(&offline_config);
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;

    unsigned int ip = inet_addr("127.0.0.1");
    unsigned int remote_ip[] = {inet_addr("127.0.0.1")};
    int num = 1;
	struct mr_info mr_info;
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
    ra_rdev_deinit(NULL, NOTIFY);
	ra_socket_deinit(NULL);
    ra_register_mr(NULL, NULL, NULL);
    ra_deregister_mr(NULL, NULL);
    ra_socket_init(0, rdev_info, NULL);
    ra_send_wrlist_ext(NULL, NULL, NULL, 0, NULL);
    ra_recv_wrlist(NULL, NULL, 0, NULL);

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

    ret = ra_qp_batch_modify(NULL, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    list_v2.len = 0x1;
    ret = ra_send_wr_v2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    struct ra_rdma_ops rdma_ops_v2 = {0};
    rdma_ops_v2.ra_send_wr_v2 = stub_ra_hdc_send_wr_v2;
    ra_qp_handle_v2.rdma_ops = &rdma_ops_v2;
    ret = ra_send_wr_v2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_EQ(0, ret);

    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;

    struct ra_socket_handle *handle1 = NULL;
    ret = ra_socket_init(5, rdev_info, &handle1);
    EXPECT_INT_NE(0, ret);

    ret = ra_rdev_init_with_backup(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct ra_rdma_handle *handle2 = NULL;
    ret =  ra_rdev_init(5, NOTIFY, rdev_info, &handle2);
    EXPECT_INT_NE(0, ret);

    struct ra_rdma_handle *rdma_handle = NULL;
    struct ra_rdma_handle *rdma_handle2 = NULL;
    ret =  ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = ra_rdev_get_handle(rdev_info.phy_id, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);

    struct ra_socket_handle *socket_handle = NULL;
    ret = ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_EQ(0, ret);
    struct socket_hdc_info *hdc_socket_handle = calloc(1, sizeof(struct socket_hdc_info));

    conn[0].socket_handle = socket_handle;
    listen[0].socket_handle = socket_handle;
    close[0].socket_handle = socket_handle;
    close[0].fd_handle = hdc_socket_handle;
    socket_info[0].socket_handle = socket_handle;
    socket_info[0].fd_handle = hdc_socket_handle;

    ret = ra_socket_set_white_list_status(0);
    EXPECT_INT_EQ(0, ret);
    ret = ra_socket_set_white_list_status(3);
    EXPECT_INT_EQ(128203, ret);
    ret = ra_socket_get_white_list_status(NULL);
    EXPECT_INT_EQ(128203, ret);
    unsigned int enable;
    ret = ra_socket_get_white_list_status(&enable);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_ifnum(NULL, NULL);
    EXPECT_INT_EQ(128303, ret);

    int ifnum = 0;
	offline_ifattr_config.nic_position = NETWORK_PEER_ONLINE;
    ret = ra_get_ifnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    offline_ifattr_config.phy_id = 129;
    offline_ifattr_config.nic_position = 1;
    ret = ra_get_ifnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

	ret = ra_get_ifaddrs(NULL, NULL, NULL);
	EXPECT_INT_EQ(128303, ret);

	struct ra_init_config peer_config = {
		.phy_id = 0,
		.nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
	};

    struct ra_get_ifattr peer_ifattr_config = {
		.phy_id = 0,
		.nic_position = 0,
        .is_all = true,
	};

    unsigned int ifaddr_num = 4;
    offline_ifattr_config.phy_id = 0;
    struct interface_info interface_infos[4] = {0};
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    g_interface_version = 0;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    g_interface_version = 1;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);
    offline_ifattr_config.is_all = true;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_NE(0, ret);
    offline_ifattr_config.is_all = false;
    mocker(memcpy_s, 10 , -1);
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(328306, ret);
    mocker(calloc, 10 , NULL);
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    mocker_clean();
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    g_interface_version = 2;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    g_interface_version = 3;
    ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);
    mocker_clean();

    ret = ra_get_ifnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);


	ret = ra_get_ifaddrs(&peer_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(0, ret);

	ifaddr_num = 0;
	ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

	ifaddr_num = 9;
	ret = ra_get_ifaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

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

    struct socket_wlist_info_t white_list[1];
    ret = ra_socket_white_list_del(socket_handle, white_list, 1);
	EXPECT_INT_EQ(0, ret);

    ret = ra_socket_white_list_add(socket_handle, white_list, 1);
	EXPECT_INT_EQ(0, ret);

	ret = ra_socket_listen_start(listen, 1);
	EXPECT_INT_EQ(0, ret);

    ret = ra_socket_batch_connect(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = ra_socket_batch_connect(&conn, 1);
    EXPECT_INT_EQ(0, ret);

    unsigned int connected_num = 0;
	ret = ra_get_sockets(server, socket_info, 1, &connected_num);
	EXPECT_INT_EQ(0, ret);

    unsigned long long sent_size = 0;
	ret = ra_socket_send(socket_info[0].fd_handle, data, size, &sent_size);
	EXPECT_INT_EQ(0, ret);

    unsigned long long received_size = 0;
	ret = ra_socket_recv(socket_info[0].fd_handle, data, size, &received_size);
	EXPECT_INT_EQ(0, ret);

    ret = ra_socket_send(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_EQ(0, ret);

    ret = ra_socket_recv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_hdc_get_sockets, 10 , -1);
    ret = ra_get_sockets(server, socket_info, 1, &connected_num);

    mocker(ra_hdc_socket_send, 10 , -1);
    ret = ra_socket_send(socket_info[0].fd_handle, data, size, &sent_size);

    mocker(ra_hdc_socket_recv, 10 , -1);
    ret = ra_socket_recv(socket_info[0].fd_handle, data, size, &received_size);

	ret = ra_socket_listen_stop(listen, 1);
	EXPECT_INT_EQ(0, ret);

    ret = ra_qp_create(rdma_handle, 3, 0, NULL);
    EXPECT_INT_NE(0, ret);

    ret = ra_qp_create_with_attrs(rdma_handle, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(rdma_handle, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct ra_qp_handle *qp_handle = NULL;
    struct ra_qp_handle *qp_handle_with_attr = NULL;
    struct ra_qp_handle *ai_qp_handle = NULL;
    struct ra_qp_handle *typical_qp_handle = NULL;
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
    ret = ra_ai_qp_create(rdma_handle, &ext_attrs, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    ext_attrs.qp_mode = RA_RS_OP_QP_MODE;
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

    struct ra_rdma_handle rdma_handle_err = {0};
    rdma_handle_err = *rdma_handle;
    rdma_handle_err.rdev_info.phy_id = 32;

    ret = ra_qp_batch_modify(&rdma_handle_err, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    struct ra_qp_handle *batch_modify_qp_hdc[1];
    batch_modify_qp_hdc[0] = qp_handle;

    ret = ra_qp_batch_modify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_batch_modify(rdma_handle, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_EQ(0, ret);

    struct qos_attr qos_attr= {0};
    qos_attr.tc = 110;
    qos_attr.sl = 3;
    ret = ra_set_qp_attr_qos(qp_handle, &qos_attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_qos(qp_handle, &qos_attr);
    EXPECT_INT_EQ(0, ret);

    unsigned int timeout = 6;
    ret = ra_set_qp_attr_timeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_timeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = ra_set_qp_attr_retry_cnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_set_qp_attr_retry_cnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

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

    ASSERT_ADDR_NE(qp_handle, NULL);

    mr_info.addr = addr;
	mr_info.size = size;
	mr_info.access = access;
    ret = ra_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_notify_base_addr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
    struct mr_info mr_info2;
    ret = ra_get_notify_mr_info(rdma_handle, &mr_info2);
    EXPECT_INT_NE(0, ret);

    ret = ra_get_qp_status(qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    ret = ra_send_wr(qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

	unsigned int complete_num = 2;
	unsigned int send_num = 2;
	struct send_wrlist_data wrlist[send_num];
	struct send_wr_rsp op_rsp_list[send_num];
	wrlist[0].mem_list = mem;
	wrlist[0].dst_addr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].send_flags = 0;
	wrlist[1].mem_list = mem;
	wrlist[1].dst_addr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].send_flags = 0;
    ret = ra_send_wrlist(qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
    EXPECT_INT_EQ(0, ret);

	mem.len = 2147483649;
	wrlist[0].mem_list = mem;
	ret = ra_send_wrlist(qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
	//EXPECT_INT_EQ(-EINVAL, ret);

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

    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];
    ret = ra_rs_typical_qp_modify(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    // typical send wr
    mem.addr = addr1;
	mem.len = 10;
	wr.buf_list = &mem;
	wr.dst_addr = 0x111;
	wr.buf_num = 1;
	wr.op = 0;
	wr.send_flag = 0;
    ret = ra_typical_send_wr(NULL, &wr, &op_rsp);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_send_wr(typical_qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_destroy(typical_qp_handle);

    ret = ra_socket_batch_close(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = ra_socket_batch_close(&close, 1);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();

    ret = ra_rdev_deinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);

	mocker(ra_hdc_socket_deinit, 1, -1);
    ret = ra_socket_deinit(socket_handle);
    EXPECT_INT_EQ(128000, ret);
	mocker_clean();

/* start 单算子lite */
    unsigned int support = 0;
    mocker(dl_drv_device_get_index_by_phy_id, 10 , 0);
    mocker(dl_hal_get_device_info, 10 , 0);
    mocker(dl_hal_mem_ctl, 10 , 0);
    ret = ra_hdc_get_drv_lite_support(0, true, &support);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(dl_drv_device_get_index_by_phy_id, 10 , 0);
    mocker_invoke((stub_fn_t)dl_hal_get_device_info, stub_dl_hal_get_device_info, 10);
    mocker(dl_hal_mem_ctl, 10 , 0);
    ret = ra_hdc_get_drv_lite_support(0, false, &support);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(0, support);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 10);
    mocker_invoke((stub_fn_t)rs_get_lite_support, stub_rs_get_lite_support, 10);
    struct ra_rdma_handle *lite_rdma_handle = NULL;
    ret =  ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &lite_rdma_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    struct ra_qp_handle *lite_qp_handle = NULL;
    ret = ra_qp_create(lite_rdma_handle, 0, RA_RS_OP_QP_MODE, &lite_qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_qp_status(lite_qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    mem.addr = addr1;
	mem.len = 10;
	wr.buf_list = &mem;
	wr.dst_addr = 0x111;
	wr.buf_num = 1;
	wr.op = 0;
	wr.send_flag = 0;
    lite_qp_handle->local_mr[0].addr = wr.buf_list[0].addr;
    lite_qp_handle->local_mr[0].len = 0x10000;
    lite_qp_handle->rem_mr[0].addr = wr.dst_addr;
    lite_qp_handle->rem_mr[0].len = 0x10000;
    lite_qp_handle->send_wr_num = 999;
    mocker_invoke((stub_fn_t)ra_rdma_lite_poll_cq, stub_ra_rdma_lite_poll_cq, 10);
    ret = ra_send_wr(lite_qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    struct cqe_err_info out_info;
    struct cqe_err_info info0;
    struct cqe_err_info info1 = { 0 };
    ra_hdc_lite_get_cqe_err_info(rdev_info.phy_id, &info0);
    ra_hdc_get_valid_cqe_err_info(&out_info, info0, info1);
    EXPECT_INT_EQ(out_info.status, info0.status);

    send_num = 2;
	wrlist[0].mem_list = mem;
	wrlist[0].dst_addr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].send_flags = 0;
	wrlist[1].mem_list = mem;
	wrlist[1].dst_addr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].send_flags = 0;
    ret = ra_send_wrlist(lite_qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
    EXPECT_INT_EQ(0, ret);

    struct send_wrlist_data_ext data_ext[2];
    struct wrlist_send_complete_num wrlist_num;
    wrlist_num.send_num = 2;
    wrlist_num.complete_num = &complete_num;
    data_ext[0].mem_list = mem;
	data_ext[0].dst_addr = 0x111;
	data_ext[0].op = 0;
	data_ext[0].send_flags = 0;
	data_ext[1].mem_list = mem;
	data_ext[1].dst_addr = 0x111;
	data_ext[1].op = 0;
	data_ext[1].send_flags = 0;
    ret = ra_hdc_send_wrlist_ext(lite_qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_qp_destroy(lite_qp_handle);

    ret = ra_rdev_deinit(lite_rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);
/* end 单算子lite */

    ret = ra_deinit(&offline_config);
    EXPECT_INT_EQ(0, ret);

    mocker(ra_hdc_deinit, 10, 1);
    ret = ra_deinit(&offline_config);

	ret = hccp_deinit(dev_id);
	EXPECT_INT_EQ(0, ret);

    free(addr);
    free(data);
	free(dl_handle);
	free(addr1);
	free(addr2);
    //free(hdc_socket_handle);
	return;
}

void tc_normal_online()
{
	int ret;
	int dev_id = 0;
	int qp_mode = 2;
	unsigned int host_tgid = 0;
	void* qp_handle1;
	void* qp_handle2;
	int ip_addr = 0;
	int flag = 0;
	int port = 10000;
	int tms = 100;
	int size = 1024;
	void *addr1 = malloc(size);
	void *addr2 = malloc(size);
	char data[8192] = {};
	struct ra_init_config online_config = {
		.phy_id = 0,
		.nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
	};

	struct mr_info mr_info;
	int sock_fd = 0;
	struct SocketListenInfoT listen[2];
	struct SocketConnectInfoT conn[2];
	struct SocketCloseInfoT close[2] = {0};
	struct SocketInfoT socket_info[2];
	socket_info[0].fd_handle = &sock_fd;
	int qp_status = 0;
	int access = 1<<1;
	int server = 0;
	int client = 1;
        char pid_sign[] = {"ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};

	mocker_invoke((stub_fn_t)dlopen, dlopen_stub, 1);
	mocker_invoke((stub_fn_t)dlsym, dlsym_stub, 19);
	mocker_invoke((stub_fn_t)dlclose, dlclose_stub, 1);

	dl_handle = malloc(1);
	hccp_init(dev_id, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
	sleep(2);
	ra_init(&online_config);

	ret = ra_socket_listen_start(listen, 2);
	EXPECT_INT_EQ(0, ret);

	ret = ra_socket_batch_connect(conn, 2);
	EXPECT_INT_EQ(0, ret);

    unsigned int connected_num = 0;
	ret = ra_get_sockets(server, socket_info, 2, &connected_num);
	EXPECT_INT_EQ(2, ret);

    unsigned long long sent_size = 0;
	ret = ra_socket_send(socket_info[0].fd_handle, data, size, &sent_size);
	EXPECT_INT_EQ(size, ret);

    unsigned long long received_size = 0;
	ret = ra_socket_recv(socket_info[0].fd_handle, data, size, &received_size);
	EXPECT_INT_EQ(size, ret);

	ret = ra_qp_create(dev_id, flag, qp_mode, &qp_handle1);

    struct qos_attr qos_attr= {0};
    qos_attr.tc = 100;
    qos_attr.sl = 3;
    ret = ra_set_qp_attr_qos(qp_handle1, &qos_attr);
    EXPECT_INT_EQ(0, ret);

    unsigned int timeout = 6;
    ret = ra_set_qp_attr_timeout(qp_handle1, &timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = ra_set_qp_attr_retry_cnt(qp_handle1, &retry_cnt);
    EXPECT_INT_EQ(0, ret);


	ret = ra_qp_create(dev_id, flag, qp_mode, &qp_handle2);
	EXPECT_INT_EQ(0, ret);

	ret = ra_qp_connect_async(qp_handle1, socket_info[0].fd_handle);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_qp_connect_async(qp_handle2, socket_info[0].fd_handle);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_get_qp_status(qp_handle1, &qp_status);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_get_qp_status(qp_handle2, &qp_status);
	EXPECT_INT_EQ(-1, ret);

    mr_info.addr = addr1;
	mr_info.size = size;
	mr_info.access = access;
	ret = ra_mr_reg(qp_handle1, &mr_info);
	EXPECT_INT_EQ(-1, ret);

    mr_info.addr = addr2;
	mr_info.size = size;
	mr_info.access = access;
	ret = ra_mr_reg(qp_handle2, &mr_info);
	EXPECT_INT_EQ(-1, ret);

	struct send_wr wr;
	struct sg_list mem;
	u32 wqe_index;

	mem.addr = addr1;
	mem.len = 10;
	wr.buf_list = &mem;
	wr.dst_addr = 0x111;
	wr.buf_num = 1;
	wr.op = 0;
	wr.send_flag = 0;
        struct send_wr_rsp *op_rsp;
	ret = ra_send_wr(qp_handle1, &wr, &op_rsp);
	EXPECT_INT_EQ(-1, ret);

	long va = 0;
	long pa = 0;
	long size_notify = 0;
	ret = ra_get_notify_base_addr(qp_handle1, &va, &size_notify);
	EXPECT_INT_EQ(-1, ret);
    struct mr_info mr_info2;
	ret = ra_get_notify_mr_info(qp_handle1, &mr_info2);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_socket_listen_stop(listen, 2);
	EXPECT_INT_EQ(-1, ret);

	close[0].fd_handle = socket_info[0].fd_handle;
	close[1].fd_handle = socket_info[1].fd_handle;
	ret = ra_socket_batch_close(close, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_mr_dereg(qp_handle1, addr1);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_mr_dereg(qp_handle2, addr2);
	EXPECT_INT_EQ(-1, ret);

	ret =ra_qp_destroy(qp_handle1);
	EXPECT_INT_EQ(-1, ret);

	ret =ra_qp_destroy(qp_handle2);
	EXPECT_INT_EQ(-1, ret);

	free(dl_handle);
	free(addr1);
	free(addr2);
	ra_deinit(&online_config);

	mocker_clean();



	ret = ra_socket_listen_start(listen, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_socket_batch_connect(conn, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = ra_get_sockets(server, socket_info, 2, &connected_num);
	EXPECT_INT_EQ(2, ret);

	ret = ra_socket_send(socket_info[0].fd_handle, data, size, &sent_size);
	EXPECT_INT_EQ(1, ret);

	ret = ra_socket_recv(socket_info[0].fd_handle, data, size, &received_size);
	EXPECT_INT_EQ(1, ret);

	free(socket_info[0].fd_handle);
	free(socket_info[1].fd_handle);
	return;
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
    unsigned long pa = NULL;
    unsigned long va = NULL;
    struct qp_peer_info *qp_info = NULL;
    struct SocketConnectInfoT conn[1];
    struct SocketListenInfoT listen[1];
    struct SocketInfoT info[1];
    struct SocketCloseInfoT close[1] = {0};
    int sock_fd = 1;
    void *qp_handle_with_attr;
    int status = 0;
    struct ra_init_config config = {
        .phy_id = dev_id,
        .nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };

    config.phy_id = 0;
    int ip_addr;
    unsigned int host_tgid = 0;
    int qp_mode = 0;
	unsigned int rdev_index;
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
    struct socket_hdc_info *hdc_socket_handle = calloc(1, sizeof(struct socket_hdc_info));
    struct QpExtAttrs ext_attrs;
    ext_attrs.version = QP_CREATE_WITH_ATTR_VERSION;
    ext_attrs.qp_mode = RA_RS_NOR_QP_MODE;
    unsigned int temp_depth = 128;
    unsigned int qp_num = 0;
    ret = ra_peer_set_tsqp_depth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_get_tsqp_depth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);
    ret = ra_peer_qp_create_with_attrs(rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle_with_attr);

    listen[0].socket_handle = socket_handle;
    conn[0].socket_handle = socket_handle;
    close[0].socket_handle = socket_handle;
    info[0].socket_handle = socket_handle;
	info[0].fd_handle = hdc_socket_handle;
    ret = ra_peer_init(&config, 0);
    EXPECT_INT_EQ(-1, ret);
	ret = ra_init(&config);
	EXPECT_INT_EQ(128000, ret);
	ret = ra_deinit(&config);
	EXPECT_INT_EQ(128000, ret);

	mocker(host_notify_base_addr_init, 1, 0);
	ret = ra_peer_rdev_init(rdma_handle, NOTIFY, rdev_info, &rdev_index);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);

	mocker(host_notify_base_addr_uninit, 1, 0);
	ret = ra_peer_rdev_deinit(rdma_handle, NOTIFY);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_socket_batch_connect(0, conn, 1);
    EXPECT_INT_EQ(0, ret);

    listen[0].port = 1;
    ret = ra_peer_socket_listen_start(0, listen, 1);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_socket_listen_stop(0, listen, 1);
    EXPECT_INT_NE(0, ret);

    ret = ra_peer_get_sockets(0, 0, info, 1);
    EXPECT_INT_EQ(1, ret);

    ret = ra_peer_socket_send(0, info[0].fd_handle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = ra_peer_socket_send(0, info[0].fd_handle, data, max_size);
    EXPECT_INT_EQ(1, ret);

    ret = ra_peer_socket_recv(0, info[0].fd_handle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = ra_peer_socket_recv(0, info[0].fd_handle, data, max_size);
    EXPECT_INT_EQ(1, ret);

	struct socket_wlist_info_t white_list[1];
	ret = ra_peer_socket_white_list_add(rdev_info, white_list, 1);
    EXPECT_INT_EQ(0, ret);
	ret = ra_peer_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(0, ret);

    struct ra_qp_handle *qp_handle = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    void *mr_handle = NULL;


    ret = ra_peer_notify_base_addr_init(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_notify_base_addr_init(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = notify_base_addr_uninit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = notify_base_addr_uninit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_qp_connect_async(qp_handle, info[0].fd_handle);
    EXPECT_INT_EQ(0, size);

    close[0].fd_handle = info[0].fd_handle;
    ret = ra_peer_socket_batch_close(0, close, 1);
    EXPECT_INT_EQ(0, size);

    // close[0].fd_handle = info[0].fd_handle;
    // mocker(memset_s, 5, 0);
    // ret = ra_peer_socket_batch_close(0, close, 1);
    // EXPECT_INT_EQ(0, size);
    // mocker_clean();

    ret = ra_peer_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(0, ret);

    struct mr_info mr_info;
    mr_info.addr = addr;
    mr_info.size = size;
    mr_info.access = access;

    ret = ra_peer_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_mr_dereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_register_mr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_deregister_mr(mr_handle, NULL);
    EXPECT_INT_EQ(0, ret);

    void *comp_channel = NULL;
    ret = ra_peer_create_comp_channel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_destroy_comp_channel(comp_channel);
    EXPECT_INT_EQ(0, ret);

    struct srq_attr attr = {0};
    ret = ra_peer_create_srq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_destroy_srq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    struct send_wr_rsp op_rsp;
    ret = ra_peer_send_wr(qp_handle, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);

    struct recv_wrlist_data rev_wr = {0};
    rev_wr.wr_id = 100;
    rev_wr.mem_list.lkey = 0xff;
    rev_wr.mem_list.addr = addr;
    rev_wr.mem_list.len = size;
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    ret = ra_peer_recv_wrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_qp_destroy(qp_handle);
    ret = ra_peer_qp_destroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);

    ret = ra_peer_deinit(&config);
    EXPECT_INT_NE(0, ret);

	// free(hdc_socket_handle);
    return;
}


extern int ra_hdc_process_msg(unsigned int opcode, unsigned int phy_id, char *data, unsigned int data_size);
extern int ra_hdc_get_all_vnic(unsigned int cur_phy_id, unsigned int *vnic_ip, unsigned int num);
extern int ra_hdc_init_apart(unsigned int phy_id, unsigned int *logic_id);
extern int ra_hdc_send_pid(unsigned int phy_id, struct process_ra_sign p_ra_sign);
extern int strcpy_s(void * dest, size_t destMax, const void * src, size_t count);
extern void ra_hdc_send_wrlist_init(union op_send_wrlist_data *send_wrlist, struct ra_qp_handle *qp_hdc,
    unsigned int complete_cnt, struct wrlist_send_complete_num wrlist_num);

void tc_hdc_fail()
{
    struct ra_qp_handle qp_hdc;
    struct send_wr swr;
    qp_hdc.phy_id = 0;
    qp_hdc.rdev_index = 0;
    qp_hdc.qpn = 0;
    swr.buf_num = 0;
    swr.dst_addr = 0;
    swr.op = 0;
    swr.send_flag = 0;
    struct send_wrlist_data wr[1] = {0};
    struct send_wr_rsp op_rsp[1] = {0};
    struct wrlist_send_complete_num wrlist_num;
    wrlist_num.send_num = 1;
    unsigned int complete_num = 0;
    wrlist_num.complete_num = &complete_num;
    unsigned int interface_version = 0;
    struct ra_rdma_handle rdma_handle = { 0 };

    ra_hdc_get_interface_version(0, 0, &interface_version);

    struct ifaddr_info ifaddr_infos[1] = {{{{0}}}};
    unsigned int num = 1;
    ra_hdc_get_ifnum(0, 0, &num);

    ra_hdc_get_ifaddrs(0, ifaddr_infos, &num);
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;

    mocker(memset_s, 5, -1);
    ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);

    ra_hdc_socket_init(rdev_info);
    ra_hdc_socket_deinit(rdev_info);
    mocker_clean();

    mocker(memcpy_s, 10, -1);
    ra_hdc_get_ifnum(0, 0, &num);

    mocker(memcpy_s, 10, -1);
    ra_hdc_get_ifaddrs(0, ifaddr_infos, &num);

    ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);

    ra_hdc_socket_init(rdev_info);

    ra_hdc_send_wrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    ra_hdc_send_wr(&qp_hdc, &swr, op_rsp);

    mocker(ra_hdc_get_all_vnic, 5, 0);
    ra_hdc_socket_init(rdev_info);
    ra_hdc_socket_deinit(rdev_info);
    mocker_clean();

    mocker(ra_hdc_process_msg, 5, 0);
    ra_hdc_get_ifnum(0, 0, &num);

    mocker(ra_hdc_process_msg, 5, 0);
    ra_hdc_get_ifaddrs(0, ifaddr_infos, &num);

    qp_hdc.qp_mode = 1;
    ra_hdc_send_wr(&qp_hdc, &swr, op_rsp);

    qp_hdc.qp_mode = 2;
    ra_hdc_send_wr(&qp_hdc, &swr, op_rsp);
    mocker_clean();

    unsigned int vnic_ip = 0;
    mocker(ra_hdc_process_msg, 10, -1);

    unsigned int temp_depth = 0;
    unsigned int qp_num = 0;
    rdma_handle.rdev_info.phy_id = 0;
    rdma_handle.rdev_index = 0;

    ra_hdc_get_tsqp_depth(&rdma_handle, &temp_depth, &qp_num);
    ra_hdc_set_tsqp_depth(&rdma_handle, 0, &qp_num);

    ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);

    ra_hdc_get_all_vnic(0, &vnic_ip, 0);

    ra_hdc_session_close(0);

    unsigned long long va = 0;
    unsigned long long size = 0;
    ra_hdc_notify_cfg_get(0, &va, &size);
    ra_hdc_notify_cfg_set(0, 0, 0);

    mocker(ra_hdc_get_all_vnic, 5, 0);
    ra_hdc_socket_init(rdev_info);
    ra_hdc_socket_deinit(rdev_info);
    mocker_clean();

    mocker(drvGetDevNum, 5, -1);
    ra_hdc_get_all_vnic(0, &vnic_ip, 0);
    mocker_clean();

    mocker(drvDeviceGetPhyIdByIndex, 5, -1);
    ra_hdc_get_all_vnic(0, &vnic_ip, 0);
    mocker_clean();

    struct ra_init_config cfg = {0};
    cfg.phy_id = 0;
    mocker(ra_hdc_session_close, 5, -1);
    ra_hdc_deinit(&cfg);
    mocker_clean();

    mocker(pthread_mutex_destroy, 5, -1);
    ra_hdc_deinit(&cfg);
    mocker_clean();

    struct process_ra_sign p_ra_sign;
    mocker(ra_hdc_init_apart, 5, -1);
    ra_hdc_init(&cfg, p_ra_sign);
    mocker_clean();


    unsigned int logic_id = 0;
    mocker(drvDeviceGetIndexByPhyId, 5, -1);
    ra_hdc_init_apart(0, &logic_id);
    mocker_clean();

    mocker(pthread_mutex_init, 5, -1);
    ra_hdc_init_apart(0, &logic_id);
    mocker_clean();

    mocker(strcpy_s, 5, -1);
    p_ra_sign.tgid = 0;
    ra_hdc_send_pid(0, p_ra_sign);
    mocker_clean();

    mocker(ra_hdc_process_msg, 10, -1);
    ra_hdc_send_pid(0, p_ra_sign);

    ra_hdc_get_notify_base_addr(&rdma_handle, &va, &size);

    ra_hdc_get_ifaddrs(0, ifaddr_infos, &num);

    ra_hdc_send_wrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    struct mr_info info;
    char addr = 0;
    info.addr = &addr;
    info.size = 0;
    info.access = 0;
    ra_hdc_mr_reg(&qp_hdc, &info);
    ra_hdc_mr_dereg(&qp_hdc, &info);

    struct qos_attr qos_attr= {0};
    qos_attr.tc = 110;
    qos_attr.sl = 3;
    ra_hdc_set_qp_attr_qos(&qp_hdc, &qos_attr);

    struct socket_hdc_info sock_handle;
    sock_handle.fd = 0;
    ra_hdc_qp_connect_async(&qp_hdc, &sock_handle);

    int status = 0;
    ra_hdc_get_qp_status(&qp_hdc, &status);

    g_interface_version = RA_RS_OPCODE_BASE_VERSION;
    status = 0;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 10);
    ra_hdc_get_qp_status(&qp_hdc, &status);

    struct ra_qp_handle *qp_handle = &qp_hdc;
    ra_hdc_qp_create(&rdma_handle, 0, 1, &qp_handle);

    mocker_clean();

    mocker(calloc, 5, NULL);
    ra_hdc_send_wrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    ra_hdc_qp_create(&rdma_handle, 0, 1, &qp_handle);
    mocker_clean();

    ra_hdc_send_wr(&qp_hdc, &swr, op_rsp);

    return;
}


void tc_peer_fail()
{
    //int server_devid = 0;
    //mocker(drvDeviceGetPhyIdByIndex, 5, -1);
    //ra_peer_get_server_devid(0, &server_devid);
    //mocker_clean();
	struct mr_info mr_info;
    void *mr_handle = NULL;
    void *comp_channel = NULL;
    int ret;

    struct SocketConnectInfoT conn[1] = {0};
    mocker(ra_get_socket_connect_info, 5, -1);
    ra_peer_socket_batch_connect(0, conn, 0);
    mocker_clean();

    struct ra_socket_handle socket_handle;
    socket_handle.rdev_info.phy_id = 0;
    socket_handle.rdev_info.family = AF_INET;
    socket_handle.rdev_info.local_ip.addr.s_addr = 0;
    conn[0].socket_handle = &socket_handle;
    mocker(ra_get_socket_connect_info, 5, 0);
    mocker(rs_socket_batch_connect, 5, -1);
    ra_peer_socket_batch_connect(0, conn, 0);
    mocker(rs_socket_set_scope_id, 5, -2);
    ret = ra_peer_socket_batch_connect(0, conn, 0);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct SocketCloseInfoT con[1] = {0};
    mocker(memset_s, 5, -1);
    ra_peer_socket_batch_close(0, con, 0);
    mocker_clean();

    mocker(memset_s, 5, 0);
    ra_peer_socket_batch_close(0, con, 0);
    mocker_clean();

    struct socket_hdc_info *hdc_socket_handle = calloc(1, sizeof(struct socket_hdc_info));
    con[0].fd_handle = hdc_socket_handle;
    mocker(memset_s, 5, 0);
    ret = ra_peer_socket_batch_close(0, con, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct SocketListenInfoT conn_listen[1] = {0};
    conn_listen[0].port = 65536;
    ra_peer_socket_listen_start(0, conn_listen, 1);
    ra_peer_socket_listen_stop(0, conn_listen, 1);

    conn_listen[0].port = 0;
    mocker(ra_get_socket_listen_info, 5, -1);
    ra_peer_socket_listen_start(0, conn_listen, 1);
    ra_peer_socket_listen_stop(0, conn_listen, 1);
    mocker_clean();

    struct ra_socket_handle socket_handles;
    socket_handles.rdev_info.phy_id = 0;
    socket_handles.rdev_info.family = AF_INET;
    conn_listen[0].socket_handle = &socket_handles;
    mocker(ra_get_socket_listen_info, 5, 0);
    ra_peer_socket_listen_start(0, conn_listen, 1);
    ra_peer_socket_listen_stop(0, conn_listen, 1);
    mocker_clean();

    mocker(ra_get_socket_listen_info, 5, 0);
    mocker(rs_socket_listen_start, 5, -1);
    ra_peer_socket_listen_start(0, conn_listen, 1);
    mocker((stub_fn_t)rs_socket_set_scope_id, 10, -2);
    ret = ra_peer_socket_listen_start(0, conn_listen, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    mocker(ra_get_socket_listen_info, 5, 0);
    mocker(rs_socket_listen_start, 5, 0);
    mocker(ra_get_socket_listen_result, 5, -1);
    ra_peer_socket_listen_start(0, conn_listen, 1);
    mocker_clean();

    struct SocketInfoT conn_p[1];
    struct socket_fd_data rs_conn[1];
    // struct ra_socket_handle socket_handle;
    // socket_handle.rdev_info.phy_id = 0;
    // socket_handle.rdev_info.family = AF_INET;
    // socket_handle.rdev_info.local_ip.addr.s_addr = 0;
    conn_p[0].status = 0;
    ra_peer_set_rs_conn_param(conn_p, 1, rs_conn, 0);

    rs_conn[0].phy_id = 0;
    rs_conn[0].fd = 0;
    conn_p[0].socket_handle = &socket_handle;
    mocker(memcpy_s, 5, -1);
    ra_peer_set_rs_conn_param(conn_p, 1, rs_conn, 2);
    mocker_clean();

    conn_p[0].fd_handle = calloc(1, sizeof(struct socket_peer_info));
    ra_peer_set_conn_param(conn_p, rs_conn, 0, 0);
    free(conn_p[0].fd_handle);
    conn_p[0].fd_handle = NULL;


    struct SocketInfoT conn_s[1] = {0};
    mocker(ra_peer_set_rs_conn_param, 5, -1);
    ra_peer_get_sockets(0, 0, conn_s, 1);
    mocker_clean();

    conn_p[0].status = 1;
    ra_peer_get_sockets(0, 0, conn_p, 1);
    free(conn_p[0].fd_handle);
    conn_p[0].fd_handle = NULL;

    mocker(ra_peer_set_rs_conn_param, 5, 0);
    mocker(ra_peer_set_conn_param, 1, 1);
    ra_peer_get_sockets(0, 0, conn_s, 1);
    mocker_clean();

    struct socket_peer_info peer_socket_handle = {0};

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

    mocker(ra_peer_set_rs_conn_param, 5, 0);
    mocker(rs_get_sockets, 5, -1);
    ra_peer_get_sockets(0, 0, conn_s, 1);
    mocker_clean();

    mocker(ra_peer_set_rs_conn_param, 5, 0);
    mocker(rs_get_sockets, 5, 0);
    mocker(rs_get_ssl_enable, 5, -1);
    ra_peer_get_sockets(0, 0, conn_s, 1);
    mocker_clean();

    struct rdev rdev_info = {0};
    rdev_info.local_ip.addr.s_addr = 0;
    struct socket_wlist_info_t white_list[1];
    white_list[0].remote_ip.addr.s_addr = 0;

    mocker(inet_ntoa, 5, NULL);
    ra_peer_socket_white_list_add(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, 1);
    mocker(rs_socket_white_list_add, 5, -1);
    ra_peer_socket_white_list_add(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, NULL);
    ra_peer_socket_white_list_del(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, 1);
    mocker(rs_socket_white_list_del, 5, -1);
    ra_peer_socket_white_list_del(rdev_info, white_list, 1);
    mocker_clean();

    mocker(rs_socket_deinit, 5, -1);
    ra_peer_socket_deinit(rdev_info);
    mocker_clean();

    struct ra_rdma_handle rdma_handle;
    rdma_handle.rdev_info.phy_id = 0;

    mocker(calloc, 5, NULL);
    void *qp_handle;
    void *qp_handle_with_attr;
    qp_handle_with_attr = NULL;
    struct QpExtAttrs ext_attrs;
    ra_peer_qp_create(&rdma_handle, 0, 0, &qp_handle);
    ret  = ra_peer_qp_create_with_attrs(&rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    mocker(rs_qp_create, 5, -1);
    mocker((stub_fn_t)rs_qp_create_with_attrs, 10, 1);
    ra_peer_qp_create(&rdma_handle, 0, 0, &qp_handle);
    ret  = ra_peer_qp_create_with_attrs(&rdma_handle, &ext_attrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    unsigned long long notify_size;
    unsigned long long va = 0;
    ret = ra_peer_get_notify_base_addr(&rdma_handle, &va, &notify_size);
    EXPECT_INT_NE(0, ret);
    mocker((stub_fn_t)rs_get_notify_mr_info, 10, -1);
    ret = ra_peer_get_notify_base_addr(&rdma_handle, &va, &notify_size);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct ra_qp_handle *qp_peer = NULL;
    qp_peer = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));

    mocker(rs_qp_destroy, 5, -1);
    ra_peer_qp_destroy(qp_peer);
    mocker_clean();

    struct ra_qp_handle qp_peer_p = {0};
    int status = 0;
    mocker(rs_get_qp_status, 5, -1);
    ra_peer_get_qp_status(&qp_peer_p, &status);
    mocker_clean();

    int addr = 0;
	mr_info.addr = addr;
	mr_info.size = 0;
	mr_info.access = 0;
    mocker(rs_mr_reg, 5, -1);
    ra_peer_mr_reg(&qp_peer_p, &mr_info);
    mocker_clean();

    mocker(rs_mr_dereg, 5, -1);
    ra_peer_mr_dereg(&qp_peer_p, &mr_info);
    mocker_clean();

    mocker(rs_register_mr, 5, -1);
    ra_peer_register_mr(&rdma_handle, &mr_info, &mr_handle);
    mocker_clean();

    mocker(rs_deregister_mr, 5, -1);
    ra_peer_deregister_mr(&rdma_handle, mr_handle);
    mocker_clean();

    mocker(rs_create_comp_channel, 5, -1);
    ret = ra_peer_create_comp_channel(&rdma_handle, &comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker(rs_destroy_comp_channel, 5, -1);
    ret = ra_peer_destroy_comp_channel(comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_peer_get_ifnum, 10, -1);
    unsigned int num = 0;
    ra_peer_get_ifnum(0, &num);
    mocker_clean();

    struct srq_attr attr = {0};
    mocker((stub_fn_t)rs_create_srq, 10, -1);
    ret = ra_peer_create_srq(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)rs_destroy_srq, 10, -1);
    ret = ra_peer_destroy_srq(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct interface_info interface_infos[1];
    mocker(rs_peer_get_ifaddrs, 5, -1);
    ra_peer_get_ifaddrs(0, interface_infos, &num);
    mocker_clean();

    unsigned int rdev_index = 0;
    //mocker(ra_peer_get_server_devid, 5, -1);
    ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, 0);
    mocker(rs_rdev_init, 5, -1);
    ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, -1);
    ra_peer_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, 0);
    mocker(rs_rdev_deinit, 5, -1);
    ra_peer_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();



    return;
}

void tc_comm_fail()
{
    struct ra_socket_handle socket_handle;
    struct SocketConnectInfoT conn[1];
    struct socket_connect_info rs_conn[1] ={0};
    char tag[SOCK_CONN_TAG_SIZE] = {0};
    socket_handle.rdev_info.phy_id = 0;
    socket_handle.rdev_info.family = AF_INET;
    socket_handle.rdev_info.local_ip.addr.s_addr = 0;
    conn[0].socket_handle = &socket_handle;
    conn[0].port = 0;
    conn[0].remote_ip.addr.s_addr = 0;
    memcpy_s(conn[0].tag, SOCK_CONN_TAG_SIZE, tag, SOCK_CONN_TAG_SIZE);


    struct SocketListenInfoT conn_listen[1];
    struct socket_listen_info rs_conn_listen[1] = {0};
    conn_listen[0].socket_handle = &socket_handle;
    conn_listen[0].port = 0;


    ra_get_socket_connect_info(NULL, 0, NULL, 0);
    ra_get_socket_listen_info(NULL, 0, NULL, 0);
    ra_get_socket_listen_result(NULL, 0, NULL, 0);


    struct socket_listen_info rs_conn_result[1];
    struct SocketListenInfoT conn_result[1] = {0};
    conn_result[0].socket_handle = &socket_handle;
    rs_conn_result[0].phase = 0;
    rs_conn_result[0].err = 0;
    rs_conn_result[0].phy_id = 0;
    rs_conn_result[0].family = 0;
    rs_conn_result[0].local_ip.addr.s_addr = 0;

    mocker(memcpy_s, 5, -1);
    ra_get_socket_connect_info(conn, 1, rs_conn, 2);

    ra_get_socket_listen_info(conn_listen, 1, rs_conn_listen, 2);

    ra_get_socket_listen_result(rs_conn_result, 1, conn_result, 2);
    mocker_clean();

    return;
}

extern int ra_assemble_sockets(union op_socket_info_data *socket_info_data, struct SocketInfoT *conn,
    unsigned int num, const int sock_fd[], size_t sock_fd_len);
void tc_hdc_fail_01()
{
    int data = 0;

    mocker(calloc, 5, NULL);
    ra_hdc_process_msg(0, 0, &data, 0);
    mocker_clean();

    struct SocketConnectInfoT conn[1];
    mocker(calloc, 5, NULL);
    ra_hdc_socket_batch_connect(0, conn, 0);
    mocker_clean();

    mocker(ra_get_socket_connect_info, 5, -1);
    ra_hdc_socket_batch_connect(0, conn, 0);
    mocker_clean();

    mocker(ra_get_socket_connect_info, 5, 0);
    mocker(ra_hdc_process_msg, 5, -1);
    ra_hdc_socket_batch_connect(0, conn, 0);
    mocker_clean();

    struct SocketListenInfoT conn_listen[1];

    mocker(ra_get_socket_listen_info, 5, -1);
    ra_hdc_socket_listen_start(0, conn_listen, 0);
    ra_hdc_socket_listen_stop(0, conn_listen, 0);
    mocker_clean();

    mocker(ra_get_socket_listen_info, 5, 0);
    mocker(ra_hdc_process_msg, 5, -1);
    ra_hdc_socket_listen_start(0, conn_listen, 0);
    ra_hdc_socket_listen_stop(0, conn_listen, 0);
    mocker_clean();

    mocker(ra_get_socket_listen_info, 5, 0);
    mocker(ra_hdc_process_msg, 5, 0);
    mocker(ra_get_socket_listen_result, 5, -1);
    ra_hdc_socket_listen_start(0, conn_listen, 0);
    mocker_clean();


    struct SocketInfoT conn_info[1] = {0};
    mocker(memcpy_s, 5, -1);
    ra_get_ip_and_tag_info(NULL, NULL, conn_info, 0);
    mocker_clean();

    struct SocketInfoT conn_s[1];
    mocker(calloc, 5, NULL);
    ra_hdc_get_sockets(0, 0, conn_s, 0);
    mocker_clean();

    mocker(ra_assemble_sockets, 5, -1);
    ra_hdc_get_sockets(0, 0, conn_s, 0);
    mocker_clean();

    struct socket_hdc_info handle;
    handle.fd = 0;

    mocker(calloc, 5, NULL);
    ra_hdc_socket_send(0, &handle, &data, 1);
    ra_hdc_socket_recv(0, &handle, &data, 1);
    mocker_clean();

    mocker(memcpy_s, 5, -1);
    ra_hdc_socket_send(0, &handle, &data, 1);
    mocker_clean();

    mocker(ra_hdc_process_msg, 5, -1);
    ra_hdc_socket_send(0, &handle, &data, 1);
    ra_hdc_socket_recv(0, &handle, &data, 1);
    mocker_clean();


    struct ra_qp_handle *qp_hdc = NULL;
    qp_hdc = (struct ra_qp_handle *)calloc(1, sizeof(struct ra_qp_handle));
    mocker(ra_hdc_process_msg, 5, -1);
    ra_hdc_qp_destroy(qp_hdc);
    mocker_clean();


    return;
}

extern int strncmp(const char *cs, const char *ct, size_t count);
void tc_host_fail()
{
    union hccp_ip_addr ip;
    char net_addr[1];
    int ret;
	unsigned long long *notify_va = NULL;
    mocker(inet_ntop, 5, NULL);
    ra_inet_pton(AF_INET, ip, net_addr, 1);
    mocker_clean();

    ra_inet_pton(AF_INET6, ip, net_addr, 1);

    mocker(drvDeviceGetIndexByPhyId, 5, -1);
    ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halNotifyGetInfo, 5, -1);
    ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halMemAlloc, 5, -1);
    ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(ra_hdc_notify_cfg_set, 5, -1);
    ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halMemFree, 5, -1);
    ra_hdc_notify_base_addr_init(NOTIFY, 0, &notify_va);
    mocker_clean();

    struct ra_init_config config = {0};
    config.phy_id = 0;
    ra_init(NULL);
    ra_deinit(NULL);

    config.phy_id = 17;
    ra_init(&config);
    ra_deinit(&config);

    config.phy_id = 0;
    config.nic_position  = NETWORK_OFFLINE;

    mocker(drvGetProcessSign, 5, -1);
    ra_init(&config);
    mocker_clean();

    mocker(strcpy_s, 5, -1);
    ra_init(&config);
    mocker_clean();

    mocker(ra_hdc_init, 5, -1);
    ra_init(&config);
    ra_deinit(&config);
    mocker_clean();

    config.nic_position  = 5;
    ra_init(&config);
    ra_deinit(&config);

    struct rdev rdev_info;
    struct ra_socket_handle *socket_handle = NULL;
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;

    ra_socket_init(NETWORK_OFFLINE, rdev_info, NULL);

    struct ra_socket_handle socket_handle_tmp;
    socket_handle_tmp.rdev_info.phy_id = 17;
    ra_socket_deinit(&socket_handle_tmp);

    socket_handle_tmp.rdev_info.phy_id = 0;
    mocker(ra_inet_pton, 10, -1);
    ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    ra_socket_deinit(&socket_handle_tmp);
    mocker_clean();

    mocker(calloc, 10, NULL);
    ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 10, -1);
    ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(ra_hdc_socket_init, 10, -1);
    ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    char local_ip[1];

    mocker(ra_get_ifaddrs, 10, 0);
    mocker(ra_inet_pton, 10, -1);
    ra_rdev_init_check_ip(NETWORK_OFFLINE, rdev_info, local_ip);
    mocker_clean();

    mocker(ra_get_ifaddrs, 10, 0);
    mocker(ra_inet_pton, 10, 0);
    mocker(strncmp, 10, -1);
    ra_rdev_init_check_ip(NETWORK_OFFLINE, rdev_info, local_ip);
    mocker_clean();

    mocker(ra_hdc_process_msg, 100, 0);
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    g_interface_version = 2;
    ra_rdev_init_check_ip(NETWORK_OFFLINE, rdev_info, local_ip);
    mocker_clean();

    struct ra_rdma_handle rdma_handle = {0};
    rdev_info.phy_id = 17;
    ra_rdev_init_check(0, rdev_info, local_ip, 0, &rdma_handle);

    rdev_info.phy_id = 0;
    ra_rdev_init_check(0, rdev_info, local_ip, 0, NULL);

    mocker(ra_inet_pton, 10, -1);
    ra_rdev_init_check(0, rdev_info, local_ip, 0, &rdma_handle);
    mocker_clean();

    struct ra_rdma_handle *rdma_handle_init = NULL;
    mocker(ra_rdev_init_check, 10, 0);
    mocker(ra_hdc_notify_base_addr_init, 10, -1);
    ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(ra_rdev_init_check, 10, 0);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    mocker(calloc, 10, NULL);
    ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(ra_rdev_init_check, 10, 0);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    ra_rdev_init(5, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(ra_rdev_init_check, 10, 0);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    mocker(memcpy_s, 10, -1);
    ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(ra_rdev_init_check, 10, 0);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    mocker(ra_hdc_rdev_init, 10, -1);
    ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    struct ra_rdma_handle *rdma_handle_t = NULL;
    rdma_handle_t = calloc(1, sizeof(struct ra_rdma_handle));

    rdma_handle_t->rdev_info.phy_id = 17;
    ra_rdev_deinit(rdma_handle_t, NOTIFY);

    rdma_handle_t->rdev_info.phy_id = 0;
    mocker(ra_inet_pton, 10, -1);
    ra_rdev_deinit(rdma_handle_t, NOTIFY);
    mocker_clean();

    free(rdma_handle_t);
    rdma_handle_t = NULL;


    struct SocketConnectInfoT conn_connect[1] = {0};
    struct ra_socket_handle socket_handle_connect = {0};
    conn_connect[0].socket_handle = &socket_handle_connect;
    ra_socket_batch_connect(conn_connect, 1);

    struct SocketCloseInfoT conn_close[1] = {0};
    conn_close[0].socket_handle = &socket_handle_connect;
    ra_socket_batch_close(conn_close, 1);

    struct SocketListenInfoT conn_listen[1] = {0};
    conn_listen[0].socket_handle = &socket_handle_connect;
    ra_socket_listen_start(conn_listen, 1);
    ra_socket_listen_stop(conn_listen, 1);

    struct SocketInfoT conn[1] = {0};
    conn[0].socket_handle = &socket_handle_connect;
    unsigned int connected_num = 0;
    ra_get_sockets(0, conn, 1, &connected_num);

    struct socket_hdc_info fd_handle = {0};
    fd_handle.socket_handle = NULL;
    int data = 0;
    unsigned long long sent_size = 0;
    unsigned long long received_size = 0;
    ra_socket_send(&fd_handle, &data, 1, &sent_size);
    ra_socket_recv(&fd_handle, &data, 1, &received_size);

    struct ra_qp_handle *qp_handle_create = NULL;
    ret = ra_qp_create(NULL, 1, -1, &qp_handle_create);
    EXPECT_INT_NE(0, ret);
    ret = ra_qp_create_with_attrs(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_typical_qp_create(NULL, 0, 0, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = ra_ai_qp_create(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct ra_qp_handle qp_handle = {0};
    qp_handle.rdma_ops = NULL;
    ra_qp_destroy(&qp_handle);

    ra_qp_connect_async(&qp_handle, &fd_handle);

    int status = 0;
    ra_get_qp_status(&qp_handle, &status);

    struct mr_info info = {0};
    ra_mr_reg(&qp_handle, &info);

    // ra_mr_dereg(void *qp_handle, struct mr_info *info);

    struct socket_init_info_t socket_init = {0};
    void* socket_handle1 = NULL;

    socket_init.scope_id = 0;
    socket_init.rdev_info.phy_id = 0;
    socket_init.rdev_info.family = AF_INET;
    socket_init.rdev_info.local_ip.addr.s_addr = 0;
    ra_socket_init_v1(NETWORK_OFFLINE, socket_init, &socket_handle1);

    socket_init.scope_id = 0;
    socket_init.rdev_info.phy_id = 0;
    socket_init.rdev_info.family = AF_INET6;
    socket_init.rdev_info.local_ip.addr.s_addr = 0;
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    ra_socket_deinit(socket_handle1);

    ra_socket_init_v1(NETWORK_ONLINE, socket_init, &socket_handle1);

    mocker(calloc, 1, NULL);
    ra_socket_init_v1(NETWORK_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    mocker(ra_inet_pton, 1, 99);
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    mocker(memcpy_s, 1, 1);
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, NULL);
    return;
}

void tc_ra_rdev_get_port_status()
{
    enum port_status status = PORT_STATUS_DOWN;
    struct ra_rdma_handle rdma_handle = { 0 };
    struct ra_rdma_ops ops = {0};
    int ret;

    ret = ra_rdev_get_port_status(NULL, NULL);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdev_info.phy_id = 100000;
    ret = ra_rdev_get_port_status(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdev_info.phy_id = 0;
    ret = ra_rdev_get_port_status(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    ops.ra_rdev_get_port_status = ra_hdc_rdev_get_port_status;
    rdma_handle.rdma_ops = &ops;
    mocker(ra_hdc_process_msg, 5, -1);
    ret = ra_rdev_get_port_status(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(ra_hdc_process_msg, 5, 0);
    ret = ra_rdev_get_port_status(&rdma_handle, &status);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_rdev_get_port_status(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
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

void tc_ra_get_qp_context()
{
    struct ra_qp_handle ra_qp_handle;
    void *qp_handle = (void *)&ra_qp_handle;
    void *qp = NULL;
    void *send_cq= NULL;
    void *recv_cq = NULL;
    struct ra_rdma_ops ops;
    ra_qp_handle.rdma_ops = NULL;
    ra_get_qp_context(qp_handle, &qp, &send_cq, &recv_cq);
    ra_get_qp_context(NULL, &qp, &send_cq, &recv_cq);
    ops.ra_get_qp_context = ra_peer_get_qp_context;
    ra_qp_handle.rdma_ops = &ops;
    ra_get_qp_context(qp_handle, &qp, &send_cq, &recv_cq);
}

void tc_peer_log_test()
{
    struct ra_init_config config = {
        .phy_id = 0,
        .nic_position = 0,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    mocker(rs_init, 1, 0);
    ra_peer_init(&config, 1);
    int ret = ra_peer_init(&config, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    struct interface_info interface_infos[1];
    unsigned int num = 0;
    mocker(rs_peer_get_ifaddrs, 5, 0);
    ra_peer_get_ifaddrs(0, interface_infos, &num);
    mocker_clean();
    ra_peer_deinit(&config);

    mocker((stub_fn_t)rs_deinit, 10, -11);
    ret = ra_peer_deinit(&config);
    EXPECT_INT_EQ(-11, ret);
    mocker_clean();
    return;
}

void tc_ra_rs_send_wr_list_v2()
{
    union op_send_wrlist_data_v2 send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;

    union op_send_wrlist_data_v2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_v2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list()
{
    union op_send_wrlist_data send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;

    union op_send_wrlist_data send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_v2();
}

void tc_ra_rs_send_wr_list_ext_v2()
{
    union op_send_wrlist_data_ext_v2 send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;
    send_wrlist.tx_data.wrlist[0].aux.data_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.reduce_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.notify_offset = 0;

    union op_send_wrlist_data_ext_v2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_ext_v2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list_ext()
{
    union op_send_wrlist_data_ext send_wrlist;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;
    send_wrlist.tx_data.wrlist[0].aux.data_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.reduce_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.notify_offset = 0;

    union op_send_wrlist_data_ext send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ra_rs_send_wr_list_ext(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    tc_ra_rs_send_wr_list_ext_v2();
}

void tc_ra_rs_send_normal_wrlist()
{
    union op_send_wrlist_data_ext_v2 send_wrlist;
    int ret = 0;

    send_wrlist.tx_data.phy_id = 0;
    send_wrlist.tx_data.rdev_index = 0;
    send_wrlist.tx_data.qpn = 0;
    send_wrlist.tx_data.send_num = 1;
    send_wrlist.tx_data.wrlist[0].op = 0;
    send_wrlist.tx_data.wrlist[0].send_flags = 0;
    send_wrlist.tx_data.wrlist[0].dst_addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.addr = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.len = 0;
    send_wrlist.tx_data.wrlist[0].mem_list.lkey = 0;
    send_wrlist.tx_data.wrlist[0].aux.data_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.reduce_type = 0;
    send_wrlist.tx_data.wrlist[0].aux.notify_offset = 0;

    union op_send_wrlist_data_ext_v2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ret = ra_rs_send_normal_wrlist(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_hdc_send_wrlist_ext_init_v2()
{
    union op_send_wrlist_data_ext_v2 send_wrlist;
    struct ra_qp_handle qp_hdc;
    unsigned int complete_cnt;
    struct wrlist_send_complete_num wrlist_num;
    ra_hdc_send_wrlist_ext_init_v2(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);
}

void tc_ra_hdc_send_wrlist_ext_init()
{
    union op_send_wrlist_data_ext send_wrlist;
    struct ra_qp_handle qp_hdc;
    unsigned int complete_cnt;
    struct wrlist_send_complete_num wrlist_num;
    ra_hdc_send_wrlist_ext_init(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);
    tc_ra_hdc_send_wrlist_ext_init_v2();
}

void tc_ra_hdc_send_wrlist_ext()
{
    mocker_clean();

    struct ra_qp_handle qp_handle;

    struct send_wrlist_data_ext wr[1];
    struct send_wr_rsp op_rsp[1];
    struct wrlist_send_complete_num wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;

    struct ra_qp_handle handle;
    handle.qp_mode = 1;

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = ra_hdc_send_wrlist(&handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
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

void tc_ra_hdc_send_normal_wrlist()
{
    struct ra_qp_handle qp_handle;
    struct wr_info wr[1];
    struct send_wr_rsp op_rsp[1];
    struct wrlist_send_complete_num wrlist_num = { 0 };
    unsigned int complete_num = 0;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;
    int ret = 0;

    mocker_clean();

    qp_handle.qp_mode = RA_RS_OP_QP_MODE;
    qp_handle.support_lite = 1;
    ra_hdc_send_normal_wrlist(&qp_handle, wr, op_rsp, wrlist_num);

    qp_handle.qp_mode = RA_RS_NOR_QP_MODE;
    wrlist_num.complete_num = &complete_num;
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    ret = ra_hdc_send_normal_wrlist(&qp_handle, wr, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
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

void tc_ra_create_cq()
{
    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    void* context;
    struct cq_attr attr;
    attr.qp_context = &context;
    attr.ib_send_cq = &ib_send_cq;
    attr.ib_recv_cq = &ib_recv_cq;
    attr.send_cq_depth = 16384;
    attr.recv_cq_depth = 16384;
    attr.send_cq_event_id = 1;
    attr.recv_cq_event_id = 2;

    struct ra_rdma_handle ra_rdma_handle;
    void *rdma_handle = (void *)&ra_rdma_handle;
    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;
    ra_cq_create(rdma_handle, &attr);
    ra_cq_destroy(rdma_handle, &attr);

    struct ra_rdma_ops ops;
    ops.ra_cq_create = ra_peer_cq_create;
    ops.ra_cq_destroy = ra_peer_cq_destroy;
    ra_rdma_handle.rdma_ops = &ops;
    ra_cq_create(rdma_handle, &attr);
    ra_cq_destroy(rdma_handle, &attr);

    ra_rdma_handle.rdev_info.phy_id = 0;
    ra_cq_create(rdma_handle, &attr);
    ra_cq_destroy(rdma_handle, &attr);
}

void tc_ra_create_notmal_qp()
{
    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    void* context;
    struct ibv_qp_init_attr qp_init_attr;
    qp_init_attr.qp_context = context;
    qp_init_attr.send_cq = ib_send_cq;
    qp_init_attr.recv_cq = ib_recv_cq;
    qp_init_attr.qp_type = 2;
    qp_init_attr.cap.max_inline_data = 32;
    qp_init_attr.cap.max_send_wr = 4096;
    qp_init_attr.cap.max_send_sge = 4096;
    qp_init_attr.cap.max_recv_wr = 4096;
    qp_init_attr.cap.max_recv_sge = 1;
	struct ibv_qp* qp;
    struct ra_qp_handle *qp_handle = NULL;

    struct ra_rdma_handle ra_rdma_handle;
    void *rdma_handle = (void *)&ra_rdma_handle;
    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_normal_qp_destroy(qp_handle);

    struct ra_rdma_ops ops;
    ops.ra_normal_qp_create = ra_peer_normal_qp_create;
    ops.ra_normal_qp_destroy = ra_peer_normal_qp_destroy;
    ra_rdma_handle.rdma_ops = &ops;
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_normal_qp_destroy(qp_handle);

    ra_normal_qp_create(rdma_handle, &qp_init_attr, NULL, &qp);
    ra_normal_qp_destroy(NULL);

    ra_rdma_handle.rdev_info.phy_id = 0;
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_normal_qp_destroy(qp_handle);

    mocker((stub_fn_t)ra_peer_normal_qp_create, 10, -1);
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    mocker_clean();
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

void tc_ra_create_comp_channel()
{
    struct ra_rdma_handle ra_rdma_handle;
    void *rdma_handle = (void *)&ra_rdma_handle;
    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;

    void *comp_channel = NULL;
    ra_create_comp_channel(rdma_handle, &comp_channel);
    ra_destroy_comp_channel(rdma_handle, comp_channel);

    comp_channel = (void *)0xabcd;
    ra_create_comp_channel(rdma_handle, &comp_channel);
    ra_destroy_comp_channel(rdma_handle, comp_channel);

    struct ra_rdma_ops ops;
    ops.ra_create_comp_channel = ra_peer_create_comp_channel;
    ops.ra_destroy_comp_channel = ra_peer_destroy_comp_channel;
    ra_rdma_handle.rdma_ops = &ops;
    ra_create_comp_channel(rdma_handle, &comp_channel);
    ra_destroy_comp_channel(rdma_handle, comp_channel);

    ra_create_comp_channel(rdma_handle, NULL);
    ra_destroy_comp_channel(rdma_handle, NULL);
    ra_create_comp_channel(NULL, NULL);
    ra_destroy_comp_channel(NULL, NULL);

    ra_rdma_handle.rdev_info.phy_id = 0;
    ra_create_comp_channel(rdma_handle, &comp_channel);
    ra_destroy_comp_channel(rdma_handle, comp_channel);
}

void tc_ra_get_cqe_err_info()
{
    int ret;
    struct cqe_err_info info = {0};

    ret = ra_get_cqe_err_info(0, NULL);
    EXPECT_INT_EQ(128103, ret);

    mocker(ra_hdc_process_msg, 1, 0);
    ret = ra_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_ra_rdev_get_cqe_err_info_list()
{
    struct ra_rdma_handle ra_rdma_handle;
    struct cqe_err_info info[128] = {0};
    unsigned int num = 128;
    int ret;

    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;

    mocker(ra_hdc_get_cqe_err_info_list, 10, 0);
    ret = ra_rdev_get_cqe_err_info_list((void *)&ra_rdma_handle, info, &num);
    EXPECT_INT_EQ(0, ret);

    ret = ra_rdev_get_cqe_err_info_list((void *)&ra_rdma_handle, info, NULL);
    EXPECT_INT_EQ(128103, ret);

    num = 129;
    ret = ra_rdev_get_cqe_err_info_list((void *)&ra_rdma_handle, info, &num);
    EXPECT_INT_EQ(128303, ret);
    mocker_clean();

    return;
}


void tc_ra_rs_get_ifnum()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_get_ifnum(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_rs_ping()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_ping_init(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_init, 1, 12);
    ret = ra_rs_ping_init(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_target_add(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_target_add, 1, 12);
    ret = ra_rs_ping_target_add(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_task_start(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_task_start, 1, 12);
    ret = ra_rs_ping_task_start(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_get_results(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_get_results, 1, 12);
    ret = ra_rs_ping_get_results(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_task_stop(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_task_stop, 1, 12);
    ret = ra_rs_ping_task_stop(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_target_del(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_target_del, 1, 12);
    ret = ra_rs_ping_target_del(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = ra_rs_ping_deinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(rs_ping_deinit, 1, 12);
    ret = ra_rs_ping_deinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    return;
}

void tc_ra_get_qp_attr()
{
    struct qp_attr qpn = {0};
    struct ra_qp_handle qp_handle = {0};
    struct ra_rdma_handle rdma_handle = {0};
    int ret;

    ret = ra_get_qp_attr(NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    qp_handle.rdma_handle = &rdma_handle;
    ret = ra_get_qp_attr(&qp_handle, &qpn);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_ra_create_srq()
{
    struct ra_rdma_handle ra_rdma_handle;
    void *rdma_handle = (void *)&ra_rdma_handle;
    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;
    struct srq_attr attr = {0};

    ra_create_srq(rdma_handle, NULL);
    ra_destroy_srq(rdma_handle, NULL);

    ra_create_srq(rdma_handle, &attr);
    ra_destroy_srq(rdma_handle, &attr);

    struct ra_rdma_ops ops;
    ops.ra_create_srq = ra_peer_create_srq;
    ops.ra_destroy_srq = ra_peer_destroy_srq;
    ra_rdma_handle.rdma_ops = &ops;
    ra_create_srq(rdma_handle, &attr);
    ra_destroy_srq(rdma_handle, &attr);

    ra_create_srq(NULL, NULL);
    ra_destroy_srq(NULL, NULL);

    ra_rdma_handle.rdev_info.phy_id = 0;
    ra_create_srq(rdma_handle, &attr);
    ra_destroy_srq(rdma_handle, &attr);
}

void tc_hdc_send_wr_op()
{
    mocker_clean();
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_qp_handle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);

	ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);


    struct send_wr wr = {0};
    struct send_wr_rsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct sg_list mem;
    mem.addr = addr;
	mem.len = 10;
	wr.buf_list = &mem;
	wr.dst_addr = 0x111;
	wr.buf_num = 1;
	wr.op = 0;
	wr.send_flag = 0;
    qp_handle->local_mr[0].addr = wr.buf_list[0].addr;
    qp_handle->local_mr[0].len = 0x10000;
    qp_handle->rem_mr[0].addr = wr.dst_addr;
    qp_handle->rem_mr[0].len = 0x10000;
    qp_handle->send_wr_num = 999;
    mocker_invoke((stub_fn_t)ra_rdma_lite_poll_cq, stub_ra_rdma_lite_poll_cq, 10);
    ret = ra_hdc_send_wr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct wrlist_send_complete_num wrlist_num;
    wrlist_num.send_num = 2;
    wrlist_num.complete_num = &complete_num;
    struct send_wrlist_data wrlist[wrlist_num.send_num];
	struct send_wr_rsp op_rsp_list[wrlist_num.send_num];
	wrlist[0].mem_list = mem;
	wrlist[0].dst_addr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].send_flags = 0;
	wrlist[1].mem_list = mem;
	wrlist[1].dst_addr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].send_flags = 0;
    qp_handle->rdma_ops = rdma_handle.rdma_ops;
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    struct send_wrlist_data_ext data_ext[wrlist_num.send_num];
    data_ext[0].mem_list = mem;
	data_ext[0].dst_addr = 0x111;
	data_ext[0].op = 0;
	data_ext[0].send_flags = 0;
	data_ext[1].mem_list = mem;
	data_ext[1].dst_addr = 0x111;
	data_ext[1].op = 0;
	data_ext[1].send_flags = 0;
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)ra_hdc_lite_post_send, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    mocker((stub_fn_t)ra_hdc_lite_post_send, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    ra_hdc_lite_period_poll_cqe(&rdma_handle);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);

    free(addr);
    mocker_clean();
}

void tc_hdc_lite_send_wr_op()
{
    mocker_clean();
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_qp_handle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);

    struct send_wr wr = {0};
    struct send_wr_rsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct sg_list mem;
    mem.addr = addr;
    mem.len = 10;
    wr.buf_list = &mem;
    wr.dst_addr = 0x111;
    wr.buf_num = 1;
    wr.op = 0;
    wr.send_flag = 0;
    qp_handle->local_mr[0].addr = wr.buf_list[0].addr;
    qp_handle->local_mr[0].len = 0x10000;
    qp_handle->rem_mr[0].addr = wr.dst_addr;
    qp_handle->rem_mr[0].len = 0x10000;
    qp_handle->send_wr_num = 999;
    qp_handle->support_lite = 1;
    mocker_invoke((stub_fn_t)ra_rdma_lite_poll_cq, stub_ra_rdma_lite_poll_cq, 10);
    ret = ra_hdc_send_wr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct wrlist_send_complete_num wrlist_num;
    wrlist_num.send_num = 2;
    wrlist_num.complete_num = &complete_num;
    struct send_wrlist_data wrlist[wrlist_num.send_num];
    struct send_wr_rsp op_rsp_list[wrlist_num.send_num];
    wrlist[0].mem_list = mem;
    wrlist[0].dst_addr = 0x111;
    wrlist[0].op = 0;
    wrlist[0].send_flags = 0;
    wrlist[1].mem_list = mem;
    wrlist[1].dst_addr = 0x111;
    wrlist[1].op = 0;
    wrlist[1].send_flags = 0;
    qp_handle->rdma_ops = rdma_handle.rdma_ops;
    ret = ra_hdc_send_wrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    struct send_wrlist_data_ext data_ext[wrlist_num.send_num];
    data_ext[0].mem_list = mem;
    data_ext[0].dst_addr = 0x111;
    data_ext[0].op = 0;
    data_ext[0].send_flags = 0;
    data_ext[1].mem_list = mem;
    data_ext[1].dst_addr = 0x111;
    data_ext[1].op = 0;
    data_ext[1].send_flags = 0;
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)ra_hdc_lite_post_send, 10, -12);
    ret = ra_hdc_send_wrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    mocker((stub_fn_t)ra_hdc_lite_post_send, 10, -12);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    ra_hdc_lite_period_poll_cqe(&rdma_handle);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);

    free(addr);
    mocker_clean();
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

void tc_ra_peer_socket_batch_abort()
{
    unsigned int dev_id;
    struct SocketConnectInfoT conn[4] = {0};
    int ret = 0;

    mocker(ra_get_socket_connect_info, 20, 1);
    ret = ra_peer_socket_batch_abort(dev_id, conn, 5);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(ra_get_socket_connect_info, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(rs_socket_batch_abort, 10, 1);
    ret = ra_peer_socket_batch_abort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 1);
    mocker_clean();

    mocker(ra_get_socket_connect_info, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(rs_socket_batch_abort, 10, 0);
    ret = ra_peer_socket_batch_abort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 0);
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


void tc_hdc_recv_wrlist()
{
    mocker_clean();
    void *addr = NULL;
    int size = 0;
    int ret = 0;
    struct recv_wrlist_data rev_wr = {0};
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_qp_handle* qp_handle = NULL;
    struct ra_qp_handle qp_handle_tmp = { 0 };

    rev_wr.wr_id = 100;
    rev_wr.mem_list.lkey = 0xff;
    rev_wr.mem_list.addr = addr;
    rev_wr.mem_list.len = size;

    // abnormal case
    qp_handle_tmp.qp_mode = 0;
    ret = ra_hdc_recv_wrlist(&qp_handle_tmp, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_NE(ret, 0);

    // normal case
    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->support_lite = 1;

    ret = ra_hdc_recv_wrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_hdc_poll_cq()
{
    mocker_clean();
    int ret = 0;
    unsigned int num_entries = 1;
    struct rdma_lite_wc_v2 lite_wc = {0};
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_qp_handle* qp_handle = NULL;
    struct ra_qp_handle qp_handle_tmp = { 0 };

    // abnormal case
    qp_handle_tmp.qp_mode = 0;
    ret = ra_hdc_poll_cq(&qp_handle_tmp, true, num_entries, &lite_wc);
    EXPECT_INT_NE(ret, 0);

    // normal case
    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    rdma_handle.disabled_lite_thread = true;
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->support_lite = 1;
    qp_handle->recv_wr_num = 1;

    ret = ra_hdc_poll_cq(qp_handle, false, num_entries, &lite_wc);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_hdc_get_lite_support()
{
    int support;

    g_interface_version = 0;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    ra_hdc_get_opcode_lite_support(0, 0x3, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    ra_hdc_get_opcode_lite_support(0, 0x3, &support);
    EXPECT_INT_EQ(support, 1);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    ra_hdc_get_opcode_lite_support(0, 0x2, &support);
    EXPECT_INT_EQ(support, 2);
    mocker_clean();

    g_interface_version = 1;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    ra_hdc_get_opcode_lite_support(0, 0x2, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();
}

void tc_ra_rdev_get_support_lite()
{
    struct ra_rdma_handle rdma_handle = {0};
    int support_lite = 1;
    int ret;

    ret = ra_rdev_get_support_lite(NULL, NULL);
    EXPECT_INT_NE(ret, 0);

    ret = ra_rdev_get_support_lite(&rdma_handle, &support_lite);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(support_lite, rdma_handle.support_lite);
}

void tc_ra_rdev_get_handle()
{
    void *rdma_handle = NULL;
    int ret;

    ret = ra_rdev_get_handle(1024, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_rdev_get_handle(0, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_rdev_get_handle(0, &rdma_handle);
    EXPECT_INT_EQ(ret, -ENODEV);
}

void tc_ra_is_first_or_last_used()
{
    s32 ret = 0;

    ret = ra_is_first_used(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_is_first_used(0);
    EXPECT_INT_EQ(ret, 1);

    ret = ra_is_first_used(0);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_is_first_used(0);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_is_last_used(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_is_last_used(0);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_is_last_used(0);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_is_last_used(0);
    EXPECT_INT_EQ(ret, 1);

    ret = ra_is_last_used(0);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_is_last_used(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_is_first_used(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = ra_is_first_used(15);
    EXPECT_INT_EQ(ret, 1);

    ret = ra_is_last_used(15);
    EXPECT_INT_EQ(ret, 1);
}

void tc_ra_rs_socket_port_is_use()
{
    unsigned int size = sizeof(union op_socket_connect_data) + sizeof(struct msg_head);
    union op_socket_connect_data socket_connect_data = {{0}};
    unsigned int port = 0x16;

    socket_connect_data.tx_data.conn[0].port = port;
    socket_connect_data.tx_data.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct msg_head), &socket_connect_data, sizeof(union op_socket_connect_data));
    memcpy(out_buf + sizeof(struct msg_head), &socket_connect_data, sizeof(union op_socket_connect_data));
    ra_rs_socket_batch_connect(in_buf, out_buf, &out_len, &op_result, size);

    socket_connect_data.tx_data.num = 1U | (1U << 31U);
    socket_connect_data.tx_data.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct msg_head), &socket_connect_data, sizeof(union op_socket_connect_data));
    memcpy(out_buf + sizeof(struct msg_head), &socket_connect_data, sizeof(union op_socket_connect_data));
    ra_rs_socket_batch_connect(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;

    size = sizeof(union op_socket_listen_data) + sizeof(struct msg_head);
    union op_socket_listen_data socket_listen_data = {{0}};
    socket_listen_data.tx_data.conn[0].port = port;
    socket_listen_data.tx_data.num = 1;

    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &socket_listen_data, sizeof(union op_socket_listen_data));
    memcpy(out_buf + sizeof(struct msg_head), &socket_listen_data, sizeof(union op_socket_listen_data));
    ra_rs_socket_listen_start(in_buf, out_buf, &out_len, &op_result, size);
    ra_rs_socket_listen_stop(in_buf, out_buf, &out_len, &op_result, size);

    socket_listen_data.tx_data.num = 1U | (1U << 31U);
    socket_listen_data.tx_data.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct msg_head), &socket_listen_data, sizeof(union op_socket_listen_data));
    memcpy(out_buf + sizeof(struct msg_head), &socket_listen_data, sizeof(union op_socket_listen_data));
    ra_rs_socket_listen_start(in_buf, out_buf, &out_len, &op_result, size);
    ra_rs_socket_listen_stop(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
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
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 100);
    ret = ra_socket_get_vnic_ip_infos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_rs_get_vnic_ip_infos_v1()
{
    unsigned int size = sizeof(union op_get_vnic_ip_infos_data_v1) + sizeof(struct msg_head);
    union op_get_vnic_ip_infos_data_v1 vnic_infos = {{0}};

    vnic_infos.tx_data.phy_id = 0;
    vnic_infos.tx_data.type = 0;
    vnic_infos.tx_data.ids[0] = 3232235521;
    vnic_infos.tx_data.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct msg_head), &vnic_infos, sizeof(union op_get_vnic_ip_infos_data_v1));
    memcpy(out_buf + sizeof(struct msg_head), &vnic_infos, sizeof(union op_get_vnic_ip_infos_data_v1));
    ra_rs_get_vnic_ip_infos_v1(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_vnic_ip_infos()
{
    unsigned int size = sizeof(union op_get_vnic_ip_infos_data) + sizeof(struct msg_head);
    union op_get_vnic_ip_infos_data vnic_infos = {{0}};
    int ret;

    vnic_infos.tx_data.phy_id = 0;
    vnic_infos.tx_data.type = 0;
    vnic_infos.tx_data.ids[0] = 3232235521;
    vnic_infos.tx_data.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct msg_head), &vnic_infos, sizeof(union op_get_vnic_ip_infos_data));
    memcpy(out_buf + sizeof(struct msg_head), &vnic_infos, sizeof(union op_get_vnic_ip_infos_data));
    ra_rs_get_vnic_ip_infos(in_buf, out_buf, &out_len, &op_result, size);

    mocker(rs_get_vnic_ip, 1, 12);
    ret = ra_rs_rdev_init_with_backup(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_typical_mr_reg()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_typical_mr_reg_v1(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_rs_typical_mr_dereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_rs_typical_mr_reg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_rs_typical_mr_dereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_rs_get_cqe_err_info()
{
    char *cqe_list_buf = NULL;
    char *cqe_num_buf = NULL;
    int op_result;
    int size;
    int ret;

    size = sizeof(union op_get_cqe_err_info_num_data) + sizeof(struct msg_head);
    cqe_num_buf = calloc(1, size);
    ret = ra_rs_get_cqe_err_info_num(cqe_num_buf, cqe_num_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    size = sizeof(union op_get_cqe_err_info_list_data) + sizeof(struct msg_head);
    cqe_list_buf = calloc(1, size);
    ret = ra_rs_get_cqe_err_info_list(cqe_list_buf, cqe_list_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(cqe_num_buf);
    free(cqe_list_buf);
}

void tc_ra_rs_rdev_init()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_rdev_init_with_backup(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_rs_get_qp_status()
{
    union op_qp_status_data qp_status_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_qp_status_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    qp_status_data.tx_data.phy_id = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct msg_head), &qp_status_data, sizeof(union op_qp_status_data));
    memcpy(out_buf + sizeof(struct msg_head), &qp_status_data, sizeof(union op_qp_status_data));
    ret = ra_rs_get_qp_status(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_qp_info()
{
    union op_qp_info_data qp_info_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_qp_info_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    qp_info_data.tx_data.phy_id = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct msg_head), &qp_info_data, sizeof(union op_qp_info_data));
    memcpy(out_buf + sizeof(struct msg_head), &qp_info_data, sizeof(union op_qp_info_data));
    ret = ra_rs_get_qp_info(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_sockets()
{
    union op_socket_info_data socket_info_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_socket_info_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    socket_info_data.tx_data.num = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct msg_head), &socket_info_data, sizeof(union op_socket_info_data));
    memcpy(out_buf + sizeof(struct msg_head), &socket_info_data, sizeof(union op_socket_info_data));
    ret = ra_rs_get_sockets(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_rdev_init_with_backup()
{
    union op_rdev_init_with_backup_data rdev_init_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_rdev_init_with_backup_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &rdev_init_data, sizeof(union op_rdev_init_with_backup_data));
    memcpy(out_buf + sizeof(struct msg_head), &rdev_init_data, sizeof(union op_rdev_init_with_backup_data));
    mocker(rs_rdev_init_with_backup, 1, 12);
    ret = ra_rs_rdev_init_with_backup(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_cqe_err_info_01()
{
    union op_get_cqe_err_info_data cqe_err_info_ret = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_get_cqe_err_info_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &cqe_err_info_ret, sizeof(union op_get_cqe_err_info_data));
    memcpy(out_buf + sizeof(struct msg_head), &cqe_err_info_ret, sizeof(union op_get_cqe_err_info_data));
    mocker(rs_get_cqe_err_info, 1, 12);
    ret = ra_rs_get_cqe_err_info(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_ai_qp_create()
{
    union op_ai_qp_create_data create_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_ai_qp_create_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &create_data, sizeof(union op_ai_qp_create_data));
    memcpy(out_buf + sizeof(struct msg_head), &create_data, sizeof(union op_ai_qp_create_data));
    mocker(rs_qp_create_with_attrs, 1, 12);
    ret = ra_rs_ai_qp_create(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_typical_qp_modify()
{
    union op_typical_qp_modify_data qp_modify_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_typical_qp_modify_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &qp_modify_data, sizeof(union op_typical_qp_modify_data));
    memcpy(out_buf + sizeof(struct msg_head), &qp_modify_data, sizeof(union op_typical_qp_modify_data));
    mocker(rs_typical_qp_modify, 1, 12);
    ret = ra_rs_typical_qp_modify(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
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

int stub_ra_hdc_get_cqe_err_num(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    if (data_size == sizeof(union op_get_cqe_err_info_num_data)) {
        union op_get_cqe_err_info_num_data *cqe_err_info_num_data = (union op_get_cqe_err_info_num_data *)data;
        cqe_err_info_num_data->rx_data.num = 10;
    } else if (data_size == sizeof(union op_get_cqe_err_info_list_data)) {
        union op_get_cqe_err_info_list_data *cqe_err_info_list =
            (union op_get_cqe_eop_get_cqe_err_info_list_datarr_info_num_data *)data;
        cqe_err_info_list->rx_data.num = 1;
    }
    return 0;
}

int stub_ra_hdc_get_cqe_err_num_v2(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    stub_ra_hdc_get_cqe_err_num(opcode, device_id, data, data_size);
    ((union op_get_cqe_err_info_list_data *)data)->rx_data.info_list[0].qpn = 12345;
    return 0;
}

int stub_ra_hdc_get_cqe_err_info_num(struct ra_rdma_handle *rdma_handle, unsigned int *num)
{
    *num = 10;
    return 0;
}

int stub_ra_hdc_get_interface_version(unsigned int phy_id, unsigned int interface_opcode, unsigned int *interface_version)
{
    *interface_version = 1;
    return 0;
}

int stub_ra_hdc_lite_get_cqe_err_info_list(struct ra_rdma_handle *rdma_handle, struct cqe_err_info *info_list,
    unsigned int *num)
{
    *num = 10;
    return 0;
}

void tc_ra_hdc_get_cqe_err_info_list()
{
    struct ra_rdma_handle rdma_handle = {0};
    struct ra_qp_handle qp_hdc = {0};
    struct cqe_err_info info[130] = {0};
    int num = 11;
    int ret = 0;

    mocker_clean();
    mocker_invoke(ra_hdc_lite_get_cqe_err_info_list, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(ra_hdc_get_cqe_err_info_num, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker((stub_fn_t)ra_hdc_process_msg, 10, 0);
    ret = ra_hdc_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 10);

    mocker_clean();
    mocker_invoke(ra_hdc_lite_get_cqe_err_info_list, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(ra_hdc_get_cqe_err_info_num, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    num = 11;
    ret = ra_hdc_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(ra_hdc_lite_get_cqe_err_info_list, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(ra_hdc_get_cqe_err_info_num, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num_v2, 10);
    num = 129;
    ret = ra_hdc_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(info[10].qpn, 12345);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(ra_hdc_lite_get_cqe_err_info_list, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(ra_hdc_get_cqe_err_info_num, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    num = 11;
    ret = ra_hdc_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    rdma_handle.support_lite = 0;
    ret = ra_hdc_lite_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    rdma_handle.support_lite = 1;
    rdma_handle.cqe_err_cnt = 0;
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = ra_hdc_lite_get_cqe_err_info_list(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(ra_hdc_get_interface_version, 10, -1);
    ret = ra_hdc_get_cqe_err_info_num(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(ra_hdc_process_msg, 10, 0);
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_hdc_get_interface_version, 1);
    ret = ra_hdc_get_cqe_err_info_num(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    return;
}

void tc_ra_hdc_lite_ctx_init()
{
    struct ra_rdma_handle rdma_handle = {0};
    unsigned int phy_id = 0;
    unsigned int rdev_index = 0;
    struct rdma_lite_context rdma_lite_context = {0};
    int ret = 0;

    rdma_handle.support_lite = 2 * 1024 * 1024;
    mocker_clean();
    mocker(ra_hdc_lite_mutex_deinit, 10, 0);
    mocker(ra_rdma_lite_free_ctx, 10, 0);
    mocker(dl_hal_sensor_node_unregister, 10, 0);
    mocker(dl_drv_device_get_index_by_phy_id, 10, 0);
    mocker(ra_sensor_node_register, 10, 0);
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_rdma_lite_alloc_ctx, 1, NULL);
    ret = ra_hdc_lite_ctx_init(&rdma_handle, phy_id, rdev_index);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(ra_hdc_lite_mutex_deinit, 10, 0);
    mocker(ra_rdma_lite_free_ctx, 10, 0);
    mocker(dl_hal_sensor_node_unregister, 10, 0);
    mocker(dl_drv_device_get_index_by_phy_id, 10, 0);
    mocker(ra_sensor_node_register, 10, 0);
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_rdma_lite_alloc_ctx, 10, &rdma_lite_context);
    mocker(ra_hdc_lite_mutex_init, 10, 0);
    rdma_handle.disabled_lite_thread = true;
    ret = ra_hdc_lite_ctx_init(&rdma_handle, phy_id, rdev_index);
    EXPECT_INT_EQ(ret, 0);

    rdma_handle.disabled_lite_thread = false;
    mocker(pthread_create, 10, -1);
    ret = ra_hdc_lite_ctx_init(&rdma_handle, phy_id, rdev_index);
    EXPECT_INT_EQ(ret, -258);

    mocker_clean();
    mocker(ra_hdc_lite_mutex_deinit, 10, 0);
    mocker(ra_rdma_lite_free_ctx, 10, 0);
    mocker(dl_hal_sensor_node_unregister, 10, 0);
    mocker(dl_drv_device_get_index_by_phy_id, 10, 0);
    mocker(ra_sensor_node_register, 10, 0);
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_rdma_lite_alloc_ctx, 10, &rdma_lite_context);
    mocker(ra_hdc_lite_mutex_init, 10, 0);
    mocker(pthread_create, 10, 0);
    ret = ra_hdc_lite_ctx_init(&rdma_handle, phy_id, rdev_index);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 10, 0);
    ret = ra_hdc_lite_mutex_init(&rdma_handle, phy_id);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 1, -1);
    ret = ra_hdc_lite_mutex_init(&rdma_handle, phy_id);
    EXPECT_INT_EQ(ret, -258);
}

struct rdma_lite_cq *stub_ra_rdma_lite_create_cq(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_cq_attr *lite_cq_attr)
{
    static cnt = 0;
    static struct rdma_lite_cq lite_cq = {0};

    cnt++;
    if (cnt == 1) {
        return NULL;
    }
    else {
        return &lite_cq;
    }
}

void rc_ra_hdc_lite_qp_create()
{
    struct ra_qp_handle qp_hdc = {0};
    struct rdma_lite_qp_cap cap = {0};
    struct rdma_lite_cq lite_cq = {0};
    struct rdma_lite_qp lite_qp = {0};
    struct ra_rdma_handle rdma_handle = {0};
    struct rdma_lite_cq_attr lite_send_cq_attr = {0};
    struct rdma_lite_cq_attr lite_recv_cq_attr = {0};
    struct rdma_lite_qp_attr lite_qp_attr = {0};
    struct rdma_lite_wc lite_wc = {0};
    unsigned int api_version = 0;
    int ret = 0;

    qp_hdc.list.next = &(qp_hdc.list);
    qp_hdc.list.prev = &(qp_hdc.list);
    rdma_handle.qp_list.next = &(rdma_handle.qp_list);
    rdma_handle.qp_list.prev = &(rdma_handle.qp_list);
    qp_hdc.support_lite = 1;
    qp_hdc.qp_mode = 2;
    rdma_handle.support_lite = 1;
    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = 1;
    cap.max_send_wr = RA_QP_TX_DEPTH;
    cap.max_recv_wr = RA_QP_TX_DEPTH;

    mocker_clean();
    mocker(ra_hdc_lite_get_cq_qp_attr, 10, 0);
    mocker(ra_hdc_lite_init_mem_pool, 10, 0);
    mocker(ra_rdma_lite_destroy_cq, 10, 0);
    mocker(ra_hdc_lite_deinit_mem_pool, 10, 0);
    mocker(ra_rdma_lite_create_cq, 1, NULL);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(ra_hdc_lite_get_cq_qp_attr, 10, 0);
    mocker(ra_hdc_lite_init_mem_pool, 10, 0);
    mocker(ra_rdma_lite_destroy_cq, 10, 0);
    mocker(ra_hdc_lite_deinit_mem_pool, 10, 0);
    mocker_invoke(ra_rdma_lite_create_cq, stub_ra_rdma_lite_create_cq, 2);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(ra_hdc_lite_get_cq_qp_attr, 10, 0);
    mocker(ra_hdc_lite_init_mem_pool, 10, 0);
    mocker(ra_rdma_lite_destroy_cq, 10, 0);
    mocker(ra_hdc_lite_deinit_mem_pool, 10, 0);
    mocker(ra_rdma_lite_create_cq, 2, &lite_cq);
    mocker(ra_rdma_lite_create_qp, 1, NULL);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(ra_hdc_lite_get_cq_qp_attr, 10, 0);
    mocker(ra_hdc_lite_init_mem_pool, 10, 0);
    mocker(ra_rdma_lite_destroy_qp, 10, 0);
    mocker(ra_rdma_lite_destroy_cq, 10, 0);
    mocker(ra_hdc_lite_deinit_mem_pool, 10, 0);
    mocker(ra_rdma_lite_create_cq, 10, &lite_cq);
    mocker(ra_rdma_lite_create_qp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, NULL);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -12);

    mocker_clean();
    mocker(ra_hdc_lite_get_cq_qp_attr, 10, 0);
    mocker(ra_hdc_lite_init_mem_pool, 10, 0);
    mocker(ra_rdma_lite_destroy_cq, 10, 0);
    mocker(ra_hdc_lite_deinit_mem_pool, 10, 0);
    mocker(ra_rdma_lite_create_cq, 10, &lite_cq);
    mocker(ra_rdma_lite_create_qp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, &lite_wc);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_hdc_lite_qp_attr_init, 10, 0);
    mocker(memcpy_s, 10, 0);
    ret = ra_hdc_lite_get_cq_qp_attr(&qp_hdc, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_rdma_lite_init_mem_pool, 10, 0);
    ret = ra_hdc_lite_init_mem_pool(&qp_hdc, &cap, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(ra_rdma_lite_deinit_mem_pool, 10, 0);
    ra_hdc_lite_deinit_mem_pool(&rdma_handle, &qp_hdc);

    mocker_clean();
    ret = ra_rdma_lite_get_api_version();
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_get_client_socket_err_info()
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

void tc_ra_get_server_socket_err_info()
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

void tc_ra_hdc_recv_handle_send_pkt_unsuccess()
{
    mocker_clean();
    mocker(dl_hal_hdc_recv, 1, 1);
    mocker(dl_drv_hdc_alloc_msg, 1, 0);
    mocker(dl_drv_hdc_free_msg, 1, 1);
    mocker(dl_drv_hdc_session_close, 1, 1);
    mocker(rs_set_ctx, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ra_hdc_recv_handle_send_pkt(0);
    mocker_clean();
}

void tc_ra_hdc_tlv_request()
{
    struct ra_tlv_handle tlv_handle_tmp = {0};
    struct tlv_msg send_msg = {0};
    struct tlv_msg recv_msg = {0};
    int ret = 0;

    tlv_handle_tmp.init_info.phy_id = 0;
    tlv_handle_tmp.module_type = TLV_MODULE_TYPE_NSLB;
    send_msg.length = TC_TLV_HDC_MSG_SIZE;
    send_msg.type = 0;
    send_msg.data = (char *)malloc(TC_TLV_HDC_MSG_SIZE);
    int i = 0;
    for (i = 0; i < TC_TLV_HDC_MSG_SIZE; i++) {
        send_msg.data[i] = (char)(i % 256);
    }

    mocker(ra_hdc_process_msg, 100, 0);
    ret = ra_hdc_tlv_request(&tlv_handle_tmp, &send_msg, &recv_msg);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(send_msg.data);
    send_msg.data = NULL;
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

void tc_ra_rs_remap_mr(void)
{
    union op_remap_mr_data op_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union op_remap_mr_data) + sizeof(struct msg_head);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct msg_head), &op_data, sizeof(union op_remap_mr_data));
    memcpy(out_buf + sizeof(struct msg_head), &op_data, sizeof(union op_remap_mr_data));
    mocker(rs_remap_mr, 2, 1);
    ret = ra_rs_remap_mr(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_get_tls_enable()
{
    struct ra_info info = {0};
    bool tls_enable;
    int ret;

    info.mode = NETWORK_PEER_ONLINE;
    ret = ra_get_tls_enable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    mocker(ra_hdc_process_msg, 1, 0);
    ret = ra_get_tls_enable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.phy_id = RA_MAX_PHY_ID_NUM;
    ret = ra_get_tls_enable(&info, &tls_enable);
    EXPECT_INT_EQ(128303, ret);

}

void tc_ra_get_sec_random()
{
    struct ra_info info = {0};
    unsigned int value = 0;
    int ret;

    mocker_clean();
    info.mode = NETWORK_PEER_ONLINE;
    ret = ra_get_sec_random(&info, NULL);
    EXPECT_INT_EQ(128303, ret);

    info.mode = NETWORK_OFFLINE;
    ret = ra_get_sec_random(&info, &value);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    mocker(ra_hdc_process_msg, 10, 0);
    mocker(ra_peer_get_sec_random, 10, -1);
    ret = ra_get_sec_random(&info, &value);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_rs_get_tls_enable()
{
    unsigned int size = sizeof(union op_get_tls_enable_data) + sizeof(struct msg_head);
    union op_get_tls_enable_data op_data  = {{0}};


    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct msg_head), &op_data , sizeof(union op_get_tls_enable_data));
    memcpy(out_buf + sizeof(struct msg_head), &op_data, sizeof(union op_get_tls_enable_data));
    ret = ra_rs_get_tls_enable(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_sec_random()
{
    unsigned int size = sizeof(union op_get_sec_random_data) + sizeof(struct msg_head);
    union op_get_sec_random_data op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct msg_head), &op_data , sizeof(union op_get_sec_random_data));
    memcpy(out_buf + sizeof(struct msg_head), &op_data, sizeof(union op_get_sec_random_data));
    ret = ra_rs_get_sec_random(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_socket_credit_add()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_socket_credit_add(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(-22, ret);

    return;
}

void tc_ra_rs_socket_batch_abort()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_socket_batch_abort(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_hdc_socket_batch_abort()
{
    struct SocketListenInfoT conn[1];
    int ret;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_batch_abort(1, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, -1);
    ret = ra_hdc_socket_batch_abort(1, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_hdc_socket_accept_credit_add()
{
    struct SocketListenInfoT conn[1];
    int ret;
    mocker(ra_get_socket_listen_info, 1, -1);
    ret = ra_hdc_socket_accept_credit_add(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();

    mocker(ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_socket_accept_credit_add(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();
}

void tc_ra_get_hccn_cfg()
{
    struct ra_info info = {0};
    char *value = calloc(1, 2048);
    unsigned int val_len = 2048;
    int ret;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    ret = ra_get_hccn_cfg(NULL, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    val_len = 1024;
    ret = ra_get_hccn_cfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phy_id = 64;
    info.mode = NETWORK_OFFLINE;
    val_len = 2048;
    ret = ra_get_hccn_cfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phy_id = 0;
    mocker(ra_hdc_process_msg, 10, 0);
    ret = ra_get_hccn_cfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(value);
}

void tc_ra_rs_get_hccn_cfg()
{
    unsigned int size = sizeof(union op_get_hccn_cfg_data) + sizeof(struct msg_head);
    union op_get_hccn_cfg_data op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct msg_head), &op_data , sizeof(union op_get_hccn_cfg_data));
    memcpy(out_buf + sizeof(struct msg_head), &op_data, sizeof(union op_get_hccn_cfg_data));
    ret = ra_rs_get_hccn_cfg(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_save_snapshot_input()
{
    enum save_snapshot_action action;
    struct ra_info *info = NULL;
    int ret;

    ret = ra_save_snapshot(info, action);
    EXPECT_INT_NE(0, ret);

    ret = ra_restore_snapshot(info);
    EXPECT_INT_NE(0, ret);

    info = calloc(1,sizeof(struct ra_info));
    info->phy_id = RA_MAX_PHY_ID_NUM;
    info->mode = NETWORK_PEER_ONLINE;
    ret = ra_save_snapshot(info, action);
    EXPECT_INT_EQ(0, ret);

    ret = ra_restore_snapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->phy_id = 0;
    action = SAVE_SNAPSHOT_ACTION_POST_PROCESSING + 1;
    ret = ra_save_snapshot(info, action);
    EXPECT_INT_NE(0, ret);

    info->mode = NETWORK_PEER_ONLINE;
    info->phy_id = 0;
    ret = ra_save_snapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = ra_restore_snapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->mode = NETWORK_OFFLINE + 1;
    ret = ra_save_snapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = ra_restore_snapshot(info);
    EXPECT_INT_NE(0, ret);

    free(info);
    info = NULL;
}

void tc_ra_save_snapshot_pre()
{
    struct ra_info info = {0};
    struct ra_rdma_handle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabled_lite_thread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notify_type = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(ra_rdev_init_check_ip, 10, 0);
    mocker((stub_fn_t)hdc_send_recv_pkt, 10, 0);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 10);
    mocker_invoke(ra_hdc_get_lite_support, ra_hdc_get_lite_support_stub, 10);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    g_interface_version = 1;
    ret = ra_rdev_init_v2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(ret, 0);

    info.mode = NETWORK_OFFLINE;
    rdma_handle->support_lite = LITE_NOT_SUPPORT;
    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->support_lite = LITE_ALIGN_4KB;
    rdma_handle->thread_status = LITE_THREAD_STATUS_RUNNING;
    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_POST_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = ra_rdev_deinit(rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_ra_save_snapshot_post()
{
    struct ra_info info = {0};
    struct ra_rdma_handle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabled_lite_thread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notify_type = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(ra_rdev_init_check_ip, 10, 0);
    mocker((stub_fn_t)hdc_send_recv_pkt, 10, 0);
    mocker_invoke(ra_hdc_get_interface_version, ra_get_interface_version_stub, 10);
    mocker_invoke(ra_hdc_get_lite_support, ra_hdc_get_lite_support_stub, 10);
    mocker(ra_hdc_notify_base_addr_init, 10, 0);
    g_interface_version = 1;
    ret = ra_rdev_init_v2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    ret = ra_restore_snapshot(&info);
    EXPECT_INT_EQ(128300, ret);

    rdma_handle->support_lite = LITE_NOT_SUPPORT;
    ret = ra_restore_snapshot(&info);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->thread_status = LITE_THREAD_STATUS_RUNNING;
    rdma_handle->support_lite = LITE_ALIGN_4KB;
    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    // delay do not cover SUSPEND
    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = ra_save_snapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0 ,ret);

    ret = ra_restore_snapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_restore_snapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_rdev_deinit(rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_hdc_async_del_req_handle()
{
    pthread_mutex_t req_mutex;
    pthread_mutex_init(&req_mutex, NULL);

    struct ra_list_head list1 = {0};

    RA_INIT_LIST_HEAD(&list1);
    ra_hw_async_del_list(&list1, &req_mutex);
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

void tc_ra_hdc_uninit_async()
{
    ra_hdc_uninit_async();
}

void tc_ra_hdc_qp_create_with_attrs()
{
    struct ra_rdma_handle rdma_handle = {0};
    struct QpExtAttrs ext_attrs = {0};
    struct AiQpInfo info = {0};
    void *qp_handle = NULL;
    int ret = 0;

    mocker(memcpy_s, 1, -1);
    ret = ra_hdc_qp_create_with_attrs(&rdma_handle, &ext_attrs, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = ra_hdc_ai_qp_create(&rdma_handle, &ext_attrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = ra_hdc_ai_qp_create_with_attrs(&rdma_handle, &ext_attrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();
}
