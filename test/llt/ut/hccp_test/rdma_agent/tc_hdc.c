/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * Description: ra hdc unit test.
 * Author:
 * Create: 2020-07-21
 */
#include "ut_dispatch.h"
#include <stdlib.h>
#include <errno.h>
#include "securec.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_hdc_lite.h"
#include "ra_hdc_tlv.h"
#include "hccp.h"
#include "ra_comm.h"
#include "ra_rs_err.h"
#include "dsmi_common_interface.h"

#define RA_QP_TX_DEPTH         32767
#define TC_TLV_HDC_MSG_SIZE    (32 * 1024) // 32KB
typedef uint32_t u32;
typedef uint16_t u16;
//typedef uint64_t u64;
typedef unsigned long long u64;
typedef signed int s32;

extern int ra_hdc_init_apart(unsigned int phy_id, unsigned int *logic_id);
extern int ra_hdc_session_close(unsigned int phy_id);
extern ra_hdc_process_msg(unsigned int opcode, unsigned int device_id, char *data, unsigned int data_size);
extern int ra_hdc_notify_cfg_set(unsigned int phy_id, unsigned long long va, unsigned long long size);
extern int ra_hdc_notify_cfg_get(unsigned int phy_id, unsigned long long *va, unsigned long long *size);
extern STATIC int ra_hdc_notify_base_addr_init(unsigned int notify_type, unsigned int phy_id, unsigned long long **notify_va);
extern void ra_hdc_lite_save_cqe_err_info(struct ra_qp_handle *qp_hdc, unsigned int status);
extern struct rdma_lite_context *ra_rdma_lite_alloc_ctx(u8 phy_id, struct dev_cap_info *cap);
extern struct rdma_lite_cq *ra_rdma_lite_create_cq(struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr);
extern struct rdma_lite_qp *ra_rdma_lite_create_qp(struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr);
extern int ra_hdc_get_lite_support(struct ra_rdma_handle *rdma_handle, unsigned int phy_id);
extern int ra_hdc_get_drv_lite_support(unsigned int phy_id, bool enabled_910a_lite, unsigned int *support);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_mem_ctl(int type, void *param_value, size_t param_value_size, void *out_value, size_t *out_size_ret);
extern int ra_rdma_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, unsigned char sl);
extern int ra_hdc_lite_post_send(struct ra_qp_handle *qp_hdc, struct lite_mr_info *local_mr,
    struct lite_mr_info *rem_mr, struct lite_send_wr *wr, struct send_wr_rsp *wr_rsp);
extern void ra_hdc_get_opcode_lite_support(unsigned int phy_id, unsigned int support_feature, int *support);
extern int ra_hdc_get_opcode_version(unsigned int phy_id, unsigned int interface_opcode,
    unsigned int *interface_version);
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
extern int ra_rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp);
extern int ra_hdc_get_tlv_recv_msg(struct tlv_msg *recv_msg, char *recv_data);
extern int ra_rdma_lite_restore_snapshot(struct rdma_lite_context *lite_ctx);
extern unsigned int ra_rdma_lite_get_api_version(void);

struct msg_head g_msg_tmp;
DLLEXPORT hdcError_t stub_drvHdcGetMsgBuffer_pid_error(struct drvHdcMsg *msg, int index,
                                             char **pBuf, int *pLen)
{
    g_msg_tmp.ret = -EPERM;
    *pBuf = &g_msg_tmp;
    *pLen = ((struct drvHdcMsgBuf *)(msg + 1))->len;
    return DRV_ERROR_NONE;
}

static unsigned int g_devid = 0;
static struct ra_init_config g_config = {
        .phy_id = 0,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
};
DLLEXPORT drvError_t stub_session_connect(int peer_node, int peer_devid, HDC_CLIENT client, HDC_SESSION *session)
{
    static HDC_SESSION g_hdc_session = 1;
    *session = g_hdc_session;
    return 0;
}
static char g_drv_recv_msg[4096];
static int g_drv_recv_msg_len = 0;
static DLLEXPORT drvError_t stub_drvHdcGetMsgBuffer(struct drvHdcMsg *msg, int index, char **pBuf, int *pLen)
{
    *pBuf = g_drv_recv_msg;
    *pLen = g_drv_recv_msg_len;
    return 0;
}

extern int dsmi_get_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t *ip_address,
                               ip_addr_t *mask_address);

DLLEXPORT drvError_t drvGetDevNum(unsigned int *num_dev);

int ra_hdc_get_all_vnic(unsigned int *vnic_ip, unsigned int num);

unsigned int g_interface_version = 0;

int stub_ra_get_interface_version(unsigned int phy_id, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

void tc_hdc_init()
{
    struct process_ra_sign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct ra_init_config config = {
        .phy_id = g_devid,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 2, 0);
    int ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);

    mocker((stub_fn_t)memset_s, 1, 0);
    ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EEXIST);

    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    ret = ra_hdc_deinit(&config);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_test_env_init()
{
    struct process_ra_sign p_ra_sign;
    p_ra_sign.tgid = 0;

    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    int ret = ra_hdc_init(&g_config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_test_env_deinit()
{
    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    int ret = ra_hdc_deinit(&g_config);
    EXPECT_INT_EQ(ret, 0);
	mocker_clean();
}

void tc_hdc_init_fail()
{
    mocker_clean();
    struct process_ra_sign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct ra_init_config config = {
        .phy_id = RA_MAX_PHY_ID_NUM,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    mocker(ra_hdc_init_apart, 1, -1);
    int ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_NE(ret, 0);

    config.phy_id = g_devid;

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_init_apart, 1, -EINVAL);
    ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -ESYSFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker((stub_fn_t)drvHdcSessionConnect, 1, -1);
    ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EPERM);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, -1);
    ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, -EPERM);
}

void tc_hdc_deinit_fail()
{
    struct ra_init_config config = {
        .phy_id = RA_MAX_PHY_ID_NUM,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    int ret;

    mocker_clean();
    mocker((stub_fn_t)drvHdcSessionClose, 10, -10);
    mocker((stub_fn_t)drvHdcClientDestroy, 10, -10);
    mocker((stub_fn_t)pthread_mutex_destroy, 10, -10);
    mocker((stub_fn_t)calloc, 1, NULL);
    config.phy_id = g_devid;
    ret = ra_hdc_deinit(&config);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();
}

void tc_hdc_socket_batch_connect()
{
    struct SocketConnectInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();

    mocker((stub_fn_t)ra_get_socket_connect_info, 10, 0);
    int ret = ra_hdc_socket_batch_connect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_batch_connect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, -1);
    ret = ra_hdc_socket_batch_connect(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, 0);
    ret = ra_hdc_socket_batch_connect(0, conn, 1);
    EXPECT_INT_EQ(ret, -22);

    mocker_clean();
}

void tc_hdc_socket_batch_close()
{
    struct SocketCloseInfoT conn[1] = {0};
    conn[0].fd_handle = calloc(sizeof(struct socket_hdc_info), 1);
    ((struct socket_hdc_info *)conn[0].fd_handle)->fd = 0;
    ((struct socket_hdc_info *)conn[0].fd_handle)->phy_id = 0;
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = ra_hdc_socket_batch_close(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    conn[0].fd_handle = calloc(sizeof(struct socket_hdc_info), 1);
    ((struct socket_hdc_info *)conn[0].fd_handle)->fd = 0;
    ((struct socket_hdc_info *)conn[0].fd_handle)->phy_id = 0;
    mocker_clean();
    mocker((stub_fn_t)calloc, 10, NULL);
    ret = ra_hdc_socket_batch_close(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    conn[0].fd_handle = calloc(sizeof(struct socket_hdc_info), 1);
    ((struct socket_hdc_info *)conn[0].fd_handle)->fd = 0;
    ((struct socket_hdc_info *)conn[0].fd_handle)->phy_id = 0;
    mocker((stub_fn_t)ra_hdc_process_msg, 10, -1);
    ret = ra_hdc_socket_batch_close(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_listen_start()
{
    int ret;
    struct SocketListenInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();
    mocker((stub_fn_t)ra_hdc_process_msg, 5, 0);
    mocker((stub_fn_t)ra_get_socket_listen_result, 10, 0);
    ret = ra_hdc_socket_listen_start(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_listen_info, 1, -1);
    ret = ra_hdc_socket_listen_start(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_listen_info, 1, 0);
    ret = ra_hdc_socket_listen_start(0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_process_msg, 1, 0);
    mocker((stub_fn_t)ra_get_socket_listen_result, 10, -1);
    ret = ra_hdc_socket_listen_start(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, -1);
    ret = ra_hdc_socket_listen_start(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_hdc_socket_batch_abort()
{
    struct SocketListenInfoT conn[1];

    mocker_clean();
    tc_hdc_test_env_init();

    mocker((stub_fn_t)ra_get_socket_connect_info, 10, 0);
    int ret = ra_hdc_socket_batch_abort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_batch_abort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, -1);
    ret = ra_hdc_socket_batch_abort(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_listen_stop()
{
    struct SocketListenInfoT conn[1];
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = ra_hdc_socket_listen_stop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, 0);

    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)ra_get_socket_connect_info, 1, -1);
    ret = ra_hdc_socket_listen_start(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)ra_get_socket_listen_info, 1, -1);
    ret = ra_hdc_socket_listen_stop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_socket_listen_stop(g_devid, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_get_sockets()
{
    struct SocketInfoT conn[1];
    conn[0].fd_handle = NULL;
    conn[0].socket_handle = calloc(sizeof(struct ra_socket_handle), 1);
    mocker_clean();
    tc_hdc_test_env_init();
    int ret = ra_hdc_get_sockets(g_devid, 0, conn, 1);
    EXPECT_INT_NE(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_get_sockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_get_sockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_get_sockets(g_devid, 0, conn, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    free(conn[0].socket_handle);
    if(conn[0].fd_handle != NULL) {
        free(conn[0].fd_handle);
        conn[0].fd_handle = NULL;
    }

}

void tc_hdc_socket_send()
{
    struct socket_hdc_info socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 10);
    struct msg_head* test_head = (struct msg_head*)g_drv_recv_msg;
    g_drv_recv_msg_len = sizeof(union op_socket_send_data) + sizeof(struct msg_head);
    test_head->opcode = RA_RS_SOCKET_SEND;
    test_head->msg_data_len = sizeof(union op_socket_send_data);
    test_head->ret = 0;

    union op_socket_send_data* data = (union op_socket_send_data*)(g_drv_recv_msg + sizeof(struct msg_head));
    data->rx_data.real_send_size = 100;

    int ret = ra_hdc_socket_send(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, 100);

    test_head->ret = -EAGAIN;
    ret = ra_hdc_socket_send(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker((stub_fn_t)drvHdcAddMsgBuffer, 10, 10);
    ret = ra_hdc_socket_send(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker((stub_fn_t)ra_hdc_session_close, 10, 0);
    tc_hdc_test_env_deinit();

    char max_send_buf[SOCKET_SEND_MAXLEN + 1] = {0};
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_send(g_devid, &socket_info, max_send_buf, sizeof(max_send_buf));
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_socket_send(g_devid, &socket_info, max_send_buf, sizeof(max_send_buf));
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

}

void tc_hdc_socket_recv()
{
    struct socket_hdc_info socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();
    mocker((stub_fn_t)ra_hdc_process_msg, 5, 0);
    mocker((stub_fn_t)memcpy_s, 10, 0);
    int ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_process_msg, 5, 0);
    mocker((stub_fn_t)memcpy_s, 10, -1);
    ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    //EXPECT_INT_EQ(ret, -ESAFEFUNC);
}

void tc_hdc_qp_create_destroy()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.support_lite = 1;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    ASSERT_ADDR_NE(qp_handle, NULL);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
	ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(ret, -1);
}

void tc_hdc_get_qp_status()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.support_lite = 1;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    ASSERT_ADDR_NE(qp_handle, NULL);
    int status = 0;
    ret = ra_hdc_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    struct ra_qp_handle test_qp_handle;
    test_qp_handle.phy_id = g_devid;
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_get_qp_status(&test_qp_handle, &status);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_qp_connect_async()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    struct socket_hdc_info socket_handle = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_qp_connect_async(qp_handle, &socket_handle);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    ret = ra_hdc_qp_connect_async(&test_qp_handle, &socket_handle);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_mr_reg()
{
    struct mr_info mr_info;
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    mr_info.addr = qp_reg;
    mr_info.access = 0;
    mr_info.lkey = 0;
    mr_info.size = sizeof(qp_reg);
    ret = ra_hdc_mr_reg(qp_handle, &mr_info);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    mr_info.addr = qp_reg;
    mr_info.access = 0;
    mr_info.lkey = 0;
    mr_info.size = sizeof(qp_reg);
    ret = ra_hdc_mr_reg(&test_qp_handle, &mr_info);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_mr_dereg()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_mr_dereg(qp_handle, qp_reg);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    ret = ra_hdc_mr_dereg(&test_qp_handle, qp_reg);
    EXPECT_INT_NE(ret, 0);
}

int stub_ra_hdc_process_msg_wrlist(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 1;
    return 0;
}

int stub_ra_hdc_process_msg_wrlist_1(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 1;
    return -ENOENT;
}

int stub_ra_hdc_process_msg_wrlist_2(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 1;
    return -3;
}

int stub_ra_hdc_process_msg_wrlist_3(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_send_wrlist_data *send_wrlist = (union op_send_wrlist_data *)data;
    send_wrlist->rx_data.complete_num = 5;
    return 0;
}

void tc_hdc_send_wrlist_v2()
{
    mocker_clean();
    struct send_wrlist_data wrlist[1];
    struct send_wr_rsp op_rsp[1];
    struct wrlist_send_complete_num wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;

    struct ra_qp_handle handle;
    handle.qp_mode = 1;

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_1, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOENT);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_2, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -3);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)calloc, 1, NULL);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();
}

void tc_hdc_send_wrlist()
{
    mocker_clean();
    struct send_wrlist_data wrlist[1];
    struct send_wr_rsp op_rsp[1];
    struct wrlist_send_complete_num wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;

    struct ra_qp_handle handle;
    handle.qp_mode = 1;

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    int ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_1, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ENOENT);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_2, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -3);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_send_wrlist(&handle, wrlist, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();


    mocker((stub_fn_t)strcpy_s, 1, -1);
    ret = ra_hdc_send_pid(0, 0);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_send_pid(0, 0);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();


    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    ret = ra_hdc_notify_cfg_set(0, 0, 0);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
    unsigned long long va = 0;
    unsigned long long size = 0;
    ret = ra_hdc_notify_cfg_get(0, &va, &size);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)drvDeviceGetIndexByPhyId, 1, -1);
    unsigned long long logic_id = 0;
    ret = ra_hdc_init_apart(0, &logic_id);
    EXPECT_INT_EQ(ret, -ENODEV);
    mocker_clean();

    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = ra_hdc_init_apart(0, &logic_id);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    char data = 0;
    ret = ra_hdc_process_msg(0, 0, &data, 0);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    tc_hdc_send_wrlist_v2();
}

void tc_hdc_send_wr()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    struct send_wr wr = {0};
    struct send_wr_rsp rsp = {0};

    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_send_wr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    struct msg_head* test_head = (struct msg_head*)g_drv_recv_msg;
    test_head->ret = -ENOENT;
    test_head->opcode = RA_RS_SEND_WR;
    g_drv_recv_msg_len = sizeof(union op_send_wr_data) + sizeof(struct msg_head);
    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 3);

    ret = ra_hdc_send_wr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, -ENOENT);

    test_head->ret = 0;
    test_head->opcode = RA_RS_QP_DESTROY;
    test_head->msg_data_len = sizeof(union op_qp_destroy_data);
    g_drv_recv_msg_len = sizeof(union op_qp_destroy_data) + sizeof(struct msg_head);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    tc_hdc_test_env_deinit();

    struct ra_qp_handle test_qp_handle;
    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_send_wr(&test_qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_send_wr(&test_qp_handle, &wr, &rsp);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_get_notify_base_addr()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    unsigned long long va;
    unsigned long long size;
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_get_notify_base_addr(qp_handle, &va, &size);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    struct ra_qp_handle test_qp_handle;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_get_notify_base_addr(&test_qp_handle, &va, &size);
    EXPECT_INT_NE(ret, 0);
}

void tc_hdc_socket_init()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};

    int ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)ra_hdc_get_all_vnic, 1, 0);
    mocker((stub_fn_t)memcpy_s, 1, -10);
    ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memset_s, 1, -1);
    ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();

    mocker((stub_fn_t)drvGetDevNum, 1, -1);
    ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    //mocker((stub_fn_t)dsmi_get_device_ip_address, 1, -1);
    //ret = ra_hdc_socket_init(rdev_info);
    //EXPECT_INT_EQ(ret, -1);
    //mocker_clean();

    mocker((stub_fn_t)ra_hdc_get_all_vnic, 1, -1);
    ret = ra_hdc_socket_init(rdev_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_hdc_socket_deinit()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    int ret = ra_hdc_socket_deinit(rdev_info);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)memset_s, 1, -10);
    ret = ra_hdc_socket_deinit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -10);
    ret = ra_hdc_socket_deinit(rdev_info);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = ra_hdc_socket_deinit(rdev_info);
    EXPECT_INT_EQ(ret, -EINVAL);
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

int stub_ra_hdc_process_rdev_init_error(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union op_rdev_init_data *rdev_init_data;

    if (opcode == RA_RS_RDEV_INIT) {
        rdev_init_data = (union rdev_init_data *)data;
        rdev_init_data->rx_data.rdev_index = 0;
    } else if (opcode == RA_RS_GET_LITE_SUPPORT) {
        return -EPROTONOSUPPORT;
    }

    return 0;
}

extern int dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
int stub_dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
    *value = (1 << 8);
    return 0;
}

void tc_hdc_rdev_init()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    unsigned int rdev_index = 0;
    struct ra_rdma_handle rdma_handle = { 0 };
    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    tc_hdc_test_env_deinit();

    mocker_clean();
	mocker((stub_fn_t)ra_hdc_notify_base_addr_init, 1, 0);
    mocker((stub_fn_t)memcpy_s, 10, -10);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
	mocker((stub_fn_t)ra_hdc_notify_base_addr_init, 1, 0);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_NE(ret, 0);

    mocker_clean();
	mocker((stub_fn_t)ra_hdc_notify_base_addr_init, 1, 0);
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -EINVAL);

	mocker_clean();
	mocker((stub_fn_t)ra_hdc_notify_base_addr_init, 1, 0);
    mocker((stub_fn_t)ra_hdc_process_msg, 1, -1);
	mocker((stub_fn_t)halMemFree, 1, -1);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
	mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init_error, 10);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, 0);
    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)ra_rdma_lite_alloc_ctx, 1, 0);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)pthread_create, 1, -1);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_get_opcode_lite_support, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    mocker((stub_fn_t)ra_hdc_get_lite_support, 1, -1);
    ret = ra_hdc_rdev_init(&rdma_handle, NOTIFY, rdev_info, &rdev_index);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    unsigned int support = 0;
    mocker((stub_fn_t)dl_drv_device_get_index_by_phy_id, 1, 0);
    mocker((stub_fn_t)dl_hal_mem_ctl, 1, 0);
    ra_hdc_get_drv_lite_support(0, true, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();

    mocker((stub_fn_t)dl_drv_device_get_index_by_phy_id, 1, 0);
    mocker_invoke((stub_fn_t)dl_hal_get_device_info, (stub_fn_t)stub_dl_hal_get_device_info, 10);
    mocker((stub_fn_t)dl_hal_mem_ctl, 1, 0);
    ra_hdc_get_drv_lite_support(0, false, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();
}

void tc_hdc_rdev_deinit()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    int ret = ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

	mocker((stub_fn_t)ra_hdc_process_msg, 1, 0);
	mocker((stub_fn_t)ra_hdc_notify_cfg_get, 1, -1);
	ret = ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker((stub_fn_t)ra_hdc_process_msg, 1, 0);
	mocker((stub_fn_t)ra_hdc_notify_cfg_get, 1, 0);
	mocker((stub_fn_t)halMemFree, 1, -2);
	ret = ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);
	EXPECT_INT_EQ(ret, -2);
	mocker_clean();
}

void tc_hdc_socket_white_list_add_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[1] = {{0}};
    g_interface_version = RA_RS_WLIST_ADD_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = ra_hdc_socket_white_list_add(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
}

void tc_hdc_socket_white_list_add()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[1] = {{0}};
    int ret = ra_hdc_socket_white_list_add(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    tc_hdc_socket_white_list_add_v2();
}

void tc_hdc_socket_white_list_del_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[1] = {{0}};
    int ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    g_interface_version = RA_RS_WLIST_DEL_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_hdc_socket_white_list_del()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct rdev rdev_info = {0};
    struct socket_wlist_info_t white_list[1] = {{0}};
    int ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = ra_hdc_socket_white_list_del(rdev_info, white_list, 1);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    tc_hdc_socket_white_list_del_v2();
}

void tc_hdc_get_ifaddrs()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ifaddr_info infos[1] = {{0}};
    unsigned int num = 1;
    int ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_get_ifaddrs_v2()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct interface_info infos[1] = {{0}};
    unsigned int num = 1;
    int ret = ra_hdc_get_ifaddrs_v2(0, 0, infos, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();
    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = ra_hdc_get_ifaddrs_v2(0, 0, infos, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_get_ifnum()
{
    mocker_clean();
    tc_hdc_test_env_init();

    unsigned int num = 1;
    int ret = ra_hdc_get_ifnum(0, 0, &num);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker(drvDeviceGetPhyIdByIndex, 1, -1);
    ret = ra_hdc_get_ifnum(0, 0, &num);
    EXPECT_INT_EQ(ret, -EINVAL);
}

void tc_hdc_message_process_fail()
{
    mocker_clean();
    struct process_ra_sign p_ra_sign;
    p_ra_sign.tgid = 0;
    struct ra_init_config config = {
        .phy_id = g_devid,
        .nic_position = NETWORK_OFFLINE,
        .hdc_type = HDC_SERVICE_TYPE_RDMA,
    };
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 2, 0);
    int ret = ra_hdc_init(&config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);

    struct ifaddr_info infos[1] = {{0}};
    unsigned int num = 1;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)memcpy_s, 1, -1);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -ESAFEFUNC);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAllocMsg, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)drvHdcAddMsgBuffer, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcSend, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)drvHdcReuseMsg, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, 0);
    mocker((stub_fn_t)drvHdcGetMsgBuffer, 1, -10);
    ret = ra_hdc_get_ifaddrs(0, infos, &num);
    EXPECT_INT_EQ(ret, -10);

    mocker_clean();
    mocker((stub_fn_t)halHdcRecv, 1, 0);
	tc_hdc_test_env_deinit();
}

void tc_hdc_socket_recv_fail()
{
    struct socket_hdc_info socket_info;
    char send_buf[16] = {0};

    mocker_clean();
    tc_hdc_test_env_init();

    mocker_invoke((stub_fn_t)drvHdcGetMsgBuffer, (stub_fn_t)stub_drvHdcGetMsgBuffer, 2);
    struct msg_head* test_head = (struct msg_head*)g_drv_recv_msg;
    test_head->ret = -EAGAIN;
    test_head->opcode = RA_RS_SOCKET_RECV;
    g_drv_recv_msg_len = sizeof(union op_socket_recv_data) + sizeof(send_buf) + sizeof(struct msg_head);
    int ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EAGAIN);

    test_head->ret = -100;
    ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -100);

    mocker((stub_fn_t)drvHdcAddMsgBuffer, 10, 10);
    ret = ra_hdc_socket_recv(g_devid, &socket_info, send_buf, sizeof(send_buf));
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker((stub_fn_t)ra_hdc_session_close, 10, 0);
    tc_hdc_test_env_deinit();
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

void tc_ra_hdc_send_wrlist_ext_v2()
{
    mocker_clean();

    struct ra_qp_handle qp_handle;
    int ret;
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
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)memcpy_s, 1, -1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)ra_get_interface_version, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -ESAFEFUNC);
    mocker_clean();
}

void tc_ra_hdc_send_wrlist_ext()
{
    mocker_clean();

    struct ra_qp_handle qp_handle;
    int ret;
    struct send_wrlist_data_ext wr[1];
    struct send_wr_rsp op_rsp[1];
    struct wrlist_send_complete_num wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;

    struct ra_qp_handle handle;
    handle.qp_mode = 1;

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    mocker_clean();

    mocker_invoke((stub_fn_t)ra_hdc_process_msg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    ret = ra_hdc_send_wrlist_ext(&qp_handle, wr, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    tc_ra_hdc_send_wrlist_ext_v2();
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

void tc_ra_hdc_set_qp_attr_qos()
{
    struct qos_attr attr = {0};
    attr.tc = 33 * 4;
    attr.sl = 4;

    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_set_qp_attr_qos(qp_handle, &attr);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    ret = ra_hdc_set_qp_attr_qos(&test_qp_handle, &attr);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_set_qp_attr_timeout()
{
    unsigned int timeout = 15;

    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_set_qp_attr_timeout(qp_handle, &timeout);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    ret = ra_hdc_set_qp_attr_timeout(&test_qp_handle, &timeout);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_set_qp_attr_retry_cnt()
{
    unsigned int retry_cnt = 6;

    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 0, &qp_handle);
    char qp_reg[16] = {0};
    ASSERT_ADDR_NE(qp_handle, NULL);

    ret = ra_hdc_set_qp_attr_retry_cnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(ret, 0);
    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    struct ra_qp_handle test_qp_handle;
    ret = ra_hdc_set_qp_attr_retry_cnt(&test_qp_handle, &retry_cnt);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
}

void tc_ra_hdc_get_cqe_err_info()
{
    mocker_clean();
    tc_hdc_test_env_init();
    int ret;
    struct cqe_err_info info = {0};
    ret = ra_hdc_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(ret, 0);

    struct ra_qp_handle qp_hdc;
    qp_hdc.phy_id = 0;
    ra_hdc_lite_save_cqe_err_info(&qp_hdc, 0x12);
    ret = ra_hdc_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.status, 0x12);
    ra_hdc_lite_save_cqe_err_info(&qp_hdc, 0x15);
    ret = ra_hdc_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(info.status, 0x15);
    tc_hdc_test_env_deinit();

    ret = ra_hdc_get_cqe_err_info(0, &info);
    EXPECT_INT_EQ(ret, 0);
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

void tc_ra_hdc_qp_create_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.support_lite = 1;
    RA_INIT_LIST_HEAD(&rdma_handle.qp_list);
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    struct rdma_lite_qp_cap cap;

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 10, 0);
    mocker((stub_fn_t)ra_rdma_lite_create_cq, 1, 0);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 10, 0);
    mocker((stub_fn_t)ra_rdma_lite_create_qp, 1, 0);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 10, 0);
    mocker((stub_fn_t)pthread_mutex_init, 1, -1);
    ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    struct ra_qp_handle qp_hdc;
    qp_hdc.support_lite = 1;
    qp_hdc.qp_mode = 2;
    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = 1;
    cap.max_send_wr = RA_QP_TX_DEPTH;
    cap.max_recv_wr = RA_QP_TX_DEPTH;
    mocker((stub_fn_t)ra_hdc_process_msg, 10, -1);
    ret = ra_hdc_lite_qp_create(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_hdc_get_qp_status_op()
{
    int status;

    mocker_clean();
    tc_hdc_test_env_init();
    struct ra_rdma_handle rdma_handle = {0};
    void* qp_handle = NULL;
    rdma_handle.support_lite = 1;
    RA_INIT_LIST_HEAD(&rdma_handle.qp_list);
	int ret = ra_hdc_qp_create(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);

    status = 0;
    mocker((stub_fn_t)ra_hdc_get_interface_version, 10, -22);
    ret = ra_hdc_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);

    g_interface_version = RA_RS_OPCODE_BASE_VERSION;
    status = 0;
    mocker_invoke(ra_hdc_get_interface_version, stub_ra_get_interface_version, 10);
    ret = ra_hdc_get_qp_status(qp_handle, &status);
    EXPECT_INT_EQ(ret, 0);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    tc_hdc_test_env_deinit();

    mocker_clean();

    struct ra_qp_handle qp_hdc;
    qp_hdc.support_lite = 1;
    qp_hdc.qp_mode = 2;
    mocker((stub_fn_t)ra_hdc_process_msg, 10, -1);
    ret = ra_hdc_lite_get_connected_info(&qp_hdc);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)ra_hdc_process_msg, 10, 0);
    mocker((stub_fn_t)ra_rdma_lite_set_qp_sl, 1, -1);
    ret = ra_hdc_lite_get_connected_info(&qp_hdc);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
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

void tc_hdc_send_wr_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
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

    tc_hdc_test_env_deinit();

    free(addr);
    mocker_clean();
}

void tc_hdc_lite_send_wr_op()
{
    mocker_clean();
    tc_hdc_test_env_init();
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
    struct send_wr_v2 wr_v2 = {0};
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

    // stub send wr send queue is full
    qp_handle->qp_mode = RA_RS_OP_QP_MODE;
    qp_handle->sq_depth = 256;
    qp_handle->send_wr_num = 256;
    qp_handle->poll_cqe_num = 0;
    ret = ra_hdc_send_wr_v2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);
    qp_handle->poll_cqe_num = 2;
    ret = ra_hdc_send_wr_v2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);
    qp_handle->send_wr_num = 0;
    qp_handle->poll_cqe_num = -256;
    ret = ra_hdc_send_wr_v2(qp_handle, &wr_v2, &rsp);
    EXPECT_INT_EQ(ret, -12);
    ret = ra_hdc_send_wrlist_ext(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    ret = ra_hdc_qp_destroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    ra_hdc_rdev_deinit(&rdma_handle, NOTIFY);

    tc_hdc_test_env_deinit();

    free(addr);
    mocker_clean();
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
    tc_hdc_test_env_init();
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
    tc_hdc_test_env_deinit();
    mocker_clean();
}

void tc_hdc_poll_cq()
{
    mocker_clean();
    int ret = 0;
    unsigned int num_entries = 1;
    struct rdma_lite_wc_v2 lite_wc = {0};
    tc_hdc_test_env_init();
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
    tc_hdc_test_env_deinit();
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
