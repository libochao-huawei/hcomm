
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <ifaddrs.h>
#include <fcntl.h>

#include "ut_dispatch.h"
#include "stub/ibverbs.h"
#include "ascend_hal.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "rs.h"
#include "hccp_ping.h"
#include "rs_ping_inner.h"
#include "rs_ping.h"
#include "rs_drv_rdma.h"
#include "rs_tls.h"
#include "rs_drv_rdma.h"
#include "rs_drv_socket.h"
#include "ra_rs_err.h"
#include "rs_epoll.h"
#include "rs_ub.h"
#include "tc_rs.h"

typedef uint32_t u32;
typedef uint16_t u16;
//typedef uint64_t u64;
typedef unsigned long long u64;
typedef signed int s32;

const char *s_tmp = "suc";
struct rs_qp_cb *qp_cb_ab2;
struct rs_rdev_cb g_rdev_cb = {0};
int g_qp_cb_state = 0;
struct rs_conn_info *g_conn_info;
struct rs_listen_info g_listen_info = {0};
struct rs_listen_info *g_plisten_info = &g_listen_info;

#define SLEEP_TIME 30000

int rs_dev2rscb(uint32_t chip_id, struct rs_cb **rs, bool init_flag_cb);
enum rs_hardware_type rs_get_device_type(unsigned int phy_id);
int rs_get_rs_cb(unsigned int phy_id, struct rs_cb **rs_cb);
void rs_free_accept_one_node(struct rs_cb *rscb, struct rs_accept_info *accept);
int rs_epoll_ctl(int epollfd, int op, int fd, unsigned int state);
int rs_get_vnic_ip(unsigned int phy_id, unsigned int *vnic_ip);
void rs_ssl_deinit(struct rs_cb *rscb);
int rs_ssl_crl_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb);
int tls_get_cert_chain(X509_STORE *store, struct rs_certs *certs,
    struct tls_cert_mng_info *mng_info);
int rs_ssl_skid_get_from_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct rs_certs *certs, struct tls_ca_new_certs *new_certs);
int rs_ssl_get_cert(struct rs_cb *rscb, struct rs_certs *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs);
int rs_check_pridata(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
int rs_peer_get_ifaddrs(struct interface_info interface_infos[], unsigned int *num, unsigned int phy_id);
int rs_peer_get_ifnum(unsigned int phy_id, unsigned int *num);
int rs_check_dst_interface(unsigned int phy_id, const char *ifa_name, enum rs_hardware_type type, bool is_all);
int rs_socket_white_list_node_destroy(struct rs_conn_cb *conn_cb,
    struct socket_wlist_info_t *white_list, unsigned int server_ip, unsigned int server_port);
int rs_socket_nodeid2vnic(uint32_t node_id, uint32_t *ip_addr);
int rs_server_valid_async_init(unsigned int chip_id, struct rs_conn_info *conn, struct socket_wlist_info_t *white_list_expect);
int rs_server_send_wlist_check_result(struct rs_conn_info *conn, bool flag);
int rs_set_fd_nonblock(int connfd);
void rs_epoll_event_ssl_listen_in_handle(struct rs_cb *rs_cb, struct rs_listen_info *listen_info, int connfd, struct sockaddr_in remote_ip);
void rs_ssl_recv_tag_in_handle(struct rs_accept_info *accept_info, struct rs_conn_info *conn_tmp);
int rs_socket_fill_wlist_by_phyID(unsigned int chip_id, struct socket_wlist_info_t *white_list_node, struct rs_conn_info *rs_conn);
uint32_t rs_socket_vnic2nodeid(uint32_t ip_addr);
int rs_socket_listen_bind_listen(int listen_fd, struct rs_conn_cb *conn_cb,
    struct socket_listen_info *conn, struct rs_listen_info *listen_info, uint32_t server_port);
extern int rs_drv_post_recv(struct rs_qp_cb *qp_cb, struct recv_wrlist_data *wr, unsigned int recv_num,
    unsigned int *complete_num);
extern __thread struct rs_cb *g_rs_cb;
extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern STATIC int rs_drv_normal_qp_create_init(struct ibv_qp_init_attr *qp_init_attr, struct rs_qp_cb *qp_cb, struct ibv_port_attr *attr);
extern int rs_drv_exp_qp_create(struct rs_qp_cb *qp_cb, int qp_mode);
extern STATIC int rs_drv_exp_qp_create_init(struct ibv_exp_qp_init_attr *qp_init_attr, struct rs_qp_cb *qp_cb, struct ibv_port_attr *attr);
extern STATIC int rs_qpcb_init(struct rs_rdev_cb *rdev_cb, struct rs_qp_cb *qp_cb, struct rs_qp_norm *qp_norm);
extern STATIC int rs_qp_query_info(unsigned int phy_id, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb, int qp_mode);
extern int rs_send_exp_wrlist(struct rs_qp_cb *qp_cb, struct wr_info *wr_list, unsigned int send_num, struct send_wr_rsp *wr_rsp, unsigned int *complete_num);
extern STATIC int rs_get_mrcb(struct rs_qp_cb *qp_cb, uint64_t addr, struct rs_mr_cb **mr_cb, struct rs_list_head *mr_list);
extern void rs_wirte_and_read_build_up_wr(struct rs_mr_cb *mr_cb, struct rs_mr_cb *rem_mr_cb, struct wr_info *wr, struct ibv_sge *list, struct ibv_send_wr *ib_wr);
extern struct ibv_mr* rs_drv_mr_reg(struct ibv_pd *pd, char *addr, size_t length, int access);
extern int rs_ssl_ca_ky_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb);
extern int rs_ssl_load_ca(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info* mng_info);

int rs_epoll_event_listen_in_handle(struct rs_cb *rs_cb, int fd);
int rs_epoll_event_qp_mr_in_handle(struct rs_cb *rs_cb, int fd);
int rs_epoll_event_heterog_tcp_recv_in_handle(struct rs_cb *rs_cb, int fd);
extern void rs_epoll_event_ssl_accept_in_handle(struct rs_cb *rs_cb, int fd);
extern struct rs_cqe_err_info g_rs_cqe_err;
extern void rs_drv_save_cqe_err_info(uint32_t status, struct rs_qp_cb *qp_cb);
extern int rs_drv_normal_qp_create(struct rs_qp_cb *qp_cb, struct ibv_qp_init_attr *qp_init_attr);
extern int rs_get_ipv6_scope_id(struct in6_addr local_ip);
extern int rs_connect_bind_client(int fd, struct rs_conn_info *conn);
extern int dl_drv_device_get_index_by_phy_id(uint32_t phyId, uint32_t *devIndex);
extern int dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
extern void rs_socket_get_bind_by_chip(unsigned int chip_id, bool *bind_ip);

extern int rs_ssl_x509_store_init(X509_STORE_CTX *ctx, X509_STORE *store, struct rs_certs *certs,
	struct tls_cert_mng_info *mng_info, struct tls_ca_new_certs *new_certs);
extern int tls_get_user_config(unsigned int save_mode, unsigned int chip_id, const char *name,
    unsigned char *buf, unsigned int *buf_size);
extern X509 *tls_load_cert(const char *inbuf, uint32_t buf_len, int type);
extern int rs_ssl_skid_get_from_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct rs_certs *certs, struct tls_ca_new_certs *new_certs);
extern int rs_ssl_put_cert_ca_pem(struct rs_certs *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs, const char *ca_file);
extern int rs_ssl_put_cert_end_pem(struct rs_certs *certs, struct tls_ca_new_certs *new_certs, const char *end_file);
extern int rs_ssl_put_end_cert(struct rs_certs *certs, const char *end_file);
extern int rs_ssl_skids_subjects_get(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct rs_certs *certs, struct rs_new_certs *new_certs);
extern int rs_ssl_X509_store_add_cert(char *cert_info, X509_STORE *store);
extern int rs_socket_close_fd(int fd);
int rs_get_conn_info(struct rs_conn_cb *conn_cb,
			struct socket_connect_info *conn,
			struct rs_conn_info **conn_info);
extern int rs_socket_connect_check_para(struct socket_connect_info *conn_info);
extern int rsGetLocalDevIDByHostDevID(unsigned int phy_id, unsigned int *chip_id);
extern int rs_dev2conncb(uint32_t chip_id, struct rs_conn_cb **conn_cb);
extern int rs_convert_ip_addr(int family, union hccp_ip_addr *ip_addr, struct rs_ip_addr_info *ip);
extern int rs_find_listen_node(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *ip_addr, uint32_t server_port,
    struct rs_listen_info **listen_info);
extern void rs_sensor_node_unregister(struct rs_rdev_cb *rdev_cb);
extern int rs_socket_listen_add_to_epoll(struct rs_conn_cb *conn_cb, struct rs_listen_info *listen_info);
extern int rs_find_white_list(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *server_ip, unsigned int server_port,
    struct rs_white_list **white_list);
extern int rs_find_white_list_node(struct rs_white_list *rs_socket_white_list,
    struct socket_wlist_info_t *white_list_expect, int family, struct rs_white_list_info **white_list_node);
extern int rs_server_send_wlist_check_result(struct rs_conn_info *conn, bool flag);
extern bool rs_socket_is_vnic_ip(unsigned int chip_id, unsigned int ip_addr);
extern int rs_get_linux_version(struct rs_linux_version_info *ver_info);

int replace_rs_qpn2qpcb(unsigned int phy_id, unsigned int rdev_index, uint32_t qpn, struct rs_qp_cb **qp_cb)
{
	static struct rs_qp_cb a_qp_cb;
    a_qp_cb.state = g_qp_cb_state;
	*qp_cb = &a_qp_cb;
	return 0;
}

struct rs_white_list_info g_white_list_node_tmp = {0};
int stub_rs_find_white_list_node(struct rs_white_list *rs_socket_white_list,
    struct socket_wlist_info_t *white_list_expect, int family, struct rs_white_list_info **white_list_node)
{
	*white_list_node = &g_white_list_node_tmp;
	return 0;
}

void tc_rs_abnormal()
{
	int ret;
	struct rs_cb *rs_cb;
	uint32_t qpn;
	struct rs_qp_cb *qp_cb;
	struct rs_qp_cb qp_cb_tmp;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;

	rs_ut_msg("\n+++++++++ABNORMAL TC Start++++++++\n");
	ret = rs_dev2rscb(0, &rs_cb, false);
	EXPECT_INT_NE(ret, 0);

	ret = rs_dev2rscb(2, &rs_cb, false);
	EXPECT_INT_NE(ret, 0);


	ret = rs_qpn2qpcb(phy_id, rdev_index, 5, &qp_cb);

	//rs_epoll_handle();
	//qp_cb_tmp.conn_info->connfd = RS_FD_INVALID;
	//rs_qp_info_sync(&qp_cb_tmp);
	//qp_cb_tmp.conn_info->connfd = 1;
	//qp_cb_tmp.state |= RS_QP_STATE_SEND;
	//rs_qp_info_sync(&qp_cb_tmp);
	//rs_send(100, &qp_cb_tmp, 1);
	//rs_recv(100, &qp_cb_tmp, 1);
	rs_ut_msg("---------ABNORMAL TC End----------\n\n");

	return;
}

int stub_dl_hal_get_device_info_pod(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 0x30;
	return 0;
}

int stub_dl_hal_get_device_info_pod_16p(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 0x50;
	return 0;
}

int stub_dl_hal_get_device_info_pod_910A(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 87;
	return 0;
}

int dl_hal_get_device_info_910A(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 256;
	return 0;
}

int dl_hal_get_device_info_sharemem(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = (5 << 8);
	return 0;
}

int dl_hal_query_dev_pid_sharemem(struct halQueryDevpidInfo info, pid_t *dev_pid)
{
	return 0;
}

void tc_rs_init()
{
	int ret;
	struct rs_rdev_cb rdev_tmp = {0};
	struct rs_init_config cfg = {0};
	char pid_sign[] = "ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a";

	rs_sensor_node_unregister(&rdev_tmp);
	rdev_tmp.sensor_handle = 1;
	rs_sensor_node_unregister(&rdev_tmp);

	rs_ut_msg("\n%s+++++++++ABNORMAL TC Start++++++++\n", __func__);
	ret = rs_init(NULL);
	EXPECT_INT_NE(ret, 0);

	cfg.chip_id = 0;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("\n%s---------ABNORMAL TC End----------\n", __func__);

	rs_set_host_pid(cfg.chip_id, 0, pid_sign);
	/* ------Resource CLEAN-------- */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct rs_init_config cfg_peer_online = {0};
	cfg_peer_online.chip_id = 1;
	cfg_peer_online.hccp_mode = NETWORK_PEER_ONLINE;
	ret = rs_init(&cfg_peer_online);

	ret = rs_deinit(&cfg_peer_online);

	struct rs_init_config cfg_online = {0};
	cfg_online.chip_id = 1;
	cfg_online.hccp_mode = NETWORK_ONLINE;
	ret = rs_init(&cfg_online);

	ret = rs_deinit(&cfg_online);

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	ret = rs_get_device_type(10);

	ret = rs_get_device_type(1);
	EXPECT_INT_EQ(ret, RS_HARDWARE_UNKNOWN);
	free(g_rs_cb);
	g_rs_cb = NULL;

	unsigned int vnic_ip = 0x10;
	ret = rs_get_vnic_ip(129, &vnic_ip);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_get_vnic_ip(0, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);
	return;
}

void tc_rs_deinit()
{
	int ret;
	uint32_t chip_id = 0;
	struct rs_cb *rs_cb;
	int eventfd_tmp;
	struct rs_init_config cfg = {0};


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	rs_dev2rscb(chip_id, &rs_cb, false);

	rs_ut_msg("\n%s+++++++++ABNORMAL TC Start++++++++\n", __func__);
	/* param error */
	ret = rs_deinit(NULL);
	EXPECT_INT_NE(ret, 0);

	/* env store */
	eventfd_tmp = rs_cb->conn_cb.eventfd;
	rs_cb->conn_cb.eventfd = -1;
	ret = rs_deinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	/* env recovery */
	rs_cb->conn_cb.eventfd = eventfd_tmp;

	rs_ut_msg("\n%s---------ABNORMAL TC End----------\n", __func__);

	/* ------Resource CLEAN-------- */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct rs_cb *rscb = NULL;
	rscb = calloc(1, sizeof(struct rs_cb));
	rscb->hccp_mode = NETWORK_OFFLINE;
	RS_INIT_LIST_HEAD(&rscb->conn_cb.client_conn_list);
	rs_init_rscb_cfg(rscb);
	rs_deinit_rscb_cfg(rscb);
	free(rscb);
	rscb = NULL;
	return;
}

/* FREE server/client conn_info, listen_info AUTOMATICALLY */
void tc_rs_deinit2()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
    rs_socket_white_list_add(rdev_info, &white_list, 1);

    strcpy(white_list.tag, "5678");
	rs_socket_white_list_add(rdev_info, &white_list, 1);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].port = 16666;
	strcpy(conn[0].tag, "1234");
	conn[1].phy_id = 0;
	conn[1].family = AF_INET;
	conn[1].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].port = 16666;
	strcpy(conn[1].tag, "5678");
	ret = rs_socket_batch_connect(&conn[0], 2);

	usleep(SLEEP_TIME); //wait for epoll thread start up..


	/* ------Resource CLEAN-------- */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;

}

void tc_rs_socket_init()
{
	int ret;
	int i;
	unsigned int vnic_ip[8] = {0};

	ret = rs_socket_init(NULL, 0);
	EXPECT_INT_EQ(ret, -22);

	ret = rs_socket_init(vnic_ip, 8);
	EXPECT_INT_EQ(ret, 0);

	return;
}

extern __thread struct rs_cb *g_rs_cb;
void tc_rs_socket_deinit()
{
	int ret;
    struct rdev rdev_info = {0};
	struct rs_cb g_rs_cb_tmp = {0};
	g_rs_cb_tmp.hccp_mode = NETWORK_PEER_ONLINE;
	g_rs_cb = &g_rs_cb_tmp;

	rdev_info.phy_id = 8;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	ret = rs_socket_deinit(rdev_info);
	g_rs_cb = NULL;
}

void tc_rs_socket_listen_ipv6()
{
	int ret;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	listen[0].phy_id = 0;
	listen[0].family = AF_INET6;
	inet_pton(AF_INET6, "::1", &listen[0].local_ip.addr6);
	listen[0].port = 16666;
	ret = rs_socket_set_scope_id(0, if_nametoindex("lo"));
	EXPECT_INT_EQ(ret, 0);
	mocker(rs_get_ipv6_scope_id, 10, if_nametoindex("lo"));
	ret = rs_socket_listen_start(&listen[0], 1);

	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	/* ------Resource CLEAN-------- */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_socket_listen()
{
	int ret;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2];


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 10;
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);
	EXPECT_INT_EQ(ret, -EINVAL);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	/* listen 1 will fail, cannot listen same IP twice */
	listen[1].phy_id = 0;
	listen[1].family = AF_INET;
	listen[1].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[1].port = 16666;
	ret = rs_socket_listen_start(&listen[1], 1);

	/* stop a non-exist node */
	listen[1].port = 16666;
	ret = rs_socket_listen_stop(&listen[1], 1);

	/* ------Resource CLEAN-------- */
	struct rs_conn_info conn_send_inc;
	mocker((stub_fn_t)send, 10, -1);
	rs_socket_tag_sync(&conn_send_inc);
	mocker_clean();

	mocker(rs_drv_socket_send, 10, -EAGAIN);
	rs_socket_tag_sync(&conn_send_inc);
	mocker_clean();

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_socket_connect()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
    rs_socket_white_list_add(rdev_info, &white_list, 1);

    strcpy(white_list.tag, "5678");
	rs_socket_white_list_add(rdev_info, &white_list, 1);

	conn[0].phy_id = 10;
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 2);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	strcpy(conn[0].tag, "1234");
	conn[1].phy_id = 0;
	conn[1].family = AF_INET6;
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 2);
	mocker_clean();

	mocker(rs_get_ipv6_scope_id, 10, -1);
	ret = rs_socket_batch_connect(&conn[0], 2);
	mocker_clean();

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phy_id = 0;
	conn[1].family = AF_INET;
	conn[1].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 2);

    strcpy(white_list.tag, "1234");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;
	/* >>>>>>> rs_socket_batch_connect test case begin <<<<<<<<<<< */
	/* repeat connect */
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	/* param error - conn NULL */
	ret = rs_socket_batch_connect(NULL, 1);

	/* param error - num error */
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 0);

	/* param error - device id error */
	conn[0].phy_id = 15;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);
	/* >>>>>>> rs_socket_batch_connect test case end <<<<<<<<<<< */


	usleep(SLEEP_TIME); //wait for epoll thread start up..

#if 1
	i = 0;

	socket_info[i].phy_id = 10;
	ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);

	socket_info[i].phy_id = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
#endif

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remote_ip.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
#endif

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	struct rs_cb *rs_cb = NULL;
	ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	rs_cb->ssl_enable = 1;

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

extern __thread struct rs_cb *g_rs_cb;
void tc_rs_socket_connect02()
{
	struct rs_conn_info conn_socket_err;
	int ret;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	g_rs_cb->conn_cb.wlist_enable = 1;
	g_rs_cb->ssl_enable = 1;
	conn_socket_err.state = 7;
	mocker((stub_fn_t)rs_socket_recv, 1, -11);
	ret = rs_socket_connect_async(&conn_socket_err, g_rs_cb);
	mocker_clean();
	mocker((stub_fn_t)rs_socket_recv, 1, 0);
	mocker((stub_fn_t)SSL_shutdown, 1, 0);
	mocker((stub_fn_t)SSL_free, 1, 0);
	ret = rs_socket_connect_async(&conn_socket_err, g_rs_cb);
	mocker_clean();
	free(g_rs_cb);
	g_rs_cb = NULL;
}

void tc_rs_socket_deinit1()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct rs_cb *rs_cb = NULL;
	struct rdev rdev_info = {0};
	struct rs_accept_info *accept = calloc(1, sizeof(struct rs_accept_info));
	struct rs_list_head list = {0};

	RS_INIT_LIST_HEAD(&list);
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, -EEXIST);

	ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	EXPECT_INT_EQ(ret, 0);
	rs_cb->ssl_enable = 1;

	accept->list = list;
	accept->ssl = NULL;

	rs_free_accept_one_node(rs_cb, accept);

	/* ------Resource CLEAN-------- */
	rs_cb->ssl_enable = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

void tc_rs_socket_deinit2()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct rs_cb *rs_cb = NULL;
	struct rdev rdev_info = {0};
	struct rs_accept_info *accept = calloc(1, sizeof(struct rs_accept_info));
	struct rs_list_head list = {0};

	RS_INIT_LIST_HEAD(&list);
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	EXPECT_INT_EQ(ret, 0);
	rs_cb->ssl_enable = 1;

	accept->ssl = malloc(sizeof(SSL_CTX));
	accept->list = list;

	rs_free_accept_one_node(rs_cb, accept);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}


void tc_rs_get_sockets()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	struct socket_fd_data conn_info;
	conn_info.local_ip.addr.s_addr = 1;
	rs_sockets_serverip_converter(&conn_info, 1, 1);

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

    rs_socket_white_list_add(rdev_info, &white_list, 1);
    strcpy(white_list.tag, "5678");
    rs_socket_white_list_add(rdev_info, &white_list, 1);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phy_id = 0;
	conn[1].family = AF_INET;
	conn[1].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 2);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

#if 1
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
#endif

#if 1 /* get Client Conn & Close it */
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* param error */
	ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, NULL, 3);
	EXPECT_INT_NE(ret, 0);

	/* physical id error */
	socket_info[i].phy_id = 55555;
	ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_NE(ret, 0);
	socket_info[i].phy_id = 0;	//recover

	/* repeat get */
	ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_EQ(ret, 0);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);
#endif

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remote_ip.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
#endif


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_set_tsqp_depth()
{
	int ret;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;
	unsigned int temp_depth = 8;
	unsigned int qp_num;
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	struct rs_init_config cfg = {0};
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_set_tsqp_depth(phy_id, rdev_index, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_tsqp_depth()
{
	int ret;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;
	unsigned int temp_depth = 8;
	unsigned int qp_num;
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	struct rs_init_config cfg = {0};
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_tsqp_depth(phy_id, rdev_index, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

int stub_rs_epoll_ctl(int epollfd, int op, int fd, int state)
{
	if (op == EPOLL_CTL_ADD) return 0;
	return 1;
}

int stub_rs_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	if (attr == NULL) {
		return -EINVAL;
	}
	attr->qp_state = 3; // IBV_QPS_RTS
	return 0;
}

void tc_rs_qp_create()
{
	int ret;
	uint32_t phy_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	int qp_mode = 1;
	int i;
	struct rs_qp_cb qp_cb_abnormal;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

 	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 1;
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 1;
	cfg.hccp_mode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chip_id = 3;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	enum port_status status = PORT_STATUS_DOWN;
	ret = rs_rdev_get_port_status(100000, rdev_index, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = rs_rdev_get_port_status(rdev_info.phy_id, rdev_index, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = rs_rdev_get_port_status(15, rdev_index, &status);
	EXPECT_INT_NE(ret, 0);
	ret = rs_rdev_get_port_status(rdev_info.phy_id, 100000, &status);
	EXPECT_INT_NE(ret, 0);
	ret = rs_rdev_get_port_status(rdev_info.phy_id, rdev_index, &status);
	EXPECT_INT_EQ(ret, 0);
	mocker(ibv_query_port, 20, -1);
	ret = rs_rdev_get_port_status(rdev_info.phy_id, rdev_index, &status);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	int support_lite = 0;
	ret = rs_get_lite_support(rdev_info.phy_id, rdev_index, &support_lite);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(support_lite, 1);

	struct lite_rdev_cap_resp rdev_resp;
	ret = rs_get_lite_rdev_cap(rdev_info.phy_id, rdev_index, &rdev_resp);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

    rs_socket_white_list_add(rdev_info, NULL, 1);
	rs_socket_white_list_del(rdev_info, NULL, 1);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);

	struct lite_qp_cq_attr_resp qp_resp;
	ret = rs_get_lite_qp_cq_attr(phy_id, rdev_index, resp.qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct lite_mem_attr_resp mem_resp;
	ret = rs_get_lite_mem_attr(rdev_info.phy_id, rdev_index, resp.qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_rdev_deinit(phy_id, NOTIFY, rdev_index);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	/* >>>>>>> rs_qp_connect_async test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = rs_qp_connect_async(10, rdev_index, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	ret = rs_qp_connect_async(phy_id, rdev_index, 4444, -1);
	EXPECT_INT_NE(ret, 0);

	ret = rs_qp_connect_async(phy_id, rdev_index, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	ret = rs_qp_connect_async(phy_id, rdev_index, resp.qpn, -1);
	EXPECT_INT_NE(ret, 0);

	mocker_invoke(rs_epoll_ctl, stub_rs_epoll_ctl, 10);
	ret = rs_qp_connect_async(phy_id, rdev_index, resp.qpn, socket_info[i].fd);
	mocker_clean();

	struct rs_qp_cb *qp_cb;
	ret = rs_qpn2qpcb(phy_id, rdev_index, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	qp_cb->state = RS_QP_STATUS_DISCONNECT;

	/* >>>>>>> rs_qp_connect_async test case end <<<<<<<<<<< */

	ret = rs_qp_connect_async(phy_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	struct lite_connected_info_resp connected_resp;
	qp_cb->state = RS_QP_STATUS_CONNECTED;
	ret = rs_get_lite_connected_info(phy_id, rdev_index, resp.qpn, &connected_resp);
	EXPECT_INT_EQ(ret, 0);

	mocker_invoke(rs_ibv_query_qp, stub_rs_ibv_query_qp, 10);
	ret = rs_qp_state_modify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	usleep(SLEEP_TIME);

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> rs_qp_create test case begin <<<<<<<<<<< */
	/* param error - device id */

	ret = rs_qp_create(15, rdev_index, qp_norm, &resp2);
	EXPECT_INT_NE(ret, 0);

	/* qp number out of boundry */
	struct rs_cb *rs_cb;
	struct rs_rdev_cb *rdev_cb;
        uint32_t chip_id = 0;
	ret = rs_dev2rscb(chip_id, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);
 	ret = rs_get_rdev_cb(rs_cb, rdev_index, &rdev_cb);
	EXPECT_INT_EQ(ret, 0);

	int qp_cnt_tmp = rdev_cb->qp_cnt;
	rdev_cb->qp_cnt = 44444;
	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_NE(ret, 0);
	rdev_cb->qp_cnt = qp_cnt_tmp;
	/* >>>>>>> rs_qp_create test case end <<<<<<<<<<< */

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, resp2.qpn, socket_info[i].fd);
#endif
	usleep(SLEEP_TIME);

	uint64_t mr_max_addr = 0x654321;
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = mr_max_addr;
	mr_reg_info.len = 1;
	mr_reg_info.access = 0;
	for (i = 0; i < 65; i++) {
		ret = rs_mr_reg(phy_id, rdev_index, resp.qpn,&mr_reg_info);
		mr_max_addr += 2;
	}

	//ret = rs_qp_destroy(dev_id, rdev_index, qpn2);
	ret = rs_qp_destroy(phy_id, rdev_index, resp.qpn);

	/* param error - qpn */
	ret = rs_qp_destroy(phy_id, rdev_index, resp2.qpn);
	EXPECT_INT_EQ(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init_with_backup(rdev_info, rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_8K_DEPTH      8192
#define RS_QP_ATTR_MAX_INLINE_DATA 32
#define RS_QP_ATTR_MAX_SEND_SGE 8
#define RS_QP_TX_DEPTH_PEER_ONLINE          4096    // host RDMA adapt
#define RS_QP_TX_DEPTH_ONLINE 4096
#define RS_QP_TX_DEPTH                      8191
#define RS_QP_TX_DEPTH_OFFLINE 128
void prepare_ext_attrs(int qpMode, struct qp_ext_attrs *extAttrs)
{
    extAttrs->qp_mode = qpMode;
    // cq attr
    extAttrs->cq_attr.send_cq_depth = RS_DRV_CQ_8K_DEPTH;
    extAttrs->cq_attr.send_cq_comp_vector = 0;
    extAttrs->cq_attr.recv_cq_depth = RS_DRV_CQ_128_DEPTH;
    extAttrs->cq_attr.recv_cq_comp_vector = 0;
    // qp attr
    extAttrs->qp_attr.qp_context = NULL;
    extAttrs->qp_attr.send_cq = NULL;
    extAttrs->qp_attr.recv_cq = NULL;
    extAttrs->qp_attr.srq = NULL;
    extAttrs->qp_attr.cap.max_send_wr = RS_QP_TX_DEPTH_OFFLINE;
    extAttrs->qp_attr.cap.max_recv_wr = RS_QP_TX_DEPTH_OFFLINE;
    extAttrs->qp_attr.cap.max_send_sge = 1;
    extAttrs->qp_attr.cap.max_recv_sge = 1;
    extAttrs->qp_attr.cap.max_inline_data = RS_QP_ATTR_MAX_INLINE_DATA;
    extAttrs->qp_attr.qp_type = IBV_QPT_RC;
    extAttrs->qp_attr.sq_sig_all = 0;
    // version control
    extAttrs->version = QP_CREATE_WITH_ATTR_VERSION;
}

void tc_rs_qp_create_with_attrs_v1()
{

	int ret;
	uint32_t phy_id = 0;
	unsigned int rdev_index = 0;
	uint32_t qpn, qpn1, qpn2;
	int qp_mode = 1;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	struct rs_qp_norm_with_attrs qp_norm = {0};
	struct rs_qp_resp_with_attrs qp_resp_create = {0};

	qp_norm.is_exp = 1;
	qp_norm.is_ext = 1;
	prepare_ext_attrs(1, &qp_norm.ext_attrs);
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 1;
	cfg.hccp_mode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chip_id = 3;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	int support_lite = 0;
	ret = rs_get_lite_support(rdev_info.phy_id, rdev_index, &support_lite);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(support_lite, 1);

	struct lite_rdev_cap_resp rdev_resp;
	ret = rs_get_lite_rdev_cap(rdev_info.phy_id, rdev_index, &rdev_resp);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

    rs_socket_white_list_add(rdev_info, NULL, 1);
	rs_socket_white_list_del(rdev_info, NULL, 1);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

    qp_norm.ext_attrs.data_plane_flag.bs.cq_cstm = 1;
    qp_norm.ai_op_support = 1;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);

	struct lite_qp_cq_attr_resp qp_resp;
	ret = rs_get_lite_qp_cq_attr(phy_id, rdev_index, qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct lite_mem_attr_resp mem_resp;
	ret = rs_get_lite_mem_attr(rdev_info.phy_id, rdev_index, qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_rdev_deinit(phy_id, NOTIFY, rdev_index);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create_with_attrs: qpn %d, ret:%d\n", qpn, ret);


	/* >>>>>>> rs_qp_connect_async test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = rs_qp_connect_async(10, rdev_index, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	ret = rs_qp_connect_async(phy_id, rdev_index, 4444, -1);
	EXPECT_INT_NE(ret, 0);

	ret = rs_qp_connect_async(phy_id, rdev_index, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	ret = rs_qp_connect_async(phy_id, rdev_index, qpn, -1);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> rs_qp_connect_async test case end <<<<<<<<<<< */

	ret = rs_qp_connect_async(phy_id, rdev_index, qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	struct rs_qp_cb *qp_cb;
	ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	struct lite_connected_info_resp connected_resp;
	qp_cb->state = RS_QP_STATUS_CONNECTED;
	ret = rs_get_lite_connected_info(phy_id, rdev_index, qpn, &connected_resp);
	EXPECT_INT_EQ(ret, 0);

	usleep(SLEEP_TIME);

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> rs_qp_create_with_attrs test case begin <<<<<<<<<<< */
	/* param error - device id */

	ret = rs_qp_create_with_attrs(15, rdev_index, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_NE(ret, 0);

	/* qp number out of boundry */
	struct rs_cb *rs_cb;
	struct rs_rdev_cb *rdev_cb;
        uint32_t chip_id = 0;
	ret = rs_dev2rscb(chip_id, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);
 	ret = rs_get_rdev_cb(rs_cb, rdev_index, &rdev_cb);
	EXPECT_INT_EQ(ret, 0);

	int qp_cnt_tmp = rdev_cb->qp_cnt;
	rdev_cb->qp_cnt = 44444;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_NE(ret, 0);
	rdev_cb->qp_cnt = qp_cnt_tmp;
	/* >>>>>>> rs_qp_create_with_attrs test case end <<<<<<<<<<< */

	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create_with_attrs: qpn2 %d, ret:%d\n", qpn2, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, qpn2, socket_info[i].fd);

	qp_norm.ext_attrs.qp_mode = 0;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp_create);
	qpn1 = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create_with_attrs: qpn1 %d, ret:%d\n", qpn1, ret);
#endif
	usleep(SLEEP_TIME);

	uint64_t mr_max_addr = 0x654321;
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = mr_max_addr;
	mr_reg_info.len = 1;
	mr_reg_info.access = 0;
	for (i = 0; i < 65; i++) {
		ret = rs_mr_reg(phy_id, rdev_index, qpn,&mr_reg_info);
		mr_max_addr += 2;
	}

	//ret = rs_qp_destroy(dev_id, rdev_index, qpn2);
	ret = rs_qp_destroy(phy_id, rdev_index, qpn);
	ret = rs_qp_destroy(phy_id, rdev_index, qpn1);
	/* param error - qpn */
	ret = rs_qp_destroy(phy_id, rdev_index, qpn2);
	EXPECT_INT_EQ(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);


	return;
}

void tc_rs_qp_create_with_attrs_v2()
{
	int ret;
	uint32_t phy_id = 0;
	unsigned int rdev_index = 0;
	struct rs_qp_norm_with_attrs qp_norm = {0};
	struct rs_qp_resp_with_attrs qp_resp = {0};
	qp_norm.is_exp = 1;

	ret = rs_qp_create_with_attrs(15, rdev_index, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, NULL, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.ext_attrs.version = -1;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.ext_attrs.version = QP_CREATE_WITH_ATTR_VERSION;
	qp_norm.ext_attrs.qp_mode = -1;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.ext_attrs.qp_mode = RA_RS_OP_QP_MODE_EXT;
	ret = rs_qp_create_with_attrs(phy_id, rdev_index, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
}

void tc_rs_qp_create_with_attrs()
{
	tc_rs_qp_create_with_attrs_v1();
	tc_rs_qp_create_with_attrs_v2();
}

void tc_rs_mr_sync()
{
	int ret;
	uint32_t phy_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	int qp_mode = 1;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	rs_ut_msg("___________________after listen:\n");

    strcpy(white_list.tag, "1234");

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

    rs_socket_white_list_add(rdev_info, &white_list, 1);;

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	rs_ut_msg("___________________after connect:\n");

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);

	rs_ut_msg("___________________after qp create:\n");

	/* >>>>>>> rs_qp_connect_async test case begin <<<<<<<<<<< */
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = rs_mr_reg(phy_id, rdev_index, resp.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	/* >>>>>>> rs_qp_connect_async test case end <<<<<<<<<<< */

	rs_ut_msg("___________________after mr reg:\n");

	ret = rs_qp_connect_async(phy_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	rs_ut_msg("___________________after qp connect async:\n");
	usleep(SLEEP_TIME);
	rs_ut_msg("___________________after qp connect async & sleep:\n");

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);
	rs_ut_msg("___________________after qp2 create:\n");

	ret = rs_qp_connect_async(phy_id, rdev_index, resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");
#endif
	usleep(SLEEP_TIME);


	ret = rs_qp_destroy(phy_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(phy_id, rdev_index, resp.qpn);

	rs_ut_msg("___________________after qp1&2 destroy:\n");

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	rs_ut_msg("___________________after close socket 0:\n");

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);

	rs_ut_msg("___________________after close socket 1:\n");

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	rs_ut_msg("___________________after stop listen:\n");

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("___________________after deinit:\n");

	return;
}

/* create 2 socket & 2 qp, and connect them */
static int tc_rs_sock_qp_create_normal(int *fd, uint32_t *qpn, int *fd2, uint32_t *qpn2)
{
	int ret;
	int i;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	int flag = 0; /* RC */
	int qp_mode = 0;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[1] = {0};
	struct socket_connect_info conn[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);
	rs_ut_msg("RS LISTEN, ret:%d !\n", ret);

    strcpy(white_list.tag, "1234");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);
	rs_ut_msg("RS CONNECT, ret:%d !\n", ret);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 0;

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	*qpn = resp.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, *qpn, socket_info[i].fd);
	*fd = socket_info[i].fd;
	rs_ut_msg("RS QP CONNECT ASYNC: ret:%d\n", ret);

	usleep(SLEEP_TIME);


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, *qpn2, socket_info[i].fd);
	*fd2 = socket_info[i].fd;

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");

	return ret;
}

/* create 2 socket & 2 qp, and connect them */
static int tc_rs_sock_qp_create(int *fd, uint32_t *qpn, int *fd2, uint32_t *qpn2)
{
	int ret;
	int i;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	int flag = 0; /* RC */
	int qp_mode = 1;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[1] = {0};
	struct socket_connect_info conn[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);
	rs_ut_msg("RS LISTEN, ret:%d !\n", ret);

    strcpy(white_list.tag, "1234");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);
	rs_ut_msg("RS CONNECT, ret:%d !\n", ret);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 1;
	qp_norm.is_ext = 1;

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	*qpn = resp.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, *qpn, socket_info[i].fd);
	*fd = socket_info[i].fd;
	rs_ut_msg("RS QP CONNECT ASYNC: ret:%d\n", ret);

	usleep(SLEEP_TIME);


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = rs_qp_connect_async(phy_id, rdev_index, *qpn2, socket_info[i].fd);
	*fd2 = socket_info[i].fd;

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");

	return ret;
}

static int tc_rs_sock_qp_destroy(int fd, uint32_t qpn, int fd2, uint32_t qpn2)
{
	int ret;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	struct rs_init_config cfg = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_listen_info listen[1] = {0};

	rs_ut_msg("resource free begin..................\n");
	usleep(SLEEP_TIME);

	ret = rs_qp_destroy(phy_id, rdev_index, qpn2);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_qp_destroy(phy_id, rdev_index, qpn);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS destroy QP: ret:%d\n", ret);

	sock_close[0].fd = fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);
	rs_ut_msg("RS socket close fd:%d, ret:%d\n", fd, ret);

	sock_close[1].fd = fd2;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);
	rs_ut_msg("RS socket2 close fd:%d, ret:%d\n", fd2, ret);

	/* ------resource CLEAN-------- */
	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);
	rs_ut_msg("RS socket listen stop: ret:%d\n", ret);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("resource free done..................\n");

	return ret;
}

struct rs_mr_cb *g_mr_cb_a;
int stub_rs_get_mrcb_a(struct rs_qp_cb *qp_cb, uint64_t addr, struct rs_mr_cb **mr_cb,
    struct rs_list_head *mr_list)
{
	*mr_cb = g_mr_cb_a;
	return -1;
}

void tc_rs_mr_create()
{
	int ret;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2, *addr3;
	int try_num = 0;
	struct rdma_mr_reg_info mr_reg_info = {0};

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(8192);
	addr3 = malloc(8192);

	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	/* repeat reg */
	ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0); //synced, do not need sync again!

	mr_reg_info.addr = addr2;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	usleep(SLEEP_TIME);

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = rs_mr_dereg(phy_id, rdev_index, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr3;
	mocker(rs_qpn2qpcb, 10, 0);
	mocker_invoke((stub_fn_t)rs_get_mrcb, stub_rs_get_mrcb_a, 1);
	ret = rs_mr_dereg(phy_id, rdev_index, 999999, addr2);
	EXPECT_INT_EQ(ret, -EFAULT);
	mocker_clean();


	free(addr);
	free(addr2);
	free(addr3);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_typical_register_mr()
{
	int ret;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	void *addr;
	struct rdma_mr_reg_info mr_reg_info = {0};
	struct ibv_mr *ra_rs_mr_handle = NULL;
	struct rs_init_config cfg = {0};

	addr = malloc(RS_TEST_MEM_SIZE);
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_typical_register_mr_v1(phy_id, rdev_index, &mr_reg_info, &ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_typical_deregister_mr(phy_id, rdev_index, (uint64_t)addr);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_typical_register_mr(phy_id, rdev_index, &mr_reg_info, &ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_typical_deregister_mr(phy_id, rdev_index, (uint64_t)ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
}

int stub_rs_ibv_query_qp_init(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	if (attr == NULL) {
		return -EINVAL;
	}
	attr->qp_state = 1; // IBV_QPS_INIT
	return 0;
}

void tc_rs_typical_qp_modify()
{
	int ret;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	void *addr;
	struct rdma_mr_reg_info mr_reg_info = {0};
	struct ibv_mr *ra_rs_mr_handle = NULL;
	struct rs_init_config cfg = {0};

	addr = malloc(RS_TEST_MEM_SIZE);
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	struct typical_qp local_qp_info = {0};
	struct typical_qp remote_qp_info = {0};
	unsigned int udp_sport;

	struct rs_qp_norm qp_norm = {0};
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int flag = 0; /* RC */
	int qp_mode = 4;
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 1;
	qp_norm.is_ext = 1;

	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_qp_create(phy_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	local_qp_info.qpn = resp.qpn;
	local_qp_info.psn = resp.psn;
	local_qp_info.gid_idx = resp.gid_idx;
	(void)memcpy_s(local_qp_info.gid, HCCP_GID_RAW_LEN, resp.gid.raw, HCCP_GID_RAW_LEN);
	remote_qp_info.qpn = resp2.qpn;
	remote_qp_info.psn = resp2.psn;
	remote_qp_info.gid_idx = resp2.gid_idx;
	(void)memcpy_s(remote_qp_info.gid, HCCP_GID_RAW_LEN, resp2.gid.raw, HCCP_GID_RAW_LEN);

	mocker_invoke(rs_ibv_query_qp, stub_rs_ibv_query_qp_init, 10);
    mocker(rs_roce_query_qpc, 10, 1);
	ret = rs_typical_qp_modify(phy_id, rdev_index, local_qp_info, remote_qp_info, &udp_sport);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_typical_qp_modify(phy_id, rdev_index, remote_qp_info, local_qp_info, &udp_sport);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	int batch_modify_qpn[2];
	batch_modify_qpn[0] = resp.qpn;
	batch_modify_qpn[1] = resp2.qpn;
 	ret = rs_qp_batch_modify(phy_id, rdev_index, 5, batch_modify_qpn, 2);
	EXPECT_INT_EQ(ret, 0);

 	ret = rs_qp_batch_modify(phy_id, rdev_index, 1, batch_modify_qpn, 2);
	EXPECT_INT_EQ(ret, 0);

 	ret = rs_qp_batch_modify(phy_id, rdev_index, 1, batch_modify_qpn, 2);
	EXPECT_INT_NE(ret, 0);

	ret = rs_qp_destroy(phy_id, rdev_index, resp.qpn);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_qp_destroy(phy_id, rdev_index, resp2.qpn);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
}

void tc_rs_mr_abnormal()
{
	int ret;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);


	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	ret = rs_mr_reg(129, rdev_index, 999999, &mr_reg_info);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_mr_reg(phy_id, rdev_index, 999999, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	mr_reg_info.addr = 0;
	ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	ret = rs_mr_dereg(129, rdev_index, 999999, addr);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_mr_dereg(phy_id, rdev_index, 999999, addr);
	EXPECT_INT_NE(ret, 0);

	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, 0);
	EXPECT_INT_NE(ret, 0);

	free(addr);
	free(addr2);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_abnormal2()
{
		int ret;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct rs_cb *rs_cb, *rs_cb2;
	char cmd, cmd2;
	struct epoll_event events;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);


/* ABNORMAL TC Start */
	rs_ut_msg("\n+++++++++ABNORMAL TC Start++++++++\n");
	ret = rs_dev2rscb(0, &rs_cb2, false);
	EXPECT_INT_EQ(ret, 0);
	usleep(1000);
	events.data.fd = 200;
	rs_epoll_event_in_handle(rs_cb2, &events);
	events.data.fd = -1;
	rs_epoll_event_in_handle(rs_cb2, &events);
	events.data.fd = 0;
	rs_epoll_event_in_handle(rs_cb2, &events);
	rs_ut_msg("---------ABNORMAL TC End----------\n\n");
/* ABNORMAL TC End */


#if 0
	struct rs_mr_cb mr_cb_3;
	struct rs_qp_cb qp_cb_3;
	end_wr(10r_cb_3.qp_cb = &sp_cb_3;
	qp_cb_3.conn_info->connfd = RS_FD_INVALID;
	ret = rs_mr_info_sync(&mr_cb_3);
	EXPECT_INT_NE(0, ret);

	qp_cb_3.conn_info->connfd = 10;
	mr_cb_3.state = RS_MR_STATE_SYNCED;
	ret = rs_mr_info_sync(&mr_cb_3);
	EXPECT_INT_NE(0, ret);
#endif

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;

}

void tc_rs_buf_print()
{
	char addr[10] = {0};
	rs_buf_print(addr, 1);
}

struct rs_qp_cb *qp_cb2;
void tc_rs_cq_handle()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct rs_qp_cb qp_cb_4;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);


	qp_cb_4.channel = NULL;
	rs_drv_poll_cq_handle(&qp_cb_4);

	ret = rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb_ab2);
	EXPECT_INT_EQ(ret, 0);
	rs_drv_poll_cq_handle(qp_cb_ab2);



	struct rs_qp_cb qpcb_tmp = {0};
	struct ibv_wc wc = {0};
	struct ibv_cq ev_cq_sq = {0};
	struct ibv_cq ev_cq_rq = {0};
	struct rs_rdev_cb rdev_cb = {0};

	qpcb_tmp.ib_send_cq = &ev_cq_sq;
	qpcb_tmp.ib_recv_cq = &ev_cq_rq;
	qpcb_tmp.rdev_cb = &rdev_cb;

	rs_cqe_callback_process(&qpcb_tmp, &wc, &ev_cq_sq);
	rs_cqe_callback_process(&qpcb_tmp, &wc, &ev_cq_sq);

	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_SUCCESS;
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_MW_BIND_ERR;
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_epoll_handle()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct epoll_event events_3;


	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);


	events_3.events = 0;
	rs_epoll_event_handle_one(NULL, &events_3);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

int stub_halGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 1;
	return 0;
}

void tc_rs_socket_ops()
{
	int ret;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;
	uint32_t ssl_enable = 1;

	ret = rs_get_ssl_enable(NULL);
	EXPECT_INT_NE(ret, 0);
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	mocker((stub_fn_t)system, 10, 0);
	mocker((stub_fn_t)access, 10, 0);
    mocker_invoke((stub_fn_t)halGetDeviceInfo, stub_halGetDeviceInfo, 10);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_get_ssl_enable(NULL);
	EXPECT_INT_NE(ret, 0);

	ret = rs_get_ssl_enable(&ssl_enable);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(ssl_enable, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	/* >>>>>>> rs_socket_listen_start test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = rs_socket_listen_start(NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	ret = rs_socket_listen_start(&listen[0], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	listen[0].phy_id = 15;
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);
	listen[0].phy_id = 0; //recover

	/* repeat listen */
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);
	/* >>>>>>> rs_socket_listen_start test case end <<<<<<<<<<< */
    strcpy(white_list.tag, "1234");

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

    rs_socket_white_list_add(rdev_info, &white_list, 1);;
    strcpy(white_list.tag, "5678");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phy_id = 0;
	conn[1].family = AF_INET;
	conn[1].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 2);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	/* ===rs_epoll_event_in_handle ut begin --- accept fail=== */
	struct epoll_event events;
	struct rs_cb *rs_cb_t;
	struct rs_listen_info *listen_info, *listen_info2;

	ret = rs_dev2rscb(0, &rs_cb_t, false);
	RS_LIST_GET_HEAD_ENTRY(listen_info, listen_info2, &rs_cb_t->conn_cb.listen_list, list, struct rs_listen_info);
	for(; (&listen_info->list) != &rs_cb_t->conn_cb.listen_list;
            listen_info = listen_info2, listen_info2 = list_entry(listen_info2->list.next, struct rs_listen_info, list)) {
		events.data.fd = listen_info->listen_fd;
	}
	events.events = EPOLLIN;
	mocker((stub_fn_t)accept, 10, -1);
	rs_epoll_event_in_handle(rs_cb_t, &events);
	rs_epoll_event_in_handle(rs_cb_t, &events);
	rs_epoll_event_in_handle(rs_cb_t, &events);
	mocker_clean();
	/* ===rs_epoll_event_in_handle ut end --- accept fail=== */

#if 1
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);


	/* >>>>>>> rs_socket_send test case begin <<<<<<<<<<< */
	int data = 0;
	int size = sizeof(data);
	/* param error */
	ret = rs_socket_send(socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_send(0, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	/* fd error */
	ret = rs_socket_send(1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_send(0, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_send(1, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> rs_socket_send test case end <<<<<<<<<<< */


	/* >>>>>>> rs_socket_recv test case begin <<<<<<<<<<< */
	/* param error */
	ret = rs_socket_recv(socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_recv(0, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_recv(1, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = rs_peer_socket_recv(0, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> rs_socket_recv test case end <<<<<<<<<<< */


	/* >>>>>>> rs_socket_batch_close test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = rs_socket_batch_close(0, NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	ret = rs_socket_batch_close(0, &sock_close[i], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	sock_close[i].fd = -1;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> rs_socket_batch_close test case end <<<<<<<<<<< */


	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);
#endif

	usleep(1000);

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remote_ip.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = rs_socket_batch_close(0, &sock_close[i], 1);
#endif


	/* >>>>>>> rs_socket_listen_stop test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = rs_socket_listen_stop(NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	listen[0].phy_id = 15;
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);
	EXPECT_INT_NE(ret, 0);
	listen[0].phy_id = 0; //recover
	/* >>>>>>> rs_socket_listen_stop test case end <<<<<<<<<<< */


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_qp_status()
{
    struct rs_qp_status_info qp_info= { 0 };
    unsigned int rdev_index = 0;
    unsigned int phy_id = 0;
    uint32_t qpn = 0;
    int ret;

    ret = rs_get_qp_status(129, rdev_index, qpn, &qp_info);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* param error - status */
    ret = rs_get_qp_status(phy_id, rdev_index, qpn, NULL);
    EXPECT_INT_NE(ret, 0);

    /* param error - status */
    ret = rs_get_qp_status(phy_id, rdev_index, 444444, &qp_info);
    EXPECT_INT_NE(ret, 0);

    mocker_invoke(rs_qpn2qpcb, replace_rs_qpn2qpcb, 1);
    g_qp_cb_state = 1;
    mocker(rs_roce_query_qpc, 10, 1);
    ret = rs_get_qp_status(phy_id, rdev_index, qpn, &qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    return;
}

void tc_rs_get_notify_ba()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	unsigned int phy_id = 0;
	struct mr_info info = {0};
	unsigned int rdev_index = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	ret = rs_get_notify_mr_info(phy_id, rdev_index, &info);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_notify_mr_info(100000, rdev_index, &info);
	EXPECT_INT_NE(ret, 0);

	ret = rs_get_notify_mr_info(phy_id, rdev_index, NULL);
	EXPECT_INT_NE(ret, 0);

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_setup_sharemem()
{
	struct rs_cb rs_cb = {0};
	int ret;

	dl_hal_init();

	ret = rs_bind_hostpid(0, getpid());
	EXPECT_INT_NE(ret, 0);

	rs_cb.hccp_mode = NETWORK_OFFLINE;
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, dl_hal_get_device_info_910A, 20);
	ret = rs_setup_sharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	ret = rs_setup_sharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	rs_cb.grp_setup_flag = false;
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, dl_hal_get_device_info_sharemem, 20);
	ret = rs_setup_sharemem(&rs_cb, false, 0);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	rs_cb.grp_setup_flag = false;
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, dl_hal_get_device_info_sharemem, 20);
	mocker_invoke((stub_fn_t)dl_hal_query_dev_pid, dl_hal_query_dev_pid_sharemem, 20);
	ret = rs_setup_sharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	dl_hal_deinit();
}

void tc_rs_post_recv()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	uint32_t size;
	int try_num;
	void *addr, *addr2;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr2;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	usleep(1000);

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = rs_mr_dereg(phy_id, rdev_index, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
	free(addr2);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;


}

void tc_rs_send_wrlist_exp()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	uint32_t index;
	struct sg_list list;
	struct wr_info wrlist[1];
	int try_num;
	void *addr, *addr2;
	struct send_wr_rsp rs_wr_info[1];
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	struct rs_wrlist_base_info base_info;
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	base_info.phy_id = phy_id;
	base_info.rdev_index = rdev_index;
	base_info.qpn = qpn;
	base_info.key_flag = 0;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr2;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct rs_mr_cb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
	addr2_mr_cb->mr_info.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mr_info.addr = mr_reg_info.addr;
	addr2_mr_cb->mr_info.len = mr_reg_info.len;
	addr2_mr_cb->mr_info.rkey = mr_reg_info.lkey;

	struct rs_qp_cb *qp_cb = NULL;
	rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
	rs_list_add_tail(&addr2_mr_cb->list, &qp_cb->rem_mr_list);

	usleep(1000);

	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	wrlist[0].mem_list = list;
	wrlist[0].dst_addr = addr2;
	wrlist[0].op = 0;
	wrlist[0].send_flags = RS_SEND_FENCE;

	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num && ret == -2);
	EXPECT_INT_EQ(ret, 0);


	wrlist[0].dst_addr = NULL;
	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -ENOENT);
	wrlist[0].dst_addr = addr2;

	wrlist[0].mem_list.addr = NULL;
	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EFAULT);
	wrlist[0].mem_list.addr = addr;


	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = rs_mr_dereg(phy_id, rdev_index, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
	free(addr2);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_send_wrlist_normal()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	uint32_t index;
	struct sg_list list;
	struct wr_info wrlist[1];
	int try_num;
	void *addr, *addr2;
	struct send_wr_rsp rs_wr_info[1];
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	struct rs_wrlist_base_info base_info;
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create_normal(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	base_info.phy_id = phy_id;
	base_info.rdev_index = rdev_index;
	base_info.qpn = qpn;
	base_info.key_flag = 0;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr2;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct rs_mr_cb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
	addr2_mr_cb->mr_info.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mr_info.addr = mr_reg_info.addr;
	addr2_mr_cb->mr_info.len = mr_reg_info.len;
	addr2_mr_cb->mr_info.rkey = mr_reg_info.lkey;

	struct rs_qp_cb *qp_cb = NULL;
	rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
	rs_list_add_tail(&addr2_mr_cb->list, &qp_cb->rem_mr_list);

	usleep(1000);

	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	wrlist[0].mem_list = list;
	wrlist[0].dst_addr = addr2;
	wrlist[0].op = 0;
	wrlist[0].send_flags = RS_SEND_FENCE;

	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	wrlist[0].dst_addr = NULL;
	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -ENOENT);

	wrlist[0].dst_addr = addr2;
	wrlist[0].mem_list.addr = NULL;
	try_num = 3;
	do {
		ret =rs_send_wrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EFAULT);
	wrlist[0].mem_list.addr = addr;

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = rs_mr_dereg(phy_id, rdev_index, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
	free(addr2);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_send_wr()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	uint32_t index;
	struct sg_list list[2];
	struct send_wr wr;
	int try_num;
	void *addr, *addr2;
	struct send_wr_rsp rs_wr_info;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr2;
	try_num = 3;
	do {
		ret = rs_mr_reg(phy_id, rdev_index, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct rs_mr_cb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct rs_mr_cb));
	addr2_mr_cb->mr_info.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mr_info.addr = mr_reg_info.addr;
	addr2_mr_cb->mr_info.len = mr_reg_info.len;
	addr2_mr_cb->mr_info.rkey = mr_reg_info.lkey;

	struct rs_qp_cb *qp_cb = NULL;
	rs_qpn2qpcb(phy_id, rdev_index, qpn, &qp_cb);
	rs_list_add_tail(&addr2_mr_cb->list, &qp_cb->rem_mr_list);

	usleep(1000);

	list[0].addr = addr;
	list[0].len = RS_TEST_MEM_SIZE;
	list[1].addr = addr;
	list[1].len = RS_TEST_MEM_SIZE;
	wr.buf_list = list;
	wr.buf_num = 2;
	wr.dst_addr = addr2;
	wr.op = 0;
	wr.send_flag = RS_SEND_FENCE;

	ret =rs_send_wr(129, rdev_index, qpn, &wr, &rs_wr_info);
	EXPECT_INT_EQ(ret, -EINVAL);

	try_num = 3;
	do {
		ret =rs_send_wr(phy_id, rdev_index, qpn, &wr, &rs_wr_info);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
		try_num--;
	} while(try_num && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	/* Write with Notify test */
	wr.op = 0x16;
	try_num = 3;
	do {
		ret = rs_send_wr(phy_id, rdev_index, qpn, &wr, &rs_wr_info);
		if (ret == 0)
			break;
		usleep(SLEEP_TIME);
		try_num--;
	} while(try_num && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	/* Not Write with Notify opcode but still fill notify info */
	wr.op = 0;
	try_num = 3;
	do {
                ret = rs_send_wr(phy_id, rdev_index, qpn, &wr, &rs_wr_info);
                if (ret == 0)
                        break;
                usleep(SLEEP_TIME);
				try_num--;
        } while(try_num && ret == -2);
        EXPECT_INT_EQ(ret, 0);

	/* qpn error */
	ret =rs_send_wr(phy_id, rdev_index, 44444, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	list[0].len = 2147483649;
    list[1].len = 2147483649;
	ret =rs_send_wr(phy_id, rdev_index, qpn, &wr,  &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	list[0].len = RS_TEST_MEM_SIZE;
	list[1].len = RS_TEST_MEM_SIZE;

	/* addr error, cannot find mrcb */
	list[0].addr = 5555;
	ret =rs_send_wr(phy_id, rdev_index, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	list[0].addr = addr;

	/* addr error, cannot find remote mrcb */
	wr.dst_addr = 5555;
	ret =rs_send_wr(phy_id, rdev_index, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	wr.dst_addr = addr2;


	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = rs_mr_dereg(phy_id, rdev_index, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_mr_dereg(phy_id, rdev_index, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
	free(addr2);


	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_dfx()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct rs_qp_cb qp_cb_4;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_ssl_test1()
{
    int ret;
    uint32_t phy_id = 0;
    int flag = 0; /* RC */
    uint32_t qpn, qpn2;
    int i;
    struct rs_init_config cfg = {0};
    struct socket_listen_info listen[2] = {0};
    struct socket_connect_info conn[2] = {0};
    struct rs_socket_close_info_t sock_close[2] = {0};
    struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
	int try_num;

    cfg.chip_id = 0;
    cfg.hccp_mode = NETWORK_OFFLINE;
    ret = rs_init(&cfg);
    EXPECT_INT_EQ(ret, 0);

    listen[0].phy_id = 10;
	listen[0].port = 16666;
    ret = rs_socket_listen_start(&listen[0], 1);
    EXPECT_INT_NE(ret, 0);

    listen[0].phy_id = 0;
	listen[0].family = AF_INET;
    listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
    ret = rs_socket_listen_start(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
    rs_socket_white_list_add(rdev_info, &white_list, 1);;

    conn[0].phy_id = 0;
	conn[0].family = AF_INET;
    conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
    conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
    ret = rs_socket_batch_connect(&conn[0], 1);
	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");


    ret = rs_deinit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    return;
}

void tc_rs_get_ifaddrs()
{
	dl_hal_init();
	int ret;
	int phy_id = 0;
	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 0;
	unsigned int ifaddr_num = 4;
	struct ifaddr_info ifaddr_infos[4] = {0};
	ret = rs_get_ifaddrs(ifaddr_infos, &ifaddr_num, phy_id);
	free(g_rs_cb);
	g_rs_cb = NULL;
	dl_hal_deinit();
	return;
}

void tc_rs_get_ifaddrs_v2()
{
	dl_hal_init();
	int ret;
	int phy_id = 0;
	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 0;
	unsigned int ifaddr_num = 4;
	struct interface_info interface_infos[4] = {0};
	ret = rs_get_ifaddrs_v2(interface_infos, &ifaddr_num, phy_id, 0);
	free(g_rs_cb);
	g_rs_cb = NULL;
	dl_hal_deinit();
	return;
}

void tc_rs_get_ifnum()
{
	dl_hal_init();
	int ret;
	int phy_id = 0;
	unsigned int ifnum = 4;
	bool is_all = false;

	ret = rs_get_ifnum(phy_id, is_all, &ifnum);

	mocker(rs_get_device_type, 1, 1);
	mocker((stub_fn_t)rs_check_dst_interface, 100, -1);
	ret = rs_fill_ifnum(phy_id, is_all, &ifnum, 0);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	mocker(rs_get_device_type, 1, 1);
    mocker((stub_fn_t)rs_check_dst_interface, 100, 1);
	ret = rs_fill_ifnum(phy_id, is_all, &ifnum, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	dl_hal_deinit();
	return;
}

void tc_rs_get_interface_version()
{
	int version;
	int ret = rs_get_interface_version(0, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_get_interface_version(0, &version);
	EXPECT_INT_EQ(ret, 0);
}

extern int kmc_dec_data(struct kmc_enc_info *enc_info, unsigned char *outbuf, unsigned int *size_out);
void tc_rs_ssl_test2()
{
	int ret;
	uint32_t chip_id = 0;
	struct rs_cb *rs_cb;
	struct rs_init_config cfg = {0};
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_dev2rscb(chip_id, &rs_cb, false);
	struct rs_sec_para *rs_para = (struct rs_sec_para *)calloc(1, sizeof(struct rs_sec_para));
	struct tls_cert_mng_info *mng_info = (struct tls_cert_mng_info *)calloc(1, sizeof(struct tls_cert_mng_info));

	mng_info->ky_enc_len = 1678;
	rs_get_pridata(rs_cb, rs_para, mng_info);

	mocker(creat, 20, 0);
	mocker(write, 20, 0);
	rs_get_pridata(rs_cb, rs_para, mng_info);
	mocker_clean();

	mocker(kmc_dec_data, 20, 0);
	rs_get_pridata(rs_cb, rs_para, mng_info);
	mocker_clean();
	free(mng_info);
	mng_info = NULL;
	free(rs_para);
	rs_para = NULL;
	return;
}

struct rs_conn_info g_conn = {0};
int stub_rs_fd2conn(int fd, struct rs_conn_info **conn)
{

    *conn = &g_conn;
    return 0;
}



void tc_rs_tls_inner_enable()
{
    int ret;
    struct rs_cb rscb = {0};
    rscb.ssl_enable = 1;

    ret = rs_tls_inner_enable(&rscb, 1);
    EXPECT_INT_NE(0, ret);

	ret = rs_tls_inner_enable(&rscb, 0);


}

void tc_rs_ssl_ca_ky_init()
{
	struct rs_cb rscb = {0};
	rscb.server_ssl_ctx = SSL_CTX_new(TLS_server_method());
	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 0);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 0);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 1);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 1);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 1);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.server_ssl_ctx, &rscb);
	mocker_clean();

	SSL_CTX_free(rscb.server_ssl_ctx);
    rscb.server_ssl_ctx = NULL;

	return;
}

void tc_rs_rs_ssl_crl_init()
{
    int ret;
    struct rs_cb rscb = {0};
    rscb.ssl_enable = 1;
    struct rs_cert_skid_subject_cb *skid_subject_cb = malloc(sizeof(struct rs_cert_skid_subject_cb));
    rscb.skid_subject_cb = skid_subject_cb;
    rscb.hccp_mode = NETWORK_PEER_ONLINE;
    rscb.chip_id = 0;
    struct tls_cert_mng_info mng_info = {0};

    rscb.client_ssl_ctx = SSL_CTX_new(TLS_client_method());
    ret = rs_ssl_crl_init(rscb.client_ssl_ctx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    rscb.server_ssl_ctx = SSL_CTX_new(TLS_client_method());
    ret = rs_ssl_crl_init(rscb.server_ssl_ctx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    rs_ssl_deinit(&rscb);
}

void tc_rs_tls_peer_cert_verify()
{
    int ret;
    SSL ssl = {0};

    ret = rs_tls_peer_cert_verify(&ssl, &g_rs_cb);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)SSL_get_verify_result, 10, X509_V_ERR_CERT_HAS_EXPIRED);
    ret = rs_tls_peer_cert_verify(&ssl, &g_rs_cb);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();

    mocker((stub_fn_t)SSL_get_verify_result, 10, 11);
    ret = rs_tls_peer_cert_verify(&ssl, &g_rs_cb);
    EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();
}

void tc_rs_ssl_err_string()
{
	rs_ssl_err_string(1, 1);
	return;
}

void tc_tls_get_cert_chain()
{
    int ret;
    struct tls_cert_mng_info mng_info;
    mng_info.cert_count = 2;
    X509_STORE_CTX ctx = {0};
    X509_STORE store = {0};
    struct rs_certs certs = {0};

    ret = tls_get_cert_chain(&store, &certs, &mng_info);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_ssl_skid_get_from_chain()
{
    int ret;
    struct tls_cert_mng_info mng_info;
    mng_info.cert_count = 2;
    struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};
    struct rs_cb rscb = {0};
    struct rs_cert_skid_subject_cb skid_subject_cb = {0};
    rscb.skid_subject_cb = &skid_subject_cb;

    ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
    EXPECT_INT_EQ(0, ret);

    rscb.skid_subject_cb = NULL;
    ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_ssl_skids_subjects_get, 20, -1);
    mocker(memset_s, 20, -1);
	ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

    free(rscb.skid_subject_cb);
    rscb.skid_subject_cb = NULL;
}

void tc_rs_check_pridata()
{
    int ret;
    struct tls_cert_mng_info mng_info = {0};;
    mng_info.cert_count = 2;
    struct rs_cb rscb = {0};
    struct rs_cert_skid_subject_cb skid_subject_cb = {0};
    rscb.skid_subject_cb = &skid_subject_cb;
    rscb.hccp_mode = NETWORK_OFFLINE;
    rscb.chip_id = 0;
    rscb.server_ssl_ctx = SSL_CTX_new(TLS_server_method());
	mng_info.ky_enc_len = 1678;
    mng_info.pwd_len = 16;
    ret = rs_check_pridata(rscb.server_ssl_ctx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    SSL_CTX_free(rscb.server_ssl_ctx);
    rscb.server_ssl_ctx = NULL;
}

void tc_rs_peer_get_ifaddrs()
{
    bool is_all = false;
    int ret;

    ret = rs_peer_get_ifaddrs(NULL, NULL, 0);
    EXPECT_INT_EQ(-EINVAL, ret);

    struct interface_info interface_infos[100] = {0};
    unsigned int num = 100;
	unsigned int if_num = 1000;

	struct rs_init_config cfg = {0};
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	ret = rs_peer_get_ifnum(0, &if_num);
	EXPECT_INT_EQ(ret, 0);

    ret = rs_peer_get_ifaddrs(interface_infos, &num, 0);
    EXPECT_INT_EQ(0, ret);
	EXPECT_INT_EQ(if_num, num);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

    ret = rs_get_ifaddrs(NULL, NULL, 0);
    EXPECT_INT_EQ(-EINVAL, ret);

    char ifa_name = 0;
    ret = rs_check_dst_interface(0, &ifa_name, RS_HARDWARE_PCIE, is_all);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_peer_get_ifnum()
{
    int ret;
	int num = 0;

    ret = rs_peer_get_ifnum(0, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = rs_peer_get_ifnum(0, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_socket_nodeid2vnic()
{
    int ret;
    ret = rs_socket_nodeid2vnic(0, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_server_valid_async_init()
{
    int ret;
    struct rs_conn_info conn;
    struct socket_wlist_info_t white_list_expect;
    conn.state = 7;
    strcpy(conn.tag, "1234");
    conn.client_ip.family = AF_INET;
    conn.client_ip.bin_addr.addr.s_addr = 16;
    ret = rs_server_valid_async_init(0, &conn, &white_list_expect);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_server_send_wlist_check_result()
{
    struct rs_conn_info conn = {0};
	int ret;

	g_rs_cb = calloc(1, sizeof(struct rs_cb));

	ret = rs_server_send_wlist_check_result(&conn, 0);
	EXPECT_INT_NE(0, ret);

	ret = rs_server_send_wlist_check_result(&conn, 1);
	EXPECT_INT_NE(0, ret);
	free(g_rs_cb);
	g_rs_cb = NULL;
}

void tc_rs_set_fd_nonblock()
{
    int ret;
    ret = rs_set_fd_nonblock(-1);
    EXPECT_INT_EQ(-EFILEOPER, ret);
}

void tc_rs_epoll_event_ssl_listen_in_handle()
{
    struct rs_cb rs_cb;
    struct rs_listen_info listen_info;
    struct sockaddr_in remote_ip;
    (void)rs_epoll_event_ssl_listen_in_handle(&rs_cb, &listen_info, 0, remote_ip);
}

void tc_rs_ssl_recv_tag_in_handle()
{
    struct rs_accept_info accept_info;
    accept_info.server_ip_addr.family = AF_INET;
    accept_info.server_ip_addr.bin_addr.addr.s_addr = 1;
    accept_info.client_ip_addr.family = AF_INET;
    accept_info.client_ip_addr.bin_addr.addr.s_addr = 1;
    accept_info.conn_fd = 1;
    accept_info.sock_port = 0;
    struct rs_conn_info conn_tmp;
    char tag[256] = {0};
    memcpy_s(conn_tmp.tag, 256, tag, 256);
    mocker(SSL_read, 20, 256);
    (void)rs_ssl_recv_tag_in_handle(&accept_info, &conn_tmp);
    mocker_clean();
}

int *stub__errno_locations()
{
    static int err_no = 0;

    err_no = EADDRINUSE;
    return &err_no;
}

int *stub__errno_location()
{
    static int err_no = 0;

    err_no = EAGAIN;
    return &err_no;
}

void tc_rs_socket_listen_bind_listen()
{
    int ret;
    struct rs_conn_cb conn_cb;
    struct socket_listen_info conn;
    struct rs_listen_info listen_info;
    conn.local_ip.addr.s_addr = 1;

    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(-ESYSFUNC, ret);

    mocker(setsockopt, 20, 0);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

    mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

    mocker(setsockopt, 20, 0);
    mocker(bind, 20, 1);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
	mocker_clean();

	mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
    mocker(listen, 20, 1);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

	mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
    mocker(listen, 20, 1);
	mocker_invoke(__errno_location, stub__errno_locations, 1);
    ret = rs_socket_listen_bind_listen(-1, &conn_cb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EADDRINUSE, ret);
    mocker_clean();
}


void tc_rs_recv_wrlist()
{
	int ret;
	struct rs_wrlist_base_info base_info = {0};
	struct recv_wrlist_data wr = {0};
    unsigned int recv_num = 0;
	unsigned int complete_num = 0;
	ret = rs_recv_wrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EINVAL, ret);

	recv_num = 1;
	mocker(rs_qpn2qpcb, 1, -1);
	ret = rs_recv_wrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EACCES, ret);
	mocker_clean();

	mocker(rs_qpn2qpcb, 1, 0);
	mocker(rs_drv_post_recv, 1, 0);
	ret = rs_recv_wrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	return;
}

void tc_rs_drv_post_recv()
{
	int ret;
	struct rs_qp_cb qp_cb = {0};
	struct ibv_qp ib_qp = {0};
	struct recv_wrlist_data wr = {0};
	unsigned int recv_num = 0;
	unsigned int complete_num = 0;

	qp_cb.ib_qp = &ib_qp;

	ret = rs_drv_post_recv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EINVAL, ret);

	recv_num = 1;
	mocker(rs_ibv_post_recv, 1, 0);
	ret = rs_drv_post_recv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	mocker(rs_ibv_post_recv, 1, -ENOMEM);
	ret = rs_drv_post_recv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	mocker(rs_ibv_post_recv, 1, -1);
	ret = rs_drv_post_recv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	mocker(calloc, 1, NULL);
	ret = rs_drv_post_recv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-ENOSPC, ret);
	mocker_clean();
	return;
}

void tc_rs_drv_reg_notify_mr()
{
	int ret;
	struct rs_rdev_cb rdev_cb = {0};
	rdev_cb.notify_type = NO_USE;

	ret = rs_drv_reg_notify_mr(&rdev_cb);
	EXPECT_INT_EQ(0, ret);

	rdev_cb.notify_type = 1000;
	ret = rs_drv_reg_notify_mr(&rdev_cb);
	EXPECT_INT_EQ(-EINVAL, ret);

	rdev_cb.notify_type = EVENTID;
	mocker(rs_ibv_exp_reg_mr, 1, NULL);
	ret = rs_drv_reg_notify_mr(&rdev_cb);
	EXPECT_INT_EQ(-EACCES, ret);
	mocker_clean();
	return;
}

void tc_rs_drv_query_notify_and_alloc_pd()
{
	int ret;
	struct rs_rdev_cb rdev_cb = {0};
	rdev_cb.notify_type = EVENTID;

	mocker(rs_ibv_exp_query_notify, 1, -1);
	ret = rs_drv_query_notify_and_alloc_pd(&rdev_cb);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();
	return;
}

void tc_rs_send_normal_wrlist()
{
	int ret;
	struct rs_qp_cb qp_cb = {0};
	struct wr_info wr_list = {0};
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	unsigned int key_flag = 1;

	mocker(rs_ibv_post_send, 1, 0);
	ret = rs_send_normal_wrlist(&qp_cb, &wr_list, send_num, &complete_num, key_flag);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	wr_list.mem_list.len = 0xffffffff;
	ret = rs_send_normal_wrlist(&qp_cb, &wr_list, send_num, &complete_num, key_flag);
	EXPECT_INT_EQ(-EINVAL, ret);
	return;
}

void tc_rs_connect_handle()
{
    int ret;
    struct rs_init_config cfg;
    struct socket_connect_info conn[2] = {0};

    /* resource prepare... */
    cfg.hccp_mode = NETWORK_OFFLINE;
    cfg.chip_id = 0;
    cfg.white_list_status = 1;
    ret = rs_init(&cfg);
    EXPECT_INT_EQ(ret, 0);

    mocker(rs_connect_bind_client, 100, -99);
    conn[0].phy_id = 0;
    conn[0].family = AF_INET6;
    inet_pton(AF_INET6, "::1", &conn[0].local_ip.addr6);
    inet_pton(AF_INET6, "::1", &conn[0].remote_ip.addr6);
    strcpy(conn[0].tag, "5678");
    conn[0].port = 16666;
    ret = rs_socket_batch_connect(&conn[0], 1);
    usleep(SLEEP_TIME);
    mocker_clean();

    /* resource free... */
    ret = rs_deinit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    ret = rs_connect_handle(&cfg);
    EXPECT_INT_EQ(ret, NULL);
    return;
}

extern void rs_close_roce_user_so(void);
void tc_rs_roce_user_api_init()
{
	rs_roce_user_api_init();
	rs_close_roce_user_so();
}

int rs_dev2rscb_stub(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
{
	struct rs_cb rs_cb_tmp = {0};
	*rs_cb = &rs_cb_tmp;
	return 0;
}
void tc_rs_notify_cfg_set()
{
	unsigned int dev_id = 0;
	unsigned long long va = 0x10000;
	unsigned long long size = 8192;
	int ret;

	ret = rs_notify_cfg_set(129, va, size);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_drv_send_exp()
{
	struct rs_qp_cb qp_cb = {0};
	struct rs_mr_cb mr_cb = {0};
	struct rs_mr_cb rem_mr_cb = {0};
	struct send_wr wr = {0};
	struct send_wr_rsp wr_rsp = {0};
	int ret = 0;

	mocker(rs_ibv_exp_post_send, 1, 0);
	qp_cb.qp_mode = 2;
	ret = rs_drv_send_exp(&qp_cb, &mr_cb, &rem_mr_cb, &wr, &wr_rsp);
	mocker_clean();

	mocker(rs_ibv_exp_post_send, 1, 0);
	qp_cb.qp_mode = 1;
	ret = rs_drv_send_exp(&qp_cb, &mr_cb, &rem_mr_cb, &wr, &wr_rsp);
	mocker_clean();
	return;
}

// lkk
void tc_rs_drv_normal_qp_create_init()
{
	struct ibv_qp_init_attr qp_init_attr = {0};
	struct rs_qp_cb qp_cb = {0};
	struct ibv_port_attr attr = {0};
	struct rs_cb rs_cb = {0};
	struct rs_rdev_cb rdev_cb = {0};
	int ret;
	qp_cb.rdev_cb = &rdev_cb;
	qp_cb.rdev_cb->rs_cb = &rs_cb;
	qp_cb.rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE;

	qp_cb.tx_depth = 10;
	qp_cb.rx_depth = 10;

	mocker(memset_s, 10, 0);
	qp_cb.qp_mode = 2;
	ret = rs_drv_normal_qp_create_init(&qp_init_attr, &qp_cb, &attr);
	qp_cb.rdev_cb->rs_cb->hccp_mode == NETWORK_PEER_ONLINE;
	ret = rs_drv_normal_qp_create_init(&qp_init_attr, &qp_cb, &attr);
	mocker_clean();

	struct ibv_qp ib_pd = {0};
	struct ibv_qp ib_qp = {0};
	struct ibv_cq ib_send_cq = {0};
	struct ibv_cq ev_cq = {0};
	struct ibv_wc wc = {0};
	// struct rq_attr attr = {0};


	qp_cb.ib_qp= &ib_qp;
	qp_cb.ib_pd= &ib_pd;
	qp_cb.ib_send_cq = &ib_send_cq;
	rs_cb.chip_id = 0;

    mocker(rs_ibv_query_qp, 5, -1);
	ret = rs_drv_qp_normal(&qp_cb, 0);
	mocker_clean();

	ret = rs_drv_post_recv(NULL, NULL, 0, NULL);
	EXPECT_INT_EQ(ret, -22);

	mocker(calloc, 1, NULL);
	ret = rs_drv_post_recv(NULL, NULL, 1, NULL);
	EXPECT_INT_EQ(ret, -28);
    mocker_clean();

	rs_cqe_callback_process(&qp_cb, &wc, &ev_cq);

	return;
}

void tc_rs_drv_exp_qp_create()
{
	struct rs_qp_cb qp_cb = {0};
	struct rs_rdev_cb rdev_cb = {0};
	struct ibv_pd ib_pd = {0};
	struct rs_cb rs_cb = {0};
	int qp_mode;

	qp_cb.rdev_cb = &rdev_cb;
	qp_cb.ib_pd = &ib_pd;
	qp_cb.rdev_cb->rs_cb = &rs_cb;

	mocker(rs_drv_exp_qp_create_init, 20, 0);
	mocker(rs_ibv_exp_create_qp, 20, &ib_pd);
	mocker(rs_ibv_query_qp, 20, 1);
	mocker(rs_ibv_destroy_qp, 20, 1);
	qp_mode = 2;
	rs_drv_exp_qp_create(&qp_cb, qp_mode);
	mocker_clean();

	mocker(rs_drv_exp_qp_create_init, 20, 0);
	mocker(rs_ibv_exp_create_qp, 20, &ib_pd);
	mocker(rs_ibv_query_qp, 20, 1);
	mocker(rs_ibv_destroy_qp, 20, 1);
	qp_mode = 1;
	rs_drv_exp_qp_create(&qp_cb, qp_mode);
	mocker_clean();
	return;
}

void tc_rs_drv_exp_qp_create_init()
{
	struct ibv_exp_qp_init_attr qp_init_attr = {0};
	struct rs_qp_cb qp_cb = {0};
	struct ibv_port_attr attr = {0};
	struct rs_cb rs_cb = {0};
	struct rs_rdev_cb rdev_cb = {0};
	int ret;
	qp_cb.rdev_cb = &rdev_cb;
	qp_cb.rdev_cb->rs_cb = &rs_cb;
	qp_cb.rdev_cb->rs_cb->hccp_mode == NETWORK_ONLINE;

	qp_cb.tx_depth = 10;
	qp_cb.rx_depth = 10;

	mocker(memset_s, 10, 0);
	qp_cb.qp_mode = 2;
	ret = rs_drv_exp_qp_create_init(&qp_init_attr, &qp_cb, &attr);
	mocker_clean();

	return;
}

void tc_rs_qpcb_init()
{
	struct rs_rdev_cb rdev_cb = {0};
	struct rs_qp_norm qp_norm = {0};
	struct rs_qp_cb qp_cb = {0};
	struct rs_cb rs_cb = {0};
	int qp_mode = 1;
	int ret = 0;

	qp_norm.qp_mode = 1;
	rdev_cb.rs_cb = &rs_cb;

	ret = rs_qpcb_init(&rdev_cb, &qp_cb, &qp_norm);

	return;
}

// void tc_rs_qp_query_info()
// {
// 	unsigned int phy_id = 15;
// 	unsigned int rdev_index = 0;
// 	struct rs_rdev_cb rdev_cb = {0};
// 	int qp_mode = 1;
// 	int ret;

// 	mocker(rsGetLocalDevIDByHostDevID, 1, 0);
// 	mocker(rs_dev2rscb, 1, 0);
// 	mocker(rs_get_rdev_cb, 1, 0);
// 	ret = rs_qp_query_info(phy_id, rdev_index, &&rdev_cb, qp_mode);
//  mocker clean;

// 	return;
// }
void tc_rs_register_mr()
{
	int ret;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;
	unsigned int qpn = 0;


	struct rs_init_config cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;

	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct rdma_mr_reg_info mr_reg_info1 = {0};
	mr_reg_info1.addr = 0xabcdef;
	mr_reg_info1.len = RS_TEST_MEM_SIZE;
	mr_reg_info1.access = RS_ACCESS_LOCAL_WRITE;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = g_rs_cb;

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	void *mr_handle = NULL;
	void *mr_handle1 = NULL;
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret =  rs_register_mr(phy_id, rdev_index, &mr_reg_info, &mr_handle);
	EXPECT_INT_EQ(0, ret);

	ret =  rs_register_mr(1, rdev_index, &mr_reg_info, &mr_handle);
	EXPECT_INT_NE(0, ret);

	mocker(rs_drv_mr_reg, 1, NULL);
	ret =  rs_register_mr(phy_id, rdev_index, &mr_reg_info1, &mr_handle1);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret =  rs_deregister_mr(mr_handle);
	EXPECT_INT_EQ(0, ret);

	ret =  rs_deregister_mr(NULL);
	EXPECT_INT_NE(0, ret);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}

void tc_rs_epoll_ctl_add()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;
    struct rs_list_head list = {0};

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    g_rs_cb->heterog_tcp_fd_list = list;

    mocker((stub_fn_t)calloc, 5, NULL);
    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -1);

    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLERR);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_add_01()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;
    struct rs_list_head list = {0};

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    g_rs_cb->heterog_tcp_fd_list = list;

    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_add_03()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    RS_INIT_LIST_HEAD(&g_rs_cb->heterog_tcp_fd_list);
    g_rs_cb->heterog_tcp_fd_list.next = &(g_rs_cb->heterog_tcp_fd_list);
    g_rs_cb->heterog_tcp_fd_list.prev = &(g_rs_cb->heterog_tcp_fd_list);

    mocker((stub_fn_t)rs_epoll_ctl, 5, -1);
    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;
    struct rs_list_head list = {0};

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    g_rs_cb->heterog_tcp_fd_list = list;

    ret = rs_epoll_ctl_mod((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -1);

    ret = rs_epoll_ctl_mod((const void *)fd_handle, RA_EPOLLERR);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_01()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;
    struct rs_list_head list = {0};

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    g_rs_cb->heterog_tcp_fd_list = list;

    ret = rs_epoll_ctl_mod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_02()
{
    struct socket_peer_info fd_handle[1];
    struct rs_conn_cb conn_cb = {0};
    int ret;
    struct rs_list_head list = {0};

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->fd_map = (const void **)&fd_handle;
    g_rs_cb->conn_cb = conn_cb;
    g_rs_cb->heterog_tcp_fd_list = list;


    mocker((stub_fn_t)rs_epoll_ctl, 5, 0);
    ret = rs_epoll_ctl_mod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_03()
{
    struct socket_peer_info fd_handle[1];
    int ret;

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    g_rs_cb = NULL;

    mocker((stub_fn_t)rs_epoll_ctl, 5, 0);
    ret = rs_epoll_ctl_add((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -22);

    g_rs_cb = NULL;

    ret = rs_epoll_ctl_mod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -22);

    g_rs_cb = NULL;
    ret = rs_epoll_ctl_del(0);
    EXPECT_INT_EQ(ret, -22);

    rs_set_ctx(0);
    mocker_clean();
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_del()
{
    int ret;
    struct rs_list_head list = {0};

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));

    g_rs_cb->heterog_tcp_fd_list = list;
    g_rs_cb->heterog_tcp_fd_list.next = &(g_rs_cb->heterog_tcp_fd_list);
    g_rs_cb->heterog_tcp_fd_list.prev = &(g_rs_cb->heterog_tcp_fd_list);
    ret = rs_epoll_ctl_del(0);
    EXPECT_INT_EQ(ret, -1);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_ctl_del_01()
{
    int ret;
    struct rs_list_head list = {0};

    RS_INIT_LIST_HEAD(&list);
    g_rs_cb = malloc(sizeof(struct rs_cb));

    g_rs_cb->heterog_tcp_fd_list = list;
    g_rs_cb->heterog_tcp_fd_list.next = &(g_rs_cb->heterog_tcp_fd_list);
    g_rs_cb->heterog_tcp_fd_list.prev = &(g_rs_cb->heterog_tcp_fd_list);

    mocker((stub_fn_t)rs_epoll_ctl, 5, 0);
    ret = rs_epoll_ctl_del(0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}
void tc_rs_set_tcp_recv_callback()
{
    (void)rs_set_tcp_recv_callback(NULL);

	g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->hccp_mode = 1;

    (void)rs_set_tcp_recv_callback(NULL);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

void tc_rs_epoll_event_in_handle()
{
    struct rs_cb rs_cb = {0};
    struct epoll_event events = {0};
    rs_cb.ssl_enable = 1;

    mocker((stub_fn_t)rs_epoll_event_listen_in_handle, 1, -ENODEV);
    mocker((stub_fn_t)rs_epoll_event_ssl_accept_in_handle, 5, 0);
    (void)rs_epoll_event_in_handle(&rs_cb, &events);
    mocker_clean();

    rs_cb.ssl_enable = 0;

    mocker((stub_fn_t)rs_epoll_event_listen_in_handle, 1, -ENODEV);
    mocker((stub_fn_t)rs_epoll_event_qp_mr_in_handle, 1, -ENODEV);
    mocker((stub_fn_t)rs_epoll_event_heterog_tcp_recv_in_handle, 5, 0);
    (void)rs_epoll_event_in_handle(&rs_cb, &events);
    mocker_clean();

    return;
}

void tc_rs_epoll_tcp_recv()
{
    int ret;
    struct rs_cb rs_cb = {0};
    struct socket_peer_info fd_handle[1];
    int callback = 0;

    fd_handle[0].phy_id = 0;
    fd_handle[0].fd = 0;

    g_rs_cb = malloc(sizeof(struct rs_cb));
    rs_cb.fd_map = (const void **)&fd_handle;

    rs_cb.tcp_recv_callback = rs_set_tcp_recv_callback;
    ret =rs_epoll_tcp_recv(&rs_cb, 0);
    EXPECT_INT_EQ(ret, 0);

    rs_cb.tcp_recv_callback = NULL;
    ret =rs_epoll_tcp_recv(&rs_cb, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(g_rs_cb);
    g_rs_cb = NULL;
    return;
}

int stub_ibv_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context)
{
	*cq = NULL;
	return 0;
}

void tc_rs_drv_poll_cq_handle()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t qp_mode = 1;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	int i;
	struct rs_init_config cfg = {0};
    struct rs_qp_cb *qp_cb = NULL;
	struct ibv_cq *ib_send_cq_t, *ib_recv_cq_t;
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = qp_mode;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);

    ret = rs_qpn2qpcb(0, rdev_index, resp.qpn, &qp_cb);
	// rs_drv_poll_cq_handle(qp_cb);


	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	ib_send_cq_t = qp_cb->ib_send_cq;
	qp_cb->ib_send_cq = NULL;

	/* reach end ? */
	mocker((stub_fn_t)ibv_req_notify_cq, 10, 0);
	mocker((stub_fn_t)ibv_poll_cq, 10, 0);
	rs_drv_poll_cq_handle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_req_notify_cq, 10, 1);
	rs_drv_poll_cq_handle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, -1);
	rs_drv_poll_cq_handle(qp_cb);
	mocker_clean();

	/* callback_flag = 1 */
	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, 0);
	rs_drv_poll_cq_handle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, -1);
	rs_drv_poll_cq_handle(qp_cb);
	mocker_clean();

	qp_cb->ib_send_cq = ib_send_cq_t;
//rs_poll_cq_handle end

	qp_cb->rdev_cb->rs_cb->hccp_mode = NETWORK_PEER_ONLINE;
	struct rs_cq_context srq_context = {0};
	qp_cb->srq_context = &srq_context;
	mocker((stub_fn_t)rs_ibv_get_cq_event, 10, 0);
	rs_drv_poll_srq_cq_handle(qp_cb);
	mocker_clean();

	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

struct rs_mr_cb *g_mr_cb;
struct ibv_mr *g_ib_mr;

int stub_rs_get_mrcb(struct rs_qp_cb *qp_cb, uint64_t addr, struct rs_mr_cb **mr_cb,
    struct rs_list_head *mr_list)
{
	*mr_cb = g_mr_cb;
	return 0;
}


int stub_rs_ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp)
{
	int wqe_index = 2;
	exp_rsp->wqe_index = wqe_index;
	exp_rsp->db_info = 1;
	return 0;
}

void tc_rs_send_exp_wrlist()
{
	dl_hal_init();
	struct rs_qp_cb qp_cb = {0};
	struct wr_info wrlist[1];
	unsigned int send_num = 1;
	struct send_wr_rsp rs_wr_info[1];
	unsigned int complete_num = 0;
	struct db_info db = {0};
	struct ibv_qp ib_qp = {0};
	struct wqe_info wqe_tmp = {0};
	struct send_wr_rsp wr_rsp = {0};
	struct wr_exp_rsp exp_rsp = {0};

	int ret;

	g_mr_cb = malloc(sizeof(struct rs_mr_cb));
	g_ib_mr = malloc(sizeof(struct ibv_mr));
	g_mr_cb->ib_mr = g_ib_mr;

	wrlist[0].mem_list.len = RS_TEST_MEM_SIZE;
	wrlist[0].mem_list.addr = 0x15;
	wrlist[0].op = 1;
	qp_cb.send_wr_num = 0;
	ib_qp.qp_num = 1;
	qp_cb.ib_qp= &ib_qp;
	qp_cb.sq_index = 1;
	mocker_invoke((stub_fn_t)rs_get_mrcb, stub_rs_get_mrcb, 2);
	mocker_invoke(rs_ibv_exp_post_send, stub_rs_ibv_exp_post_send, 1);
	qp_cb.qp_mode = 3;
	rs_wr_info[0].db = db;
	rs_wr_info[0].wqe_tmp = wqe_tmp;
	ret = rs_send_exp_wrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
	mocker_clean();

    mocker_invoke((stub_fn_t)rs_get_mrcb, stub_rs_get_mrcb, 2);
    mocker(rs_ibv_exp_post_send, 1, -12);
    ret = rs_send_exp_wrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)rs_get_mrcb, stub_rs_get_mrcb, 2);
    mocker(rs_ibv_exp_post_send, 1, -1);
    ret = rs_send_exp_wrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

	mocker_invoke((stub_fn_t)rs_get_mrcb, stub_rs_get_mrcb, 2);
	mocker_invoke(rs_ibv_exp_post_send, stub_rs_ibv_exp_post_send, 1);
	wrlist[0].op = 0xf6;
	ret = rs_send_exp_wrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
	mocker_clean();

	free(g_ib_mr);
	free(g_mr_cb);

	EXPECT_INT_EQ(ret, 0);

	unsigned int result = rs_get_hccp_mode(0);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_socket_init(NULL, 0);
	EXPECT_INT_EQ(ret, -22);

    unsigned int vnic_ip[8] = {0};
	ret = rs_socket_init(vnic_ip, 8);
	EXPECT_INT_EQ(ret, 0);

	mocker(memcpy_s, 5, -1);
	ret = rs_socket_init(vnic_ip, 8);
	EXPECT_INT_EQ(ret, -256);

    struct rdev rdev_info;
	rdev_info.phy_id = 128;
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, -22);

    rdev_info.phy_id = 0;
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);

	rdev_info.family = AF_INET6;
	mocker(rsGetLocalDevIDByHostDevID, 5, 0);
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);
	mocker_clean();

	rdev_info.family = AF_INET;
	mocker(rsGetLocalDevIDByHostDevID, 5, 0);
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);
	mocker_clean();

	rdev_info.phy_id = 128;
	struct rs_cb rs_cb ={0};
    ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	EXPECT_INT_EQ(ret, -22);

	rdev_info.phy_id = 0;
    ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	EXPECT_INT_EQ(ret, -19);

	rdev_info.phy_id = 0;
	mocker(rsGetLocalDevIDByHostDevID, 5, -1);
    ret = rs_get_rs_cb(rdev_info.phy_id, &rs_cb);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

    ret = rs_rdev_deinit(0, 0, 0);
	EXPECT_INT_EQ(ret, -19);

    ret = rs_rdev_deinit(128, 0, 0);
	EXPECT_INT_EQ(ret, -22);

	ret = rs_get_vnic_ip(128, NULL);
	EXPECT_INT_EQ(ret, -22);

	ret = rs_get_vnic_ip(8, NULL);
	EXPECT_INT_EQ(ret, -22);

	ret = rs_get_vnic_ip(8, vnic_ip);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_interface_version(0, NULL);
	EXPECT_INT_EQ(ret, -22);

	unsigned int version[1] ={0};
	ret = rs_get_interface_version(0, version);
	EXPECT_INT_EQ(ret, 0);


	dl_hal_deinit();
    return;
}

void tc_tls_abnormal()
{
	struct rs_conn_info conn = {0};
	SSL ssl = {0};
	conn.ssl = &ssl;

	rs_drv_socket_recv(-1, NULL, 0, 0);
	rs_drv_socket_send(-1, NULL, 0, 0);
	rs_drv_ssl_bind_fd(&conn, 0);
}

void tc_rs_get_qp_context()
{
	void *qp, *send_cq, *recv_cq;
	rs_get_qp_context(RS_MAX_DEV_NUM, 0, 0, &qp, &send_cq, &recv_cq);

	mocker(rs_qpn2qpcb, 1, -1);
	rs_get_qp_context(0, 0, 0, &qp, &send_cq, &recv_cq);
	mocker_clean();

	mocker_invoke(rs_qpn2qpcb, replace_rs_qpn2qpcb, 1);
	rs_get_qp_context(0, 0, 0, &qp, &send_cq, &recv_cq);
	mocker_clean();
}

void tc_rs_normal_qp_create()
{
	int ret;
	uint32_t phy_id = 0;
	uint32_t rdev_index = 0;
	int flag = 0; /* RC */
	int qp_mode = 1;
	uint32_t qpn, qpn2, qpn3;
	int i;
	int try_num = 10;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
	struct rs_qp_resp qp_resp = {0};
	struct rs_qp_resp qp_resp2 = {0};
    struct socket_wlist_info_t white_list;
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_PEER_ONLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	rs_ut_msg("___________________after listen:\n");
    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
    rs_socket_white_list_add(rdev_info, &white_list, 1);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	rs_ut_msg("___________________after connect:\n");

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

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
	attr.send_channel = NULL;
	attr.recv_channel = NULL;
    ret = rs_cq_create(phy_id, rdev_index, &attr);
    EXPECT_INT_EQ(ret, 0);

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

    mocker((stub_fn_t)rs_drv_normal_qp_create, 10, -ENOMEM);
    ret = rs_normal_qp_create(phy_id, rdev_index, &qp_init_attr, &qp_resp, &qp);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    ret = rs_normal_qp_create(phy_id, rdev_index, &qp_init_attr, &qp_resp, &qp);
    EXPECT_INT_EQ(0, ret);

    qp_init_attr.qp_context = NULL;
	ret = rs_normal_qp_create(phy_id, rdev_index, &qp_init_attr, &qp_resp, &qp);
	EXPECT_INT_EQ(-22, ret);

	rs_ut_msg("___________________after qp create:\n");

	/* >>>>>>> rs_qp_connect_async test case begin <<<<<<<<<<< */
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = rs_mr_reg(phy_id, rdev_index, qp_resp.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	/* >>>>>>> rs_qp_connect_async test case end <<<<<<<<<<< */

	rs_ut_msg("___________________after mr reg:\n");

	ret = rs_qp_connect_async(phy_id, rdev_index, qp_resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	rs_ut_msg("___________________after qp connect async:\n");
	usleep(SLEEP_TIME);
	rs_ut_msg("___________________after qp connect async & sleep:\n");

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

    struct ibv_cq *ib_send_cq2;
    struct ibv_cq *ib_recv_cq2;
    void* context2;
    struct cq_attr attr2;
    attr2.qp_context = &context2;
    attr2.ib_send_cq = &ib_send_cq2;
    attr2.ib_recv_cq = &ib_recv_cq2;
    attr2.send_cq_depth = 16384;
    attr2.recv_cq_depth = 16384;
    attr2.send_cq_event_id = 1;
    attr2.recv_cq_event_id = 2;
	attr2.send_channel = NULL;
	attr2.recv_channel = NULL;
    ret = rs_cq_create(phy_id, rdev_index, &attr2);
    EXPECT_INT_EQ(ret, 0);

    struct ibv_qp_init_attr qp_init_attr2;
    qp_init_attr2.qp_context = context2;
    qp_init_attr2.send_cq = ib_send_cq2;
    qp_init_attr2.recv_cq = ib_recv_cq2;
    qp_init_attr2.qp_type = 2;
    qp_init_attr2.cap.max_inline_data = 32;
    qp_init_attr2.cap.max_send_wr = 4096;
    qp_init_attr2.cap.max_send_sge = 4096;
    qp_init_attr2.cap.max_recv_wr = 4096;
    qp_init_attr2.cap.max_recv_sge = 1;
	// void **qp2 = NULL;
	struct ibv_qp* qp2;
    ret = rs_normal_qp_create(phy_id, rdev_index, &qp_init_attr2, &qp_resp2, &qp2);
    EXPECT_INT_EQ(0, ret);


	rs_ut_msg("___________________after qp2 create:\n");

	ret = rs_qp_connect_async(phy_id, rdev_index, qp_resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");
#endif

#if 1
	struct ibv_cq *ib_send_cq3;
    struct ibv_cq *ib_recv_cq3;
    void* context3;
    struct cq_attr attr3;
    attr3.qp_context = &context3;
    attr3.ib_send_cq = &ib_send_cq3;
    attr3.ib_recv_cq = &ib_send_cq3;
    attr3.send_cq_depth = 16384;
    attr3.recv_cq_depth = 16384;
    attr3.send_cq_event_id = 1;
    attr3.recv_cq_event_id = 2;
	attr3.send_channel = (void*)0xabcd;
	attr3.recv_channel = (void*)0xabcd;

    ret = rs_cq_create(phy_id, rdev_index, &attr3);
    EXPECT_INT_EQ(ret, 0);

	struct ibv_cq *ib_send_cq4;
    struct ibv_cq *ib_recv_cq4;
    void* context4;
    struct cq_attr attr4;
    attr4.qp_context = &context4;
    attr4.ib_send_cq = &ib_send_cq4;
    attr4.ib_recv_cq = &ib_send_cq4;
    attr4.send_cq_depth = 16384;
    attr4.recv_cq_depth = 16384;
    attr4.send_cq_event_id = 1;
    attr4.recv_cq_event_id = 2;
	attr4.send_channel = NULL;
	attr4.recv_channel = (void*)0xabcd;

    ret = rs_cq_create(phy_id, rdev_index, &attr4);
    EXPECT_INT_EQ(ret, -1);
#endif
	usleep(SLEEP_TIME);

	ret = rs_normal_qp_destroy(phy_id, rdev_index, qp_resp2.qpn);
	ret = rs_normal_qp_destroy(phy_id, rdev_index, qp_resp.qpn);

	rs_ut_msg("___________________after qp1&2 destroy:\n");

	ret = rs_cq_destroy(phy_id, rdev_index, &attr3);
	ret = rs_cq_destroy(phy_id, rdev_index, &attr2);
	ret = rs_cq_destroy(phy_id, rdev_index, &attr);

	rs_ut_msg("___________________after cq1&2 destroy:\n");

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	rs_ut_msg("___________________after close socket 0:\n");

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);

	rs_ut_msg("___________________after close socket 1:\n");

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	rs_ut_msg("___________________after stop listen:\n");

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("___________________after deinit:\n");

    return;
}

void tc_rs_create_comp_channel()
{
	int ret;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;

	struct rs_init_config cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = g_rs_cb;

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	void *comp_channel = NULL;
	void *comp_channel1 = NULL;
	ret =  rs_create_comp_channel(phy_id, rdev_index, &comp_channel);
	EXPECT_INT_EQ(0, ret);

	mocker(rsGetLocalDevIDByHostDevID, 1, -19);
	ret =  rs_create_comp_channel(phy_id, rdev_index, &comp_channel1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(rs_rdev2rdev_cb, 1, -19);
	ret =  rs_create_comp_channel(phy_id, rdev_index, &comp_channel1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(rs_ibv_create_comp_channel, 1, NULL);
	ret =  rs_create_comp_channel(phy_id, rdev_index, &comp_channel1);
	EXPECT_INT_EQ(-259, ret);
	mocker_clean();

	mocker(rs_ibv_destroy_comp_channel, 1, -1);
	ret =  rs_destroy_comp_channel(comp_channel1);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	ret =  rs_destroy_comp_channel(comp_channel);
	EXPECT_INT_EQ(0, ret);

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}


void tc_rs_get_cqe_err_info()
{
	int ret;
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;
    struct cqe_err_info *temp_info = &err_info->info;
    struct cqe_err_info cqe_info;

	temp_info->status = 0;
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
	mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
    ret = rs_drv_get_cqe_err_info(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)memcpy_s, 1, 1);
    ret = rs_drv_get_cqe_err_info(&cqe_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)memset_s, 1, 1);
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
    mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
	ret = rs_drv_get_cqe_err_info(&cqe_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
    mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
	ret = rs_drv_get_cqe_err_info(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker((stub_fn_t)rs_drv_get_cqe_err_info, 1, 0);
    ret = rs_get_cqe_err_info(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_get_cqe_err_info_num()
{
    unsigned int num = 0;
    unsigned int phy_id;
    int ret;

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)rs_rdev2rdev_cb, 10, 0);
    phy_id = 128;
    ret = rs_get_cqe_err_info_num(phy_id, 0, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)rs_rdev2rdev_cb, 10, 0);
    phy_id = 0;
    ret = rs_get_cqe_err_info_num(0, 0, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    return;
}

int stub_rs_rdev2rdev_cb(unsigned int chip_id, unsigned int rdev_index, struct rs_rdev_cb **rdev_cb)
{
    *rdev_cb = &g_rdev_cb;
    return 0;
}

void tc_rs_get_cqe_err_info_list()
{
    struct cqe_err_info info = {0};
    struct rs_qp_cb qp_cb = {0};
    unsigned int num = 1;
    unsigned int phy_id;
    int ret;

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)rs_rdev2rdev_cb, 10, 0);
    phy_id = 128;
    ret = rs_get_cqe_err_info_list(phy_id, 0, &info, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker_invoke((stub_fn_t)rs_rdev2rdev_cb, stub_rs_rdev2rdev_cb, 10);
    phy_id = 0;
    RS_INIT_LIST_HEAD(&g_rdev_cb.qp_list);
    ret = rs_get_cqe_err_info_list(phy_id, 0, &info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    rs_list_add_tail(&qp_cb.list, &g_rdev_cb.qp_list);
    qp_cb.rdev_cb = &g_rdev_cb;
    qp_cb.cqe_err_info.info.status = 1;
    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker_invoke((stub_fn_t)rs_rdev2rdev_cb, stub_rs_rdev2rdev_cb, 10);
    ret = rs_get_cqe_err_info_list(phy_id, 0, &info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    return;
}

void tc_rs_save_cqe_err_info()
{
	int ret;
    struct rs_cqe_err_info *err_info = &g_rs_cqe_err;
    struct cqe_err_info *temp_info = &err_info->info;
	struct rs_qp_cb qp_cb;

	temp_info->status = 0;
    rs_drv_save_cqe_err_info(0x15, &qp_cb);
    mocker_clean();

	temp_info->status = 1;
    rs_drv_save_cqe_err_info(0x15, &qp_cb);
    mocker_clean();
}

void tc_rs_cqe_callback_process()
{
	struct rs_qp_cb qpcb_tmp = {0};
	struct ibv_wc wc = {0};
	struct ibv_cq ev_cq_sq = {0};
	struct ibv_cq ev_cq_rq = {0};
	struct rs_rdev_cb rdev_cb = {0};

	qpcb_tmp.ib_send_cq = &ev_cq_sq;
	qpcb_tmp.ib_recv_cq = &ev_cq_rq;
	qpcb_tmp.rdev_cb = &rdev_cb;

	wc.status = IBV_WC_MW_BIND_ERR;
    mocker((stub_fn_t)rs_drv_save_cqe_err_info, 1, 0);
	rs_cqe_callback_process(&qpcb_tmp, &wc, ev_cq_rq);
    mocker_clean();
}

void tc_rs_get_device_type()
{
	dl_hal_init();
	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, stub_dl_hal_get_device_info_pod_16p, 20);
	int ret = rs_get_device_type(0);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, stub_dl_hal_get_device_info_pod, 20);
	ret = rs_get_device_type(0);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)dl_hal_get_device_info, dl_hal_get_device_info_910A, 20);
	ret = rs_get_device_type(0);
	EXPECT_INT_EQ(ret, 0);

	mocker_clean();
	dl_hal_deinit();
}

void tc_rs_white_list()
{
	int ret;
	int try_num = 10;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen;
	struct socket_connect_info conn;
	struct socket_connect_info conn1;

	struct rs_socket_close_info_t sock_close;
	struct rs_socket_close_info_t sock_close1;

	struct socket_fd_data socket_info;
	struct socket_fd_data socket_info1;
    struct socket_wlist_info_t white_list;
    struct socket_wlist_info_t white_list1;
    u32 server_ip = inet_addr("127.0.0.1");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	listen.phy_id = 0;
	listen.family = AF_INET;
	listen.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	listen.port = 18888;
	ret = rs_socket_listen_start(&listen, 1);

	conn.phy_id = 0;
	conn.family = AF_INET;
	conn.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn.tag, "LinkCheck");
	conn.port = 18888;
	ret = rs_socket_batch_connect(&conn, 1);
	EXPECT_INT_EQ(ret, 0);

	conn1.phy_id = 0;
	conn1.family = AF_INET;
	conn1.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	conn1.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn1.tag, "2345");
	conn1.port = 18888;
    sleep(1);
    white_list.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list.conn_limit = 1;
    strcpy(white_list.tag, "LinkCheck");
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = server_ip;
    rs_socket_white_list_add(rdev_info, &white_list, 1);

    white_list1.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
    white_list1.conn_limit = 1;
    strcpy(white_list1.tag, "2345");
    rs_socket_white_list_add(rdev_info, &white_list1, 1);

    socket_info.phy_id = 0;
	socket_info.family = AF_INET;
 	socket_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	socket_info.remote_ip.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info.tag, "LinkCheck");
	try_num = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info, 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info.fd, socket_info.status);

    rs_socket_white_list_del(rdev_info, &white_list, 1);
    rs_socket_white_list_del(rdev_info, &white_list1, 1);
	sock_close.fd = socket_info.fd;
	ret = rs_socket_batch_close(0, &sock_close, 1);

	sock_close1.fd = socket_info1.fd;

	listen.port = 18888;
	ret = rs_socket_listen_stop(&listen, 1);

	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_create_srq()
{
	int ret;
	unsigned int phy_id = 0;
	unsigned int rdev_index = 0;

	struct rs_init_config cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = g_rs_cb;

	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	struct srq_attr attr = {0};
 	struct ibv_srq *ib_srq = NULL;
    struct ibv_cq *ib_recv_cq = NULL;
	void *context = NULL;
    attr.ib_srq = &ib_srq;
    attr.ib_recv_cq = &ib_recv_cq;
    attr.max_sge = 1;
    attr.context = &context;
    attr.srq_event_id = 1;
    attr.srq_depth = 63;
    attr.cq_depth = 64;
	struct srq_attr attr1 = {0};
	struct ibv_srq *ib_srq1 = 0xab;
    struct ibv_cq *ib_recv_cq1 = 0xab;
	struct rs_cq_context cq_context1 = {0};
	cq_context1.ib_srq_cq =  &ib_recv_cq1;
    attr1.ib_srq = &ib_srq1;
    attr1.ib_recv_cq = &ib_recv_cq1;
    attr1.max_sge = 1;
	void *context1 = &cq_context1;
    attr1.context = &context1;
    attr1.srq_event_id = 1;
    attr1.srq_depth = 63;
    attr1.cq_depth = 64;

	mocker(rsGetLocalDevIDByHostDevID, 1, -19);
	ret =  rs_create_srq(phy_id, rdev_index, &attr1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(rs_ibv_create_comp_channel, 1, NULL);
	ret =  rs_create_srq(phy_id, rdev_index, &attr1);
	EXPECT_INT_EQ(-22, ret);
	mocker_clean();

	mocker(calloc, 1, NULL);
	ret =  rs_create_srq(phy_id, rdev_index, &attr1);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

	mocker(rs_ibv_create_srq, 1, NULL);
	ret =  rs_create_srq(phy_id, rdev_index, &attr1);
	EXPECT_INT_EQ(-EOPENSRC, ret);
	mocker_clean();

	ret =  rs_create_srq(phy_id, rdev_index, &attr);
	EXPECT_INT_EQ(0, ret);

	struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq3;
    void* context2;
    struct cq_attr attr2;
    attr2.qp_context = &context2;
    attr2.ib_send_cq = &ib_send_cq;
    attr2.ib_recv_cq = &ib_recv_cq;
    attr2.send_cq_depth = 16384;
    attr2.recv_cq_depth = 16384;
    attr2.send_cq_event_id = 1;
    attr2.recv_cq_event_id = 2;
	attr2.send_channel = NULL;
	attr2.recv_channel = NULL;

	mocker(rs_epoll_ctl, 5, 0);
    ret = rs_cq_create(phy_id, rdev_index, &attr2);
    EXPECT_INT_EQ(ret, 0);

	ret = rs_cq_destroy(phy_id, rdev_index, &attr2);
    EXPECT_INT_EQ(ret, 0);

	mocker(rs_ibv_ack_cq_events, 1, NULL);
	ret =  rs_destroy_srq(phy_id, rdev_index, &attr);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}

void tc_rs_get_ipv6_scope_id()
{
	int ret;
	struct in6_addr local_ip;

	ret = rs_get_ipv6_scope_id(local_ip);
	EXPECT_INT_EQ(-EINVAL, ret);

	mocker(getifaddrs, 1, -1);
	ret = rs_get_ipv6_scope_id(local_ip);
	EXPECT_INT_EQ(-ESYSFUNC, ret);
}

void tc_rs_create_event_handle()
{
	int ret;
	int fd;

	ret = rs_create_event_handle(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_create_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = rs_destroy_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_ctl_event_handle()
{
	struct socket_peer_info fd_handle[1];
	int ret;
	int fd;

	ret = rs_ctl_event_handle(-1, NULL, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_create_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = rs_ctl_event_handle(fd, NULL, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	fd_handle[0].phy_id = 0;
	fd_handle[0].fd = 0;
	ret = rs_ctl_event_handle(fd, (const void *)fd_handle, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_ctl_event_handle(fd, (const void *)fd_handle, EPOLL_CTL_ADD, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_ctl_event_handle(fd, (const void *)fd_handle, EPOLL_CTL_ADD, RA_EPOLLONESHOT);

	ret = rs_destroy_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_wait_event_handle()
{
	struct socket_event_info event_info[1];
	unsigned int events_num = 0;
	int ret;
	int fd;

	ret = rs_wait_event_handle(-1, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_create_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = rs_wait_event_handle(fd, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_wait_event_handle(fd, event_info, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_wait_event_handle(fd, event_info, 0, 1, &events_num);
	EXPECT_INT_EQ(0, ret);

	ret = rs_destroy_event_handle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_destroy_event_handle()
{
	int ret;

	ret = rs_destroy_event_handle(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_epoll_create_epollfd()
{
	int ret;

	ret = rs_epoll_create_epollfd(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_epoll_destroy_fd()
{
	int fd;
	int ret;

	ret = rs_epoll_destroy_fd(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = rs_epoll_create_epollfd(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = rs_epoll_destroy_fd(&fd);
	EXPECT_INT_EQ(-1, fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_epoll_wait_handle()
{
	int fd;
	int ret;

	ret = rs_epoll_wait_handle(-1, NULL, 0, -1, 0);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_ssl_verify_callback()
{
	X509_STORE_CTX ctx;
	int ret;

	mocker((stub_fn_t)X509_STORE_CTX_get_error, 10, X509_V_ERR_CERT_HAS_EXPIRED);
	ret = verify_callback(0, &ctx);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();
}

void tc_rs_ssl_verify_cert()
{
	X509_STORE_CTX ctx;
	int ret;

	mocker((stub_fn_t)X509_verify_cert, 10, 0);
	mocker((stub_fn_t)X509_STORE_CTX_get_error, 10, X509_V_ERR_CERT_HAS_EXPIRED);
	ret = rs_ssl_verify_cert(&ctx);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)X509_verify_cert, 10, 0);
	mocker((stub_fn_t)X509_STORE_CTX_get_error, 10, 11);
	ret = rs_ssl_verify_cert(&ctx);
	EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();

}

void tc_rs_mem_pool()
{
    struct lite_mem_attr_resp mem_resp = {0};
    struct rs_rdev_cb rdev_cb = {0};
    struct rs_qp_cb qp_cb = {0};
    struct rs_cb rs_cb = {0};
    int ret;

    qp_cb.qp_mode = RA_RS_OP_QP_MODE;
    qp_cb.mem_align = LITE_ALIGN_4KB;
    ret = rs_init_mem_pool(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    rs_deinit_mem_pool(&qp_cb);

    qp_cb.mem_align = LITE_ALIGN_2MB;
    qp_cb.mem_resp = mem_resp;
    qp_cb.rdev_cb = &rdev_cb;
    rdev_cb.rs_cb = &rs_cb;
    mocker(rs_roce_init_mem_pool, 100, -EINVAL);
    ret = rs_init_mem_pool(&qp_cb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
    mocker(rs_roce_deinit_mem_pool, 100, -EINVAL);
    rs_deinit_mem_pool(&qp_cb);
    mocker_clean();
}

void tc_rs_get_vnic_ip_info()
{
	int ret;
	unsigned int ids[1] = {0};
	struct ip_info info[1] = {0};

	dl_hal_init();

	ret = rs_get_vnic_ip_infos(0, 0, NULL, 0, NULL);
	EXPECT_INT_NE(ret, 0);

	ret = rs_get_vnic_ip_infos(0, 0, ids, 1, NULL);
	EXPECT_INT_NE(ret, 0);

	ret = rs_get_vnic_ip_infos(0, 2, ids, 1, info);
	EXPECT_INT_NE(ret, 0);

	ret = rs_get_vnic_ip_info(0, 0, 0, &info[0]);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_vnic_ip_info(0, 0, 1, &info[0]);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_get_vnic_ip_info(0, 0, 2, &info[0]);
	EXPECT_INT_NE(ret, 0);

	dl_hal_deinit();
}

int stub_dl_hal_get_chip_info_910_93(unsigned int dev_id, halChipInfo *chip_info)
{
    strcpy(chip_info->name, "910_93xx");

    return 0;
}

void tc_rs_socket_get_bind_by_chip()
{
	unsigned int chip_id = 0;
	bool bind_ip = false;

	mocker(dl_drv_device_get_index_by_phy_id, 1, -1);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker(dl_hal_get_device_info, 1, -2);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker(dl_hal_get_device_info, 1, 0);
	mocker(dl_hal_get_chip_info, 1, -2);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker(dl_hal_get_device_info, 1, 0);
	mocker_invoke(dl_hal_get_chip_info, stub_dl_hal_get_chip_info_910_93, 100);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	EXPECT_INT_EQ(bind_ip, true);
	mocker_clean();
}

void tc_rs_get_ifaddrs1()
{
	dl_hal_init();
	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	int ret;
	int phy_id = 0;
	unsigned int ifaddr_num = 4;
	struct ifaddr_info ifaddr_infos[4] = {0};
	bool is_all = false;

	ifaddr_num = 0;
	ret = rs_fill_ifaddr_infos(ifaddr_infos, &ifaddr_num, phy_id);

	ret = rs_check_dst_interface(phy_id, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 1);

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = rs_check_dst_interface(phy_id, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = rs_check_dst_interface(phy_id, "eth0", 1, true);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = rs_check_dst_interface(phy_id, "eth0", 0, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, 2);
	ret = rs_check_dst_interface(phy_id, "bond0", 2, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = rs_check_dst_interface(phy_id, "bond0", 2, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	free(g_rs_cb);
	g_rs_cb = NULL;
	dl_hal_deinit();
	return;
}

void tc_rs_get_host_rdev_index()
{
	struct rs_rdev_cb rdev_cb = {0};
	struct rdev rdev_info = {0};
	struct rs_cb rs_cb = {0};
	int rdev_index = 0;
	int ret;

	rdev_cb.dev_list = ibv_get_device_list(&(rdev_cb.dev_num));
	rdev_cb.rs_cb = &rs_cb;
	mocker((stub_fn_t)pthread_mutex_lock, 20, 0);
	mocker((stub_fn_t)pthread_mutex_unlock, 20, 0);
	mocker(rs_ibv_get_device_name, 20, "910B");
	mocker(rs_convert_ip_addr, 20, -EINVAL);
	ret = rs_get_host_rdev_index(rdev_info, &rdev_cb, &rdev_index, 0);
	EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();
}

void tc_rs_ssl_get_cert()
{
	int ret;
	struct tls_cert_mng_info mng_info = {0};
	struct rs_cb rscb = {0};
    struct rs_cert_skid_subject_cb skid_subject_cb = {0};
	struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	mocker(tls_get_user_config, 10, -1);
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(tls_get_user_config, 20, 0);
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker_ret(tls_get_user_config, 0, 0, -1);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret(tls_get_user_config, 0, -2, -1);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret(tls_get_user_config, 0, -1, -1);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -1, 0);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -2, -1);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -2, -2);
	rscb.hccp_mode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	return;
}

void tc_rs_ssl_X509_store_init()
{
	int ret;
	X509_STORE_CTX ctx = {0};
	X509_STORE store = {0};
	struct tls_cert_mng_info mng_info = {0};
	struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	mocker(tls_get_cert_chain, 10, -1);
	ret = rs_ssl_x509_store_init(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(tls_get_cert_chain, 10, 0);
	ret = rs_ssl_x509_store_init(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	new_certs[0].ncert_count = 1;
	strcpy(new_certs[0].certs[0].ncert_info ,"pub cert");
	ret = rs_ssl_x509_store_init(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	new_certs[0].ncert_count = 2;
	strcpy(new_certs[0].certs[1].ncert_info ,"root ca cert");
	mocker(tls_load_cert, 10, NULL);
	ret = rs_ssl_x509_store_init(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

void tc_rs_ssl_skids_subjects_get()
{
	int ret;
	struct tls_cert_mng_info mng_info = {0};
	struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};
	struct rs_cb rscb = {0};

	mng_info.cert_count = 2;
	mocker(tls_load_cert, 10, NULL);
	ret = rs_ssl_skids_subjects_get(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	new_certs[0].ncert_count = 2;
	strcpy(new_certs[0].certs[1].ncert_info, "root ca cert");
	mocker(tls_load_cert, 10, NULL);
	ret = rs_ssl_skids_subjects_get(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

void tc_rs_ssl_put_cert_ca_pem()
{
	int ret;
	char ca_file[20];
	struct tls_cert_mng_info mng_info = {0};
	struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	strcpy(ca_file, "ca file name");
	mocker(creat, 10, -1);
	ret = rs_ssl_put_cert_ca_pem(&certs, &mng_info, &new_certs, &ca_file);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();

	mocker(creat, 10, 0);
	mng_info.cert_count = 2;
	mocker(write, 10, -1);
	ret = rs_ssl_put_cert_ca_pem(&certs, &mng_info, &new_certs, &ca_file);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	mocker(creat, 10, 0);
	new_certs[0].ncert_count = 2;
	strcpy(new_certs[0].certs[0].ncert_info, "pub cert");
	strcpy(new_certs[0].certs[1].ncert_info, "root ca cert");
	mocker(write, 10, 0);
	ret = rs_ssl_put_cert_ca_pem(&certs, &mng_info, &new_certs, &ca_file);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

void tc_rs_ssl_put_cert_end_pem()
{
	int ret;
	char end_file[20];
	struct rs_certs certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	strcpy(end_file, "end file name");
	new_certs[0].ncert_count = 2;
	mocker(creat, 10, 0);
	mocker(write, 10, -1);
	ret = rs_ssl_put_cert_end_pem(&certs, &new_certs, &end_file);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();

	strcpy(new_certs[0].certs[0].ncert_info, "pub cert");
	mocker(creat, 10, 0);
	mocker(write, 10, strlen(new_certs[0].certs[0].ncert_info));
	ret = rs_ssl_put_cert_end_pem(&certs, &new_certs, &end_file);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	new_certs[0].ncert_count = 0;
	mocker(rs_ssl_put_end_cert, 10, -1);
	ret = rs_ssl_put_cert_end_pem(&certs, &new_certs, &end_file);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	return;
}

void tc_rs_ssl_X509_store_add_cert()
{
	int ret;
	char cert_info[20];
	X509_STORE store;

	mocker(tls_load_cert, 10, NULL);
	ret = rs_ssl_X509_store_add_cert(&cert_info, &store);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

int stub_rs_get_conn_info(struct rs_conn_cb *conn_cb, struct socket_connect_info *conn,
    struct rs_conn_info **conn_info, int server_port)
{
    (*conn_info) = g_conn_info;

    return 0;
}

void tc_rs_socket_batch_abort()
{
    struct socket_connect_info conn[1] = { 0 };
    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_conn_info = malloc(sizeof(struct rs_conn_info));
    int ret = 0;

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(rs_get_conn_info, 1, -1);
    ret = rs_socket_batch_abort(conn, 1);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker_invoke(rs_get_conn_info, stub_rs_get_conn_info, 1);
    mocker(setsockopt, 1, -1);
    mocker(rs_socket_close_fd, 1, -1);

    g_conn_info->state = 2;
    g_conn_info->list.prev = &g_conn_info->list;
    g_conn_info->list.next = &g_conn_info->list;
    ret = rs_socket_batch_abort(conn, 1);
    EXPECT_INT_EQ(ret, -1);

    free(g_rs_cb);
    g_rs_cb = NULL;
}

void tc_rs_socket_send_and_recv_log_test()
{
    int ret = 0;

    g_rs_cb = malloc(sizeof(struct rs_cb));
    g_rs_cb->ssl_enable = 0;

    mocker_clean();
    mocker(send, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_drv_socket_send(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(send, 1, -1);
    ret = rs_drv_socket_send(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(recv, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_drv_socket_recv(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(recv, 1, -1);
    ret = rs_drv_socket_recv(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(send, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_peer_socket_send(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(send, 1, -1);
    ret = rs_peer_socket_send(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(recv, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = rs_peer_socket_recv(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(recv, 1, -1);
    ret = rs_peer_socket_recv(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    free(g_rs_cb);
    g_rs_cb = NULL;
}

void stub_hccp_time_max_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    *msec = 90001.0;
}

void stub_hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    *msec = 5001.0;
}

void tc_rs_tcp_recv_tag_in_handle()
{
    struct rs_listen_info listen_info = {0};
    struct rs_conn_info conn_tmp = {0};
    struct rs_ip_addr_info remote_ip = {0};
    struct rs_cb *rs_cb = NULL;
    struct rs_accept_info accept_info = {0};
    int ret = 0;

    mocker_clean();
    mocker(recv, 1, 0);
    ret = rs_tcp_recv_tag_in_handle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, -ESOCKCLOSED);

    mocker_clean();
    mocker(recv, 1, 1);
    mocker_invoke(hccp_time_interval, stub_hccp_time_max_interval, 1);
    ret = rs_tcp_recv_tag_in_handle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, -ETIME);

    mocker_clean();
    mocker(recv, 1, 256);
    mocker_invoke(hccp_time_interval, stub_hccp_time_interval, 1);
    ret = rs_tcp_recv_tag_in_handle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(rs_tcp_recv_tag_in_handle, 1, 1);
    mocker(close, 1, 1);
    rs_epoll_event_tcp_listen_in_handle(rs_cb, &listen_info, 1, &remote_ip);

    mocker_clean();
    mocker(rs_tcp_recv_tag_in_handle, 1, 0);
    mocker(rs_wlist_check_conn_add, 1, 1);
    rs_epoll_event_tcp_listen_in_handle(rs_cb, &listen_info, 1, &remote_ip);

    mocker_clean();
    mocker((stub_fn_t)SSL_read, 10, -1);
    mocker_invoke(hccp_time_interval, stub_hccp_time_interval, 1);
    rs_ssl_recv_tag_in_handle(&accept_info, &conn_tmp);
    mocker_clean();
}

void tc_rs_peer_socket_recv()
{
    struct socket_connect_info conn[10] = {0};
    struct socket_err_info err[10] = {0};
    int ret = 0;

    mocker_clean();

    conn[0].family = AF_INET;
    mocker(rs_socket_connect_check_para, 1, 0);
    mocker(rs_socket_nodeid2vnic, 1, 0);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(rs_dev2conncb, 1, 0);
    mocker(rs_get_conn_info, 1, 0);
    mocker(memcpy_s, 1, 0);
    mocker(memset_s, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = rs_socket_get_client_socket_err_info(conn, err, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    return;
}

void tc_rs_socket_get_server_socket_err_info()
{
    struct socket_listen_info conn[10] = {0};
    struct server_socket_err_info err[10] = {0};
    int ret = 0;

    mocker_clean();

    conn[0].family = AF_INET;
    mocker(rs_socket_nodeid2vnic, 1, 0);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(rs_dev2conncb, 1, 0);
    mocker(rs_convert_ip_addr, 1, 0);
    mocker(rs_find_listen_node, 1, 0);
    mocker(memcpy_s, 2, 0);
    mocker(memset_s, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = rs_socket_get_server_socket_err_info(conn, err, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    return;
}

int stub_rs_find_listen_node(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *ip_addr, uint32_t server_port,
    struct rs_listen_info **listen_info)
{
    *listen_info = g_plisten_info;

    return 0;
}

void tc_rs_socket_accept_credit_add()
{
    struct socket_listen_info conn[10] = {0};
    struct rs_conn_cb conn_cb = {0};
    int ret = 0;

    mocker_clean();

    mocker(rs_convert_ip_addr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    mocker(rs_find_listen_node, 1, -1);
    ret = rs_socket_accept_credit_add(conn, 1, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_convert_ip_addr, 1, 0);
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker_invoke(rs_find_listen_node, stub_rs_find_listen_node, 1);
    mocker(rs_socket_listen_add_to_epoll, 1, 0);
    ret = rs_socket_accept_credit_add(conn, 1, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(rs_epoll_ctl, 1, 0);
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    g_listen_info.fd_state = LISTEN_FD_STATE_DELETED;
    ret = rs_socket_listen_add_to_epoll(&conn_cb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    g_listen_info.fd_state = LISTEN_FD_STATE_DELETED;
    ret = rs_socket_listen_del_from_epoll(&conn_cb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker(rs_epoll_ctl, 1, -1);
    g_listen_info.fd_state = LISTEN_FD_STATE_ADDED;
    ret = rs_socket_listen_del_from_epoll(&conn_cb, g_plisten_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_listen_info.accept_credit_flag = 1;
    g_listen_info.accept_credit_limit = 1;
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker(rs_epoll_ctl, 1, 0);
    ret = rs_socket_check_credit(&conn_cb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_epoll_event_ssl_recv_tag_in_handle()
{
    struct rs_cb rs_cb = {0};
    struct rs_accept_info *accept_info = NULL;
    struct rs_list_head list = {0};

    RS_INIT_LIST_HEAD(&list);
    accept_info = malloc(sizeof(struct rs_accept_info));
    accept_info->list = list;
    mocker_clean();
    mocker(rs_ssl_recv_tag_in_handle, 1, 0);
    mocker(rs_wlist_check_conn_add, 1, -1);
    mocker(SSL_free, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    rs_epoll_event_ssl_recv_tag_in_handle(&rs_cb, accept_info);
    mocker_clean();
}

void tc_rs_server_valid_async_abnormal()
{
    struct rs_conn_info conn = {0};
    struct rs_conn_cb conn_cb = {0};

    mocker_clean();
    mocker(rs_find_white_list, 1, 0);
    mocker(rs_server_valid_async_init, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    mocker(rs_find_white_list_node, 1, -1);
    mocker(rs_server_send_wlist_check_result, 1, 0);
    rs_server_valid_async(0, &conn_cb, &conn);
    mocker_clean();
}

void tc_rs_server_valid_async_abnormal_01()
{
    struct rs_conn_info conn = {0};
    struct rs_conn_cb conn_cb = {0};

    mocker_clean();
    mocker(rs_server_valid_async_init, 1, 0);
    mocker(rs_find_white_list, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
	mocker_invoke((stub_fn_t)rs_find_white_list_node, stub_rs_find_white_list_node, 1);
    mocker(rs_server_send_wlist_check_result, 1, -1);
    rs_server_valid_async(0, &conn_cb, &conn);
    mocker_clean();
}

void tc_rs_remap_mr()
{
    struct mem_remap_info mem_list[1] = {0};
    struct rs_mr_cb mr_cb = {0};
    struct ibv_mr ib_mr = {0};

    mocker_clean();
    RS_INIT_LIST_HEAD(&g_rdev_cb.typical_mr_list);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)rs_rdev2rdev_cb, stub_rs_rdev2rdev_cb, 10);
    mocker(rs_roce_remap_mr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    rs_remap_mr(0, 0, mem_list, 1);
    mocker_clean();

    RS_INIT_LIST_HEAD(&g_rdev_cb.typical_mr_list);
    ib_mr.length = 100;
    mr_cb.ib_mr = &ib_mr;
    rs_list_add_tail(&mr_cb.list, &g_rdev_cb.typical_mr_list);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)rs_rdev2rdev_cb, stub_rs_rdev2rdev_cb, 10);
    mocker(rs_roce_remap_mr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    rs_remap_mr(0, 0, mem_list, 1);
    mocker_clean();
}

void tc_rs_roce_get_api_version()
{
    unsigned int api_version = 0;

    api_version = rs_roce_get_api_version();
    EXPECT_INT_EQ(api_version, 0);
}

void tc_rs_get_tls_enable()
{
    unsigned int phy_id = 0;
    bool tls_enable;
    int ret;

    mocker(rs_get_rs_cb, 1, 1);
    ret = rs_get_tls_enable(phy_id, &tls_enable);
    EXPECT_INT_EQ(ret, 1);

    ret = rs_get_tls_enable(phy_id, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();
}

int rs_get_linux_version_stub(struct rs_linux_version_info *ver_info)
{
    ver_info->major = 5;
    ver_info->minor = 19;
    ver_info->patch = 0;
    return 0;
}

void tc_rs_get_sec_random()
{
	unsigned int value = 0;
    int ret;
	ret = rs_get_sec_random(&value);
    EXPECT_INT_EQ(ret, -257);

    mocker(strstr, 2, NULL);
    ret = rs_get_sec_random(&value);
    EXPECT_INT_NE(0, ret);

    mocker(read, 2, -1);
    ret = rs_get_sec_random(&value);
    EXPECT_INT_NE(0, ret);

    mocker(open, 2, -1);
    ret = rs_get_sec_random(&value);
    EXPECT_INT_NE(0, ret);

	mocker_invoke(rs_get_linux_version, rs_get_linux_version_stub, 10);

	ret = rs_get_sec_random(&value);
    EXPECT_INT_NE(ret, 0);
}

void tc_rs_socket_fill_wlist_by_phyID()
{
	struct socket_wlist_info_t white_list_node = {0};
	struct rs_conn_info rs_conn = {0};

	mocker(rs_socket_is_vnic_ip, 1, 1);
	rs_conn.client_ip.family = AF_INET;
	rs_socket_fill_wlist_by_phyID(0, &white_list_node, &rs_conn);
	mocker_clean();
	return;
}

void tc_rs_get_hccn_cfg()
{
	char *value[2048] = {0};
	unsigned int value_len = 2048;

    int ret;

    mocker(file_read_cfg, 2, 0);
	ret = rs_get_hccn_cfg(0, HCCN_CFG_UDP_PORT_MODE, value, &value_len);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();
}

void tc_dl_hal_set_clear_user_config()
{
    dl_hal_set_user_config(0, NULL, NULL, 1);
    dl_hal_clear_user_config(0, NULL);
}

void tc_rs_free_dev_list(void)
{
    struct rs_cb rscb = {0};

    // list empty
    RS_INIT_LIST_HEAD(&rscb.rdev_list);
    rs_free_udev_list(&rscb);

    // protocol invalid
    rscb.protocol = PROTOCOL_UNSUPPORT;
    rs_free_dev_list(&rscb);
}

void tc_rs_free_rdev_list(void)
{
    struct rs_rdev_cb rdev_cb = {0};
    struct rs_cb rscb = {0};

    // rsGetDevIDByLocalDevID fail
    rscb.protocol = PROTOCOL_RDMA;
    mocker(rsGetDevIDByLocalDevID, 1, -1);
    rs_free_rdev_list(&rscb);
    mocker_clean();

    // rs_rdev_deinit fail
	RS_INIT_LIST_HEAD(&rscb.rdev_list);
    rs_list_add_tail(&rdev_cb.list, &rscb.rdev_list);
    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(rs_rdev_deinit, 1, -1);
    rs_free_rdev_list(&rscb);
    mocker_clean();

    // success
    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(rs_rdev_deinit, 1, 0);
    rs_free_rdev_list(&rscb);
    mocker_clean();
}

void tc_rs_free_udev_list(void)
{
    struct rs_ub_dev_cb udev_cb = {0};
    struct rs_cb rscb = {0};

    // rs_ub_ctx_deinit fail
	RS_INIT_LIST_HEAD(&rscb.rdev_list);
    mocker(rs_ub_ctx_deinit, 1, -1);
    rs_list_add_tail(&udev_cb.list, &rscb.rdev_list);
    rs_free_udev_list(&rscb);
    mocker_clean();

    // success
    mocker_clean();
    mocker(rs_ub_ctx_deinit, 1, 0);
    rs_free_udev_list(&rscb);
    mocker_clean();
}

void tc_rs_drv_create_cq_with_attrs_abnormal()
{
    struct cq_ext_attr cq_attr = {0};
    struct rs_rdev_cb rdev_cb = {0};
    struct rs_qp_cb qp_cb = {0};
    struct ibv_cq scq = {0};
    int is_ext = 0;
    int ret;

    qp_cb.rdev_cb = &rdev_cb;
    mocker_ret(rs_ibv_create_cq, &scq, NULL, NULL);
    mocker(rs_ibv_destroy_cq, 1, 0);
    ret = rs_drv_create_cq_with_attrs(&qp_cb, is_ext, &cq_attr);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();
}
