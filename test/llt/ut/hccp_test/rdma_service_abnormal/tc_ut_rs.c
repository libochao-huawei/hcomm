#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>

#include "ut_dispatch.h"
#include "stub/ibverbs.h"
#include "rs.h"
#include "rs_inner.h"
#include "rs_ping_inner.h"
#include "rs_ping.h"
#include "rs_drv_rdma.h"
#include "rs_drv_socket.h"
#include "ra_rs_err.h"
#include "tc_ut_rs.h"
#include "stub/verbs_exp.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "dl.h"
#include "tls.h"
#include "rs_esched.h"
#include "rs_ctx_inner.h"

extern __thread struct rs_cb *g_rs_cb;
int dl_drv_get_local_dev_id_by_host_dev_id(unsigned int dev_id, unsigned int* chip_id);

int SSL_CTX_set_min_proto_version(SSL_CTX *ctx, int version);

int SSL_CTX_set_cipher_list(SSL_CTX *ctx, const char *str);

int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file);

int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile, const char *CApath);

int SSL_CTX_check_private_key(const SSL_CTX *ctx);

int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey);

void EVP_PKEY_free(EVP_PKEY *x);

X509_STORE *SSL_CTX_get_cert_store(const SSL_CTX * ctx);

int X509_STORE_set_flags(X509_STORE *ctx, unsigned long flags);

long SSL_ctrl(SSL *s, int cmd, long larg, void *parg);

int X509_STORE_add_crl(X509_STORE *ctx, X509_CRL *x);

void X509_STORE_free(X509_STORE *vfy);

const SSL_METHOD *TLS_server_method(void);

const SSL_METHOD *TLS_client_method(void);

SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth);

void SSL_CTX_free(SSL_CTX *ctx);

int SSL_shutdown(SSL *s);

void SSL_free(SSL *ssl);

SSL *SSL_new(SSL_CTX *ctx);

int SSL_get_error(const SSL *s, int ret_code);

int SSL_set_fd(SSL *s, int fd);

long SSL_ctrl(SSL *ssl, int cmd, long larg, void *parg);

void SSL_set_connect_state(SSL *s);

void SSL_set_accept_state(SSL *s);

int SSL_do_handshake(SSL *s);

long SSL_get_verify_result(const SSL *ssl);

X509 *SSL_get_peer_certificate(const SSL *s);

int SSL_write(SSL *ssl, const void *buf, int num);

int SSL_read(SSL *ssl, void *buf, int num);
#define STACK_OF(type) struct stack_st_##type
BIO *BIO_new_mem_buf(const void *buf, int len);

X509 *d2i_X509_bio(BIO *bp, X509 **x509);

X509 *PEM_read_bio_X509(BIO *bp, X509 **x, pem_password_cb *cb, void *u);

X509_STORE *X509_STORE_new(void);

X509_STORE_CTX *X509_STORE_CTX_new(void);

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509, STACK_OF(X509) *chain);

int X509_verify_cert(X509_STORE_CTX *ctx);

int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx);

const char *X509_verify_cert_error_string(long n);

void X509_STORE_CTX_cleanup(X509_STORE_CTX *ctx);

void X509_STORE_CTX_free(X509_STORE_CTX *ctx);

void X509_STORE_free(X509_STORE *vfy);

void X509_free(X509 *buf);

void X509_STORE_CTX_trusted_stack(X509_STORE_CTX *ctx, STACK_OF(X509) *sk);

int tls_get_user_config(unsigned int save_mode, unsigned int chip_id, const char *name,
    unsigned char *buf, unsigned int *buf_size);

void tls_get_enable_info(unsigned int save_mode, unsigned int chip_id, unsigned char *buf, unsigned int buf_size);

int rs_ssl_get_crl_data(struct rs_cb *rscb, FILE* fp, struct tls_cert_manage_info *mng_info, X509_CRL *crl);

int rs_get_pridata(struct rs_cb *rscb, struct rs_sec_para *rs_para, struct tls_cert_mng_info *mng_info);

int rs_ssl_put_certs(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info, struct rs_certs *certs,
    struct tls_ca_new_certs *new_certs, struct cert_file *file_name);

#define SLEEP_TIME 500000
#define rs_ut_msg(fmt, args...)	fprintf(stderr, "\t>>>>> " fmt, ##args)

int try_again;
struct rs_qp_cb qp_cb_tmp2;
const char *s_tmp = "suc";
struct rs_conn_info *g_conn_info;

int rs_alloc_conn_node(struct rs_conn_info **conn, unsigned short server_port);
int rs_get_conn_info(struct rs_conn_cb *conn_cb, 
			struct socket_connect_info *conn, 
			struct rs_conn_info **conn_info);
int rs_get_rs_cb(unsigned int phy_id, struct rs_cb **rs_cb);
int rs_qp_exp_create(struct rs_qp_cb *qp_cb);
int rs_notify_mr_list_add(struct rs_qp_cb *qp_cb, char *buf);
void rs_epoll_recv_handle(struct rs_qp_cb *qp_cb, char *buf, int size);
void rs_drv_poll_cq_handle(struct rs_qp_cb *qp_cb);
int rs_create_cq(struct rs_qp_cb *qp_cb);
int rs_qp_state_modify(struct rs_qp_cb *qp_cb);
void rs_epoll_event_in_handle(struct rs_cb *rs_cb, struct epoll_event *events);
void rs_epoll_event_handle_one(struct rs_cb *rs_cb, struct epoll_event *events);
int rs_mr_info_sync(struct rs_mr_cb *mr_cb);
int rs_drv_get_gid_index(struct rs_rdev_cb *rdev_cb, struct ibv_port_attr *attr, int *idx);
void rs_epoll_ctl(int epollfd, int op, int fd, int state);
int rs_socket_connect(struct rs_conn_info *conn);
int rs_post_recv_stub(struct ibv_qp *qp, struct ibv_recv_wr *wr,
				struct ibv_recv_wr **bad_wr);
int rs_drv_get_random_num(int *rand_num);
void rs_socket_tag_sync(struct rs_conn_info *conn);
int rs_socket_state_init(unsigned int chip_id, struct rs_conn_info *conn, uint32_t ssl_enable, struct rs_cb *rscb);
int rs_find_sockets(struct rs_conn_info *conn_tmp, struct socket_fd_data conn[],
                    int num, int role);
int rs_alloc_client_conn_node(struct rs_conn_cb *conn_cb, enum rs_conn_role role,
                    struct rs_conn_info **conn, struct socket_connect_info *socket_conn, int server_port);
uint32_t rs_socket_vnic2nodeid(uint32_t ip_addr);
int roce_set_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int roce_get_tsqp_depth(const char *dev_name, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int rs_socket_nodeid2vnic(uint32_t node_id, uint32_t *ip_addr);
int rs_server_valid_async_init(unsigned int chip_id, struct rs_conn_info *conn, struct socket_wlist_info_t *white_list_expect);
extern int rs_connect_bind_client(int fd, struct rs_conn_info *conn);
extern void rs_socket_get_bind_by_chip(unsigned int chip_id, bool *bind_ip);
extern int rs_init_rscb_cfg(struct rs_cb *rscb);
extern void rs_deinit_rscb_cfg(struct rs_cb *rscb);
extern int rs_socket_close_fd(int fd);
extern int rs_find_white_list(struct rs_conn_cb *conn_cb, struct rs_ip_addr_info *server_ip, unsigned int server_port,
    struct rs_white_list **white_list);
extern int rs_find_white_list_node(struct rs_white_list *rs_socket_white_list,
    struct socket_wlist_info_t *white_list_expect, int family, struct rs_white_list_info **white_list_node);
extern int rs_server_send_wlist_check_result(struct rs_conn_info *conn, bool flag);
extern uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id);
extern uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id);

long unsigned int stub_calloc(long unsigned int num, long unsigned int size)
{
	static int hit = 0;
	if (hit == 1) {
		return 0;
	}
	hit ++;
	return malloc(num * size);
}

int stub_ibv_get_cq_event(struct ibv_comp_channel *channel, struct ibv_cq **cq, void **cq_context)
{
	*cq = NULL;
	return 0;
}

struct rs_white_list_info g_white_list_node_tmp = {0};
int stub_rs_find_white_list_node(struct rs_white_list *rs_socket_white_list,
    struct socket_wlist_info_t *white_list_expect, int family, struct rs_white_list_info **white_list_node)
{
	*white_list_node = &g_white_list_node_tmp;
	return 0;
}

int rs_post_send_stub(struct ibv_qp *qp, struct ibv_send_wr *wr,
				struct ibv_send_wr **bad_wr);

int rs_qp_info_sync(struct rs_qp_cb *qp_cb);
int rs_create_epoll(struct rs_cb *rs_cb);
int rs_get_mrcb(struct rs_qp_cb *qp_cb, uint64_t addr, struct rs_mr_cb **mr_cb, struct rs_list_head *mr_list);

void* rs_epoll_handle();
int memcpy_s(void * dest, size_t destMax, const void * src, size_t count);
int memset_s(void * dest, size_t destMax, int c, size_t count);
//int strcpy_s(void * dest, size_t destMax, int c, size_t count);

#if 0
int tc_rs_abnormal()
{
	int ret;
	struct rs_cb *rs_cb;
	struct rs_qp_cb *qp_cb;
	struct rs_qp_cb qp_cb_tmp;
	uint32_t qpn = 0;

	fprintf(stderr, "\n+++++++++ABNORMAL TC Start++++++++\n");
	ret = rs_dev2rscb(0, &rs_cb);
	ret = rs_dev2rscb(2, &rs_cb);
	ret = rs_get_rscb(&rs_cb);
	//ret = rs_get_qpcb(rs_cb, 0, &qp_cb);

	ret = rs_qpn2qpcb(5, &qp_cb);

	rs_epoll_handle();
	qp_cb_tmp.connfd = RS_FD_INVALID;
	rs_qp_info_sync(&qp_cb_tmp);
	qp_cb_tmp.connfd = 1;
	qp_cb_tmp.flag |= RS_INFO_SYNCED;
	rs_qp_info_sync(&qp_cb_tmp);
	rs_send(100, &qp_cb_tmp, 1);
	rs_recv(100, &qp_cb_tmp, 1);

	rs_get_sq_index(qpn, &ret);
	fprintf(stderr, "---------ABNORMAL TC End----------\n\n");

	return 0;
}
#endif
void tc_rs_init2()
{
	int ret;
	struct rs_init_config cfg = {0};

	struct rs_conn_info *info;
	ret = rs_fd2conn(0, &info);
	EXPECT_INT_EQ(ret, -ENODEV);	

	cfg.hccp_mode = NETWORK_OFFLINE;

	mocker((stub_fn_t)pthread_mutex_init, 20, 1);
	ret = rs_init(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)epoll_create, 20, -1);
	mocker((stub_fn_t)pthread_create, 20, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_create, 20, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)pthread_create, 0, -1, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_detach, 10, -1);
	rs_connect_handle(&cfg);
	mocker_clean();

	cfg.chip_id = 3;
	cfg.hccp_mode = NETWORK_ONLINE;
	mocker((stub_fn_t)calloc, 10, NULL);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, -12);
	mocker_clean();

	return;
}

void tc_rs_deinit2()
{
	int ret;
	uint32_t dev_id = 0;
	struct rs_init_config cfg;
	struct rs_cb *rs_cb = NULL;


	/* resource prepare... */
	cfg.hccp_mode = NETWORK_OFFLINE;
	cfg.chip_id = 0;
	cfg.white_list_status = 1;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	/* resource free... */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_deinit2: rs_deinit1\n");
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_dev2rscb(dev_id, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)write, 20, 1);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();
    rs_ut_msg("!!!!!!tc_rs_deinit2: rs_deinit2\n");

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct rs_cb *rscb = NULL;
	rscb = calloc(1, sizeof(struct rs_cb));
	rscb->hccp_mode = NETWORK_OFFLINE;
	RS_INIT_LIST_HEAD(&rscb->conn_cb.client_conn_list);
	rs_init_rscb_cfg(rscb);
	rs_deinit_rscb_cfg(rscb);
	mocker((stub_fn_t)write, 20, 1);
	rs_deinit_rscb_cfg(rscb);
	mocker_clean();
	free(rscb);
	rscb = NULL;
	return;
}

void tc_rs_rdev_init()
{
	int ret;
	unsigned int rdev_index = 0;
	struct rdev rdev_info = {0};
	rdev_info.phy_id = 10;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);

	rdev_info.phy_id = 0;

	mocker((stub_fn_t)rs_get_rs_cb, 20, 1);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)calloc, 20, NULL);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_mutex_init, 20, 1);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_get_device_list, 10, -1);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_exp_query_notify, 20, 1);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_alloc_pd, 20, 0);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_reg_mr, 20, 0);
	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
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
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");
	struct rs_cb g_rs_cb_tmp = {0};
	g_rs_cb = &g_rs_cb_tmp;

	mocker((stub_fn_t)rs_dev2rscb, 20, 1);
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	rdev_info.family = 3;
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_NE(ret, 0);

	rdev_info.phy_id = 10;
	ret = rs_socket_deinit(rdev_info);
	EXPECT_INT_NE(ret, 0);
	g_rs_cb = NULL;
}

void tc_rs_rdev_deinit()
{
	int ret;
	unsigned int rdev_index = 00;

	ret = rs_rdev_deinit(10, NOTIFY, 1);
	EXPECT_INT_NE(ret, 0);

	ret = rs_rdev_deinit(0, NOTIFY, rdev_index);
	EXPECT_INT_NE(ret, 0);
}

void tc_rs_socket_listen_start2()
{
	int ret;
	uint32_t dev_id = 0;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen_node[2] = {0};
	struct rs_cb *rs_cb;


	/* resource prepare... */
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen_node[0].phy_id = 0;
	listen_node[0].family = AF_INET;
    listen_node[0].local_ip.addr.s_addr = inet_addr("127.0.0.1");
	mocker((stub_fn_t)calloc, 10, NULL);
	listen_node[0].port = 16666;
	ret = rs_socket_listen_start(listen_node, 1);
	mocker_clean();

	mocker((stub_fn_t)socket, 10, -1);
	listen_node[0].port = 16666;
	ret = rs_socket_listen_start(listen_node, 1);
	mocker_clean();

	mocker((stub_fn_t)socket, 10, 0);
	mocker((stub_fn_t)setsockopt, 10, -1);
	listen_node[0].port = 16666;
        ret = rs_socket_listen_start(listen_node, 1);
        mocker_clean();


	listen_node[0].family = AF_INET;
    listen_node[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	mocker((stub_fn_t)bind, 10, 1);
	listen_node[0].port = 16666;
	ret = rs_socket_listen_start(listen_node, 1);
	EXPECT_INT_NE(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)listen, 10, -1);
	listen_node[0].port = 16666;
	ret = rs_socket_listen_start(listen_node, 1);
	EXPECT_INT_NE(ret, -1);
	mocker_clean();

	/* twice listen but first failed */
	struct socket_listen_info listen_twice[2] = {0};
	listen_twice[0].local_ip.addr.s_addr  = inet_addr("127.0.0.4");
	listen_twice[1].phy_id = 1;
	listen_twice[0].family = AF_INET;
	listen_twice[1].family = AF_INET;
	listen_twice[0].port = 16666;
	listen_twice[1].port = 16666;
	ret = rs_socket_listen_start(listen_twice, 2);
	mocker_clean();

	/* resource free... */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_listen_start2: rs_deinit\n");
	return;
}

void tc_rs_socket_batch_connect2()
{
	int ret;
	uint32_t dev_id = 0;
	struct rs_init_config cfg = {0};
	struct socket_connect_info conn_node[2] = {0};
	struct rs_conn_info conn_socket_err;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, -1);
	ret = rs_socket_state_reset(0, &conn_socket_err, g_rs_cb->ssl_enable, g_rs_cb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
	free(g_rs_cb);
	g_rs_cb = NULL;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 0;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, 1);
	ret = rs_socket_state_reset(0, &conn_socket_err, g_rs_cb->ssl_enable, g_rs_cb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	memset(g_rs_cb, 0, sizeof(struct rs_cb));
	conn_socket_err.state = 4;
	mocker((stub_fn_t)rs_socket_tag_sync, 10, -1);
	ret = rs_socket_connect_async(&conn_socket_err, g_rs_cb);
	mocker_clean();
	free(g_rs_cb);
	g_rs_cb = NULL;

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

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)rs_socket_state_init, 10, -1);
	ret = rs_socket_connect_async(&conn_socket_err, g_rs_cb);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();
	free(g_rs_cb);
	g_rs_cb = NULL;

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, 0);
	mocker((stub_fn_t)connect, 10, -1);
	ret = rs_socket_state_reset(0, &conn_socket_err, 0, g_rs_cb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
	free(g_rs_cb);
	g_rs_cb = NULL;


	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)connect, 10, -1);
	mocker((stub_fn_t)rs_socket_tag_sync, 10, 0);
	ret = rs_socket_state_init(0, &conn_socket_err, 0, g_rs_cb);
	free(g_rs_cb);
	g_rs_cb = NULL;
	EXPECT_INT_NE(ret, 0);
    mocker_clean();

	g_rs_cb = malloc(sizeof(struct rs_cb));
	g_rs_cb->hccp_mode = 1;
	conn_socket_err.state = 1;
    mocker((stub_fn_t)connect, 10, 0);
	mocker((stub_fn_t)getsockname, 10, 0);
	mocker((stub_fn_t)rs_socket_tag_sync, 10, 0);
    ret = rs_socket_state_init(0, &conn_socket_err, 0, g_rs_cb);
	free(g_rs_cb);
	g_rs_cb = NULL;
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

	/* resource prepare... */
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);


	conn_node[0].phy_id = 0;

	mocker((stub_fn_t)calloc, 10, NULL);
	conn_node[0].port = 16666;
	ret = rs_socket_batch_connect(conn_node, 1);
	mocker_clean();

	mocker((stub_fn_t)rs_alloc_client_conn_node, 10, 1);
	conn_node[0].port = 16666;
	ret = rs_socket_batch_connect(conn_node, 1);
	mocker_clean();

    rs_ut_msg("--------rs_socket_batch_connect--------\n");

    conn_node[0].phy_id = 0;
    conn_node[0].family = AF_INET;
    strcpy(conn_node[0].tag, "1234");
    conn_node[1].phy_id = 0;
    conn_node[1].family = AF_INET6;
    strcpy(conn_node[1].tag, "5678");
	conn_node[0].port = 16666;
	conn_node[1].port = 16666;
    ret = rs_socket_batch_connect(conn_node, 2);

	/* resource free... */
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_batch_connect2: rs_deinit\n");
	return;
}

void tc_rs_set_tsqp_depth_abnormal()
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

	ret = rs_set_tsqp_depth(129, rdev_index, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_set_tsqp_depth(phy_id, rdev_index, 1, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_set_tsqp_depth(phy_id, rdev_index, temp_depth, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)dl_drv_get_local_dev_id_by_host_dev_id, 10, 1);
	ret = rs_set_tsqp_depth(phy_id, rdev_index, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)rs_rdev2rdev_cb, 10, 1);
	ret = rs_set_tsqp_depth(phy_id, rdev_index, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)roce_set_tsqp_depth, 10, 1);
	ret = rs_set_tsqp_depth(phy_id, rdev_index, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_set_tsqp_depth_abnormal: rs_deinit\n");
	return;
}

void tc_rs_get_tsqp_depth_abnormal()
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

	ret = rs_get_tsqp_depth(129, rdev_index, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = rs_get_tsqp_depth(phy_id, rdev_index, &temp_depth, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)dl_drv_get_local_dev_id_by_host_dev_id, 10, 1);
	ret = rs_get_tsqp_depth(phy_id, rdev_index, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)rs_rdev2rdev_cb, 10, 1);
	ret = rs_get_tsqp_depth(phy_id, rdev_index, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)roce_get_tsqp_depth, 10, 1);
	ret = rs_get_tsqp_depth(phy_id, rdev_index, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = rs_rdev_deinit(phy_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_get_tsqp_depth_abnormal: rs_deinit\n");
	return;
}

int stub_rs_ibv_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	if (attr == NULL) {
		return -EINVAL;
	}
	attr->qp_state = 3; // IBV_QPS_RTS
	return 0;
}

void tc_rs_qp_create2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t flag = 0;
	unsigned int rdev_index = 0;
	struct rs_init_config cfg = {0};
	struct rs_qp_resp resp = {0};
	struct rs_qp_cb *qp_cb_t;

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;

	qp_cb_t = calloc(1, sizeof(struct rs_qp_cb));
	rs_ut_msg("____qp_cb_t:%p\n", qp_cb_t);

	/* resource prepare... */
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)calloc, 10, NULL);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_create_comp_channel, 10, NULL);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)rs_drv_create_cq, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)rs_drv_qp_create, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_req_notify_cq, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	// mocker_u64_invoke((stub_u64_fn_t)calloc, (stub_u64_fn_t)stub_calloc, 20);
	// ret = rs_qp_create(dev_id, rdev_index, qp_norm, &qpn);
	// EXPECT_INT_NE(ret, 0);
	// mocker_clean();

	struct rs_qp_cb qp_cb_abnormal;
	struct rs_rdev_cb rdev_cb_abnormal;
	struct rs_cb rs_cb_abnormal;
	struct ibv_qp ib_qp_abnormal;
	rdev_cb_abnormal.rs_cb = &rs_cb_abnormal;
	qp_cb_abnormal.rdev_cb = &rdev_cb_abnormal;
	qp_cb_abnormal.ib_qp = &ib_qp_abnormal;
	mocker((stub_fn_t)memset_s, 10, 1);
	ret = rs_qp_state_modify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 1);
	mocker_clean();

	mocker_invoke(rs_ibv_query_qp, stub_rs_ibv_query_qp, 10);
	ret = rs_qp_state_modify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

#if 1 //rs_qp_exp_create
	mocker((stub_fn_t)memset_s, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_exp_create_qp, 10, NULL);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_qp, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_modify_qp, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_port, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)rs_drv_get_gid_index, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	//!!!!
	mocker((stub_fn_t)rs_drv_get_gid_index, 20, 0);
	mocker((stub_fn_t)ibv_query_gid, 20, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)memset_s , 0, 1, 0);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)memset_s , 0, 0, 1);
        ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
        EXPECT_INT_EQ(ret, -ESAFEFUNC);
        mocker_clean();

	mocker((stub_fn_t)rs_drv_get_random_num, 10, -1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker((stub_fn_t)open, 10, -1);
        ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
        EXPECT_INT_EQ(ret, -EFILEOPER);
        mocker_clean();

	mocker((stub_fn_t)read, 10, -1);
	mocker((stub_fn_t)close, 10, -1);
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();
#endif

	/* resource free... */
	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_qp_create2: rs_deinit\n");
	free(qp_cb_t);

	return;
}

void tc_rs_epoll_ops2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	mocker((stub_fn_t)strcpy_s, 10, -1);
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	
	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while(ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

 	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


//// epoll ctrl error begin
	struct socket_connect_info conn_ctl;
	conn_ctl.phy_id = 0;
	conn_ctl.family = AF_INET;
	conn_ctl.local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn_ctl.remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	memset(conn_ctl.tag, 0, 128);
	strcpy(conn_ctl.tag, "abcde");
	conn_ctl.port = 16666;
	ret = rs_socket_batch_connect(&conn_ctl, 1);

	usleep(SLEEP_TIME);
	usleep(SLEEP_TIME);

	struct socket_fd_data info_ctl;
	info_ctl.phy_id = 0;
	info_ctl.family = AF_INET;
	info_ctl.local_ip.addr.s_addr = inet_addr("127.0.0.3");
	info_ctl.remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	memset(info_ctl.tag, 0, 128);
	strcpy(info_ctl.tag, "abcde");

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &info_ctl, 1);
		usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);

	struct rs_qp_resp resp_ctl = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;
	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp_ctl);
    EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)rs_epoll_ctl, 10, -2);
	ret = rs_qp_connect_async(dev_id, rdev_index, resp_ctl.qpn, info_ctl.fd);
	mocker_clean();

	mocker((stub_fn_t)rs_socket_send, 10, -1);
	ret = rs_qp_connect_async(dev_id, rdev_index, resp_ctl.qpn, info_ctl.fd);
	mocker_clean();
	
	ret = rs_qp_destroy(dev_id, rdev_index, resp_ctl.qpn);

//epoll ctrl error end
/* ===rs_epoll_event_in_handle ut begin --- accept fail=== */
	struct epoll_event events;
	struct rs_cb *rs_cb_t;
	struct rs_rdev_cb *rdev_cb;
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

//!!!!!
	mocker((stub_fn_t)calloc, 10, NULL);
	struct rs_conn_cb con_cb_node;
	ret = rs_alloc_conn_node(&con_cb_node, 16666);
	EXPECT_INT_EQ(ret, -12);

	mocker((stub_fn_t)accept, 1, 9900999);	//will close a unknown fd
	mocker((stub_fn_t)rs_alloc_conn_node, 1, -1);
	rs_epoll_event_in_handle(rs_cb_t, &events);
	mocker_clean();


	/* poll cq */
	struct rs_qp_cb *qp_cb_t, *qp_cb_t2;
	ret = rs_get_rdev_cb(rs_cb, rdev_index, &rdev_cb);
	RS_LIST_GET_HEAD_ENTRY(qp_cb_t, qp_cb_t2, &rdev_cb->qp_list, list, struct rs_qp_cb);
	for(; (&qp_cb_t->list) != &rdev_cb->qp_list;               \
            qp_cb_t = qp_cb_t2, qp_cb_t2 = list_entry(qp_cb_t2->list.next, struct rs_qp_cb, list)){
		events.data.fd = qp_cb_t->channel->fd;
	}
	rs_epoll_event_in_handle(rs_cb_t, &events);

	/* qp info message, rs_socket_recv = 0 error ! */
	ret = rs_qpn2qpcb(dev_id, rdev_index, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	if (qp_cb_t->conn_info == NULL) {
		return;
	}
	int fd_tmp = qp_cb_t->conn_info->connfd;
	qp_cb_t->conn_info->connfd = 99999; //close a non-exist fd
	mocker((stub_fn_t)rs_socket_recv, 1, 0);
	events.data.fd = qp_cb_t->conn_info->connfd;
	rs_epoll_event_in_handle(rs_cb_t, &events);
	qp_cb_t->conn_info->connfd = fd_tmp;
	mocker_clean();


	events.events = EPOLLOUT;
	rs_epoll_event_handle_one(rs_cb_t, &events);

	mocker((stub_fn_t)pthread_detach, 1, 1);
	rs_epoll_handle(rs_cb_t);
	mocker_clean();

	int epollfd_t = rs_cb_t->conn_cb.epollfd;
	int eventfd_t = rs_cb_t->conn_cb.eventfd;
	mocker((stub_fn_t)epoll_create, 1, 1);
	mocker((stub_fn_t)eventfd, 1, -1);

	ret = rs_create_epoll(rs_cb_t);
	EXPECT_INT_NE(ret, 0);
	rs_cb_t->conn_cb.epollfd = epollfd_t;
	rs_cb_t->conn_cb.eventfd = eventfd_t;
	mocker_clean();


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	int try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

// epoll ctrl error begin

	usleep(SLEEP_TIME);

        mocker((stub_fn_t)rs_epoll_ctl, 10, -1);
		listen[0].port = 16666;
        ret = rs_socket_listen_stop(&listen[0], 1);
        EXPECT_INT_NE(ret, 0);
        mocker_clean();

        struct socket_listen_info listen_ctl;
        listen_ctl.phy_id = 0;
        listen_ctl.local_ip.addr.s_addr = inet_addr("127.0.0.9");
		listen_ctl.port = 16666;
        mocker((stub_fn_t)rs_epoll_ctl, 10, -1);
        ret = rs_socket_listen_start(&listen_ctl, 1);
        mocker_clean();

	usleep(SLEEP_TIME);

	struct rs_cb rs_cb_ctl;
	mocker((stub_fn_t)rs_epoll_ctl, 10, -1);
	ret = rs_create_epoll(&rs_cb_ctl);
	mocker_clean();
//epoll ctrl error end


	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);
	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_epoll_ops2: rs_deinit\n");

	return;
}

int stub_rs_epoll_ctl(int epollfd, int op, int fd, int state)
{
	if (op == EPOLL_CTL_ADD) return 0;
	return 1;
}

void tc_rs_qp_connect_async2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(SLEEP_TIME);
	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	int try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	mocker((stub_fn_t)rs_find_sockets, 10, 2);
	ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 2);
	mocker_clean();

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

 	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);

	mocker_invoke(rs_epoll_ctl, stub_rs_epoll_ctl, 10);
	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	mocker_clean();

	struct rs_qp_cb *qp_cb;
	ret = rs_qpn2qpcb(dev_id, rdev_index, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	qp_cb->state = RS_QP_STATUS_DISCONNECT;

	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


/* ===rs_qp_connect_async ut begin === */
	struct rs_qp_cb *qp_cb_t;

	ret = rs_qpn2qpcb(dev_id, rdev_index, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	int state_tmp = qp_cb_t->state;
	qp_cb_t->state = RS_QP_STATUS_CONNECTED;
	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);
	qp_cb_t->state = state_tmp;
#if 0 //!!!!!!!!!!
	qp_cb_t->state = RS_QP_STATUS_DISCONNECT;
	mocker((stub_fn_t)rs_socket_send, 10, 1);
	mocker((stub_fn_t)rs_epoll_ctl, 10, 1);
	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	//qp_cb_t->state = state_tmp;
	mocker_clean();
#endif
/* ===rs_qp_connect_async ut end === */



	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);


	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);


	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_qp_connect_async2: rs_deinit\n");

	return;
}

void tc_rs_send_wr2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phy_id = 0;
	rdev_info.family = AF_INET;
	rdev_info.local_ip.addr.s_addr = inet_addr("127.0.0.1");

	ret = rs_rdev_init(rdev_info, NOTIFY, &rdev_index);
	EXPECT_INT_EQ(ret, 0);

 	struct rs_qp_norm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qp_mode = 1;
	qp_norm.is_exp = 1;

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


/* === rs_send_async ut begin === */

/* === rs_send_async ut end === */


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret, 1);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);


	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);


	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_send_wr2: rs_deinit\n");

	return;
}



void tc_rs_get_gid_index2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t qp_mode = 1;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;


	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(1000); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

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
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


	/* ===rs_get_gid_index ut begin=== */
	struct ibv_port_attr attr = {0};
	int index;
	struct rs_cb *rs_cb_t;
	struct rs_rdev_cb rdev_cb = {0};
	ret = rs_dev2rscb(0, &rs_cb_t, false);
	attr.gid_tbl_len = 3;
	rdev_cb.ib_port = 0;

	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);
	EXPECT_INT_NE(ret, 0);

	attr.state = IBV_PORT_ACTIVE;
	mocker((stub_fn_t)ibv_query_gid_type, 20, 1);
	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_gid, 20, 1);
	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_query_gid , 0, 1, 1);
	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_gid, 20, 0);
	mocker_ret((stub_fn_t)rs_drv_compare_ip_gid, 0, 1, 0);
	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);

	mocker((stub_fn_t)ibv_query_gid, 20, 0);
	mocker_ret((stub_fn_t)rs_drv_compare_ip_gid, 1, 0, 1);
	rdev_cb.local_ip.family = AF_INET6;
	ret = rs_drv_get_gid_index(&rdev_cb, &attr, &index);
	mocker_clean();
	/* ===rs_get_gid_index ut end=== */


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);


	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);


	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_get_gid_index2:rs_deinit\n");

	return;
}


void tc_rs_mr_abnormal2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	uint32_t qp_mode = 1;
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;

	struct rs_mr_cb *mr_cb_normal;
	ret = rs_calloc_mr(0, &mr_cb_normal);
	EXPECT_INT_EQ(ret, -EINVAL);

	struct rs_qp_cb *qp_cb_normal;
	ret = rs_calloc_qpcb(0, &qp_cb_normal);
	EXPECT_INT_EQ(ret, -EINVAL);
	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;


	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(10000); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 100;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(300000);
		try_again--;
	} while(ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

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

	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


	/* ===rs_mr_info_sync ut begin=== */
	void *addr;
	struct rs_mr_cb *mr_cb;
	addr = malloc(RS_TEST_MEM_SIZE);
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	int try_num = 3;
	do {
		ret = rs_mr_reg(dev_id, rdev_index, resp.qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", resp.qpn, ret);
		try_num--;
		usleep(3000);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct rs_qp_cb *qp_cb_t;
	ret = rs_qpn2qpcb(dev_id, rdev_index, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_get_mrcb(qp_cb_t, addr, &mr_cb, &qp_cb_t->mr_list);
	if (mr_cb->qp_cb->conn_info == NULL) {
		free(addr);
		return;
	}
	int connfd_tmp = mr_cb->qp_cb->conn_info->connfd;
	mr_cb->qp_cb->conn_info->connfd = RS_FD_INVALID;
	mr_cb->state = 0;
	ret = rs_mr_info_sync(mr_cb);
	mr_cb->qp_cb->conn_info->connfd = connfd_tmp;

	int state_tmp2 = mr_cb->state;
	mr_cb->state = 0;
	mocker((stub_fn_t)rs_socket_send, 20, 0);
	ret = rs_mr_info_sync(mr_cb);
	EXPECT_INT_NE(ret, 0);
	mr_cb->state = state_tmp2;

	mocker_clean();

	/* ===rs_qp_destroy ut begin=== */
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);
	/* ===rs_qp_destroy ut end=== */



	ret = rs_mr_dereg(dev_id, rdev_index, resp.qpn, addr);
	//EXPECT_INT_EQ(ret, 0);
	free(addr);
	/* ===rs_mr_info_sync ut end=== */


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);


	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	//ret = rs_qp_destroy(dev_id, rdev_index, qpn);


	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_mr_abnormal2:rs_deinit\n");

	return;
}

int stub_halGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 1;
	return 0;
}

void tc_rs_socket_ops2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	uint32_t qp_mode = 1;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct socket_wlist_info_t white_list;
    struct rs_cb *rs_cb;

	ret = rs_socket_vnic2nodeid(0);
	EXPECT_INT_EQ(ret, 0);

	struct socket_fd_data conn_server;
	conn_server.local_ip.addr.s_addr = 1;
	rs_sockets_serverip_converter(&conn_server, 1, 1);

	ret = rs_get_sockets(0, &conn_server, 1);
	EXPECT_INT_NE(ret, 0);

	struct rs_conn_info conn_tmp;
	conn_tmp.state = 0;
	conn_tmp.server_ip.family = AF_INET;
	conn_tmp.server_ip.bin_addr.addr.s_addr = 2;
	//ret = rs_find_sockets(&conn_tmp, &conn_server, 1, 0);
	//EXPECT_INT_EQ(ret, -22);

	conn_tmp.state = 0;
	conn_server.local_ip.addr.s_addr = 1;
	conn_tmp.server_ip.family = AF_INET;
	conn_tmp.server_ip.bin_addr.addr.s_addr = 1;
	memset(conn_server.tag, 0, sizeof(conn_server.tag));
	memset(conn_tmp.tag, 1, sizeof(conn_tmp.tag));
	//ret = rs_find_sockets(&conn_tmp, &conn_server, 1, 0);
	//EXPECT_INT_EQ(ret, -22);

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 1;
	cfg.hccp_mode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	mocker((stub_fn_t)system, 10, 0);
	mocker((stub_fn_t)access, 10, 0);
    mocker_invoke((stub_fn_t)halGetDeviceInfo, stub_halGetDeviceInfo, 10);
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);
    strcpy(white_list.tag, "1234");

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

#if 1
	{
		/* ===rs_get_sockets ut begin=== */
		struct rs_conn_cb *conn_cb;
		struct rs_conn_info *conn_tmp, *conn_tmp2;
		int state_tmp = 0;
		ret = rs_dev2conncb(dev_id, &conn_cb);
		RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, &conn_cb->client_conn_list, list, struct rs_conn_info);
		for(; (&conn_tmp->list) != &conn_cb->client_conn_list;
            conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct rs_conn_info, list)) {
			if (conn_tmp->server_ip.bin_addr.addr.s_addr == socket_info[i].local_ip.addr.s_addr) {
				state_tmp = conn_tmp->state;
				break;
			}
		}
		rs_ut_msg("ori state:%d\n", state_tmp);
		conn_tmp->state = RS_CONN_STATE_INIT;
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();


		/* wrong server ip address */
		socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.4");
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");

		rs_ut_msg("conn_tmp->state:%d\n", conn_tmp->state);
		conn_tmp->state = RS_CONN_STATE_CONNECTED;
		mocker((stub_fn_t)send, 10, SOCK_CONN_TAG_SIZE);
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();

		conn_tmp->state = RS_CONN_STATE_TIMEOUT;
		// mocker((stub_fn_t)send, 10, 1);
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();

		conn_tmp->state = RS_CONN_STATE_TAG_SYNC;
		// mocker((stub_fn_t)memcpy_s, 10, 1);
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();
	}
#endif	

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

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
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("[server]socket_info[1].fd:%d, status:%d\n",
		socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);


	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	struct rs_conn_info conn_send_inc;
	mocker((stub_fn_t)send, 10, -1);
	rs_socket_tag_sync(&conn_send_inc);
	mocker_clean();

	mocker(rs_drv_socket_send, 10, -EAGAIN);
	rs_socket_tag_sync(&conn_send_inc);
	mocker_clean();

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);
	cfg.chip_id = 0;
	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_ops2: rs_deinit\n");

	return;
}

void tc_rs_socket_close2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdev_index = 0;
	int flag = 0; /* RC */
	struct rs_qp_resp resp = {0};
	struct rs_qp_resp resp2 = {0};
	uint32_t qp_mode = 1;
	int i;
	struct rs_init_config cfg = {0};
	struct socket_listen_info listen[2] = {0};
	struct socket_connect_info conn[2] = {0};
	struct rs_socket_close_info_t sock_close[2] = {0};
	struct socket_fd_data socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chip_id = 0;
	cfg.hccp_mode = NETWORK_OFFLINE;
	ret = rs_init(&cfg);
	EXPECT_INT_EQ(ret, 0);

    ret = rs_dev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->conn_cb.wlist_enable = 0;

	usleep(SLEEP_TIME);

	listen[0].phy_id = 0;
	listen[0].family = AF_INET;
	listen[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = rs_socket_listen_start(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phy_id = 0;
	conn[0].family = AF_INET;
	conn[0].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = rs_socket_batch_connect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remote_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

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
	rs_ut_msg("rs_qp_create: qpn %d, ret:%d\n", resp.qpn, ret);


	ret = rs_qp_connect_async(dev_id, rdev_index, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***rs_qp_connect_async: %d****\n", ret);

	usleep(SLEEP_TIME);


	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].local_ip.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = rs_get_sockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("[server]socket_info[1].fd:%d, status:%d\n",
		socket_info[i].fd, socket_info[i].status);

	ret = rs_qp_create(dev_id, rdev_index, qp_norm, &resp2);
	rs_ut_msg("rs_qp_create: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[i].fd);

	usleep(1000);

	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = rs_socket_batch_close(0, &sock_close[0], 1);
	usleep(SLEEP_TIME);

	ret = rs_qp_connect_async(dev_id, rdev_index, resp2.qpn, socket_info[1].fd);

	void* addr;
	addr = malloc(RS_TEST_MEM_SIZE);
	struct rdma_mr_reg_info mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = rs_mr_reg(dev_id, rdev_index, resp2.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	free(addr);

	sock_close[1].fd = socket_info[1].fd;
	ret = rs_socket_batch_close(0, &sock_close[1], 1);

	ret = rs_qp_destroy(dev_id, rdev_index, resp2.qpn);
	
	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = rs_socket_listen_stop(&listen[0], 1);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_close2: rs_deinit\n");

	return;

}
void tc_rs_abnormal2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t rdev_index = 0;
	uint32_t err_dev_id = 10;
	struct rs_init_config cfg = {0};
	struct rs_qp_resp resp = {0};
	uint32_t qp_mode = 1;
	struct rs_qp_cb *qp_cb = NULL;
	char buf[64] = {0};
	uint32_t *cmd;
	struct ibv_cq *ib_send_cq_t, *ib_recv_cq_t;
	unsigned int total_size = 2;
	unsigned int cur_size = 1;
	bool flag = true;
	char *buf_tmp = NULL;

	/* resource prepare... */
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

	ret = rs_qpn2qpcb(dev_id, rdev_index, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);


// rs_notify_mr_list_add begin
	mocker((stub_fn_t)calloc, 10, NULL);
	ret = rs_notify_mr_list_add(qp_cb, buf);
	//EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	ret = rs_notify_mr_list_add(qp_cb, buf);
	//EXPECT_INT_NE(ret, 0);
	mocker_clean();
// rs_notify_mr_list_add end


// rs_epoll_recv_handle begin
	cmd = (uint32_t *)buf;
	*cmd = RS_CMD_QP_INFO;
	mocker((stub_fn_t)memcpy_s, 10, 1);
	rs_epoll_recv_handle(qp_cb, buf, 64);
	mocker_clean();

	mocker((stub_fn_t)rs_qp_state_modify, 10, 1);
	rs_epoll_recv_handle(qp_cb, buf, 64);
	mocker_clean();

	*cmd = RS_CMD_MR_INFO;
	mocker((stub_fn_t)calloc, 10, NULL);
	rs_epoll_recv_handle(qp_cb, buf, 64);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	rs_epoll_recv_handle(qp_cb, buf, 64);
	mocker_clean();

	/* unknown cmd */
	*cmd = RS_CMD_QP_INFO + 444;
	rs_epoll_recv_handle(qp_cb, buf, 64);
// rs_epoll_recv_handle end


//rs_epoll_recv_handle_remain begin
	mocker((stub_fn_t)memcpy_s, 10, 1);
	rs_epoll_recv_handle_remain(qp_cb, total_size, cur_size, flag, buf_tmp);
	mocker_clean();
//rs_epoll_recv_handle_remain end
// rs_create_cq begin
	mocker((stub_fn_t)ibv_create_cq, 10, NULL);
	ib_send_cq_t = qp_cb->ib_send_cq;
	ib_recv_cq_t = qp_cb->ib_recv_cq;
	ret = rs_drv_create_cq(qp_cb, 0);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_create_cq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = rs_drv_create_cq(qp_cb, 0);
	qp_cb->ib_send_cq = ib_send_cq_t;
	qp_cb->ib_recv_cq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_create_cq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = rs_drv_create_cq(qp_cb, 0);
	qp_cb->ib_send_cq = ib_send_cq_t;
	qp_cb->ib_recv_cq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)rs_ibv_exp_create_cq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = rs_drv_create_cq(qp_cb, 1);
	qp_cb->ib_send_cq = ib_send_cq_t;
	qp_cb->ib_recv_cq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
// rs_create_cq end

//rs_qp_state_modify begin
	mocker((stub_fn_t)rs_ibv_query_qp, 10, 1);
	rs_qp_state_modify(qp_cb);
	mocker_clean();

	mocker((stub_fn_t)ibv_modify_qp, 10, 1);
	rs_qp_state_modify(qp_cb);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_modify_qp , 0, 1, 0);
	rs_qp_state_modify(qp_cb);
	mocker_clean();
//rs_qp_state_modify end

	mocker((stub_fn_t)pthread_mutex_lock, 10, 1);
	mocker((stub_fn_t)pthread_mutex_unlock, 10, 1);
	(void)pthread_mutex_lock(NULL);
	(void)pthread_mutex_unlock(NULL);
	mocker_clean();

//rs_poll_cq_handle begin
	rs_drv_poll_cq_handle(qp_cb);

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

	qp_cb->ib_send_cq = ib_send_cq_t;
//rs_poll_cq_handle end

	/* resource free... */
	ret = rs_qp_destroy(dev_id, rdev_index, resp.qpn);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_rdev_deinit(dev_id, NOTIFY, rdev_index);
	EXPECT_INT_EQ(ret, 0);

	ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_abnormal2: rs_deinit\n");
	return;
}

struct rs_conn_info g_conn = {0};

int stub_rs_fd2conn(int fd, struct rs_conn_info **conn)
{

    *conn = &g_conn;
    return 0;
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

    ret = rs_connect_handle(NULL);
    EXPECT_INT_EQ(ret, NULL);
    return;
}

int replace_rs_qpn2qpcb(unsigned int phy_id, unsigned int rdev_index, uint32_t qpn, struct rs_qp_cb **qp_cb)
{
	static struct rs_qp_cb a_qp_cb;
	*qp_cb = &a_qp_cb;
	return 0;
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

void tc_tls_abnormal1()
{
    int ret;
    uint32_t dev_id = 0;
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

    cfg.chip_id = 0;
    cfg.hccp_mode = NETWORK_OFFLINE;
    ret = rs_init(&cfg);
    EXPECT_INT_EQ(ret, 0);

    g_rs_cb->ssl_enable = 1;
    mocker_invoke((stub_fn_t)rs_fd2conn, stub_rs_fd2conn, 10);
    mocker((stub_fn_t)SSL_write, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, 2);
    rs_drv_socket_send(socket_info[0].fd, "1", 1, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)rs_fd2conn, stub_rs_fd2conn, 10);
    mocker((stub_fn_t)SSL_read, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, 2);
    rs_drv_socket_recv(socket_info[1].fd, "1", 1, 0);
    mocker_clean();

    mocker((stub_fn_t)rs_fd2conn, 10, -1);
    rs_drv_socket_recv(socket_info[1].fd, "1", 1, 0);
    mocker_clean();
    g_rs_cb->ssl_enable = 0;
    rs_drv_socket_recv(socket_info[1].fd, "1", 1, 0);

    mocker((stub_fn_t)fcntl, 10, -1);
    ret = rs_set_fd_nonblock(-1);
    EXPECT_INT_EQ(ret, -EFILEOPER);
    mocker_clean();

    mocker_ret((stub_fn_t)fcntl, 1, -1, 0);
    ret = rs_set_fd_nonblock(-1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    struct rs_conn_info conn_ssl;
    conn_ssl.ssl = NULL;
    conn_ssl.client_ip.family = AF_INET;
    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    ret = rs_socket_ssl_connect(&conn_ssl, g_rs_cb);
    EXPECT_INT_EQ(ret, -EAGAIN);
    mocker_clean();

    mocker((stub_fn_t)SSL_get_verify_result, 10, -1);
    ret = rs_socket_ssl_connect(&conn_ssl, g_rs_cb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)fcntl, 10, -1);
    ret = rs_socket_state_reset(0, &conn_ssl, 1, g_rs_cb);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker((stub_fn_t)SSL_set_fd, 10, -1);
    ret = rs_socket_state_connected(&conn_ssl, 1, g_rs_cb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    ret = rs_socket_state_ssl_fd_bind(&conn_ssl, 1, g_rs_cb);
    EXPECT_INT_EQ(ret, -EAGAIN);
    mocker_clean();

    conn_ssl.state = RS_CONN_STATE_SSL_BIND_FD;
    ret = rs_socket_connect_async(&conn_ssl, g_rs_cb);
    EXPECT_INT_EQ(ret, 0);

    struct rs_accept_info ssl_info;
    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    rs_do_ssl_handshake(&ssl_info, g_rs_cb);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, SSL_ERROR_WANT_WRITE);
    rs_do_ssl_handshake(&ssl_info, g_rs_cb);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, SSL_ERROR_WANT_READ);
    rs_do_ssl_handshake(&ssl_info, g_rs_cb);
    mocker_clean();

    mocker((stub_fn_t)calloc, 10, 0);
    ret = rs_get_pk(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker((stub_fn_t)SSL_new, 10, 0);
    ret = rs_drv_ssl_bind_fd(&conn_ssl, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    ret = rs_drv_socket_send(-1, "abc", 3, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = rs_drv_socket_recv(-1, "abc", 3, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker((stub_fn_t)rs_ssl_get_crl_data, 10, 0);
    mocker((stub_fn_t)SSL_CTX_get_cert_store, 10, 0);
    ret = rs_ssl_crl_init(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();


    struct tls_cert_mng_info mng_info;
    struct rs_cb err_rscb;
	struct rs_certs certs;
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM];
    err_rscb.chip_id = 0;
    mng_info.cert_count = 0;
    mng_info.total_cert_len = 100;
    ret = rs_ssl_put_certs(&err_rscb, &mng_info, &certs, &new_certs, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();


    mocker((stub_fn_t)X509_STORE_new, 10, 0);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mocker((stub_fn_t)X509_STORE_CTX_new, 10, 0);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    int rs_ssl_verify_cert_chain(X509_STORE_CTX *ctx, X509_STORE *store,
        struct rs_certs *certs, STACK_OF(X509) *cert_chain, struct tls_cert_mng_info *mng_info);
    mocker((stub_fn_t)rs_ssl_verify_cert_chain, 10, -1);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker((stub_fn_t)rs_ssl_verify_cert_chain, 10, -1);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    X509 *tls_load_cert(const uint8_t *inbuf, uint32_t buf_len, int type);
    mocker((stub_fn_t)tls_load_cert, 10, 0);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)X509_STORE_CTX_init, 10, 0);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)X509_verify_cert, 10, 0);
    ret = rs_ssl_check_cert_chain(&mng_info, &certs);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)BIO_new_mem_buf, 10, 0);
    tls_load_cert(NULL, 0, 0);
    mocker_clean();
    
    mocker((stub_fn_t)d2i_X509_bio, 10, 0);
    X509* cert = tls_load_cert(NULL, 0, SSL_FILETYPE_ASN1);
    free(cert);
    mocker_clean();
    
    mocker((stub_fn_t)d2i_X509_bio, 10, 0);
    tls_load_cert(NULL, 0, 100);
    mocker_clean();
    
    mocker((stub_fn_t)PEM_read_bio_X509, 10, 0);
    tls_load_cert(NULL, 0, SSL_FILETYPE_PEM);
    mocker_clean();

    ret = rs_remove_certs("123.txt", "456.txt");
    EXPECT_INT_EQ(ret, 0);

    mocker((stub_fn_t)calloc, 10, 0);
    err_rscb.skid_subject_cb = NULL;
    ret = rs_ssl_skid_get_from_chain(&err_rscb, NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    mng_info.cert_count = 2;
    mocker((stub_fn_t)BIO_new_mem_buf, 10, 0);
    ret = rs_ssl_skid_get_from_chain(&err_rscb, &mng_info, &certs, &new_certs);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)tls_get_user_config, 10, -1);
    ret = rs_ssl_init(&err_rscb);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

	mng_info.ky_len = RS_SSL_PRI_LEN + 1;
	mocker((stub_fn_t)rs_get_pridata, 10, 0);
	ret = rs_get_pk(&err_rscb, &mng_info, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);
	mng_info.ky_len = 0;
	mocker_clean();

    ret = rs_deinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_tls_abnormal1: rs_deinit\n");
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

	mocker((stub_fn_t)dl_drv_device_get_index_by_phy_id, 1, -1);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker((stub_fn_t)dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker((stub_fn_t)dl_hal_get_device_info, 1, -2);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker((stub_fn_t)dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker((stub_fn_t)dl_hal_get_device_info, 1, 0);
	mocker((stub_fn_t)dl_hal_get_chip_info, 1, -2);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	mocker_clean();

	mocker(dl_drv_device_get_index_by_phy_id, 1, 0);
	mocker(dl_hal_get_device_info, 1, 0);
	mocker_invoke(dl_hal_get_chip_info, stub_dl_hal_get_chip_info_910_93, 100);
	rs_socket_get_bind_by_chip(chip_id, &bind_ip);
	EXPECT_INT_EQ(bind_ip, true);
	mocker_clean();
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

int *stub__errno_location()
{
    static int err_no = 0;

    err_no = EAGAIN;
    return &err_no;
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
