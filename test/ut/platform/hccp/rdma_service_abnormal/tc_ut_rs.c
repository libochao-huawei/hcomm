/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

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
#include "hccp_common.h"
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

extern __thread struct rs_cb *gRsCb;
int DlDrvGetLocalDevIdByHostDevId(unsigned int devId, unsigned int* chipId);

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

int tls_get_user_config(unsigned int save_mode, unsigned int chipId, const char *name,
    unsigned char *buf, unsigned int *buf_size);

void tls_get_enable_info(unsigned int save_mode, unsigned int chipId, unsigned char *buf, unsigned int buf_size);

int rs_ssl_get_crl_data(struct rs_cb *rscb, FILE* fp, struct tls_cert_manage_info *mng_info, X509_CRL *crl);

int rs_get_pridata(struct rs_cb *rscb, struct RsSecPara *rs_para, struct tls_cert_mng_info *mng_info);

int rs_ssl_put_certs(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info, struct RsCerts *certs,
    struct tls_ca_new_certs *new_certs, struct CertFile *file_name);

#define SLEEP_TIME 500000
#define rs_ut_msg(fmt, args...)	fprintf(stderr, "\t>>>>> " fmt, ##args)

int try_again;
struct RsQpCb qp_cb_tmp2;
const char *s_tmp = "suc";
struct RsConnInfo *g_conn_info;

int RsAllocConnNode(struct RsConnInfo **conn, unsigned short server_port);
int RsGetConnInfo(struct RsConnCb *connCb, struct SocketConnectInfo *conn,
    struct RsConnInfo **connInfo, unsigned int serverPort);
int RsGetRsCb(unsigned int phyId, struct rs_cb **rs_cb);
int rs_qp_exp_create(struct RsQpCb *qp_cb);
int RsNotifyMrListAdd(struct RsQpCb *qp_cb, char *buf);
void RsEpollRecvHandle(struct RsQpCb *qp_cb, char *buf, int size);
void RsDrvPollCqHandle(struct RsQpCb *qp_cb);
int rs_create_cq(struct RsQpCb *qp_cb);
int RsQpStateModify(struct RsQpCb *qp_cb);
void RsEpollEventInHandle(struct rs_cb *rs_cb, struct epoll_event *events);
void RsEpollEventHandleOne(struct rs_cb *rs_cb, struct epoll_event *events);
int RsMrInfoSync(struct RsMrCb *mr_cb);
int RsDrvGetGidIndex(struct RsRdevCb *rdevCb, struct ibv_port_attr *attr, int *idx);
void RsEpollCtl(int epollfd, int op, int fd, int state);
int rs_socket_connect(struct RsConnInfo *conn);
int rs_post_recv_stub(struct ibv_qp *qp, struct ibv_recv_wr *wr,
				struct ibv_recv_wr **bad_wr);
int RsDrvGetRandomNum(int *rand_num);
void RsSocketTagSync(struct RsConnInfo *conn);
int RsSocketStateInit(unsigned int chipId, struct RsConnInfo *conn, uint32_t sslEnable, struct rs_cb *rscb);
int RsFindSockets(struct RsConnInfo *conn_tmp, struct SocketFdData conn[],
                    int num, int role);
int RsAllocClientConnNode(struct RsConnCb *connCb, enum RsConnRole role,
                    struct RsConnInfo **conn, struct SocketConnectInfo *socket_conn, int server_port);
uint32_t RsSocketVnic2nodeid(uint32_t ip_addr);
int roce_set_tsqp_depth(const char *dev_name, unsigned int rdevIndex, unsigned int temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int roce_get_tsqp_depth(const char *dev_name, unsigned int rdevIndex, unsigned int *temp_depth,
    unsigned int *qp_num, unsigned int *sq_depth);
int RsSocketNodeid2vnic(uint32_t node_id, uint32_t *ip_addr);
int RsServerValidAsyncInit(unsigned int chipId, struct RsConnInfo *conn, struct SocketWlistInfoT *white_list_expect);
extern int RsConnectBindClient(int fd, struct RsConnInfo *conn);
extern void RsSocketGetBindByChip(unsigned int chipId, bool *bind_ip);
extern int RsInitRscbCfg(struct rs_cb *rscb);
extern void RsDeinitRscbCfg(struct rs_cb *rscb);
extern int RsSocketCloseFd(int fd);
extern int RsFindWhiteList(struct RsConnCb *connCb, struct RsIpAddrInfo *serverIp, struct RsWhiteList **whiteList);
extern int RsFindWhiteListNode(struct RsWhiteList *rs_socket_white_list,
    struct SocketWlistInfoT *white_list_expect, int family, struct RsWhiteListInfo **white_list_node);
extern int RsServerSendWlistCheckResult(struct RsConnInfo *conn, bool flag);
extern uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id);
extern uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id);
extern int rs_net_adapt_api_init(void);

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

struct RsWhiteListInfo g_white_list_node_tmp = {0};
int stub_rs_find_white_list_node(struct RsWhiteList *rs_socket_white_list,
    struct SocketWlistInfoT *white_list_expect, int family, struct RsWhiteListInfo **white_list_node)
{
	*white_list_node = &g_white_list_node_tmp;
	return 0;
}

int rs_post_send_stub(struct ibv_qp *qp, struct ibv_send_wr *wr,
				struct ibv_send_wr **bad_wr);

int rs_qp_info_sync(struct RsQpCb *qp_cb);
int RsCreateEpoll(struct rs_cb *rs_cb);
int RsGetMrcb(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb, struct RsListHead *mr_list);

void* RsEpollHandle();
int memcpy_s(void * dest, size_t destMax, const void * src, size_t count);
int memset_s(void * dest, size_t destMax, int c, size_t count);

void tc_rs_init2()
{
	int ret;
	struct RsInitConfig cfg = {0};

	struct RsConnInfo *info;
	ret = RsFd2conn(0, &info);
	EXPECT_INT_EQ(ret, -ENODEV);

	cfg.hccpMode = NETWORK_OFFLINE;

	mocker((stub_fn_t)pthread_mutex_init, 20, 1);
	ret = RsInit(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)epoll_create, 20, -1);
	mocker((stub_fn_t)pthread_create, 20, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_create, 20, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)pthread_create, 0, -1, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_NE(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_detach, 10, -1);
	RsConnectHandle(&cfg);
	mocker_clean();

	cfg.chipId = 3;
	cfg.hccpMode = NETWORK_ONLINE;
	mocker((stub_fn_t)calloc, 10, NULL);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, -12);
	mocker_clean();

	return;
}

void tc_rs_deinit2()
{
	int ret;
	uint32_t dev_id = 0;
	struct RsInitConfig cfg;
	struct rs_cb *rs_cb = NULL;

	/* resource prepare... */
	cfg.hccpMode = NETWORK_OFFLINE;
	cfg.chipId = 0;
	cfg.whiteListStatus = 1;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	/* resource free... */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_deinit2: rs_deinit1\n");
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDev2rscb(dev_id, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)write, 20, 1);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();
    rs_ut_msg("!!!!!!tc_rs_deinit2: rs_deinit2\n");

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct rs_cb *rscb = NULL;
	rscb = calloc(1, sizeof(struct rs_cb));
	rscb->hccpMode = NETWORK_OFFLINE;
	RS_INIT_LIST_HEAD(&rscb->connCb.clientConnList);
	RsInitRscbCfg(rscb);
	RsDeinitRscbCfg(rscb);
	mocker((stub_fn_t)write, 20, 1);
	RsDeinitRscbCfg(rscb);
	mocker_clean();
	free(rscb);
	rscb = NULL;
	return;
}

void tc_rs_rdev_init()
{
	int ret;
	unsigned int rdevIndex = 0;
	struct rdev rdev_info = {0};
	rdev_info.phyId = 10;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);

	rdev_info.phyId = 0;

	mocker((stub_fn_t)RsGetRsCb, 20, 1);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)calloc, 20, NULL);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)pthread_mutex_init, 20, 1);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_get_device_list, 10, -1);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_exp_query_notify, 20, 1);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_alloc_pd, 20, 0);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_reg_mr, 20, 0);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
}

void tc_rs_socket_init()
{
	int ret;
	int i;
	unsigned int vnic_ip[8] = {0};

	ret = RsSocketInit(NULL, 0);
	EXPECT_INT_EQ(ret, -22);

	ret = RsSocketInit(vnic_ip, 8);
	EXPECT_INT_EQ(ret, 0);

	return;
}

extern __thread struct rs_cb *gRsCb;
void tc_rs_socket_deinit()
{
	int ret;
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	struct rs_cb g_rs_cb_tmp = {0};
	gRsCb = &g_rs_cb_tmp;

	mocker((stub_fn_t)RsDev2rscb, 20, 1);
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	rdev_info.family = 3;
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_NE(ret, 0);

	rdev_info.phyId = 10;
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_NE(ret, 0);
	gRsCb = NULL;
}

void tc_rs_rdev_deinit()
{
	int ret;
	unsigned int rdevIndex = 00;

	ret = RsRdevDeinit(10, NOTIFY, 1);
	EXPECT_INT_NE(ret, 0);

	ret = RsRdevDeinit(0, NOTIFY, rdevIndex);
	EXPECT_INT_NE(ret, 0);
}

void tc_rs_socket_listen_start2()
{
	int ret;
	uint32_t dev_id = 0;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen_node[2] = {0};
	struct rs_cb *rs_cb;

	/* resource prepare... */
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen_node[0].phyId = 0;
	listen_node[0].family = AF_INET;
    listen_node[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	mocker((stub_fn_t)calloc, 10, NULL);
	listen_node[0].port = 16666;
	ret = RsSocketListenStart(listen_node, 1);
	mocker_clean();

	mocker((stub_fn_t)socket, 10, -1);
	listen_node[0].port = 16666;
	ret = RsSocketListenStart(listen_node, 1);
	mocker_clean();

	mocker((stub_fn_t)socket, 10, 0);
	mocker((stub_fn_t)setsockopt, 10, -1);
	listen_node[0].port = 16666;
        ret = RsSocketListenStart(listen_node, 1);
        mocker_clean();

	listen_node[0].family = AF_INET;
    listen_node[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	mocker((stub_fn_t)bind, 10, 1);
	listen_node[0].port = 16666;
	ret = RsSocketListenStart(listen_node, 1);
	EXPECT_INT_NE(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)listen, 10, -1);
	listen_node[0].port = 16666;
	ret = RsSocketListenStart(listen_node, 1);
	EXPECT_INT_NE(ret, -1);
	mocker_clean();

	/* twice listen but first failed */
	struct SocketListenInfo listen_twice[2] = {0};
	listen_twice[0].localIp.addr.s_addr  = inet_addr("127.0.0.4");
	listen_twice[1].phyId = 1;
	listen_twice[0].family = AF_INET;
	listen_twice[1].family = AF_INET;
	listen_twice[0].port = 16666;
	listen_twice[1].port = 16666;
	ret = RsSocketListenStart(listen_twice, 2);
	mocker_clean();

	/* resource free... */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_listen_start2: RsDeinit\n");
	return;
}

void tc_rs_socket_batch_connect2()
{
	int ret;
	uint32_t dev_id = 0;
	struct RsInitConfig cfg = {0};
	struct SocketConnectInfo conn_node[2] = {0};
	struct RsConnInfo conn_socket_err;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, -1);
	ret = RsSocketStateReset(0, &conn_socket_err, gRsCb->sslEnable, gRsCb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 0;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, 1);
	ret = RsSocketStateReset(0, &conn_socket_err, gRsCb->sslEnable, gRsCb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	memset(gRsCb, 0, sizeof(struct rs_cb));
	conn_socket_err.state = 4;
	mocker((stub_fn_t)RsSocketTagSync, 10, -1);
	ret = RsSocketConnectAsync(&conn_socket_err, gRsCb);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	gRsCb->connCb.wlistEnable = 1;
	gRsCb->sslEnable = 1;
	conn_socket_err.state = 7;
	mocker((stub_fn_t)RsSocketRecv, 1, -11);
	ret = RsSocketConnectAsync(&conn_socket_err, gRsCb);
	mocker_clean();
	mocker((stub_fn_t)RsSocketRecv, 1, 0);
	mocker((stub_fn_t)SSL_shutdown, 1, 0);
	mocker((stub_fn_t)SSL_free, 1, 0);
	ret = RsSocketConnectAsync(&conn_socket_err, gRsCb);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)RsSocketStateInit, 10, -1);
	ret = RsSocketConnectAsync(&conn_socket_err, gRsCb);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)socket, 10, 0);
	mocker((stub_fn_t)connect, 10, -1);
	ret = RsSocketStateReset(0, &conn_socket_err, 0, gRsCb);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	conn_socket_err.state = 1;
	mocker((stub_fn_t)connect, 10, -1);
	mocker((stub_fn_t)RsSocketTagSync, 10, 0);
	ret = RsSocketStateInit(0, &conn_socket_err, 0, gRsCb);
	free(gRsCb);
	gRsCb = NULL;
	EXPECT_INT_NE(ret, 0);
    mocker_clean();

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	conn_socket_err.state = 1;
    mocker((stub_fn_t)connect, 10, 0);
	mocker((stub_fn_t)getsockname, 10, 0);
	mocker((stub_fn_t)RsSocketTagSync, 10, 0);
    ret = RsSocketStateInit(0, &conn_socket_err, 0, gRsCb);
	free(gRsCb);
	gRsCb = NULL;
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

	/* resource prepare... */
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	conn_node[0].phyId = 0;

	mocker((stub_fn_t)calloc, 10, NULL);
	conn_node[0].port = 16666;
	ret = RsSocketBatchConnect(conn_node, 1);
	mocker_clean();

	mocker((stub_fn_t)RsAllocClientConnNode, 10, 1);
	conn_node[0].port = 16666;
	ret = RsSocketBatchConnect(conn_node, 1);
	mocker_clean();

    rs_ut_msg("--------RsSocketBatchConnect--------\n");

    conn_node[0].phyId = 0;
    conn_node[0].family = AF_INET;
    strcpy(conn_node[0].tag, "1234");
    conn_node[1].phyId = 0;
    conn_node[1].family = AF_INET6;
    strcpy(conn_node[1].tag, "5678");
	conn_node[0].port = 16666;
	conn_node[1].port = 16666;
    ret = RsSocketBatchConnect(conn_node, 2);

	/* resource free... */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_batch_connect2: RsDeinit\n");
	return;
}

void tc_rs_set_tsqp_depth_abnormal()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	unsigned int temp_depth = 8;
	unsigned int qp_num;
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	struct RsInitConfig cfg = {0};
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSetTsqpDepth(129, rdevIndex, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsSetTsqpDepth(phyId, rdevIndex, 1, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsSetTsqpDepth(phyId, rdevIndex, temp_depth, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)DlDrvGetLocalDevIdByHostDevId, 10, 1);
	ret = RsSetTsqpDepth(phyId, rdevIndex, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)RsRdev2rdevCb, 10, 1);
	ret = RsSetTsqpDepth(phyId, rdevIndex, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)roce_set_tsqp_depth, 10, 1);
	ret = RsSetTsqpDepth(phyId, rdevIndex, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_set_tsqp_depth_abnormal: RsDeinit\n");
	return;
}

void tc_rs_get_tsqp_depth_abnormal()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	unsigned int temp_depth = 8;
	unsigned int qp_num;
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	struct RsInitConfig cfg = {0};
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetTsqpDepth(129, rdevIndex, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetTsqpDepth(phyId, rdevIndex, &temp_depth, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)DlDrvGetLocalDevIdByHostDevId, 10, 1);
	ret = RsGetTsqpDepth(phyId, rdevIndex, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)RsRdev2rdevCb, 10, 1);
	ret = RsGetTsqpDepth(phyId, rdevIndex, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)roce_get_tsqp_depth, 10, 1);
	ret = RsGetTsqpDepth(phyId, rdevIndex, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_get_tsqp_depth_abnormal: RsDeinit\n");
	return;
}

int stub_RsIbvQueryQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	if (attr == NULL) {
		return -EINVAL;
	}
	attr->qp_state = 3;
	return 0;
}

void tc_rs_qp_create2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t flag = 0;
	unsigned int rdevIndex = 0;
	struct RsInitConfig cfg = {0};
	struct RsQpResp resp = {0};
	struct RsQpCb *qp_cb_t;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

	qp_cb_t = calloc(1, sizeof(struct RsQpCb));
	rs_ut_msg("____qp_cb_t:%p\n", qp_cb_t);

	/* resource prepare... */
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)calloc, 10, NULL);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_create_comp_channel, 10, NULL);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)RsDrvCreateCq, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)RsDrvQpCreate, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_req_notify_cq, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	struct RsQpCb qp_cb_abnormal;
	struct RsRdevCb rdev_cb_abnormal;
	struct rs_cb rs_cb_abnormal;
	struct ibv_qp ib_qp_abnormal;
	rdev_cb_abnormal.rs_cb = &rs_cb_abnormal;
	qp_cb_abnormal.rdevCb = &rdev_cb_abnormal;
	qp_cb_abnormal.ibQp = &ib_qp_abnormal;
	mocker((stub_fn_t)memset_s, 10, 1);
	ret = RsQpStateModify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 1);
	mocker_clean();

	mocker_invoke(RsIbvQueryQp, stub_RsIbvQueryQp, 10);
	ret = RsQpStateModify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)memset_s, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_exp_create_qp, 10, NULL);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_qp, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_modify_qp, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_port, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)RsDrvGetGidIndex, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)RsDrvGetGidIndex, 20, 0);
	mocker((stub_fn_t)ibv_query_gid, 20, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)memset_s , 0, 1, 0);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)memset_s , 0, 0, 1);
        ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
        EXPECT_INT_EQ(ret, -ESAFEFUNC);
        mocker_clean();

	mocker((stub_fn_t)RsDrvGetRandomNum, 10, -1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker((stub_fn_t)open, 10, -1);
        ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
        EXPECT_INT_EQ(ret, -EFILEOPER);
        mocker_clean();

	mocker((stub_fn_t)read, 10, -1);
	mocker((stub_fn_t)close, 10, -1);
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, -EFILEOPER);
	mocker_clean();

	/* resource free... */
	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_qp_create2: RsDeinit\n");
	free(qp_cb_t);

	return;
}

void tc_rs_epoll_ops2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	mocker((stub_fn_t)strcpy_s, 10, -1);
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while(ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

 	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

	struct SocketConnectInfo conn_ctl;
	conn_ctl.phyId = 0;
	conn_ctl.family = AF_INET;
	conn_ctl.localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn_ctl.remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	memset(conn_ctl.tag, 0, 128);
	strcpy(conn_ctl.tag, "abcde");
	conn_ctl.port = 16666;
	ret = RsSocketBatchConnect(&conn_ctl, 1);

	usleep(SLEEP_TIME);
	usleep(SLEEP_TIME);

	struct SocketFdData info_ctl;
	info_ctl.phyId = 0;
	info_ctl.family = AF_INET;
	info_ctl.localIp.addr.s_addr = inet_addr("127.0.0.3");
	info_ctl.remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	memset(info_ctl.tag, 0, 128);
	strcpy(info_ctl.tag, "abcde");

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &info_ctl, 1);
		usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);

	struct RsQpResp resp_ctl = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp_ctl);
    EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)RsEpollCtl, 10, -2);
	ret = RsQpConnectAsync(dev_id, rdevIndex, resp_ctl.qpn, info_ctl.fd);
	mocker_clean();

	mocker((stub_fn_t)RsSocketSend, 10, -1);
	ret = RsQpConnectAsync(dev_id, rdevIndex, resp_ctl.qpn, info_ctl.fd);
	mocker_clean();

	ret = RsQpDestroy(dev_id, rdevIndex, resp_ctl.qpn);

/* ===RsEpollEventInHandle ut begin --- accept fail=== */
	struct epoll_event events;
	struct rs_cb *rs_cb_t;
	struct RsRdevCb *rdevCb;
	struct RsListenInfo *listen_info, *listen_info2;

	ret = RsDev2rscb(0, &rs_cb_t, false);

	RS_LIST_GET_HEAD_ENTRY(listen_info, listen_info2, &rs_cb_t->connCb.listenList, list, struct RsListenInfo);
	for(; (&listen_info->list) != &rs_cb_t->connCb.listenList;
            listen_info = listen_info2, listen_info2 = list_entry(listen_info2->list.next, struct RsListenInfo, list)) {
		events.data.fd = listen_info->listenFd;
	}
	events.events = EPOLLIN;
	mocker((stub_fn_t)accept, 10, -1);
	RsEpollEventInHandle(rs_cb_t, &events);
	RsEpollEventInHandle(rs_cb_t, &events);
	RsEpollEventInHandle(rs_cb_t, &events);
	mocker_clean();

	mocker((stub_fn_t)calloc, 10, NULL);
	struct RsConnCb con_cb_node;
	ret = RsAllocConnNode(&con_cb_node, 16666);
	EXPECT_INT_EQ(ret, -12);

	mocker((stub_fn_t)accept, 1, 9900999);
	mocker((stub_fn_t)RsAllocConnNode, 1, -1);
	RsEpollEventInHandle(rs_cb_t, &events);
	mocker_clean();

	/* poll cq */
	struct RsQpCb *qp_cb_t, *qp_cb_t2;
	ret = RsGetRdevCb(rs_cb, rdevIndex, &rdevCb);
	RS_LIST_GET_HEAD_ENTRY(qp_cb_t, qp_cb_t2, &rdevCb->qpList, list, struct RsQpCb);
	for(; (&qp_cb_t->list) != &rdevCb->qpList;               \
            qp_cb_t = qp_cb_t2, qp_cb_t2 = list_entry(qp_cb_t2->list.next, struct RsQpCb, list)){
		events.data.fd = qp_cb_t->channel->fd;
	}
	RsEpollEventInHandle(rs_cb_t, &events);

	/* qp info message, RsSocketRecv = 0 error ! */
	ret = RsQpn2qpcb(dev_id, rdevIndex, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	if (qp_cb_t->connInfo == NULL) {
		return;
	}
	int fd_tmp = qp_cb_t->connInfo->connfd;
	qp_cb_t->connInfo->connfd = 99999;
	mocker((stub_fn_t)RsSocketRecv, 1, 0);
	events.data.fd = qp_cb_t->connInfo->connfd;
	RsEpollEventInHandle(rs_cb_t, &events);
	qp_cb_t->connInfo->connfd = fd_tmp;
	mocker_clean();

	events.events = EPOLLOUT;
	RsEpollEventHandleOne(rs_cb_t, &events);

	mocker((stub_fn_t)pthread_detach, 1, 1);
	RsEpollHandle(rs_cb_t);
	mocker_clean();

	int epollfd_t = rs_cb_t->connCb.epollfd;
	int eventfd_t = rs_cb_t->connCb.eventfd;
	mocker((stub_fn_t)epoll_create, 1, 1);
	mocker((stub_fn_t)eventfd, 1, -1);

	ret = RsCreateEpoll(rs_cb_t);
	EXPECT_INT_NE(ret, 0);
	rs_cb_t->connCb.epollfd = epollfd_t;
	rs_cb_t->connCb.eventfd = eventfd_t;
	mocker_clean();

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	int try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	usleep(SLEEP_TIME);

        mocker((stub_fn_t)RsEpollCtl, 10, -1);
		listen[0].port = 16666;
        ret = RsSocketListenStop(&listen[0], 1);
        EXPECT_INT_NE(ret, 0);
        mocker_clean();

        struct SocketListenInfo listen_ctl;
        listen_ctl.phyId = 0;
        listen_ctl.localIp.addr.s_addr = inet_addr("127.0.0.9");
		listen_ctl.port = 16666;
        mocker((stub_fn_t)RsEpollCtl, 10, -1);
        ret = RsSocketListenStart(&listen_ctl, 1);
        mocker_clean();

	usleep(SLEEP_TIME);

	struct rs_cb rs_cb_ctl;
	mocker((stub_fn_t)RsEpollCtl, 10, -1);
	ret = RsCreateEpoll(&rs_cb_ctl);
	mocker_clean();

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);
	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_epoll_ops2: RsDeinit\n");

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
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);
	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	int try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	mocker((stub_fn_t)RsFindSockets, 10, 2);
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 2);
	mocker_clean();

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

 	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	mocker_invoke(RsEpollCtl, stub_rs_epoll_ctl, 10);
	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	mocker_clean();

	struct RsQpCb *qp_cb;
	ret = RsQpn2qpcb(dev_id, rdevIndex, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	qp_cb->state = RS_QP_STATUS_DISCONNECT;

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

/* ===RsQpConnectAsync ut begin === */
	struct RsQpCb *qp_cb_t;

	ret = RsQpn2qpcb(dev_id, rdevIndex, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	int state_tmp = qp_cb_t->state;
	qp_cb_t->state = RS_QP_STATUS_CONNECTED;
	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);
	qp_cb_t->state = state_tmp;

/* ===RsQpConnectAsync ut end === */

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
    EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_qp_connect_async2: RsDeinit\n");

	return;
}

void tc_rs_send_wr2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

 	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

/* === rs_send_async ut begin === */

/* === rs_send_async ut end === */

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret, 1);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_send_wr2: RsDeinit\n");

	return;
}

void tc_rs_get_gid_index2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t qpMode = 1;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(1000);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

	/* ===rs_get_gid_index ut begin=== */
	struct ibv_port_attr attr = {0};
	int index;
	struct rs_cb *rs_cb_t;
	struct RsRdevCb rdevCb = {0};
	ret = RsDev2rscb(0, &rs_cb_t, false);
	attr.gid_tbl_len = 3;
	rdevCb.ibPort = 0;

	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);
	EXPECT_INT_NE(ret, 0);

	attr.state = IBV_PORT_ACTIVE;
	mocker((stub_fn_t)ibv_query_gid_type, 20, 1);
	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_gid, 20, 1);
	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_query_gid , 0, 1, 1);
	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);
	mocker_clean();

	mocker((stub_fn_t)ibv_query_gid, 20, 0);
	mocker_ret((stub_fn_t)RsDrvCompareIpGid, 0, 1, 0);
	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);

	mocker((stub_fn_t)ibv_query_gid, 20, 0);
	mocker_ret((stub_fn_t)RsDrvCompareIpGid, 1, 0, 1);
	rdevCb.localIp.family = AF_INET6;
	ret = RsDrvGetGidIndex(&rdevCb, &attr, &index);
	mocker_clean();
	/* ===rs_get_gid_index ut end=== */

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_get_gid_index2:RsDeinit\n");

	return;
}

void tc_rs_mr_abnormal2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	uint32_t qpMode = 1;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	struct RsMrCb *mr_cb_normal;
	ret = RsCallocMr(0, &mr_cb_normal);
	EXPECT_INT_EQ(ret, -EINVAL);

	struct RsQpCb *qp_cb_normal;
	ret = RsCallocQpcb(0, &qp_cb_normal);
	EXPECT_INT_EQ(ret, -EINVAL);
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(10000);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 100;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(300000);
		try_again--;
	} while(ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

	/* ===RsMrInfoSync ut begin=== */
	void *addr;
	struct RsMrCb *mr_cb;
	addr = malloc(RS_TEST_MEM_SIZE);
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	int try_num = 3;
	do {
		ret = RsMrReg(dev_id, rdevIndex, resp.qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", resp.qpn, ret);
		try_num--;
		usleep(3000);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct RsQpCb *qp_cb_t;
	ret = RsQpn2qpcb(dev_id, rdevIndex, resp.qpn, &qp_cb_t);
	EXPECT_INT_EQ(ret, 0);
	ret = RsGetMrcb(qp_cb_t, addr, &mr_cb, &qp_cb_t->mrList);
	if (mr_cb->qpCb->connInfo == NULL) {
		free(addr);
		return;
	}
	int connfd_tmp = mr_cb->qpCb->connInfo->connfd;
	mr_cb->qpCb->connInfo->connfd = RS_FD_INVALID;
	mr_cb->state = 0;
	ret = RsMrInfoSync(mr_cb);
	mr_cb->qpCb->connInfo->connfd = connfd_tmp;

	int state_tmp2 = mr_cb->state;
	mr_cb->state = 0;
	mocker((stub_fn_t)RsSocketSend, 20, 0);
	ret = RsMrInfoSync(mr_cb);
	EXPECT_INT_NE(ret, 0);
	mr_cb->state = state_tmp2;

	mocker_clean();

	/* ===RsQpDestroy ut begin=== */
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);
	/* ===RsQpDestroy ut end=== */

	ret = RsMrDereg(dev_id, rdevIndex, resp.qpn, addr);
	free(addr);
	/* ===RsMrInfoSync ut end=== */

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_mr_abnormal2:RsDeinit\n");

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
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	uint32_t qpMode = 1;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    struct rs_cb *rs_cb;

	ret = RsSocketVnic2nodeid(0);
	EXPECT_INT_EQ(ret, 0);

	struct SocketFdData conn_server;
	conn_server.localIp.addr.s_addr = 1;
	RsSocketsServeripConverter(&conn_server, 1, 1);

	ret = RsGetSockets(0, &conn_server, 1);
	EXPECT_INT_NE(ret, 0);

	struct RsConnInfo conn_tmp;
	conn_tmp.state = 0;
	conn_tmp.serverIp.family = AF_INET;
	conn_tmp.serverIp.binAddr.addr.s_addr = 2;

	conn_tmp.state = 0;
	conn_server.localIp.addr.s_addr = 1;
	conn_tmp.serverIp.family = AF_INET;
	conn_tmp.serverIp.binAddr.addr.s_addr = 1;
	memset(conn_server.tag, 0, sizeof(conn_server.tag));
	memset(conn_tmp.tag, 1, sizeof(conn_tmp.tag));

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 1;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)system, 10, 0);
	mocker((stub_fn_t)access, 10, 0);
    mocker_invoke((stub_fn_t)halGetDeviceInfo, stub_halGetDeviceInfo, 10);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);
    strcpy(white_list.tag, "1234");

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	{
		/* ===RsGetSockets ut begin=== */
		struct RsConnCb *connCb;
		struct RsConnInfo *conn_tmp, *conn_tmp2;
		int state_tmp = 0;
		ret = RsDev2conncb(dev_id, &connCb);
		RS_LIST_GET_HEAD_ENTRY(conn_tmp, conn_tmp2, &connCb->clientConnList, list, struct RsConnInfo);
		for(; (&conn_tmp->list) != &connCb->clientConnList;
            conn_tmp = conn_tmp2, conn_tmp2 = list_entry(conn_tmp2->list.next, struct RsConnInfo, list)) {
			if (conn_tmp->serverIp.binAddr.addr.s_addr == socket_info[i].localIp.addr.s_addr) {
				state_tmp = conn_tmp->state;
				break;
			}
		}
		rs_ut_msg("ori state:%d\n", state_tmp);
		conn_tmp->state = RS_CONN_STATE_INIT;
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();

		/* wrong server ip address */
		socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.4");
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");

		rs_ut_msg("conn_tmp->state:%d\n", conn_tmp->state);
		conn_tmp->state = RS_CONN_STATE_CONNECTED;
		mocker((stub_fn_t)send, 10, SOCK_CONN_TAG_SIZE);
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();

		conn_tmp->state = RS_CONN_STATE_TIMEOUT;
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();

		conn_tmp->state = RS_CONN_STATE_TAG_SYNC;
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		conn_tmp->state = state_tmp;
		mocker_clean();
	}

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("[server]socket_info[1].fd:%d, status:%d\n",
		socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	struct RsConnInfo conn_send_inc;
	mocker((stub_fn_t)send, 10, -1);
	RsSocketTagSync(&conn_send_inc);
	mocker_clean();

	mocker(RsDrvSocketSend, 10, -EAGAIN);
	RsSocketTagSync(&conn_send_inc);
	mocker_clean();

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_ops2: RsDeinit\n");

	return;
}

void tc_rs_socket_close2()
{
	int ret;
	uint32_t dev_id = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	uint32_t qpMode = 1;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct rs_cb *rs_cb;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

    ret = RsDev2rscb(dev_id, &rs_cb, false);
    EXPECT_INT_EQ(ret, 0);
    rs_cb->connCb.wlistEnable = 0;

	usleep(SLEEP_TIME);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	usleep(SLEEP_TIME);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	conn[0].tag[0] = 1;
	conn[0].tag[1] = 2;
	conn[0].tag[2] = 3;
	conn[0].tag[3] = 4;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].tag[0] = 1;
	socket_info[i].tag[1] = 2;
	socket_info[i].tag[2] = 3;
	socket_info[i].tag[3] = 4;

	try_again = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        	usleep(30000);
		try_again--;
	} while (ret != 1 && try_again);
	rs_ut_msg("[server]socket_info[1].fd:%d, status:%d\n",
		socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp2);
	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[i].fd);

	usleep(1000);

	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);
	usleep(SLEEP_TIME);

	ret = RsQpConnectAsync(dev_id, rdevIndex, resp2.qpn, socket_info[1].fd);

	void* addr;
	addr = malloc(RS_TEST_MEM_SIZE);
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(dev_id, rdevIndex, resp2.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	free(addr);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	ret = RsQpDestroy(dev_id, rdevIndex, resp2.qpn);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_socket_close2: RsDeinit\n");

	return;

}
void tc_rs_abnormal2()
{
	int ret;
	uint32_t dev_id = 0;
	uint32_t rdevIndex = 0;
	uint32_t err_dev_id = 10;
	struct RsInitConfig cfg = {0};
	struct RsQpResp resp = {0};
	uint32_t qpMode = 1;
	struct RsQpCb *qp_cb = NULL;
	char buf[64] = {0};
	uint32_t *cmd;
	struct ibv_cq *ib_send_cq_t, *ib_recv_cq_t;
	unsigned int total_size = 2;
	unsigned int cur_size = 1;
	bool flag = true;
	char *buf_tmp = NULL;

	/* resource prepare... */
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	ret = RsQpCreate(dev_id, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpn2qpcb(dev_id, rdevIndex, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)calloc, 10, NULL);
	ret = RsNotifyMrListAdd(qp_cb, buf);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	ret = RsNotifyMrListAdd(qp_cb, buf);
	mocker_clean();

	cmd = (uint32_t *)buf;
	*cmd = RS_CMD_QP_INFO;
	mocker((stub_fn_t)memcpy_s, 10, 1);
	RsEpollRecvHandle(qp_cb, buf, 64);
	mocker_clean();

	mocker((stub_fn_t)RsQpStateModify, 10, 1);
	RsEpollRecvHandle(qp_cb, buf, 64);
	mocker_clean();

	*cmd = RS_CMD_MR_INFO;
	mocker((stub_fn_t)calloc, 10, NULL);
	RsEpollRecvHandle(qp_cb, buf, 64);
	mocker_clean();

	mocker((stub_fn_t)memcpy_s, 10, 1);
	RsEpollRecvHandle(qp_cb, buf, 64);
	mocker_clean();

	/* unknown cmd */
	*cmd = RS_CMD_QP_INFO + 444;
	RsEpollRecvHandle(qp_cb, buf, 64);

	mocker((stub_fn_t)memcpy_s, 10, 1);
	RsEpollRecvHandleRemain(qp_cb, total_size, cur_size, flag, buf_tmp);
	mocker_clean();
	mocker((stub_fn_t)ibv_create_cq, 10, NULL);
	ib_send_cq_t = qp_cb->ibSendCq;
	ib_recv_cq_t = qp_cb->ibRecvCq;
	ret = RsDrvCreateCq(qp_cb, 0);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_create_cq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = RsDrvCreateCq(qp_cb, 0);
	qp_cb->ibSendCq = ib_send_cq_t;
	qp_cb->ibRecvCq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_create_cq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = RsDrvCreateCq(qp_cb, 0);
	qp_cb->ibSendCq = ib_send_cq_t;
	qp_cb->ibRecvCq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker_ret((stub_fn_t)RsIbvExpCreateCq , 1, NULL, 0);
	mocker((stub_fn_t)ibv_destroy_cq, 10, 0);
	ret = RsDrvCreateCq(qp_cb, 1);
	qp_cb->ibSendCq = ib_send_cq_t;
	qp_cb->ibRecvCq = ib_recv_cq_t;
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)RsIbvQueryQp, 10, 1);
	RsQpStateModify(qp_cb);
	mocker_clean();

	mocker((stub_fn_t)ibv_modify_qp, 10, 1);
	RsQpStateModify(qp_cb);
	mocker_clean();

	mocker_ret((stub_fn_t)ibv_modify_qp , 0, 1, 0);
	RsQpStateModify(qp_cb);
	mocker_clean();

	mocker((stub_fn_t)pthread_mutex_lock, 10, 1);
	mocker((stub_fn_t)pthread_mutex_unlock, 10, 1);
	(void)pthread_mutex_lock(NULL);
	(void)pthread_mutex_unlock(NULL);
	mocker_clean();

	RsDrvPollCqHandle(qp_cb);

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	ib_send_cq_t = qp_cb->ibSendCq;
	qp_cb->ibSendCq = NULL;

	/* reach end ? */
	mocker((stub_fn_t)ibv_req_notify_cq, 10, 0);
	mocker((stub_fn_t)ibv_poll_cq, 10, 0);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_req_notify_cq, 10, 1);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, -1);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	qp_cb->ibSendCq = ib_send_cq_t;

	/* resource free... */
	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_rs_abnormal2: RsDeinit\n");
	return;
}

struct RsConnInfo g_conn = {0};

int stub_RsFd2conn(int fd, struct RsConnInfo **conn)
{

    *conn = &g_conn;
    return 0;
}

void tc_rs_socket_nodeid2vnic()
{
    int ret;
    ret = RsSocketNodeid2vnic(0, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_server_valid_async_init()
{
    int ret;
    struct RsConnInfo conn;
    struct SocketWlistInfoT white_list_expect;
    conn.state = 7;
    strcpy(conn.tag, "1234");
    conn.clientIp.family = AF_INET;
    conn.clientIp.binAddr.addr.s_addr = 16;
    ret = RsServerValidAsyncInit(0, &conn, &white_list_expect);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_connect_handle()
{
    int ret;
    struct RsInitConfig cfg;
    struct SocketConnectInfo conn[2] = {0};

    /* resource prepare... */
    cfg.hccpMode = NETWORK_OFFLINE;
    cfg.chipId = 0;
    cfg.whiteListStatus = 1;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    mocker(RsConnectBindClient, 100, -99);
    conn[0].phyId = 0;
    conn[0].family = AF_INET6;
    inet_pton(AF_INET6, "::1", &conn[0].localIp.addr6);
    inet_pton(AF_INET6, "::1", &conn[0].remoteIp.addr6);
    strcpy(conn[0].tag, "5678");
    conn[0].port = 16666;
    ret = RsSocketBatchConnect(&conn[0], 1);
    usleep(SLEEP_TIME);
    mocker_clean();

    /* resource free... */
    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    ret = RsConnectHandle(NULL);
    EXPECT_INT_EQ(ret, NULL);
    return;
}

int replace_rs_qpn2qpcb(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct RsQpCb **qp_cb)
{
	static struct RsQpCb a_qp_cb;
	*qp_cb = &a_qp_cb;
	return 0;
}

void tc_rs_get_qp_context()
{
	void *qp, *send_cq, *recv_cq;
	RsGetQpContext(RS_MAX_DEV_NUM, 0, 0, &qp, &send_cq, &recv_cq);

	mocker(RsQpn2qpcb, 1, -1);
	RsGetQpContext(0, 0, 0, &qp, &send_cq, &recv_cq);
	mocker_clean();

	mocker_invoke(RsQpn2qpcb, replace_rs_qpn2qpcb, 1);
	RsGetQpContext(0, 0, 0, &qp, &send_cq, &recv_cq);
	mocker_clean();
}

void tc_tls_abnormal1()
{
    int ret;
    uint32_t dev_id = 0;
    int flag = 0; /* RC */
    uint32_t qpn, qpn2;
    int i;
    struct RsInitConfig cfg = {0};
    struct SocketListenInfo listen[2] = {0};
    struct SocketConnectInfo conn[2] = {0};
    struct RsSocketCloseInfoT sock_close[2] = {0};
    struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

    cfg.chipId = 0;
    cfg.hccpMode = NETWORK_OFFLINE;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    gRsCb->sslEnable = 1;
    mocker_invoke((stub_fn_t)RsFd2conn, stub_RsFd2conn, 10);
    mocker((stub_fn_t)SSL_write, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, 2);
    RsDrvSocketSend(socket_info[0].fd, "1", 1, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)RsFd2conn, stub_RsFd2conn, 10);
    mocker((stub_fn_t)SSL_read, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, 2);
    RsDrvSocketRecv(socket_info[1].fd, "1", 1, 0);
    mocker_clean();

    mocker((stub_fn_t)RsFd2conn, 10, -1);
    RsDrvSocketRecv(socket_info[1].fd, "1", 1, 0);
    mocker_clean();
    gRsCb->sslEnable = 0;
    RsDrvSocketRecv(socket_info[1].fd, "1", 1, 0);

    mocker((stub_fn_t)fcntl, 10, -1);
    ret = RsSetFdNonblock(-1);
    EXPECT_INT_EQ(ret, -EFILEOPER);
    mocker_clean();

    mocker_ret((stub_fn_t)fcntl, 1, -1, 0);
    ret = RsSetFdNonblock(-1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    struct RsConnInfo conn_ssl;
    conn_ssl.ssl = NULL;
    conn_ssl.clientIp.family = AF_INET;
    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    ret = RsSocketSslConnect(&conn_ssl, gRsCb);
    EXPECT_INT_EQ(ret, -EAGAIN);
    mocker_clean();

    mocker((stub_fn_t)SSL_get_verify_result, 10, -1);
    ret = RsSocketSslConnect(&conn_ssl, gRsCb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)fcntl, 10, -1);
    ret = RsSocketStateReset(0, &conn_ssl, 1, gRsCb);
    EXPECT_INT_EQ(ret, -ESYSFUNC);
    mocker_clean();

    mocker((stub_fn_t)SSL_set_fd, 10, -1);
    ret = RsSocketStateConnected(&conn_ssl, 1, gRsCb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    ret = RsSocketStateSslFdBind(&conn_ssl, 1, gRsCb);
    EXPECT_INT_EQ(ret, -EAGAIN);
    mocker_clean();

    conn_ssl.state = RS_CONN_STATE_SSL_BIND_FD;
    ret = RsSocketConnectAsync(&conn_ssl, gRsCb);
    EXPECT_INT_EQ(ret, 0);

    struct RsAcceptInfo ssl_info;
    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    RsDoSslHandshake(&ssl_info, gRsCb);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, SSL_ERROR_WANT_WRITE);
    RsDoSslHandshake(&ssl_info, gRsCb);
    mocker_clean();

    mocker((stub_fn_t)SSL_do_handshake, 10, -1);
    mocker((stub_fn_t)SSL_get_error, 10, SSL_ERROR_WANT_READ);
    RsDoSslHandshake(&ssl_info, gRsCb);
    mocker_clean();

    mocker((stub_fn_t)calloc, 10, 0);
    ret = rs_get_pk(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker((stub_fn_t)SSL_new, 10, 0);
    ret = RsDrvSslBindFd(&conn_ssl, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    ret = RsDrvSocketSend(-1, "abc", 3, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RsDrvSocketRecv(-1, "abc", 3, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    mocker((stub_fn_t)rs_ssl_get_crl_data, 10, 0);
    mocker((stub_fn_t)SSL_CTX_get_cert_store, 10, 0);
    ret = rs_ssl_crl_init(NULL, NULL, NULL);
    EXPECT_INT_EQ(ret, -EFAULT);
    mocker_clean();

    struct tls_cert_mng_info mng_info;
    struct rs_cb err_rscb;
	struct RsCerts certs;
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM];
    err_rscb.chipId = 0;
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
        struct RsCerts *certs, STACK_OF(X509) *cert_chain, struct tls_cert_mng_info *mng_info);
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
    err_rscb.skidSubjectCb = NULL;
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

    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);
    rs_ut_msg("!!!!!!tc_tls_abnormal1: RsDeinit\n");
}

int stub_dl_hal_get_chip_info_910_93(unsigned int dev_id, halChipInfo *chip_info)
{
    strcpy(chip_info->name, "910_93xx");

    return 0;
}

void tc_rs_socket_get_bind_by_chip()
{
	unsigned int chipId = 0;
	bool bind_ip = false;

	mocker((stub_fn_t)DlDrvDeviceGetIndexByPhyId, 1, -1);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker((stub_fn_t)DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker((stub_fn_t)DlHalGetDeviceInfo, 1, -2);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker((stub_fn_t)DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker((stub_fn_t)DlHalGetDeviceInfo, 1, 0);
	mocker((stub_fn_t)DlHalGetChipInfo, 1, -2);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker(DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker(DlHalGetDeviceInfo, 1, 0);
	mocker_invoke(DlHalGetChipInfo, stub_dl_hal_get_chip_info_910_93, 100);
	RsSocketGetBindByChip(chipId, &bind_ip);
	EXPECT_INT_EQ(bind_ip, true);
	mocker_clean();
}

int stub_rs_get_conn_info(struct RsConnCb *connCb, struct SocketConnectInfo *conn,
    struct RsConnInfo **conn_info, int server_port)
{
    (*conn_info) = g_conn_info;

    return 0;
}

void tc_rs_socket_batch_abort()
{
    struct SocketConnectInfo conn[1] = { 0 };
    gRsCb = malloc(sizeof(struct rs_cb));
    g_conn_info = malloc(sizeof(struct RsConnInfo));
    int ret = 0;

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(RsGetConnInfo, 1, -1);
    ret = RsSocketBatchAbort(conn, 1);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker_invoke(RsGetConnInfo, stub_rs_get_conn_info, 1);
    mocker(setsockopt, 1, -1);
    mocker(RsSocketCloseFd, 1, -1);

    g_conn_info->state = 2;
    g_conn_info->list.prev = &g_conn_info->list;
    g_conn_info->list.next = &g_conn_info->list;
    ret = RsSocketBatchAbort(conn, 1);
    EXPECT_INT_EQ(ret, -1);

    free(gRsCb);
    gRsCb = NULL;
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

    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->sslEnable = 0;

    mocker_clean();
    mocker(send, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsDrvSocketSend(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(send, 1, -1);
    ret = RsDrvSocketSend(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(recv, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsDrvSocketRecv(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(recv, 1, -1);
    ret = RsDrvSocketRecv(1, "1", 1, 0);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(send, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsPeerSocketSend(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(send, 1, -1);
    ret = RsPeerSocketSend(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    mocker_clean();
    mocker(recv, 1, -1);
    mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsPeerSocketRecv(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EAGAIN);

    mocker_clean();
    mocker(recv, 1, -1);
    ret = RsPeerSocketRecv(0, 1, "1", 1);
    EXPECT_INT_EQ(ret, -EFILEOPER);

    free(gRsCb);
    gRsCb = NULL;
}

void stub_hccp_time_max_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    *msec = 90001.0;
}

void stub_HccpTimeInterval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    *msec = 5001.0;
}

void tc_rs_tcp_recv_tag_in_handle()
{
    struct RsListenInfo listen_info = {0};
    struct RsConnInfo conn_tmp = {0};
    struct RsIpAddrInfo remote_ip = {0};
    struct rs_cb *rs_cb = NULL;
    struct RsAcceptInfo accept_info = {0};
    int ret = 0;

    mocker_clean();
    mocker(recv, 1, 0);
    ret = RsTcpRecvTagInHandle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, -ESOCKCLOSED);

    mocker_clean();
    mocker(recv, 1, 1);
    mocker_invoke(HccpTimeInterval, stub_hccp_time_max_interval, 1);
    ret = RsTcpRecvTagInHandle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, -ETIME);

    mocker_clean();
    mocker(recv, 1, 256);
    mocker_invoke(HccpTimeInterval, stub_HccpTimeInterval, 1);
    ret = RsTcpRecvTagInHandle(&listen_info, 0, &conn_tmp, &remote_ip);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RsTcpRecvTagInHandle, 1, 1);
    mocker(close, 1, 1);
    RsEpollEventTcpListenInHandle(rs_cb, &listen_info, 1, &remote_ip);

    mocker_clean();
    mocker(RsTcpRecvTagInHandle, 1, 0);
    mocker(RsWlistCheckConnAdd, 1, 1);
    RsEpollEventTcpListenInHandle(rs_cb, &listen_info, 1, &remote_ip);
    mocker_clean();

    mocker((stub_fn_t)SSL_read, 10, -1);
    mocker_invoke(HccpTimeInterval, stub_HccpTimeInterval, 1);
    RsSslRecvTagInHandle(&accept_info, &conn_tmp);
    mocker_clean();
}

void tc_rs_server_valid_async_abnormal()
{
    struct RsConnInfo conn = {0};
    struct RsConnCb connCb = {0};

    mocker_clean();
    mocker(RsFindWhiteList, 1, 0);
    mocker(RsServerValidAsyncInit, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    mocker(RsFindWhiteListNode, 1, -1);
    mocker(RsServerSendWlistCheckResult, 1, 0);
    RsServerValidAsync(0, &connCb, &conn);
    mocker_clean();
}

void tc_rs_server_valid_async_abnormal_01()
{
    struct RsConnInfo conn = {0};
    struct RsConnCb connCb = {0};

    mocker_clean();
    mocker(RsServerValidAsyncInit, 1, 0);
    mocker(RsFindWhiteList, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
	mocker_invoke((stub_fn_t)RsFindWhiteListNode, stub_rs_find_white_list_node, 1);
    mocker(RsServerSendWlistCheckResult, 1, -1);
    RsServerValidAsync(0, &connCb, &conn);
    mocker_clean();
}

void tc_rs_net_api_init_fail()
{
    int ret = 0;

	mocker(rs_net_adapt_api_init, 1, -1);
    ret = rs_net_api_init();
    EXPECT_INT_EQ(-1, ret);
	mocker_clean();
}
