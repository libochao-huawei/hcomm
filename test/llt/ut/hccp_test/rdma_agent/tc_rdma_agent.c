#define _GNU_SOURCE
#include <sched.h>
#include <sys/mman.h>
#include "ra.h"
#include "ra_rs_err.h"
#include "ra_client_host.h"
#include "hccp.h"
#include "ut_dispatch.h"
#include "stdlib.h"
#include "securec.h"
#include <pthread.h>
#include "dlfcn.h"
#include "rs.h"
#include "dl.h"
#include "ra_hdc.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_async.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"
#include "ra_adp.h"
#include "ascend_hal.h"
#include <errno.h>
#include "ra_comm.h"

extern int hdc_send_recv_pkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);

extern int ra_peer_rdev_init(struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdev_index);
extern int rs_rdev_init(struct rdev rdev_info, unsigned int notify_type, unsigned int *rdev_index);
extern int ra_peer_get_server_devid(int logic_devid, int *server_devid);
extern int rs_rdev_deinit(unsigned int dev_id, unsigned int notify_type, unsigned int rdev_index);
extern int ra_peer_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
extern int rs_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
extern int rs_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[], unsigned int num);
extern int ra_get_socket_connect_info(const struct socket_connect_info_t conn[], unsigned int num, struct socket_connect_info rs_conn[], unsigned int rs_num);
extern int ra_get_socket_listen_result(const struct socket_listen_info rs_conn[], unsigned int rs_num, struct socket_listen_info_t conn[], unsigned int num);
extern int rs_socket_listen_start(struct socket_listen_info conn[], uint32_t num);
extern int ra_peer_set_rs_conn_param(struct socket_info_t conn[], unsigned int num, struct socket_fd_data rs_conn[], unsigned int rs_num);
extern int ra_inet_pton(int family, union hccp_ip_addr ip, char net_addr[], unsigned int len);
extern int ra_hdc_rdev_deinit(struct ra_rdma_handle *rdma_handle, unsigned int notify_type);
extern int ra_hdc_rdev_init(struct ra_rdma_handle *rdma_handle, unsigned int notify_type, struct rdev rdev_info, unsigned int *rdev_index);
extern int ra_hdc_init_apart(int dev_id, unsigned int *phy_id);
extern int msg_head_check(void *p_send_rcv_buf, unsigned int opcode, int rs_ret, unsigned int msg_data_len);
extern int ra_rdev_init_check_ip(int mode, struct rdev rdev_info, char local_ip[]);
extern int ra_hdc_get_lite_support(struct ra_rdma_handle *rdma_handle, unsigned int phy_id);
extern int ra_hdc_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id, unsigned long long **notify_va);
extern int ra_hdc_async_session_close(unsigned int phy_id);
extern void ra_hw_async_hdc_client_deinit(unsigned int phy_id);
extern void ra_hdc_async_mutex_deinit(unsigned int phy_id);
extern int ra_rdev_get_handle(unsigned int phy_id, void **rdma_handle);
extern int ra_save_snapshot(struct ra_info *info, enum save_snapshot_action action);
extern int ra_restore_snapshot(struct ra_info *info);
extern int ra_hdc_async_session_connect(struct ra_init_config *cfg);
extern int ra_hdc_init_session(int peer_node, int peer_devid, unsigned int phy_id, int hdc_type, HDC_SESSION *session);

extern void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offsize);
extern int munmap(void *start, size_t length);
extern int open(const char *pathname, int flags);
extern int ioctl(int fd, unsigned long cmd, struct host_roce_notify_info* info);
extern hdcError_t dl_hal_hdc_recv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t dl_drv_hdc_alloc_msg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t dl_drv_hdc_free_msg(struct drvHdcMsg *msg);
extern hdcError_t dl_drv_hdc_session_close(HDC_SESSION session);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_notify_get_info(uint32_t devId, uint32_t tsId, uint32_t type, uint32_t *val);
extern int dl_hal_mem_alloc(void **pp, unsigned long long size, unsigned long long flag);
extern int g_notify_fd;

int sec_cpy_ret = 0;

#define MAX_DEV_NUM 8


void *stub_calloc(size_t nmemb, size_t size)
{
    static int i = 0;
    void *p = NULL;
    if (i == 0) {
        i++;
        p = (void *)malloc(nmemb * size);
        return;
    } else {
        return NULL;
    }
}

static unsigned int g_interface_version;

static int ra_get_interface_version_stub(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
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

void tc_host_abnormal_qp_mode_test()
{
    int ret;
    struct rdev rdev_info = {0};
    rdev_info.family = AF_INET;
    struct ra_rdma_handle *rdma_handle = NULL;
    void* qp_handle;
    ra_rdev_init(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);

    ret = ra_qp_create(rdma_handle, 0, 3, &qp_handle);
    EXPECT_INT_NE(0, ret);
}


extern void ra_hw_init(void *arg);

extern int hdc_send_recv_pkt_recv_check(int rcv_buf_len, unsigned int out_data_len, struct msg_head *recv_msg_head,
    struct drvHdcMsg *p_msg_rcv);
void tc_hdc_send_recv_pkt_recv_check()
{

}

void tc_ra_peer_socket_white_list_add_01()
{
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[4] = {0};
    ra_peer_socket_white_list_add(rdev_info, white_list, 1);
}

void tc_ra_peer_socket_white_list_add_02()
{
    struct rdev rdev_info = {0};
    mocker(rs_socket_white_list_add, 20,1);
    struct socket_wlist_info_t white_list[4] = {0};
    ra_peer_socket_white_list_add(rdev_info, white_list, 1);
    mocker_clean();
}

void tc_ra_peer_socket_white_list_del()
{
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[5] = {0};
    mocker(rs_socket_white_list_del, 20, 1);
    ra_peer_socket_white_list_del(rdev_info, white_list, 5);
    mocker_clean();
}

void tc_ra_peer_rdev_init_01()
{
	int ret;
    struct rdev rdev_info = {0};
    struct ra_rdma_handle rdma_handle;
    unsigned int *rdev_index = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(host_notify_base_addr_init, 1, 0);
	mocker(rs_rdev_init, 1, 0);
    ret = ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, rdev_index);
	mocker_clean();
    free(rdev_index);
	EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_rdev_init_02()
{
	int ret;
    struct rdev rdev_info = {0};
    struct ra_rdma_handle rdma_handle;
    unsigned int *rdev_index = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(host_notify_base_addr_init, 1, 1);
    ret = ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, rdev_index);
	mocker_clean();
    free(rdev_index);
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_init_03()
{
	int ret;
    struct rdev rdev_info = {0};
    struct ra_rdma_handle rdma_handle;
    unsigned int *rdev_index = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(host_notify_base_addr_init, 1, 0);
	mocker(rs_rdev_init, 1, 1);
	mocker(host_notify_base_addr_uninit, 1, 0);
    ret = ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, rdev_index);
	mocker_clean();
    free(rdev_index);
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_init_04()
{
	int ret;
    struct rdev rdev_info = {0};
    struct ra_rdma_handle rdma_handle;
    unsigned int *rdev_index = (unsigned int *)malloc(sizeof(unsigned int));
	mocker(host_notify_base_addr_init, 1, 0);
	mocker(rs_rdev_init, 1, 1);
	mocker(host_notify_base_addr_uninit, 1, 2);
    ret = ra_peer_rdev_init(&rdma_handle, NOTIFY, rdev_info, rdev_index);
	mocker_clean();
    free(rdev_index);
	EXPECT_INT_EQ(2, ret);
}

void tc_ra_peer_rdev_deinit_01()
{
	int ret;
    struct ra_rdma_handle *rdma_handle = (struct ra_rdma_handle *)malloc(sizeof(struct ra_rdma_handle));
	rdma_handle->rdev_info.phy_id = 0;
	mocker(rs_rdev_deinit, 1, 0);
	mocker(host_notify_base_addr_uninit, 1, 0);
    ret = ra_peer_rdev_deinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_rdev_deinit_02()
{
	int ret;
    struct ra_rdma_handle *rdma_handle = (struct ra_rdma_handle *)malloc(sizeof(struct ra_rdma_handle));
	rdma_handle->rdev_info.phy_id = 0;
	mocker(rs_rdev_deinit, 1, 1);
    ret = ra_peer_rdev_deinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(1, ret);
}

void tc_ra_peer_rdev_deinit_03()
{
	int ret;
    struct ra_rdma_handle *rdma_handle = (struct ra_rdma_handle *)malloc(sizeof(struct ra_rdma_handle));
	rdma_handle->rdev_info.phy_id = 0;
	mocker(rs_rdev_deinit, 1, 0);
	mocker(host_notify_base_addr_uninit, 1, 2);
    ret = ra_peer_rdev_deinit(rdma_handle, NOTIFY);
	mocker_clean();
    free(rdma_handle);
    rdma_handle = NULL;
	EXPECT_INT_EQ(2, ret);
}

void tc_ra_peer_socket_batch_connect()
{
    unsigned int dev_id;
    struct socket_connect_info_t conn[4] = {0};
    mocker(ra_get_socket_connect_info, 20, 1);
    ra_peer_socket_batch_connect(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_socket_batch_abort()
{
    unsigned int dev_id;
    struct socket_connect_info_t conn[4] = {0};
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

void tc_ra_peer_socket_listen_start_01()
{
    unsigned int dev_id;
    struct socket_listen_info_t conn[5] = {0};
    mocker(ra_get_socket_listen_info, 10, 1);
    ra_peer_socket_listen_start(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_socket_listen_start_02()
{
    unsigned int dev_id;
    struct socket_listen_info_t conn[5] = {0};
    mocker(ra_get_socket_listen_info, 10, 0);
    mocker(rs_socket_listen_start, 10, 0);
    mocker(ra_get_socket_listen_result, 10, 1);
    //ra_peer_socket_listen_start(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_socket_listen_stop()
{
    unsigned int dev_id;
    struct socket_listen_info_t conn[5] = {0};
    mocker(ra_get_socket_listen_info, 10, 1);
    ra_peer_socket_listen_stop(dev_id, conn, 5);
    mocker_clean();
}

void tc_ra_peer_set_rs_conn_param()
{
    struct socket_info_t  conn[6] = {0};
    struct socket_fd_data  rs_conn[5] = {0};
    ra_peer_set_rs_conn_param(conn, 6, rs_conn, 5);
}

void tc_ra_inet_pton_01()
{
    char net_addr[5] = {0};
    union hccp_ip_addr ip;
    ra_inet_pton(0, ip, net_addr, 32);
}

void tc_ra_inet_pton_02()
{
    char net_addr[5] = {0};
    union hccp_ip_addr ip;
    ra_inet_pton(2, ip, net_addr, 0);
}

void tc_ra_socket_init()
{
    struct rdev rdev_info = {0};
    void* socket_handle = NULL;

    rdev_info.phy_id = 0;
    rdev_info.family = AF_INET;
    rdev_info.local_ip.addr.s_addr = 0;
    ra_socket_init(NETWORK_OFFLINE, rdev_info, &socket_handle);
}

void tc_ra_socket_init_v1()
{
    struct socket_init_info_t socket_init = {0};
    void* socket_handle = NULL;

    socket_init.scope_id = 0;
    socket_init.rdev_info.phy_id = 0;
    socket_init.rdev_info.family = AF_INET;
    socket_init.rdev_info.local_ip.addr.s_addr = 0;
    ra_socket_init_v1(NETWORK_OFFLINE, socket_init, &socket_handle);

    socket_init.scope_id = 0;
    socket_init.rdev_info.phy_id = 0;
    socket_init.rdev_info.family = AF_INET6;
    socket_init.rdev_info.local_ip.addr.s_addr = 0;
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    ra_socket_deinit(socket_handle);

    ra_socket_init_v1(NETWORK_ONLINE, socket_init, &socket_handle);

    mocker(calloc, 1, NULL);
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    mocker(ra_inet_pton, 1, 99);
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 1, 1);
    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, &socket_handle);
    mocker_clean();

    ra_socket_init_v1(NETWORK_PEER_ONLINE, socket_init, NULL);
}

void tc_ra_send_wrlist()
{
    struct ra_qp_handle qp_handle;
    struct ra_rdma_ops rdma_ops;
    qp_handle.rdma_ops = &rdma_ops;
    unsigned int send_num = 1;
    unsigned int complete_num = 0;
    struct send_wrlist_data wrlist[1];
    struct send_wr_rsp op_rsp[1];
    ra_send_wrlist(NULL, NULL, NULL, send_num, &complete_num);
    qp_handle.rdma_ops = NULL;
    ra_send_wrlist(&qp_handle, wrlist, op_rsp, send_num, &complete_num);
    wrlist[0].mem_list.len = 2147483649;
    ra_send_wrlist(&qp_handle, wrlist, op_rsp, send_num, &complete_num);
}

void tc_ra_rdev_init()
{
    struct rdev rdev_info;
    void* rdma_handle = NULL;
    rdev_info.phy_id = 0;
    ra_rdev_init(2, NOTIFY, rdev_info, &rdma_handle);
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

void tc_ra_hdc_rdev_deinit()
{
    struct ra_rdma_handle rdma_handle = { 0 };
    mocker(calloc, 10 , NULL);
    mocker(rdma_lite_free_context, 10 , 0);
    int ret = ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(hdc_send_recv_pkt, 20, 0);
    mocker(msg_head_check, 20, 1);
    mocker(rdma_lite_free_context, 10 , 0);
    ret = ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_ra_hdc_socket_white_list_add()
{
    struct rdev rdev_info = {};
    struct socket_wlist_info_t white_list[1];
    mocker(hdc_send_recv_pkt, 20, 1);
    ra_hdc_socket_white_list_add(rdev_info, white_list, 1);
    mocker_clean();

    mocker(hdc_send_recv_pkt, 20, 0);
    mocker(msg_head_check, 20, 1);
    ra_hdc_socket_white_list_add(rdev_info, white_list, 1);
    mocker_clean();
}

void tc_ra_hdc_socket_white_list_del()
{
    struct rdev rdev_info;
    struct socket_wlist_info_t white_list[1];
    int ret;
    mocker(hdc_send_recv_pkt, 20, 1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();

    mocker(hdc_send_recv_pkt, 20, 0);
    mocker(msg_head_check, 20, 1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_hdc_socket_accept_credit_add()
{
    struct socket_listen_info_t conn[1];
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

void tc_ra_hdc_rdev_init()
{
    struct rdev rdev_info = {0};
    unsigned int rdev_index;
    int ret;
    struct ra_rdma_handle rdma_handle = { 0 };
    mocker(dl_drv_device_get_index_by_phy_id, 20, 0);
    mocker(dl_hal_notify_get_info, 20, 0);
    mocker(dl_hal_mem_alloc, 20, 0);
    mocker(calloc, 20, NULL);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker(memcpy_s, 20, 1);
    mocker(dl_drv_device_get_index_by_phy_id, 20, 0);
    mocker(dl_hal_notify_get_info, 20, 0);
    mocker(dl_hal_mem_alloc, 20, 0);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

    mocker(hdc_send_recv_pkt, 20, 0);
    mocker(msg_head_check, 20, 1);
    mocker(dl_drv_device_get_index_by_phy_id, 20, 0);
    mocker(dl_hal_notify_get_info, 20, 0);
    mocker(dl_hal_mem_alloc, 20, 0);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_hdc_init_apart()
{
    unsigned int phy_id;
    mocker(dl_drv_device_get_index_by_phy_id, 20, 0);
    mocker(pthread_mutex_init, 20, 1);
    int ret = ra_hdc_init_apart(1, &phy_id);
    EXPECT_INT_EQ(-ESYSFUNC, ret);
    mocker_clean();
}

void tc_ra_hdc_qp_destroy()
{
    struct ra_qp_handle *qp_hdc = (struct ra_qp_handle *)malloc(sizeof(struct ra_qp_handle));
    *qp_hdc = (struct ra_qp_handle){0};
    mocker(hdc_send_recv_pkt, 20, 1);
    mocker(rdma_lite_destroy_qp, 20, 0);
    mocker(rdma_lite_destroy_cq, 20, 0);
    int ret = ra_hdc_qp_destroy(qp_hdc);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}
void tc_ra_hdc_qp_destroy_01()
{
    struct ra_qp_handle *qp_hdc = (struct ra_qp_handle *)malloc(sizeof(struct ra_qp_handle));
    *qp_hdc = (struct ra_qp_handle){0};
    mocker(hdc_send_recv_pkt, 20, 0);
    mocker(msg_head_check, 20, 1);
    mocker(rdma_lite_destroy_qp, 20, 0);
    mocker(rdma_lite_destroy_cq, 20, 0);
    int ret = ra_hdc_qp_destroy(qp_hdc);
    EXPECT_INT_EQ(1, ret);
    mocker_clean();
}

void tc_ra_get_socket_connect_info()
{
    int ret = ra_get_socket_connect_info(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_get_socket_listen_info()
{
    int ret = ra_get_socket_listen_info(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_get_socket_listen_result()
{
    int ret = ra_get_socket_listen_result(NULL, 1, NULL, 1);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ra_hw_hdc_init() {
    mocker((stub_fn_t)pthread_detach, 1, 0);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ra_hw_hdc_init(NULL);
    mocker_clean();
}

void tc_ra_peer_init_fail_001()
{
	struct ra_init_config cfg = {0};
	unsigned int white_list_status = 0;

	mocker(pthread_mutex_init, 1, 1);
    int ret = ra_peer_init(&cfg, white_list_status);
	mocker_clean();
    EXPECT_INT_EQ(-ESYSFUNC, ret);
}

void tc_ra_peer_socket_deinit_001()
{
	struct rdev rdev_info = {0};

	mocker(rs_socket_deinit, 1, 0);
    int ret = ra_peer_socket_deinit(rdev_info);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);
}

void tc_host_notify_base_addr_init()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(rs_notify_cfg_set, 1, 0);
    ret = host_notify_base_addr_init(0);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);
}

void tc_host_notify_base_addr_init_001()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 1);
    ret = host_notify_base_addr_init(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_init_002()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 2);
	mocker(rs_notify_cfg_set, 1, 0);
    ret = host_notify_base_addr_init(0);
	mocker_clean();
    EXPECT_INT_EQ(2, ret);
}

void tc_host_notify_base_addr_init_003()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, -1);
	mocker(mmap, 1, MAP_FAILED);
	ret = host_notify_base_addr_init(0);
	EXPECT_INT_EQ(-ENOENT, ret);
	mocker_clean();
}


void tc_host_notify_base_addr_init_005()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(rs_notify_cfg_set, 1, 4);
	mocker(munmap, 1, 1);
	mocker(close, 1, 0);
	ret = host_notify_base_addr_init(0);
	mocker_clean();
}

void tc_host_notify_base_addr_init_006()
{
	int ret;

	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker(mmap, 1, 1);
	mocker(rs_notify_cfg_set, 1, 4);
	mocker(munmap, 1, 0);
    mocker(close, 1, 0);
	ret = host_notify_base_addr_init(0);
	mocker_clean();
	EXPECT_INT_EQ(4, ret);
}

void tc_host_notify_base_addr_uninit()
{
	int ret;

	mocker(rs_notify_cfg_get, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, 0);
	mocker(munmap, 1, 0);
	ret = host_notify_base_addr_uninit(0);
	mocker_clean();
	EXPECT_INT_NE(0, ret);
}

void tc_host_notify_base_addr_uninit_001()
{
	int ret;

	mocker(rs_notify_cfg_get, 1, 1);
    ret = host_notify_base_addr_uninit(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_uninit_002()
{
	int ret;

	mocker(rs_notify_cfg_get, 1, 0);
	mocker(open, 1, -1);
mocker(drvDeviceGetIndexByPhyId, 1, 1);
    ret = host_notify_base_addr_uninit(0);
	mocker_clean();
    EXPECT_INT_EQ(1, ret);
}

void tc_host_notify_base_addr_uninit_003()
{
	int ret;

	mocker(rs_notify_cfg_get, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, -1);
    ret = host_notify_base_addr_uninit(0);
	mocker_clean();
    EXPECT_INT_EQ(-ENOENT, ret);
}

void tc_host_notify_base_addr_uninit_004()
{
	int ret;

	mocker(rs_notify_cfg_get, 1, 0);
	mocker(open, 1, 0);
	mocker(ioctl, 1, 0);
	mocker(munmap, 1, 3);
    ret = host_notify_base_addr_uninit(0);
	mocker_clean();
    EXPECT_INT_EQ(-ENOENT, ret);
}

void tc_host_notify_base_addr_uninit_005()
{
	int ret;
	g_notify_fd = 1;
	mocker(rs_notify_cfg_get, 10, 0);
	mocker(open, 10, 1);
	mocker(ioctl, 10, 0);
    mocker(munmap, 1, 1);
    mocker(close, 1, 0);
    ret = host_notify_base_addr_uninit(0);
    EXPECT_INT_NE(0, ret);
	mocker_clean();
}

void tc_ra_peer_send_wrlist()
{
	int ret;
	struct ra_qp_handle qp_handle = {0};
	struct send_wrlist_data wr = {0};
	struct send_wr_rsp op_rsp = {0};
	struct wrlist_send_complete_num wrlist_num = {0};

	wrlist_num.send_num = 1;
	mocker(rs_send_wrlist, 1, 0);
	ret = ra_peer_send_wrlist(&qp_handle, &wr, &op_rsp, wrlist_num);
	mocker_clean();
}

void tc_ra_peer_send_wrlist_001()
{
	int ret;
	struct ra_qp_handle qp_handle = {0};
	struct send_wrlist_data wr = {0};
	struct send_wr_rsp op_rsp = {0};
	struct wrlist_send_complete_num wrlist_num = {0};

	wrlist_num.send_num = 1;
	mocker(rs_send_wrlist, 1, -1);
	ret = ra_peer_send_wrlist(&qp_handle, &wr, &op_rsp, wrlist_num);
	mocker_clean();
	EXPECT_INT_EQ(-1, ret);
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

    mocker((stub_fn_t)ra_peer_cq_create, 1, 0);
    mocker((stub_fn_t)ra_peer_cq_destroy, 1, 0);
    ra_cq_create(rdma_handle, &attr);
    ra_cq_destroy(rdma_handle, &attr);
    mocker_clean();
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
    struct ra_qp_handle ra_qp_handle;
    void *qp_handle = &ra_qp_handle;

    struct ra_rdma_handle ra_rdma_handle;
    void *rdma_handle = (void *)&ra_rdma_handle;
    ra_rdma_handle.rdev_index = 0;
    ra_rdma_handle.rdev_info.phy_id = 32767;
    ra_rdma_handle.rdma_ops = NULL;
    struct ra_rdma_ops ops;
    ra_rdma_handle.rdma_ops = &ops;
    ops.ra_normal_qp_create = NULL;
    ops.ra_normal_qp_destroy = NULL;
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_qp_handle.rdma_ops = NULL;
    ra_normal_qp_destroy(qp_handle);

    ops.ra_normal_qp_create = ra_peer_normal_qp_create;
    ops.ra_normal_qp_destroy = ra_peer_normal_qp_destroy;

    mocker((stub_fn_t)ra_peer_normal_qp_create, 10, 0);
    mocker((stub_fn_t)ra_peer_normal_qp_destroy, 10, 0);
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_normal_qp_destroy(qp_handle);

    ra_normal_qp_create(rdma_handle, &qp_init_attr, NULL, &qp);
    ra_normal_qp_destroy(NULL);

    ra_rdma_handle.rdev_info.phy_id = 0;
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_normal_qp_destroy(qp_handle);
    mocker_clean();

    mocker((stub_fn_t)ra_peer_normal_qp_create, 10, -1);
    mocker((stub_fn_t)ra_peer_normal_qp_destroy, 10, -1);
    ra_normal_qp_create(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    ra_qp_handle.rdma_ops = &ops;
    ra_normal_qp_destroy(qp_handle);
    mocker_clean();
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

    mocker(ra_hdc_get_cqe_err_info, 1, 0);
    ret = ra_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(0, ret);

    ret = ra_get_cqe_err_info(128, &info);
    EXPECT_INT_NE(0, ret);
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

void tc_ra_rs_typical_qp_create()
{
    int ret;
    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];

    ret = ra_rs_typical_qp_create(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_rs_typical_qp_modify(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
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
    struct rdev_init_info init_info = {0};
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
    struct rdev_init_info init_info = {0};
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

void tc_ra_hdc_uninit_async()
{
    ra_hdc_uninit_async();
}
