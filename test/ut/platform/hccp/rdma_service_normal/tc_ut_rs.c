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
#define SOCK_CONN_TAG_SIZE 192
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
#include <ifaddrs.h>
#include <fcntl.h>
#include <stdint.h>
#include "ascend_hal.h"
#include "dl_hal_function.h"
#include "dl_ibverbs_function.h"
#include "rs_socket.h"
#include "rs_tls.h"
#include "ut_dispatch.h"
#include "stub/ibverbs.h"
#include "rs.h"
#include "rs_inner.h"
#include "rs_ping_inner.h"
#include "rs_ping.h"
#include "ra_rs_err.h"
#include "rs_drv_rdma.h"
#include "rs_ub.h"
#include "stub/verbs_exp.h"
#include "tls.h"
#include "encrypt.h"
#include "rs_epoll.h"
#include "tc_ut_rs.h"

extern void RsGetCurTime(struct timeval *time);
extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int RsPthreadMutexInit(struct rs_cb *rscb, struct RsInitConfig *cfg);
extern void RsSslRecvTagInHandle(struct RsAcceptInfo *accept_info, struct RsConnInfo *conn_tmp);
extern void RsEpollEventSslRecvTagInHandle(struct rs_cb *rs_cb, struct RsAcceptInfo *accept_info);
extern int RsRdev2rdevCb(unsigned int chipId, unsigned int rdevIndex, struct RsRdevCb **rdevCb);
extern int RsCompareIpGid(struct rdev rdev_info, union ibv_gid *gid);
extern int RsGetIbCtxAndRdevIndex(struct rdev rdev_info, struct RsRdevCb *rdevCb, unsigned int *rdevIndex);
extern int RsQueryGid(struct rdev rdev_info, struct ibv_context *ib_ctx_tmp, uint8_t ib_port, int *gid_idx);
extern int RsRdevCbInit(struct rdev rdev_info, struct RsRdevCb *rdevCb, struct rs_cb *rs_cb, unsigned int *rdevIndex);
extern int RsDrvQueryNotifyAndAllocPd(struct RsRdevCb *rdevCb);
extern int RsDrvRegNotifyMr(struct RsRdevCb *rdevCb);
extern int RsGetRsCb(unsigned int phyId, struct rs_cb **rs_cb);
extern int RsDrvMrDereg(struct ibv_mr *ibMr);
extern void RsSslFree(struct rs_cb *rscb);
extern int RsDrvPostRecv(struct RsQpCb *qp_cb, struct RecvWrlistData *wr, unsigned int recv_num,
    unsigned int *complete_num);
extern int RsDrvSslBindFd(struct RsConnInfo *conn, int fd);
extern void rs_ssl_err_string(int fd, int err);
extern void rs_ssl_deinit(struct rs_cb *rscb);
extern int rs_ssl_init(struct rs_cb *rscb);
extern int rs_tls_inner_enable(struct rs_cb *rs_cb, unsigned int enable);
extern int rs_ssl_inner_init(struct rs_cb *rscb);
extern int rs_ssl_ca_ky_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb);
extern int rs_ssl_load_ca(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info* mng_info);
extern int rs_ssl_crl_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
extern int rs_ssl_get_crl_data(struct rs_cb *rscb, FILE* fp, struct tls_cert_mng_info *mng_info, X509_CRL **crl);
extern int rs_check_pridata(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
extern int rs_get_pk(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info, EVP_PKEY **pky);
extern int rs_ssl_get_ca_data(struct rs_cb *rscb, const char* end_file, const char* ca_file,
    struct tls_cert_mng_info* mng_info);
extern int rs_remove_certs(const char* end_file, const char* ca_file);
extern int rs_ssl_put_certs(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info, struct RsCerts *certs,
    struct tls_ca_new_certs *new_certs, struct CertFile *file_name);
extern int rs_ssl_skid_get_from_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct tls_ca_new_certs *new_certs);
extern int rs_socket_fill_wlist_by_phyID(unsigned int chipId, struct SocketWlistInfoT *white_list_node,
    struct RsConnInfo *rs_conn);
extern int rs_ssl_check_mng_and_cert_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct tls_ca_new_certs *new_certs, struct CertFile *file_name);
extern int rs_ssl_check_cert_chain(struct tls_cert_mng_info *mng_info, struct RsCerts *certs,
    struct tls_ca_new_certs *new_certs);
extern int rs_ssl_verify_cert(X509_STORE_CTX *ctx);
extern int rs_ssl_verify_cert_chain(X509_STORE_CTX *ctx, X509_STORE *store,
    struct RsCerts *certs, struct tls_cert_mng_info *mng_info, struct tls_ca_new_certs *new_certs);
extern X509 *tls_load_cert(const uint8_t *inbuf, uint32_t buf_len, int type);
extern int rs_ssl_get_leaf_cert(struct RsCerts *certs, X509 **leaf_cert);
extern int tls_get_cert_chain(X509_STORE *store, struct RsCerts *certs, struct tls_cert_mng_info *mng_info);
extern int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb);
extern int RsEpollEventSslAcceptInHandle(struct rs_cb *rs_cb, int fd);
extern int NetCommGetSelfHome(char *userNamePath, unsigned int pathLen);
extern int get_tls_config_path(char *user_name_path, unsigned int path_len);
extern int RsDrvQpNormal(struct RsQpCb *qp_cb, int qpMode);
extern int RsDrvNormalQpCreateInit(struct ibv_qp_init_attr *qp_init_attr, struct RsQpCb *qp_cb, struct ibv_port_attr *attr);
extern int RsSendExpWrlist(struct RsQpCb *qp_cb, struct WrInfo *wr_list, unsigned int send_num, struct SendWrRsp *wr_rsp, unsigned int *complete_num);
extern int RsGetMrcb(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb, struct RsListHead *mr_list);
extern void RsWirteAndReadBuildUpWr(struct RsMrCb *mr_cb, struct RsMrCb *rem_mr_cb, struct WrInfo *wr, struct ibv_sge *list, struct ibv_send_wr *ib_wr);
extern struct ibv_mr* RsDrvMrReg(struct ibv_pd *pd, char *addr, size_t length, int access);
extern int RsQueryEvent(int cq_event_id, struct event_summary **event);
extern int tls_get_user_config(unsigned int save_mode, unsigned int chipId, const char *name,
    unsigned char *buf, unsigned int *buf_size);
extern void tls_get_enable_info(unsigned int save_mode, unsigned int chipId, unsigned char *buf,
    unsigned int buf_size);
extern int RsGetIpv6ScopeId(struct in6_addr localIp);
extern int verify_callback(int prev_ok, X509_STORE_CTX *ctx);
extern int RsOpenBackupIbCtx(struct RsRdevCb *rdevCb);
extern int RsGetSqDepthAndQpMaxNum(struct RsRdevCb *rdevCb, unsigned int rdevIndex);
extern int RsSetupPdAndNotify(struct RsRdevCb *rdevCb);
extern int RsSocketConnectCheckPara(struct SocketConnectInfo *conn_info);
extern int RsSocketNodeid2vnic(uint32_t node_id, uint32_t *ip_addr);
extern int rsGetLocalDevIDByHostDevID(unsigned int phyId, unsigned int *chipId);
extern int RsDev2conncb(uint32_t chipId, struct RsConnCb **connCb);
extern int RsGetConnInfo(struct RsConnCb *connCb, struct SocketConnectInfo *conn,
    struct RsConnInfo **connInfo, unsigned int serverPort);
extern int RsConvertIpAddr(int family, union HccpIpAddr *ip_addr, struct RsIpAddrInfo *ip);
extern int RsFindListenNode(struct RsConnCb *connCb, struct RsIpAddrInfo *ip_addr, uint32_t server_port,
    struct RsListenInfo **listen_info);
extern int RsSocketListenAddToEpoll(struct RsConnCb *connCb, struct RsListenInfo *listen_info);
extern bool RsSocketIsVnicIp(unsigned int chipId, unsigned int ip_addr);
extern int kmc_dec_data(struct kmc_enc_info *enc_info, unsigned char *outbuf, unsigned int *size_out);

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned long long u64;
typedef signed int s32;

const char *s_tmp = "suc";
struct RsQpCb *qp_cb_ab2;
struct RsRdevCb g_rdev_cb = {0};
struct RsListenInfo g_listen_info = {0};
struct RsListenInfo *g_plisten_info = &g_listen_info;

#define SLEEP_TIME 50000

int rs_conn_init(int index);
void rs_conn_prepare(void *arg);
int rs_conn_get_sockets(int index);
int rs_conn_info_update(int index);
int rs_conn_qp_info_update(int index, int i);
int rs_conn_mr_info_update(int index, int i);
uint64_t str2long(const char *str);
int rs_conn_send_imm(uint32_t qpn, void *src_addr, int len, const char *dstAddr, int imm_data);

int rs_conn_close_check_timeout(int real_dev_id);
int dev_read_flash(unsigned int chipId, const char* name, unsigned char* buf, unsigned int *buf_size);

int RsDev2rscb(uint32_t chipId, struct rs_cb **rs_cb, bool init_flag);
int memset_s(void * dest, size_t destMax, int c, size_t count);
void RsFreeAcceptOneNode(struct rs_cb *rscb, struct RsAcceptInfo *accept);
int RsEpollEventListenInHandle(struct rs_cb *rs_cb, int fd);
int RsEpollEventQpMrInHandle(struct rs_cb *rs_cb, int fd);
int RsEpollEventHeterogTcpRecvInHandle(struct rs_cb *rs_cb, int fd);
void RsFreeHeterogTcpFdList(struct rs_cb *rs_cb);
extern __thread struct rs_cb *gRsCb;
extern struct RsCqeErrInfo gRsCqeErr;
extern void RsDrvSaveCqeErrInfo(uint32_t status, struct RsQpCb *qp_cb);
extern int RsDrvNormalQpCreate(struct RsQpCb *qp_cb, struct ibv_qp_init_attr *qp_init_attr);
extern int DlHalGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);

extern int rs_ssl_get_cert(struct rs_cb *rscb, struct RsCerts *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs);
extern int rs_ssl_x509_store_init(X509_STORE *store, struct RsCerts *certs,
    struct tls_cert_mng_info *mng_info, struct tls_ca_new_certs *new_certs);
extern int rs_ssl_skids_subjects_get(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct tls_ca_new_certs *new_certs);
extern int rs_ssl_put_cert_ca_pem(struct RsCerts *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs, const char *ca_file);
extern int rs_ssl_put_cert_end_pem(struct RsCerts *certs, struct tls_ca_new_certs *new_certs, const char *end_file);
extern int rs_ssl_put_end_cert(struct RsCerts *certs, const char *end_file);
extern int rs_ssl_X509_store_add_cert(char *cert_info, X509_STORE *store);
extern int RsGetLinuxVersion(struct RsLinuxVersionInfo *ver_info);
extern void freeifaddrs(struct ifaddrs *ifa);
extern int DlHalSensorNodeUpdateState(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion);

void tc_rs_abnormal()
{
	int ret;
	struct rs_cb *rs_cb;
	uint32_t qpn;
	struct RsQpCb *qp_cb;
	struct RsQpCb qp_cb_tmp;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	rs_ut_msg("\n+++++++++ABNORMAL TC Start++++++++\n");
	ret = RsDev2rscb(0, &rs_cb, false);
	EXPECT_INT_NE(ret, 0);

	ret = RsDev2rscb(2, &rs_cb, false);
	EXPECT_INT_NE(ret, 0);

	ret = RsQpn2qpcb(phyId, rdevIndex, 5, &qp_cb);

	rs_ut_msg("---------ABNORMAL TC End----------\n\n");

	return;
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

extern int sprintf_s(char *strDest, size_t destMax, const char *format, ...);
void tc_rs_init()
{
	int ret;
	struct RsInitConfig cfg = {0};

	rs_ut_msg("\n%s+++++++++ABNORMAL TC Start++++++++\n", __func__);
	ret = RsInit(NULL);
	EXPECT_INT_NE(ret, 0);

	rs_ut_msg("\n%s---------ABNORMAL TC End----------\n", __func__);

	cfg.chipId = 0;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	RsSetHostPid(cfg.chipId, 0, NULL);
	EXPECT_INT_EQ(ret, 0);

	RsSetHostPid(15, 0, NULL);
	EXPECT_INT_EQ(ret, 0);

	cfg.chipId = 0;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, -17);

	/* ------Resource CLEAN-------- */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_deinit()
{
	int ret;
	uint32_t chipId = 0;
	struct rs_cb *rs_cb;
	int eventfd_tmp;
	struct RsInitConfig cfg = {0};

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	RsDev2rscb(chipId, &rs_cb, false);

	rs_ut_msg("\n%s+++++++++ABNORMAL TC Start++++++++\n", __func__);
	/* param error */
	ret = RsDeinit(NULL);
	EXPECT_INT_NE(ret, 0);

	/* env store */
	eventfd_tmp = rs_cb->connCb.eventfd;
	rs_cb->connCb.eventfd = -1;
	ret = RsDeinit(&cfg);
	EXPECT_INT_NE(ret, 0);
	/* env recovery */
	rs_cb->connCb.eventfd = eventfd_tmp;

	rs_ut_msg("\n%s---------ABNORMAL TC End----------\n", __func__);

	/* ------Resource CLEAN-------- */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

/* FREE server/client conn_info, listen_info AUTOMATICALLY */
void tc_rs_deinit2()
{
	int ret;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
    struct SocketWlistInfoT white_list;
   	white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);
    strcpy(white_list.tag, "5678");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET;
	conn[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 2);

	usleep(SLEEP_TIME);

	/* ------Resource CLEAN-------- */
	struct rs_cb *rs_cb = NULL;
	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	rs_cb->sslEnable = 1;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;

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

void tc_rs_socket_deinit1()
{
	int ret;
	int i;
	struct RsInitConfig cfg = {0};
	struct rs_cb *rs_cb = NULL;
	struct rdev rdev_info = {0};
	struct RsAcceptInfo *accept = calloc(1, sizeof(struct RsAcceptInfo));
	struct RsListHead list = {0};

	RS_INIT_LIST_HEAD(&list);
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	rs_cb->sslEnable = 1;

	accept->list = list;
	accept->ssl = NULL;

	RsFreeAcceptOneNode(rs_cb, accept);
}

void tc_rs_socket_deinit2()
{
	int ret;
	int i;
	struct RsInitConfig cfg = {0};
	struct rs_cb *rs_cb = NULL;
	struct rdev rdev_info = {0};
	struct RsAcceptInfo *accept = calloc(1, sizeof(struct RsAcceptInfo));
	struct RsListHead list = {0};

	RS_INIT_LIST_HEAD(&list);
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	rs_cb->sslEnable = 1;
	accept->ssl = malloc(sizeof(SSL_CTX));
	accept->list = list;

	RsFreeAcceptOneNode(rs_cb, accept);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

void tc_rs_socket_listen_ipv6()
{
	int ret;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET6;
	inet_pton(AF_INET6, "::1", &listen[0].localIp.addr6);
	listen[0].port = 16666;
	ret = RsSocketSetScopeId(0, if_nametoindex("lo"));
	mocker(RsGetIpv6ScopeId, 10, if_nametoindex("lo"));
	EXPECT_INT_EQ(ret, 0);
	ret = RsSocketListenStart(&listen[0], 1);

	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	/* ------Resource CLEAN-------- */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_socket_listen()
{
	int ret;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2];

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	/* listen 1 will fail, cannot listen same IP twice */
	listen[1].phyId = 0;
	listen[1].family = AF_INET;
	listen[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[1].port = 16666;
	ret = RsSocketListenStart(&listen[1], 1);

	listen[1].port = 16666;
	ret = RsSocketListenStop(&listen[1], 1);

	/* stop a non-exist node */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);
	EXPECT_INT_EQ(ret, 0);

	/* ------Resource CLEAN-------- */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_socket_connect()
{
	int ret;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
	white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num = 10;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);
    strcpy(white_list.tag, "5678");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET;
	conn[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(conn, 2);

    strcpy(white_list.tag, "1234");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);
	/* >>>>>>> RsSocketBatchConnect test case begin <<<<<<<<<<< */
	/* repeat connect */
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	/* param error - conn NULL */
	ret = RsSocketBatchConnect(NULL, 1);

	/* param error - num error */
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 0);

	/* param error - device id error */
	conn[0].phyId = 15;
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);
	/* >>>>>>> RsSocketBatchConnect test case end <<<<<<<<<<< */

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");

	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");

	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_sockets()
{
	int ret;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
   	white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num = 10;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);
    strcpy(white_list.tag, "5678");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET;
	conn[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 2);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
		rs_ut_msg(">>**RsGetSockets ret:%d\n", ret);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* param error */
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, NULL, 3);
	EXPECT_INT_NE(ret, 0);

	/* device id error */
	socket_info[i].phyId = 55555;
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_NE(ret, 0);
	socket_info[i].phyId = 0;

	/* repeat get */
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_EQ(ret, 0);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.3");
	socket_info[i].status = RS_SOCK_STATUS_OK;
	struct RsConnInfo conn_tmp;
	conn_tmp.state = RS_CONN_STATE_VALID_SYNC;
	RsFindSockets(&conn_tmp, &socket_info[i], 1, RS_CONN_ROLE_CLIENT);
	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_set_tsqp_depth()
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

	ret = RsSetTsqpDepth(phyId, rdevIndex, temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_tsqp_depth()
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

	ret = RsGetTsqpDepth(phyId, rdevIndex, &temp_depth, &qp_num);
	EXPECT_INT_EQ(ret, 0);
	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

int stub_ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct ibv_port_attr *port_attr)
{
	port_attr->gid_tbl_len = 2;
	return 0;
}

extern int ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct ibv_port_attr *port_attr);
void tc_rs_qp_create()
{
	int ret;
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
 	struct RsQpNorm qp_norm = {0};

	qp_norm.flag = flag;
	qp_norm.qpMode = 1;
	qp_norm.isExp = 1;

    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 1;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)drvGetDevNum, 10, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	mocker_invoke((stub_fn_t)ibv_query_port, stub_ibv_query_port, 10);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, -ENOLINK);
    mocker_clean();

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	enum PortStatus status = PORT_STATUS_DOWN;
	ret = RsRdevGetPortStatus(100000, rdevIndex, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = RsRdevGetPortStatus(rdev_info.phyId, rdevIndex, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = RsRdevGetPortStatus(15, rdevIndex, &status);
	EXPECT_INT_NE(ret, 0);
	ret = RsRdevGetPortStatus(rdev_info.phyId, 100000, &status);
	EXPECT_INT_NE(ret, 0);
	ret = RsRdevGetPortStatus(rdev_info.phyId, rdevIndex, &status);
	EXPECT_INT_EQ(ret, 0);
	mocker(ibv_query_port, 20, -1);
	ret = RsRdevGetPortStatus(rdev_info.phyId, rdevIndex, &status);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	int support_lite = 0;
	ret = RsGetLiteSupport(rdev_info.phyId, rdevIndex, &support_lite);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(support_lite, 1);

	struct LiteRdevCapResp rdev_resp;
	ret = RsGetLiteRdevCap(rdev_info.phyId, rdevIndex, &rdev_resp);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	struct LiteQpCqAttrResp qp_resp;
	ret = RsGetLiteQpCqAttr(phyId, rdevIndex, resp.qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct LiteMemAttrResp mem_resp;
	ret = RsGetLiteMemAttr(rdev_info.phyId, rdevIndex, resp.qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	struct QosAttr QosAttr = {0};
	QosAttr.tc = 100;
	QosAttr.sl = 3;
	ret = RsSetQpAttrQos(phyId, rdevIndex, resp.qpn, &QosAttr);

	unsigned int timeout = 15;
    unsigned int retry_cnt = 6;
	ret = RsSetQpAttrTimeout(phyId, rdevIndex, resp.qpn, &timeout);

	ret = RsSetQpAttrRetryCnt(phyId, rdevIndex, resp.qpn, &retry_cnt);

	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = RsQpConnectAsync(phyId, rdevIndex, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, -1);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> RsQpConnectAsync test case end <<<<<<<<<<< */

	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	struct RsQpCb *qp_cb;
	ret = RsQpn2qpcb(phyId, rdevIndex, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	struct LiteConnectedInfoResp connected_resp;
	qp_cb->state = RS_QP_STATUS_CONNECTED;
	ret = RsGetLiteConnectedInfo(phyId, rdevIndex, resp.qpn, &connected_resp);
	EXPECT_INT_EQ(ret, 0);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> RsQpCreate test case begin <<<<<<<<<<< */
	/* param error - device id */
	ret = RsQpCreate(15, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_NE(ret, 0);

	/* qp number out of boundry */
	struct rs_cb *rs_cb;
	struct RsRdevCb *rdevCb;
	struct RsAcceptInfo accept_tmp = {0};
	struct RsAcceptInfo *accept = &accept_tmp;
	uint32_t chipId = 0;
	ret = RsDev2rscb(chipId, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);

    ret = RsGetRdevCb(rs_cb, rdevIndex, &rdevCb);
	EXPECT_INT_EQ(ret, 0);

	int qp_cnt_tmp = rdevCb->qpCnt;
	rdevCb->qpCnt = 44444;
	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_NE(ret, 0);
	rdevCb->qpCnt = qp_cnt_tmp;
	/* >>>>>>> RsQpCreate test case end <<<<<<<<<<< */

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, resp2.qpn, socket_info[i].fd);
	usleep(SLEEP_TIME);

	ret = RsQpDestroy(phyId, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(phyId, rdevIndex, resp.qpn);

	/* param error - qpn */
	ret = RsQpDestroy(phyId, rdevIndex, resp.qpn);
	EXPECT_INT_NE(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);
	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

        /* >>>>>>> 1910 create_qp test case begin <<<<<<<<<<< */
        struct RsInitConfig cfg_1910 = {0};
        cfg_1910.chipId = 0;
        cfg_1910.hccpMode = NETWORK_ONLINE;
        ret = RsInit(&cfg_1910);
        EXPECT_INT_EQ(ret, 0);

		rdev_info.phyId = cfg_1910.chipId;
		rdev_info.family = AF_INET;
		rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
		ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
		EXPECT_INT_EQ(ret, 0);
		struct RsQpResp resp1910 = {0};
		ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp1910);
        EXPECT_INT_EQ(ret, 0);

		ret = RsQpDestroy(phyId, rdevIndex, resp1910.qpn);

		ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
        EXPECT_INT_EQ(ret, 0);
		ret = RsSocketDeinit(rdev_info);
		EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInitWithBackup(rdev_info, rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

        ret = RsDeinit(&cfg_1910);
		EXPECT_INT_EQ(ret, 0);
        /* >>>>>>> 1910 create_qp test case end <<<<<<<<<<< */

	return;
}

#define RS_DRV_CQ_DEPTH         16384
#define RS_DRV_CQ_128_DEPTH     128
#define RS_DRV_CQ_8K_DEPTH      8192
#define RS_QP_ATTR_MAX_INLINE_DATA 32
#define RS_QP_ATTR_MAX_SEND_SGE 8
#define RS_QP_TX_DEPTH_PEER_ONLINE          4096
#define RS_QP_TX_DEPTH_ONLINE 4096
#define RS_QP_TX_DEPTH                      8191
#define RS_QP_TX_DEPTH_OFFLINE 128
void QpExtAttrs(int qpMode, struct QpExtAttrs *extAttrs)
{
    extAttrs->qpMode = qpMode;
    extAttrs->cqAttr.sendCqDepth = RS_DRV_CQ_8K_DEPTH;
    extAttrs->cqAttr.sendCqCompVector = 0;
    extAttrs->cqAttr.recvCqDepth = RS_DRV_CQ_128_DEPTH;
    extAttrs->cqAttr.recvCqCompVector = 0;
    extAttrs->qpAttr.qp_context = NULL;
    extAttrs->qpAttr.send_cq = NULL;
    extAttrs->qpAttr.recv_cq = NULL;
    extAttrs->qpAttr.srq = NULL;
    extAttrs->qpAttr.cap.max_send_wr = RS_QP_TX_DEPTH_OFFLINE;
    extAttrs->qpAttr.cap.max_recv_wr = RS_QP_TX_DEPTH_OFFLINE;
    extAttrs->qpAttr.cap.max_send_sge = 1;
    extAttrs->qpAttr.cap.max_recv_sge = 1;
    extAttrs->qpAttr.cap.max_inline_data = RS_QP_ATTR_MAX_INLINE_DATA;
    extAttrs->qpAttr.qp_type = IBV_QPT_RC;
    extAttrs->qpAttr.sq_sig_all = 0;
    extAttrs->version = QP_CREATE_WITH_ATTR_VERSION;
}

void tc_rs_qp_create_with_attrs_v1()
{
	int ret;
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	uint32_t qpn, qpn1, qpn2;
	int i;
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
 	struct RsQpNormWithAttrs  qp_norm = {0};
	struct RsQpRespWithAttrs qp_resp_create = {0};

	qp_norm.isExp = 1;
	qp_norm.isExt = 1;
	QpExtAttrs(1, &qp_norm.extAttrs);

    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 1;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)drvGetDevNum, 10, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	mocker_invoke((stub_fn_t)ibv_query_port, stub_ibv_query_port, 10);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, -ENOLINK);
    mocker_clean();

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	int support_lite = 0;
	ret = RsGetLiteSupport(rdev_info.phyId, rdevIndex, &support_lite);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(support_lite, 1);

	struct LiteRdevCapResp rdev_resp;
	ret = RsGetLiteRdevCap(rdev_info.phyId, rdevIndex, &rdev_resp);
	EXPECT_INT_EQ(ret, 0);

    qp_norm.extAttrs.dataPlaneFlag.bs.cqCstm = 1;
    qp_norm.aiOpSupport = 1;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreateWithAttrs: qpn %d, ret:%d\n", qpn, ret);

	struct LiteQpCqAttrResp qp_resp;
	ret = RsGetLiteQpCqAttr(phyId, rdevIndex, qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct LiteMemAttrResp mem_resp;
	ret = RsGetLiteMemAttr(rdev_info.phyId, rdevIndex, qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	struct QosAttr QosAttr = {0};
	QosAttr.tc = 100;
	QosAttr.sl = 3;
	ret = RsSetQpAttrQos(phyId, rdevIndex, qpn, &QosAttr);

	unsigned int timeout = 15;
    unsigned int retry_cnt = 6;
	ret = RsSetQpAttrTimeout(phyId, rdevIndex, qpn, &timeout);

	ret = RsSetQpAttrRetryCnt(phyId, rdevIndex, qpn, &retry_cnt);

	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = RsQpConnectAsync(phyId, rdevIndex, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	ret = RsQpConnectAsync(phyId, rdevIndex, qpn, -1);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> RsQpConnectAsync test case end <<<<<<<<<<< */

	ret = RsQpConnectAsync(phyId, rdevIndex, qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	struct RsQpCb *qp_cb;
	ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	struct LiteConnectedInfoResp connected_resp;
	qp_cb->state = RS_QP_STATUS_CONNECTED;
	ret = RsGetLiteConnectedInfo(phyId, rdevIndex, qpn, &connected_resp);
	EXPECT_INT_EQ(ret, 0);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> RsQpCreate test case begin <<<<<<<<<<< */
	/* param error - device id */
	ret = RsQpCreateWithAttrs(15, rdevIndex, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_NE(ret, 0);

	/* qp number out of boundry */
	struct rs_cb *rs_cb;
	struct RsRdevCb *rdevCb;
	struct RsAcceptInfo accept_tmp = {0};
	struct RsAcceptInfo *accept = &accept_tmp;
	uint32_t chipId = 0;
	ret = RsDev2rscb(chipId, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);

    ret = RsGetRdevCb(rs_cb, rdevIndex, &rdevCb);
	EXPECT_INT_EQ(ret, 0);

	int qp_cnt_tmp = rdevCb->qpCnt;
	rdevCb->qpCnt = 44444;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	EXPECT_INT_NE(ret, 0);
	rdevCb->qpCnt = qp_cnt_tmp;
	/* >>>>>>> RsQpCreateWithAttrs test case end <<<<<<<<<<< */

	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreateWithAttrs: qpn2 %d, ret:%d\n", qpn2, ret);
	ret = RsQpConnectAsync(phyId, rdevIndex, qpn2, socket_info[i].fd);

	qp_norm.extAttrs.qpMode = 0;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn1 = qp_resp_create.qpn;
	rs_ut_msg("RsQpCreateWithAttrs: qpn1 %d, ret:%d\n", qpn1, ret);

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(phyId, rdevIndex, qpn2);
	ret = RsQpDestroy(phyId, rdevIndex, qpn1);
	ret = RsQpDestroy(phyId, rdevIndex, qpn);

	/* param error - qpn */
	ret = RsQpDestroy(phyId, rdevIndex, qpn);
	EXPECT_INT_NE(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);
	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

        /* >>>>>>> 1910 create_qp test case begin <<<<<<<<<<< */
        struct RsInitConfig cfg_1910 = {0};
        cfg_1910.chipId = 0;
        cfg_1910.hccpMode = NETWORK_ONLINE;
        ret = RsInit(&cfg_1910);
        EXPECT_INT_EQ(ret, 0);

		rdev_info.phyId = cfg_1910.chipId;
		rdev_info.family = AF_INET;
		rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
		ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
		EXPECT_INT_EQ(ret, 0);
        int qpn_1910;
		ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
		qpn_1910 = qp_resp_create.qpn;
        EXPECT_INT_EQ(ret, 0);

		ret = RsQpDestroy(phyId, rdevIndex, qpn_1910);

		ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
        EXPECT_INT_EQ(ret, 0);
		ret = RsSocketDeinit(rdev_info);
		EXPECT_INT_EQ(ret, 0);
        ret = RsDeinit(&cfg_1910);
		EXPECT_INT_EQ(ret, 0);
        /* >>>>>>> 1910 create_qp test case end <<<<<<<<<<< */

	return;
}

void tc_rs_mr_sync()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	int qpMode = 1;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	rs_ut_msg("___________________after listen:\n");
    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	rs_ut_msg("___________________after connect:\n");

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);

	rs_ut_msg("___________________after qp create:\n");

	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(phyId, rdevIndex, resp.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	/* >>>>>>> RsQpConnectAsync test case end <<<<<<<<<<< */

	rs_ut_msg("___________________after mr reg:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	rs_ut_msg("___________________after qp connect async:\n");
	usleep(SLEEP_TIME);
	rs_ut_msg("___________________after qp connect async & sleep:\n");

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);
	rs_ut_msg("___________________after qp2 create:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");

	usleep(SLEEP_TIME);

	ret = RsQpDestroy(phyId, rdevIndex, resp2.qpn);
	ret = RsQpDestroy(phyId, rdevIndex, resp.qpn);

	rs_ut_msg("___________________after qp1&2 destroy:\n");

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	rs_ut_msg("___________________after close socket 0:\n");

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	rs_ut_msg("___________________after close socket 1:\n");

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	rs_ut_msg("___________________after stop listen:\n");

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("___________________after deinit:\n");

	return;
}

/* create 2 socket & 2 qp, and connect them */
static int tc_rs_sock_qp_create_normal(int *fd, uint32_t *qpn, int *fd2, uint32_t *qpn2)
{
	int ret;
	int i;
	int try_num = 10;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	int qpMode = 0;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[1] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	rs_ut_msg("RS LISTEN, ret:%d !\n", ret);

    strcpy(white_list.tag, "1234");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);
	rs_ut_msg("RS CONNECT, ret:%d !\n", ret);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 0;

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	*qpn = resp.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn, socket_info[i].fd);
	*fd = socket_info[i].fd;
	rs_ut_msg("RS QP CONNECT ASYNC: ret:%d\n", ret);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn2, socket_info[i].fd);
	*fd2 = socket_info[i].fd;

	usleep(SLEEP_TIME);

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");

	return ret;
}

/* create 2 socket & 2 qp, and connect them */
static int tc_rs_sock_qp_create(int *fd, uint32_t *qpn, int *fd2, uint32_t *qpn2)
{
	int ret;
	int i;
	int try_num = 10;
	uint32_t phyId = 0;
	int flag = 0; /* RC */
	int qpMode = 1;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[1] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	unsigned int rdevIndex = 0;
	struct RsQpNorm qp_norm = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	rs_ut_msg("RS LISTEN, ret:%d !\n", ret);
    strcpy(white_list.tag, "1234");

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);
	rs_ut_msg("RS CONNECT, ret:%d !\n", ret);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	*qpn = resp.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn, socket_info[i].fd);
	*fd = socket_info[i].fd;
	rs_ut_msg("RS QP CONNECT ASYNC: ret:%d\n", ret);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn2, socket_info[i].fd);
	*fd2 = socket_info[i].fd;

	usleep(SLEEP_TIME * 10);

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");

	return ret;
}

static int tc_rs_sock_qp_destroy(int fd, uint32_t qpn, int fd2, uint32_t qpn2)
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	struct RsInitConfig cfg = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketListenInfo listen[1] = {0};
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	rs_ut_msg("resource free begin..................\n");
	usleep(SLEEP_TIME);

	ret = RsQpDestroy(phyId, rdevIndex, qpn2);
	EXPECT_INT_EQ(ret, 0);
	ret = RsQpDestroy(phyId, rdevIndex, qpn);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS destroy QP: ret:%d\n", ret);

	sock_close[0].fd = fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);
	rs_ut_msg("RS socket close fd:%d, ret:%d\n", fd, ret);

	sock_close[1].fd = fd2;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);
	rs_ut_msg("RS socket2 close fd:%d, ret:%d\n", fd2, ret);

	/* ------resource CLEAN-------- */
	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);
	rs_ut_msg("RS socket listen stop: ret:%d\n", ret);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("resource free done..................\n");

	return ret;
}

void tc_rs_mr_create()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2;
	int try_num = 0;
	struct RdmaMrRegInfo mr_reg_info = {0};

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(8192);

	try_num = 3;
	do {
		mr_reg_info.addr = addr;
		mr_reg_info.len = RS_TEST_MEM_SIZE;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	/* repeat reg */
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);

	try_num = 3;
	do {
		mr_reg_info.addr = addr2;
		mr_reg_info.len = 8192;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn2, &mr_reg_info);
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
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
	free(addr2);

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}
struct RsMrCb *g_mr_cb_a;

int stub_rs_get_mrcb_a(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb,
    struct RsListHead *mr_list)
{
	*mr_cb = g_mr_cb_a;
	return -1;
}

void tc_rs_mr_abnormal()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2;
	struct RdmaMrRegInfo mr_reg_info = {0};

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(phyId, rdevIndex, 999999, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	mr_reg_info.addr = 0;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	ret = RsMrDereg(phyId, rdevIndex, 999999, addr);
	EXPECT_INT_NE(ret, 0);

	ret = RsMrDereg(phyId, rdevIndex, qpn2, 0);
	EXPECT_INT_NE(ret, 0);

	mocker(RsQpn2qpcb, 10, 0);
	mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb_a, 1);
	ret = RsMrDereg(phyId, rdevIndex, 999999, addr2);
	EXPECT_INT_EQ(ret, -EFAULT);
	mocker_clean();

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
	ret = RsDev2rscb(0, &rs_cb2, false);
	EXPECT_INT_EQ(ret, 0);
	usleep(1000);
	events.data.fd = 200;
	RsEpollEventInHandle(rs_cb2, &events);
	events.data.fd = -1;
	RsEpollEventInHandle(rs_cb2, &events);
	events.data.fd = 0;
	RsEpollEventInHandle(rs_cb2, &events);
	rs_ut_msg("---------ABNORMAL TC End----------\n\n");
/* ABNORMAL TC End */

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;

}

struct RsQpCb *qp_cb2;
void tc_rs_cq_handle()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct RsQpCb qp_cb_4;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	qp_cb_4.channel = NULL;
	RsDrvPollCqHandle(&qp_cb_4);

	ret = RsQpn2qpcb(phyId, rdevIndex, qpn, &qp_cb_ab2);
	EXPECT_INT_EQ(ret, 0);
	RsDrvPollCqHandle(qp_cb_ab2);

	struct RsQpCb qpcb_tmp = {0};
	struct ibv_wc wc = {0};
	struct ibv_cq ev_cq_sq = {0};
	struct ibv_cq ev_cq_rq = {0};
	struct RsRdevCb rdevCb = {0};

	qpcb_tmp.ibSendCq = &ev_cq_sq;
	qpcb_tmp.ibRecvCq = &ev_cq_rq;
	qpcb_tmp.rdevCb = &rdevCb;

	RsCqeCallbackProcess(&qpcb_tmp, &wc, &ev_cq_sq);
	RsCqeCallbackProcess(&qpcb_tmp, &wc, &ev_cq_sq);

	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_WR_FLUSH_ERR;
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_SUCCESS;
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

	wc.status = IBV_WC_MW_BIND_ERR;
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);

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
	RsEpollEventHandleOne(NULL, &events_3);

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
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
	struct SocketWlistInfoT white_list;
	white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)system, 10, 0);
	mocker((stub_fn_t)access, 10, 0);
	mocker_invoke((stub_fn_t)halGetDeviceInfo, stub_halGetDeviceInfo, 10);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	/* >>>>>>> RsSocketListenStart test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = RsSocketListenStart(NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	listen[0].phyId = 15;
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	listen[0].phyId = 0;

	/* repeat listen */
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	/* >>>>>>> RsSocketListenStart test case end <<<<<<<<<<< */

	strcpy_s(white_list.tag, SOCK_CONN_TAG_SIZE, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	RsSocketWhiteListAdd(rdev_info, &white_list, 1);
	strcpy_s(white_list.tag, SOCK_CONN_TAG_SIZE, "5678");
	RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy_s(conn[0].tag, SOCK_CONN_TAG_SIZE, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET;
	conn[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy_s(conn[1].tag, SOCK_CONN_TAG_SIZE, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 2);

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy_s(socket_info[i].tag, SOCK_CONN_TAG_SIZE, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy_s(socket_info[i].tag, SOCK_CONN_TAG_SIZE, "5678");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> RsSocketSend test case begin <<<<<<<<<<< */
	int data = 0;
	int size = sizeof(data);
	/* param error */
	ret = RsSocketSend(socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketSend(0, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketSend(1, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);

	/* fd error */
	ret = RsSocketSend(1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketSend(0, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketSend(1, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> RsSocketSend test case end <<<<<<<<<<< */

	/* >>>>>>> RsSocketRecv test case begin <<<<<<<<<<< */
	/* param error */
	ret = RsSocketRecv(socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketRecv(0, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketRecv(1, socket_info[i].fd, NULL, 0);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketRecv(1, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	ret = RsPeerSocketRecv(0, 1111, &data, size);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> RsSocketRecv test case end <<<<<<<<<<< */

	/* >>>>>>> RsSocketBatchClose test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = RsSocketBatchClose(0, NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	ret = RsSocketBatchClose(0, &sock_close[i], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	sock_close[i].fd = -1;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);
	/* >>>>>>> RsSocketBatchClose test case end <<<<<<<<<<< */

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	usleep(1000);

 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy_s(socket_info[i].tag, SOCK_CONN_TAG_SIZE, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* >>>>>>> RsSocketListenStop test case begin <<<<<<<<<<< */
	/* param error - close_info NULL */
	ret = RsSocketListenStop(NULL, 1);
	EXPECT_INT_NE(ret, 0);

	/* param error - num = 0 */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	listen[0].phyId = 15;
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);
	EXPECT_INT_NE(ret, 0);
	listen[0].phyId = 0;
	/* >>>>>>> RsSocketListenStop test case end <<<<<<<<<<< */

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

int replace_rs_qpn2qpcb(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct RsQpCb **qp_cb)
{
    static struct RsQpCb a_qp_cb;

    *qp_cb = &a_qp_cb;
    a_qp_cb.state = 1;
    return 0;
}

void tc_rs_get_qp_status()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	uint32_t qpn, qpn2;
	int fd, fd2;
	struct RsQpStatusInfo status;

    struct RsQpCb qp_cb;
    struct rs_cb rs_cb;
	struct RsRdevCb rdevCb;
    qp_cb.rdevCb = &rdevCb;

	mocker((stub_fn_t)ibv_query_port, 1, 1);
    ret = RsDrvSetMtu(&qp_cb);
	EXPECT_INT_EQ(ret, -EOPENSRC);
    mocker_clean();

	/* +++++Resource Prepare+++++ */
	mocker((stub_fn_t)RsDrvSetMtu, 10, 5);
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);
    mocker_clean();

    mocker_invoke(RsQpn2qpcb, replace_rs_qpn2qpcb, 1);
    mocker(RsRoceQueryQpc, 10, 1);
    ret = RsGetQpStatus(phyId, rdevIndex, qpn, &status);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_notify_ba()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	unsigned int phyId = 0;
	struct MrInfoT info = {0};
	unsigned int rdevIndex = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	ret = RsGetNotifyMrInfo(phyId, rdevIndex, &info);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetNotifyMrInfo(100000, rdevIndex, &info);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetNotifyMrInfo(phyId, rdevIndex, NULL);
	EXPECT_INT_NE(ret, 0);

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_setup_sharemem()
{
	struct RsRdevCb rdevCb = {0};
	struct rs_cb rs_cb = {0};
	int ret;

	DlHalInit();

	ret = RsBindHostpid(0, getpid());
	EXPECT_INT_NE(ret, 0);

	rs_cb.hccpMode = NETWORK_OFFLINE;
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_910A, 20);
	ret = RsSetupSharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	ret = RsSetupSharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	rs_cb.grpSetupFlag = false;
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_sharemem, 20);
	ret = RsSetupSharemem(&rs_cb, false, 0);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	rs_cb.grpSetupFlag = false;
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_sharemem, 20);
	mocker_invoke((stub_fn_t)DlHalQueryDevPid, dl_hal_query_dev_pid_sharemem, 20);
	ret = RsSetupSharemem(&rs_cb, false, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	rs_cb.grpSetupFlag = false;
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_sharemem, 20);
	ret = RsSetupSharemem(&rs_cb, true, 0);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	ret = RsOpenBackupIbCtx(&rdevCb);
	EXPECT_INT_NE(ret, 0);

	DlHalDeinit();
}

void tc_rs_post_recv()
{
	int ret;
	uint32_t qpn, qpn2;
	int fd, fd2;
	uint32_t size;
	int try_num;
	void *addr, *addr2;
	struct RdmaMrRegInfo mr_reg_info = {0};
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	try_num = 3;
	do {
		mr_reg_info.addr = addr;
		mr_reg_info.len = RS_TEST_MEM_SIZE;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
		EXPECT_INT_EQ(ret, 0);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	try_num = 3;
	do {
		mr_reg_info.addr = addr2;
		mr_reg_info.len = RS_TEST_MEM_SIZE;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn2, &mr_reg_info);
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
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
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
	struct SgList list;
	struct WrInfo wrlist[1];
	int try_num;
	void *addr, *addr2;
	struct SendWrRsp rs_wr_info[1];
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	struct RsWrlistBaseInfo base_info;
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	base_info.phyId = phyId;
	base_info.rdevIndex = rdevIndex;
	base_info.qpn = qpn;
	base_info.keyFlag = 0;
	try_num = 3;
	do {
		ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
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
		ret = RsMrReg(phyId, rdevIndex, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct RsMrCb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct RsMrCb));
	addr2_mr_cb->mrInfo.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mrInfo.addr = mr_reg_info.addr;
	addr2_mr_cb->mrInfo.len = mr_reg_info.len;
	addr2_mr_cb->mrInfo.rkey = mr_reg_info.lkey;

	struct RsQpCb *qp_cb = NULL;
	RsQpn2qpcb(phyId, rdevIndex, qpn, &qp_cb);
	RsListAddTail(&addr2_mr_cb->list, &qp_cb->remMrList);

	usleep(1000);

	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	wrlist[0].memList = list;
	wrlist[0].dstAddr = addr2;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = RS_SEND_FENCE;

	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	wrlist[0].dstAddr = NULL;
	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -ENOENT);
	wrlist[0].dstAddr = addr2;

	wrlist[0].memList.addr = NULL;
	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EFAULT);
	wrlist[0].memList.addr = addr;

	list.len = 2147483649;
	wrlist[0].memList = list;
	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EINVAL);
	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
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
	struct SgList list;
	struct WrInfo wrlist[1];
	int try_num;
	void *addr, *addr2;
	struct SendWrRsp rs_wr_info[1];
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	struct RsWrlistBaseInfo base_info;
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create_normal(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	base_info.phyId = phyId;
	base_info.rdevIndex = rdevIndex;
	base_info.qpn = qpn;
	base_info.keyFlag = 0;
	try_num = 3;
	do {
		ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
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
		ret = RsMrReg(phyId, rdevIndex, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct RsMrCb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct RsMrCb));
	addr2_mr_cb->mrInfo.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mrInfo.addr = mr_reg_info.addr;
	addr2_mr_cb->mrInfo.len = mr_reg_info.len;
	addr2_mr_cb->mrInfo.rkey = mr_reg_info.lkey;

	struct RsQpCb *qp_cb = NULL;
	RsQpn2qpcb(phyId, rdevIndex, qpn, &qp_cb);
	RsListAddTail(&addr2_mr_cb->list, &qp_cb->remMrList);

	usleep(1000);

	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	list.addr = addr;
	list.len = RS_TEST_MEM_SIZE;
	wrlist[0].memList = list;
	wrlist[0].dstAddr = addr2;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = RS_SEND_FENCE;

	mocker(RsQpn2qpcb, 1, -1);
	ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
	EXPECT_INT_EQ(ret, -EACCES);
	mocker_clean();

	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	wrlist[0].dstAddr = NULL;
	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -ENOENT);

	wrlist[0].dstAddr = addr2;
	wrlist[0].memList.addr = NULL;
	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, send_num, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EFAULT);
	wrlist[0].memList.addr = addr;

	try_num = 3;
	do {
		ret =RsSendWrlist(base_info, wrlist, 1028, rs_wr_info, &complete_num);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, -EINVAL);

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
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
	struct SgList list[2];
	struct SendWr wr;
	int try_num;
	void *addr, *addr2;
	struct wr_exp_rsp rs_wr_info;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	struct RdmaMrRegInfo mr_reg_info = {0};

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	try_num = 3;
	do {
		mr_reg_info.addr = addr;
		mr_reg_info.len = RS_TEST_MEM_SIZE;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG1: qpn %d, ret:%d\n", qpn, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));

	try_num = 3;
	do {
		mr_reg_info.addr = addr2;
		mr_reg_info.len = RS_TEST_MEM_SIZE;
		mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
		ret = RsMrReg(phyId, rdevIndex, qpn2, &mr_reg_info);
		if (0 == ret)
			break;
		rs_ut_msg("MR REG2: qpn2 %d, ret:%d\n", qpn2, ret);
		try_num--;
		sleep(1);
	} while(try_num && (-EAGAIN == ret));
	EXPECT_INT_EQ(ret, 0);

	struct RsMrCb *addr2_mr_cb;
    addr2_mr_cb = calloc(1, sizeof(struct RsMrCb));
	addr2_mr_cb->mrInfo.cmd = RS_CMD_MR_INFO;
	addr2_mr_cb->mrInfo.addr = mr_reg_info.addr;
	addr2_mr_cb->mrInfo.len = mr_reg_info.len;
	addr2_mr_cb->mrInfo.rkey = mr_reg_info.lkey;

	struct RsQpCb *qp_cb = NULL;
	RsQpn2qpcb(phyId, rdevIndex, qpn, &qp_cb);
	RsListAddTail(&addr2_mr_cb->list, &qp_cb->remMrList);

	usleep(1000);

	list[0].addr = addr;
	list[0].len = RS_TEST_MEM_SIZE;
	list[1].addr = addr;
	list[1].len = RS_TEST_MEM_SIZE;
	wr.bufList = list;
	wr.bufNum = 2;
	wr.dstAddr = addr2;
	wr.op = 0;
	wr.sendFlag = RS_SEND_FENCE;

	try_num = 3;
	do {
		ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
		if (0 == ret)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	/* RDMA Write with Notify Test */
	try_num = 3;
	wr.op = 0x16;
	do {
		ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
		if (ret == 0)
			break;
		usleep(SLEEP_TIME);
	} while(try_num-- && ret == -2);
	EXPECT_INT_EQ(ret, 0);

	/* qpn error */
	ret = RsSendWr(phyId, rdevIndex, 44444, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	wr.bufNum = MAX_SGE_NUM + 1;
	ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	wr.bufNum =  2;

	list[0].len = 2147483649;
    list[1].len = 2147483649;
	ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	list[0].len = RS_TEST_MEM_SIZE;
	list[1].len = RS_TEST_MEM_SIZE;
	/* addr error, cannot find mrcb */
	list[0].addr = 5555;
	ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	list[0].addr = addr;

	/* addr error, cannot find remote mrcb */
	wr.dstAddr = 5555;
	ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	wr.dstAddr = addr2;

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
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
	struct RsQpCb qp_cb_4;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	/* +++++Resource Free+++++ */
	ret = tc_rs_sock_qp_destroy(fd, qpn, fd2, qpn2);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_white_list()
{
	int ret;
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen;
	struct SocketConnectInfo conn;
	struct SocketConnectInfo conn1;

	struct RsSocketCloseInfoT sock_close;
	struct RsSocketCloseInfoT sock_close1;

	struct SocketFdData socket_info;
	struct SocketFdData socket_info1;
    struct SocketWlistInfoT white_list;
    struct SocketWlistInfoT white_list1;
    u32 server_ip = inet_addr("127.0.0.1");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen.phyId = 0;
	listen.family = AF_INET;
	listen.localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen.port = 18888;
	ret = RsSocketListenStart(&listen, 1);

	conn.phyId = 0;
	conn.family = AF_INET;
	conn.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	conn.localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn.tag, "LinkCheck");
	conn.port = 18888;
	ret = RsSocketBatchConnect(&conn, 1);
	EXPECT_INT_EQ(ret, 0);

	conn1.phyId = 0;
	conn1.family = AF_INET;
	conn1.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	conn1.localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn1.tag, "2345");
	conn1.port = 18888;
    sleep(1);
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
    strcpy(white_list.tag, "LinkCheck");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = server_ip;
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

    white_list1.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list1.connLimit = 1;
    strcpy(white_list1.tag, "2345");
    RsSocketWhiteListAdd(rdev_info, &white_list1, 1);

    socket_info.phyId = 0;
	socket_info.family = AF_INET;
 	socket_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info.tag, "LinkCheck");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info, 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info.fd, socket_info.status);

    RsSocketWhiteListDel(rdev_info, &white_list, 1);
    RsSocketWhiteListDel(rdev_info, &white_list1, 1);
	sock_close.fd = socket_info.fd;
	ret = RsSocketBatchClose(0, &sock_close, 1);

	sock_close1.fd = socket_info1.fd;

	listen.port = 18888;
	ret = RsSocketListenStop(&listen, 1);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_ssl_test1()
{
    int ret;
	int try_num = 10;
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
	uint32_t sslEnable = 1;

	ret = RsGetSslEnable(NULL);
	EXPECT_INT_NE(ret, 0);

    cfg.chipId = 0;
    cfg.hccpMode = NETWORK_OFFLINE;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);

	ret = RsGetSslEnable(NULL);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetSslEnable(&sslEnable);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(sslEnable, 0);

    /* pridata tls */
    struct rs_cb rscb = {0};
    struct RsSecPara rs_para = {0};
    struct tls_cert_mng_info mng_info = {0};

    mng_info.work_key_len = 10;
    mng_info.ky_len = 512;
    mng_info.ky_enc_len = 1678;
    mng_info.pwd_enc_len = 15;
    mng_info.pwd_len = 16;

    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_EQ(ret, 0);

    mocker(dev_read_flash, 10, -1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(kmc_dec_data, 10, -1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(crypto_decrypt_with_aes_gcm, 10, -1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker(crypto_gen_key_with_pbkdf2, 10, -1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)memset_s, 10, 1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    mocker((stub_fn_t)memset_s, 10, -1);
    ret = rs_get_pridata(&rscb, &rs_para, &mng_info);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    /* tls pridata end */

	struct rs_cb *tag_rs_cb = NULL;
	ret = RsGetRsCb(cfg.chipId, &tag_rs_cb);
	tag_rs_cb->sslEnable = 1;
	struct RsConnInfo tag_conn = {0};
	tag_conn.connfd = -1;
	RsSocketTagSync(&tag_conn);

    listen[0].phyId = 0;
	listen[0].family = AF_INET;
    listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 26666;
    ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

    conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
    conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    strcpy(conn[0].tag, "1234");
	conn[0].port = 26666;
    ret = RsSocketBatchConnect(&conn[0], 1);
	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	usleep(SLEEP_TIME);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	usleep(SLEEP_TIME);

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    return;
}

extern int RsFillIfaddrInfos(struct IfaddrInfo ifaddr_infos[], unsigned int *num, unsigned int phyId);
extern int RsFillIfaddrInfosV2(struct InterfaceInfo interface_infos[], unsigned int *num, unsigned int phyId);
extern enum RsHardwareType RsGetDeviceType(unsigned int phyId);
extern int RsCheckDstInterface(unsigned int phyId, const char *ifaName, enum RsHardwareType type, bool isAll);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);
extern int RsFillIfnum(unsigned int phyId, bool is_all, unsigned int *num, unsigned int is_peer);

void tc_rs_get_interface_version()
{
	int version;
	int ret = RsGetInterfaceVersion(0, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetInterfaceVersion(0, &version);
	EXPECT_INT_EQ(ret, 0);

    mocker(RsRoceGetApiVersion, 1, 0);
    ret = RsGetInterfaceVersion(96, &version);
    EXPECT_INT_EQ(ret, 0);
}

int stub_dl_hal_get_device_info(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value)
{
	*value = 0x10;
	return 0;
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

void tc_rs_get_ifaddrs()
{
	DlHalInit();
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	int ret;
	int phyId = 0;
	unsigned int ifaddr_num = 4;
	struct IfaddrInfo ifaddr_infos[4] = {0};
	bool is_all = false;

	ret = RsGetIfaddrs(ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)RsFillIfaddrInfos, 1, 1);
	ret = RsGetIfaddrs(ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = RsGetIfaddrs(NULL, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetIfaddrs(ifaddr_infos, &ifaddr_num, 129);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)RsGetDeviceType, 1, 2);
	ret = RsFillIfaddrInfos(ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info_pod, 10);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info_pod_16p, 10);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)DlHalGetDeviceInfo, 1, -1);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 3);
	mocker_clean();

	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info, 10);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_910A, 20);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 1);

	mocker((stub_fn_t)getifaddrs, 1, -1);
	ret = RsFillIfaddrInfos(ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, -ESYSFUNC);
	mocker_clean();

	mocker((stub_fn_t)RsCheckDstInterface, 1, -1);
	ret = RsFillIfaddrInfos(ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	ifaddr_num = 0;
	ret = RsFillIfaddrInfos(ifaddr_infos, &ifaddr_num, phyId);

	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 1);

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = RsCheckDstInterface(phyId, "eth0", 0, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, 2);
	ret = RsCheckDstInterface(phyId, "bond0", 0, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = RsCheckDstInterface(phyId, "bond0", 0, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
	return;
}

enum {
	IFF_UP = 1 << 0,
	IFF_RUNNING = 1 << 6,
};

int stub_getifaddrs(struct ifaddrs **ifap)
{
	*ifap = malloc(sizeof(struct ifaddrs));
	if (*ifap == NULL)
		return -ENOMEM;
	memset(*ifap, 0, sizeof(struct ifaddrs));

	struct sockaddr_in *sa_addr = malloc(sizeof(struct sockaddr_in));
	if (sa_addr == NULL)
		return -ENOMEM;
	memset(sa_addr, 0, sizeof(struct sockaddr_in));

	struct sockaddr_in *sa_netmask = malloc(sizeof(struct sockaddr_in));
	if (sa_netmask == NULL)
		return -ENOMEM;
	memset(sa_netmask, 0, sizeof(struct sockaddr_in));

	sa_addr->sin_family = AF_INET;
	sa_addr->sin_port = 0;
	inet_pton(AF_INET, "192.168.100.50", &(sa_addr->sin_addr));

	sa_netmask->sin_family = AF_INET;
	sa_netmask->sin_port = 0;
	inet_pton(AF_INET, "255.255.255.0", &(sa_netmask->sin_addr));
	(*ifap)->ifa_addr = (struct sockaddr *)sa_addr;
	(*ifap)->ifa_netmask = (struct sockaddr *)sa_netmask;

	(*ifap)->ifa_next = NULL;
	(*ifap)->ifa_name = "eth0";
	(*ifap)->ifa_flags = IFF_UP | IFF_RUNNING;

	return 0;
}

void stub_freeifaddrs(struct ifaddrs *ifa)
{
	if (!ifa) return;
	if (ifa->ifa_next) stub_freeifaddrs(ifa->ifa_next);
	if (ifa->ifa_addr) free(ifa->ifa_addr);
	if (ifa->ifa_netmask) free(ifa->ifa_netmask);
	if (ifa->ifa_ifu.ifu_broadaddr) free(ifa->ifa_ifu.ifu_broadaddr);
	free(ifa);
}

void tc_rs_get_ifaddrs_v2()
{
	DlHalInit();
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	int ret;
	int phyId = 0;
	unsigned int ifaddr_num = 4;
	struct InterfaceInfo interface_infos[4] = {0};
	bool is_all = false;

	ret = RsGetIfaddrsV2(interface_infos, &ifaddr_num, phyId, is_all);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)RsFillIfaddrInfosV2, 1, 1);
	ret = RsGetIfaddrsV2(interface_infos, &ifaddr_num, phyId, is_all);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = RsGetIfaddrsV2(NULL, &ifaddr_num, phyId, 0);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetIfaddrsV2(interface_infos, &ifaddr_num, 129, is_all);
	EXPECT_INT_EQ(ret, -EINVAL);

	ifaddr_num = 1;
	mocker((stub_fn_t)RsGetDeviceType, 1, 2);
	mocker_invoke(getifaddrs, stub_getifaddrs, 10);
	mocker_invoke(freeifaddrs, stub_freeifaddrs, 10);
	ret = RsFillIfaddrInfosV2(interface_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info_pod, 10);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)getifaddrs, 1, -1);
	ret = RsFillIfaddrInfosV2(interface_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, -ESYSFUNC);
	mocker_clean();

	mocker((stub_fn_t)RsCheckDstInterface, 1, -1);
	ret = RsFillIfaddrInfosV2(interface_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	ifaddr_num = 0;
	ret = RsFillIfaddrInfosV2(interface_infos, &ifaddr_num, phyId);

	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 1);

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = RsCheckDstInterface(phyId, "eth0", 0, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
	return;
}

void tc_rs_peer_get_ifaddrs()
{
	int ret;
	int phyId = 0;
	unsigned int ifaddr_num = 1000;
	unsigned int if_num = 1000;
	struct InterfaceInfo ifaddr_infos[1000] = {0};
	struct RsInitConfig cfg = {0};
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	ret = RsPeerGetIfnum(phyId, &if_num);
	EXPECT_INT_EQ(ret, 0);

	ret = RsPeerGetIfaddrs(&ifaddr_infos, &ifaddr_num, phyId);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(if_num, ifaddr_num);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

void tc_rs_get_ifnum()
{
	DlHalInit();
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	int ret;
	int phyId = 0;
	unsigned int ifnum = 0;
	bool is_all = false;

	ret = RsGetIfnum(phyId, is_all, &ifnum);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)RsFillIfnum, 1, 1);
	ret = RsGetIfnum(phyId, is_all, &ifnum);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	ret = RsGetIfnum(phyId, is_all, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	mocker((stub_fn_t)getifaddrs, 1, -1);
	ret = RsFillIfnum(phyId, is_all, &ifnum, 1);
	EXPECT_INT_EQ(ret, -ESYSFUNC);
	mocker_clean();

	mocker((stub_fn_t)RsCheckDstInterface, 1, -1);
	ret = RsFillIfnum(phyId, is_all, &ifnum, 0);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
	return;
}

void tc_rs_get_cur_time()
{
	struct timeval time;
	mocker(gettimeofday, 20, 1);
	mocker(memset_s, 20, 1);
	RsGetCurTime(&time);
	mocker_clean();
	return;
}

void tc_RsRdev2rdevCb()
{
	struct RsRdevCb *rdevCb;
	mocker(RsDev2rscb, 20, 0);
	mocker(RsGetRdevCb, 20, 1);
	RsRdev2rdevCb(1, 1, &rdevCb);
	mocker_clean();
	return;
}

void tc_rs_compare_ip_gid()
{
	struct rdev rdev_info;
	union ibv_gid gid;
	rdev_info.family = 10;
	int ret = RsCompareIpGid(rdev_info,  &gid);
	EXPECT_INT_EQ(ret, -ENODEV);

    mocker(memcmp, 20, 0);
	ret = RsCompareIpGid(rdev_info,  &gid);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	return;
}

void tc_rs_query_gid()
{
	struct ibv_context ib_ctx_tmp;
	struct RsRdevCb rdevCb;
	struct rdev rdev_info;
	uint8_t ib_port = 1;
	int gid_idx;
	int ret;

	mocker(ibv_query_port, 20, 1);
	ret = RsQueryGid(rdev_info, &ib_ctx_tmp, ib_port, &gid_idx);
	EXPECT_INT_EQ(ret, -EOPENSRC);
	mocker_clean();

	mocker(ibv_query_gid_type, 20, 1);
	ret = RsQueryGid(rdev_info, &ib_ctx_tmp, ib_port, &gid_idx);
	EXPECT_INT_EQ(ret, -EOPENSRC);
	mocker_clean();

	mocker(ibv_query_gid, 20, 1);
	ret = RsQueryGid(rdev_info, &ib_ctx_tmp, ib_port, &gid_idx);
	EXPECT_INT_EQ(ret, -EOPENSRC);
	mocker_clean();

	mocker(RsCompareIpGid, 20, 1);
	RsQueryGid(rdev_info, &ib_ctx_tmp, ib_port, &gid_idx);
	mocker_clean();
	return;
}

void tc_rs_get_host_rdev_index()
{
	struct RsRdevCb rdevCb = {0};
	struct rdev rdev_info = {0};
	struct rs_cb rs_cb = {0};
	int rdevIndex = 0;
	int ret;

	rdevCb.devList = ibv_get_device_list(&(rdevCb.devName));
	rdevCb.rs_cb = &rs_cb;
	mocker((stub_fn_t)pthread_mutex_lock, 20, 0);
	mocker((stub_fn_t)pthread_mutex_unlock, 20, 0);
	mocker(RsIbvGetDeviceName, 20, "910B");
	mocker(RsConvertIpAddr, 20, -EINVAL);
	ret = RsGetHostRdevIndex(rdev_info, &rdevCb, &rdevIndex, 0);
	EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();
}

void tc_rs_get_ib_ctx_and_rdev_index()
{
	struct rdev rdev_info = {0};
	int rdevIndex = 0;
	struct RsRdevCb rdevCb = {0};
	int ret;

	rdevCb.devList = ibv_get_device_list(&(rdevCb.devName));
	mocker(ibv_open_device, 20, NULL);
	ret = RsGetIbCtxAndRdevIndex(rdev_info, &rdevCb, &rdevIndex);
    EXPECT_INT_EQ(ret, -ENODEV);
	mocker_clean();

	mocker(RsQueryGid, 20, -EEXIST);
	ret = RsGetIbCtxAndRdevIndex(rdev_info, &rdevCb, &rdevIndex);
    EXPECT_INT_EQ(ret, -EEXIST);
	mocker_clean();

	mocker(RsQueryGid, 20, 1);
	ret = RsGetIbCtxAndRdevIndex(rdev_info, &rdevCb, &rdevIndex);
    EXPECT_INT_EQ(ret, 1);
	mocker_clean();
	return;
}

void tc_rs_rdev_cb_init()
{
	struct rdev rdev_info = {0};
	struct rs_cb rs_cb;
	struct RsRdevCb rdevCb = {0};
	int rdevIndex;
	mocker(RsInetNtop, 20, 0);
	mocker(pthread_mutex_init, 20, 1);
	int ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, -ESYSFUNC);
	mocker_clean();

	mocker(RsInetNtop, 20, 0);
	mocker(RsGetIbCtxAndRdevIndex, 20, 1);
	ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker(RsInetNtop, 20, 0);
	mocker(RsGetIbCtxAndRdevIndex, 20, 0);
	mocker(RsGetSqDepthAndQpMaxNum, 20, 1);
	mocker(RsRoceUnmmapAiDbReg, 20, 1);
	ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker(RsInetNtop, 20, 0);
	mocker(RsGetIbCtxAndRdevIndex, 20, 0);
	mocker(RsGetSqDepthAndQpMaxNum, 20, 0);
	mocker(RsRoceUnmmapAiDbReg, 20, 0);
    mocker(RsSetupPdAndNotify, 20, 1);
	ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker(RsInetNtop, 20, 0);
	mocker(RsGetIbCtxAndRdevIndex, 20, 0);
	mocker(RsDrvQueryNotifyAndAllocPd, 20, 1);
	mocker(ibv_close_device, 20, 1);
	ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();

	mocker(RsInetNtop, 20, 0);
	mocker(RsGetIbCtxAndRdevIndex, 20, 0);
	mocker(RsDrvQueryNotifyAndAllocPd, 20, 0);
	mocker(RsDrvRegNotifyMr, 20, 1);
	mocker(ibv_dealloc_pd, 20, 1);
	mocker(ibv_close_device, 20, 1);
	ret = RsRdevCbInit(rdev_info, &rdevCb, &rs_cb, &rdevIndex);
	EXPECT_INT_EQ(ret, 1);
	mocker_clean();
	return;
}

void tc_rs_rdev_init()
{
	struct rdev rdev_info;
	int rdevIndex;
	int ret;
	mocker(RsGetRsCb, 20, 0);
	mocker_clean();

	mocker(RsGetRsCb, 20, 0);
	mocker(calloc, 20, NULL);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, -ENOMEM);
	mocker_clean();

	mocker(ibv_get_device_list, 20, NULL);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();

	mocker(RsApiInit, 1, -EINVAL);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();

	return;
}

void tc_rs_ssl_free()
{
	struct rs_cb rscb = {0};
	struct RsCertSkidSubjectCb *skidSubjectCb = (struct RsCertSkidSubjectCb *)malloc(sizeof(struct RsCertSkidSubjectCb));
	SSL_CTX *clientSslCtx = (SSL_CTX *)malloc(sizeof(SSL_CTX));
	SSL_CTX *serverSslCtx = (SSL_CTX *)malloc(sizeof(SSL_CTX));
	rscb.sslEnable = 1;
	rscb.skidSubjectCb = skidSubjectCb;
	rscb.clientSslCtx = clientSslCtx;
	rscb.serverSslCtx = serverSslCtx;
	mocker(memset_s, 1, 1);
	RsSslFree(&rscb);
    mocker_clean();
	return;
}

void tc_rs_drv_connect()
{
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	mocker(connect, 20, 1);
	RsDrvConnect(1, 1, 1, 1);
	mocker_clean();
	free(gRsCb);
	gRsCb = NULL;
	return;
}

void tc_rs_listen_invalid_port()
{
	int ret;
	struct SocketListenInfo listen[2] = {0};
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 0;
	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 65536;
	ret = RsSocketListenStart(&listen[0], 1);
	EXPECT_INT_EQ(ret, -22);
	free(gRsCb);
	gRsCb = NULL;
	return;
}

void tc_rs_qpn2qpcb()
{
	struct RsQpCb *qp_cb;
	RsQpn2qpcb(1, 1, 1, &qp_cb);
	return;
}

void tc_rs_ssl_deinit()
{
	struct rs_cb rscb;
	rscb.sslEnable = 1;
	rscb.skidSubjectCb = malloc(sizeof(struct RsCertSkidSubjectCb));
	mocker(SSL_CTX_free, 20, 1);
	rs_ssl_deinit(&rscb);
	mocker_clean();
	return;
}

void tc_rs_tls_inner_enable()
{
	struct rs_cb rscb;
	rscb.sslEnable = 1;
	mocker(rs_ssl_inner_init, 20, 1);
	int ret = rs_tls_inner_enable(&rscb, 1);
	EXPECT_INT_EQ(1, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_inner_init()
{
	struct rs_cb rscb = {0};
	mocker(SSL_CTX_new, 20, NULL);
	rscb.skidSubjectCb = malloc(sizeof(struct RsCertSkidSubjectCb));
	int ret = rs_ssl_inner_init(&rscb);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

    mocker(rs_ssl_ca_ky_init, 20, 1);
	ret = rs_ssl_inner_init(&rscb);
	EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_ca_ky_init()
{
	struct rs_cb rscb = {0};
	rscb.serverSslCtx = SSL_CTX_new(TLS_server_method());
	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 0);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 0);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 1);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 1);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 1);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	mocker(SSL_CTX_set_options, 20, 1);
	mocker(SSL_CTX_set_min_proto_version, 20, 1);
	mocker(SSL_CTX_set_cipher_list, 20, 1);
	mocker(rs_ssl_load_ca, 20, 0);
	mocker(rs_ssl_crl_init, 20, 0);
	mocker(rs_check_pridata, 20, 0);
	rs_ssl_ca_ky_init(rscb.serverSslCtx, &rscb);
	mocker_clean();

	SSL_CTX_free(rscb.serverSslCtx);
    rscb.serverSslCtx = NULL;

	return;
}

void tc_rs_ssl_crl_init()
{
	SSL_CTX ssl_ctx;
	struct rs_cb rscb;
	struct tls_cert_mng_info mng_info;
	int ret;

	mocker(rs_ssl_get_crl_data, 20, 0);
	mocker(SSL_CTX_get_cert_store, 20, NULL);
	ret = rs_ssl_crl_init(&ssl_ctx, &rscb, &mng_info);
    EXPECT_INT_EQ(-EFAULT, ret);
	mocker_clean();

	mocker(rs_ssl_get_crl_data, 20, 0);
	mocker(SSL_CTX_get_cert_store, 20, 1);
	mocker(X509_STORE_set_flags, 20, 0);
	ret = rs_ssl_crl_init(&ssl_ctx, &rscb, &mng_info);
	EXPECT_INT_EQ(-EFAULT, ret);
	mocker_clean();

	mocker(rs_ssl_get_crl_data, 20, 0);
	mocker(SSL_CTX_get_cert_store, 20, 1);
	mocker(X509_STORE_add_crl, 20, 0);
	ret = rs_ssl_crl_init(&ssl_ctx, &rscb, &mng_info);
	EXPECT_INT_EQ(-EFAULT, ret);
	mocker_clean();

	mocker(rs_ssl_get_crl_data, 20, 2);
	ret = rs_ssl_crl_init(&ssl_ctx, &rscb, &mng_info);
	EXPECT_INT_EQ(2, ret);
	mocker_clean();
	return;
}

void tc_rs_check_pridata()
{
	SSL_CTX ssl_ctx;
	struct rs_cb rscb;
	struct tls_cert_mng_info mng_info;
	int ret;
	mocker(rs_get_pk, 20, 0);
	mocker(SSL_CTX_use_PrivateKey, 20, 0);
	rs_check_pridata(&ssl_ctx, &rscb, &mng_info);

	mocker_clean();

	mocker(rs_get_pk, 20, 0);
	mocker(SSL_CTX_use_PrivateKey, 20, 1);
	mocker(SSL_CTX_check_private_key, 20, 0);
	ret = rs_check_pridata(&ssl_ctx, &rscb, &mng_info);
	mocker_clean();

	mocker(rs_get_pk, 20, 0);
	mocker(SSL_CTX_use_PrivateKey, 20, 1);
	mocker(SSL_CTX_check_private_key, 20, 1);
	ret = rs_check_pridata(&ssl_ctx, &rscb, &mng_info);
	return;
}

void tc_rs_ssl_load_ca()
{
	SSL_CTX ssl_ctx;
	struct rs_cb rscb;
	struct tls_cert_mng_info mng_info;
	int ret;

	mocker(NetCommGetSelfHome, 20, 1);
	ret = rs_ssl_load_ca(&ssl_ctx, &rscb, &mng_info);
	EXPECT_INT_EQ(1, ret);
	mocker_clean();

	mocker(rs_ssl_get_ca_data, 20, 1);
	ret = rs_ssl_load_ca(&ssl_ctx, &rscb, &mng_info);
    EXPECT_INT_EQ(1, ret);
	mocker_clean();

	mocker(rs_ssl_get_ca_data, 20, 0);
	mocker(SSL_CTX_load_verify_locations, 20, 0);
	ret = rs_ssl_load_ca(&ssl_ctx, &rscb, &mng_info);
	mocker_clean();

	mocker(rs_ssl_get_ca_data, 20, 0);
	mocker(SSL_CTX_load_verify_locations, 20, 1);
	mocker(SSL_CTX_use_certificate_chain_file, 20, 0);
	ret = rs_ssl_load_ca(&ssl_ctx, &rscb, &mng_info);
	mocker_clean();

	mocker(rs_ssl_get_ca_data, 20, 0);
	mocker(SSL_CTX_load_verify_locations, 20, 1);
	mocker(SSL_CTX_use_certificate_chain_file, 20, 1);
	mocker(rs_remove_certs, 20, 1);
	ret = rs_ssl_load_ca(&ssl_ctx, &rscb, &mng_info);
	mocker_clean();

	return;
}

void tc_rs_ssl_get_ca_data()
{
	int ret;
	char end_file;
	char ca_file;
	struct rs_cb rscb = {0};
	struct tls_cert_mng_info mng_info = {0};

	mocker(calloc, 20, NULL);
	ret = rs_ssl_get_ca_data(&rscb, &end_file, &ca_file, &mng_info);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

	mocker(rs_ssl_get_cert, 10, -1);
	ret = rs_ssl_get_ca_data(&rscb, &end_file, &ca_file, &mng_info);
	EXPECT_INT_EQ(ret, -EACCES);
	mocker_clean();

	mocker(rs_ssl_get_cert, 10, 0);
	mocker(rs_ssl_put_certs, 10, -1);
	ret = rs_ssl_get_ca_data(&rscb, &end_file, &ca_file, &mng_info);
	EXPECT_INT_EQ(ret, -EACCES);
	mocker_clean();

	mocker(rs_ssl_get_cert, 10, 0);
	mocker(rs_ssl_put_certs, 10, 0);
	mocker(memset_s, 10, 0);
	ret = rs_ssl_get_ca_data(&rscb, &end_file, &ca_file, &mng_info);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	return;
}

void tc_rs_ssl_get_crl_data_1()
{
	struct rs_cb rscb;
	FILE *fp = NULL;
	struct tls_cert_mng_info mng_info;
	X509_CRL *crl = NULL;
	int ret;
	mocker(tls_get_user_config, 20, -2);
	ret = rs_ssl_get_crl_data(&rscb, fp, &mng_info, &crl);
	EXPECT_INT_EQ(-ENODEV, ret);
	return;
}

void tc_rs_ssl_get_ca_data_1()
{
	struct rs_cb rscb;
	char end_file;
	char ca_file;
	struct tls_cert_mng_info mng_info;
	int ret;
	mocker(tls_get_user_config, 20, -2);
	ret = rs_ssl_get_ca_data(&rscb, &end_file, &ca_file, &mng_info);
	EXPECT_INT_EQ(-EACCES, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_put_certs()
{
	int ret;
	struct rs_cb rscb;
	struct CertFile file_name;
	struct tls_cert_mng_info mng_info;
	struct RsCerts certs;
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM];

	mocker(rs_ssl_check_mng_and_cert_chain, 20, -1);
	ret = rs_ssl_put_certs(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	mocker(rs_ssl_check_mng_and_cert_chain, 20, 0);
	mocker(rs_ssl_put_cert_end_pem, 20, -1);
	ret = rs_ssl_put_certs(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	mocker(rs_ssl_check_mng_and_cert_chain, 20, 0);
	mocker(rs_ssl_put_cert_end_pem, 20, 0);
	mocker(rs_ssl_put_cert_ca_pem, 20, -1);
	ret = rs_ssl_put_certs(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_check_cert_chain()
{
	int ret;
	struct tls_cert_mng_info mng_info = {0};
	struct RsCerts certs = {0};
	struct tls_ca_new_certs new_certs = {{0}};

	mocker(X509_STORE_new, 20, NULL);
	ret = rs_ssl_check_cert_chain(&mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -ENOMEM);
	mocker_clean();

	mocker(X509_STORE_CTX_new, 20, NULL);
	ret = rs_ssl_check_cert_chain(&mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -ENOMEM);
	mocker_clean();

	mocker(rs_ssl_verify_cert_chain, 20, -1);
	ret = rs_ssl_check_cert_chain(&mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(rs_ssl_verify_cert_chain, 20, 0);
	ret = rs_ssl_check_cert_chain(&mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	return;
}

void tc_rs_ssl_skid_get_from_chain()
{
	struct tls_cert_mng_info mng_info;
	struct RsCerts certs;
	struct rs_cb rscb;
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};
	rscb.skidSubjectCb = NULL;

	mocker(calloc, 20, NULL);
	int ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

	mng_info.cert_count = 2;
	mocker(tls_load_cert, 20, NULL);
	ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(-22, ret);
	mocker_clean();

	mocker(rs_ssl_skids_subjects_get, 20, -1);
    mocker(memset_s, 20, -1);
	ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	return;
}

void tc_rs_ssl_verify_cert_chain()
{
	int ret;
	X509_STORE_CTX ctx;
	X509_STORE store;
	struct RsCerts certs = {0};
	struct tls_cert_mng_info mng_info = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	new_certs[0].ncert_count = 2;
	mocker(tls_load_cert, 20, NULL);
	ret = rs_ssl_verify_cert_chain(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	new_certs[0].ncert_count = 0;
	mocker(tls_load_cert, 20, NULL);
	ret = rs_ssl_verify_cert_chain(&ctx, &store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

void tc_tls_get_cert_chain()
{
	X509_STORE store;
	struct RsCerts certs;
	struct tls_cert_mng_info mng_info;
	mng_info.cert_count = 2;
	mocker(tls_load_cert, 20 ,NULL);

	int ret = tls_get_cert_chain(&store, &certs, &mng_info);
	EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();

	mocker(X509_STORE_add_cert, 20 ,0);
	ret = tls_get_cert_chain(&store, &certs, &mng_info);
	EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_get_leaf_cert()
{
	struct RsCerts certs;
	X509 *leaf_cert;
	mocker(tls_load_cert, 20, NULL);
	int ret = rs_ssl_get_leaf_cert(&certs, &leaf_cert);
	EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();
	return;
}

void tc_tls_load_cert()
{
	char inbuf;
	mocker(BIO_new_mem_buf, 20, NULL);
	int ret = tls_load_cert(&inbuf, 1, 1);
	EXPECT_INT_EQ(NULL, ret);
	mocker_clean();

	mocker(PEM_read_bio_X509, 20, NULL);
	ret = tls_load_cert(&inbuf, 1, 1);
	EXPECT_INT_EQ(NULL, ret);
	mocker_clean();

	ret = tls_load_cert(&inbuf, 1, 0);
    EXPECT_INT_EQ(NULL, ret);

	mocker(d2i_X509_bio, 20, NULL);
	ret = tls_load_cert(&inbuf, 1, 2);
	EXPECT_INT_EQ(NULL, ret);
	mocker_clean();
	return;
}

struct SSL {
    int fd;
};

void tc_rs_tls_peer_cert_verify()
{
	SSL ssl;
	int ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
	EXPECT_INT_EQ(0, ret);

	mocker(SSL_get_verify_result, 20, 1);
	ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
	EXPECT_INT_EQ(-EINVAL, ret);
	mocker_clean();

    mocker((stub_fn_t)SSL_get_verify_result, 10, X509_V_ERR_CERT_HAS_EXPIRED);
    ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();
	return;
}

void tc_rs_ssl_err_string()
{
	rs_ssl_err_string(1, 1);
	return;
}

void tc_rs_server_send_wlist_check_result()
{
	struct RsConnInfo conn = {0};
	int ret;

	gRsCb = calloc(1, sizeof(struct rs_cb));

	ret = RsServerSendWlistCheckResult(&conn, 0);
	EXPECT_INT_NE(0, ret);

	ret = RsServerSendWlistCheckResult(&conn, 1);
	EXPECT_INT_NE(0, ret);
	free(gRsCb);
	gRsCb = NULL;
}

void tc_rs_drv_ssl_bind_fd()
{
	struct RsConnInfo conn;
	RsDrvSslBindFd(&conn, 1);
	return;
}

void tc_rs_socket_fill_wlist_by_phyID()
{
	struct SocketWlistInfoT white_list_node = {0};
	struct RsConnInfo rs_conn = {0};

	mocker(RsSocketIsVnicIp, 1, 1);
	rs_conn.clientIp.family = AF_INET;
	rs_socket_fill_wlist_by_phyID(0, &white_list_node, &rs_conn);
	mocker_clean();
	return;
}

void tc_rs_get_vnic_ip()
{
	unsigned int phyId = 0;
	unsigned int vnic_ip = 0;
	int ret;

	ret = RsGetVnicIp(phyId, &vnic_ip);
	EXPECT_INT_EQ(0, ret);
}

int RsDev2rscb_stub(uint32_t dev_id, struct rs_cb **rs_cb, bool init_flag)
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
	mocker(rsGetLocalDevIDByHostDevID, 1, 0);
	mocker_invoke(RsDev2rscb, RsDev2rscb_stub, 1);
	ret = RsNotifyCfgSet(dev_id, va, size);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret = RsNotifyCfgSet(129, va, size);
	EXPECT_INT_EQ(-EINVAL, ret);

	size = 8192;
	va = 0x10000;
	mocker(rsGetLocalDevIDByHostDevID, 1, -1);
	ret = RsNotifyCfgSet(dev_id, va, size);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	ret = RsNotifyCfgSet(dev_id, va, size);
}

void tc_rs_notify_cfg_get()
{
	unsigned int dev_id = 0;
	unsigned long long va = 0;
	unsigned long long size = 0;
	int ret;
	mocker(rsGetLocalDevIDByHostDevID, 1, 0);
	mocker_invoke(RsDev2rscb, RsDev2rscb_stub, 1);
	ret = RsNotifyCfgGet(dev_id, &va, &size);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret = RsNotifyCfgGet(129, &va, &size);
	EXPECT_INT_EQ(-EINVAL, ret);

	mocker(rsGetLocalDevIDByHostDevID, 1, -1);
	ret = RsNotifyCfgGet(dev_id, &va, &size);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	ret = RsNotifyCfgGet(dev_id, &va, &size);

}

void tc_crypto_decrypt_with_aes_gcm()
{
	return;
}

void tc_rs_drv_qp_normal_fail()
{
	struct RsQpCb qp_cb = {0};
	struct ibv_qp ibQp = {0};
	int ret;
	qp_cb.ibQp = &ibQp;

	mocker(memset_s, 1, -1);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-ESAFEFUNC, ret);
	mocker_clean();

	mocker_ret((stub_fn_t)memset_s , 0, 1, 1);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-ESAFEFUNC, ret);
	mocker_clean();

	mocker(RsDrvNormalQpCreateInit, 1, -3);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-3, ret);
	mocker_clean();

	mocker(RsDrvNormalQpCreateInit, 1, 0);
	mocker(RsIbvCreateQp, 1, NULL);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

	mocker(RsDrvNormalQpCreateInit, 1, 0);
	mocker(RsIbvCreateQp, 1, 1);
	mocker(RsIbvQueryQp, 1, -1);
	mocker(RsIbvDestroyQp, 1, 0);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-EOPENSRC, ret);
	mocker_clean();

	mocker(RsDrvNormalQpCreateInit, 1, 0);
	mocker(RsIbvQueryQp, 1, 0);
	mocker(RsDrvQpInfoRelated, 1, -1);
	mocker(RsIbvDestroyQp, 1, 0);
	ret = RsDrvQpNormal(&qp_cb, 0);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	return;
}

extern void RsCloseRoceUserSo(void);
void tc_RsApiInit()
{
	RsRoceUserApiInit();
	RsCloseRoceUserSo();
	return;
}

void tc_rs_recv_wrlist()
{
	int ret;
	struct RsWrlistBaseInfo base_info = {0};
	struct RecvWrlistData wr = {0};
    unsigned int recv_num = 0;
	unsigned int complete_num = 0;
	ret = RsRecvWrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EINVAL, ret);

	recv_num = 1;
	mocker(RsQpn2qpcb, 1, -1);
	ret = RsRecvWrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EACCES, ret);
	mocker_clean();

	mocker(RsQpn2qpcb, 1, 0);
	mocker(RsDrvPostRecv, 1, 0);
	ret = RsRecvWrlist(base_info, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	return;
}

void tc_rs_drv_post_recv()
{
	int ret;
	struct RsQpCb qp_cb = {0};
	struct ibv_qp ibQp = {0};
	struct RecvWrlistData wr = {0};
	unsigned int recv_num = 0;
	unsigned int complete_num = 0;

	qp_cb.ibQp = &ibQp;

	ret = RsDrvPostRecv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-EINVAL, ret);

	recv_num = 1;
	mocker(RsIbvPostRecv, 1, 0);
	ret = RsDrvPostRecv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	mocker(RsIbvPostRecv, 1, -ENOMEM);
	ret = RsDrvPostRecv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	mocker(RsIbvPostRecv, 1, -1);
	ret = RsDrvPostRecv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	mocker(calloc, 1, NULL);
	ret = RsDrvPostRecv(&qp_cb, &wr, recv_num, &complete_num);
	EXPECT_INT_EQ(-ENOSPC, ret);
	mocker_clean();
	return;
}

void tc_rs_drv_reg_notify_mr()
{
	int ret;
	struct RsRdevCb rdevCb = {0};
	rdevCb.notifyType = NO_USE;

	ret = RsDrvRegNotifyMr(&rdevCb);
	EXPECT_INT_EQ(0, ret);

	rdevCb.notifyType = 1000;
	ret = RsDrvRegNotifyMr(&rdevCb);
	EXPECT_INT_EQ(-EINVAL, ret);

	rdevCb.notifyType = EVENTID;
	mocker(RsIbvExpRegMr, 1, NULL);
	ret = RsDrvRegNotifyMr(&rdevCb);
	EXPECT_INT_EQ(-EACCES, ret);
	mocker_clean();
	return;
}

void tc_rs_drv_query_notify_and_alloc_pd()
{
	int ret;
	struct RsRdevCb rdevCb = {0};
	rdevCb.notifyType = NOTIFY;
	rdevCb.backupInfo.backupFlag = true;

	mocker(RsOpenBackupIbCtx, 1, 0);
	mocker(RsIbvExpQueryNotify, 1, -1);
	ret = RsDrvQueryNotifyAndAllocPd(&rdevCb);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();
	return;
}

void tc_rs_send_normal_wrlist()
{
	int ret;
	struct RsQpCb qp_cb = {0};
	struct WrInfo wr_list = {0};
	unsigned int send_num = 1;
	unsigned int complete_num = 0;
	unsigned int keyFlag = 1;

	mocker(RsIbvPostSend, 1, 0);
	ret = RsSendNormalWrlist(&qp_cb, &wr_list, send_num, &complete_num, keyFlag);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	wr_list.memList.len = 0xffffffff;
	ret = RsSendNormalWrlist(&qp_cb, &wr_list, send_num, &complete_num, keyFlag);
	EXPECT_INT_EQ(-EINVAL, ret);
	return;
}

void tc_rs_drv_send_exp()
{
	struct RsQpCb qp_cb = {0};
	struct RsMrCb mr_cb = {0};
	struct RsMrCb rem_mr_cb = {0};
	struct SendWr wr = {0};
	struct SendWrRsp wr_rsp = {0};
	int ret = 0;

	mocker(RsIbvExpPostSend, 1, 0);
	qp_cb.qpMode = 2;
	ret = RsDrvSendExp(&qp_cb, &mr_cb, &rem_mr_cb, &wr, &wr_rsp);
	mocker_clean();

	mocker(RsIbvExpPostSend, 1, 0);
	qp_cb.qpMode = 1;
	ret = RsDrvSendExp(&qp_cb, &mr_cb, &rem_mr_cb, &wr, &wr_rsp);
	mocker_clean();
	return;
}

void tc_rs_drv_normal_qp_create_init()
{
	struct ibv_qp_init_attr qp_init_attr = {0};
	struct RsQpCb qp_cb = {0};
	struct ibv_port_attr attr = {0};
	struct rs_cb rs_cb = {0};
	struct RsRdevCb rdevCb = {0};
	int ret;
	qp_cb.rdevCb = &rdevCb;
	qp_cb.rdevCb->rs_cb = &rs_cb;
	qp_cb.rdevCb->rs_cb->hccpMode == NETWORK_PEER_ONLINE;

	qp_cb.txDepth = 10;
	qp_cb.rxDepth = 10;

	mocker(memset_s, 10, 0);
	qp_cb.qpMode = 2;
	ret = RsDrvNormalQpCreateInit(&qp_init_attr, &qp_cb, &attr);
	qp_cb.rdevCb->rs_cb->hccpMode == NETWORK_OFFLINE;
	ret = RsDrvNormalQpCreateInit(&qp_init_attr, &qp_cb, &attr);
	mocker_clean();

	return;
}

void tc_rs_register_mr()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	unsigned int qpn = 0;

	struct RsInitConfig cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;

	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct RdmaMrRegInfo mr_reg_info1 = {0};
	mr_reg_info1.addr = 0xabcdef;
	mr_reg_info1.len = RS_TEST_MEM_SIZE;
	mr_reg_info1.access = RS_ACCESS_LOCAL_WRITE;

	gRsCb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = gRsCb;

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	void *mr_handle = NULL;
	void *mr_handle1 = NULL;
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret =  RsRegisterMr(phyId, rdevIndex, &mr_reg_info, &mr_handle);
	EXPECT_INT_EQ(0, ret);

	ret =  RsRegisterMr(1, rdevIndex, &mr_reg_info, &mr_handle);
	EXPECT_INT_NE(0, ret);

	mocker(RsDrvMrReg, 1, NULL);
	ret =  RsRegisterMr(phyId, rdevIndex, &mr_reg_info1, &mr_handle1);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret =  RsDeregisterMr(mr_handle);
	EXPECT_INT_EQ(0, ret);

	ret =  RsDeregisterMr(NULL);
	EXPECT_INT_NE(0, ret);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}

void tc_rs_epoll_ctl_add()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;

    mocker((stub_fn_t)calloc, 5, NULL);
    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -ENOMEM);
    mocker_clean();

    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -1);

    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLERR);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_add_01()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;

    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_add_02()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;
    gRsCb->heterogTcpFdList.next = &(gRsCb->heterogTcpFdList);
    gRsCb->heterogTcpFdList.prev = &(gRsCb->heterogTcpFdList);

    mocker((stub_fn_t)RsEpollCtl, 5, 0);
    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
    ret = RsEpollCtlDel(0);
    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_add_03()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    RS_INIT_LIST_HEAD(&gRsCb->heterogTcpFdList);
    gRsCb->heterogTcpFdList.next = &(gRsCb->heterogTcpFdList);
    gRsCb->heterogTcpFdList.prev = &(gRsCb->heterogTcpFdList);

    mocker((stub_fn_t)RsEpollCtl, 5, -1);
    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;

    ret = RsEpollCtlMod((const void *)fd_handle, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(ret, -1);

    ret = RsEpollCtlMod((const void *)fd_handle, RA_EPOLLERR);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_01()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;

    ret = RsEpollCtlMod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -1);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_02()
{
    struct SocketPeerInfo fd_handle[1];
    struct RsConnCb connCb = {0};
    int ret;
    struct RsListHead list = {0};

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->fdMap = (const void **)&fd_handle;
    gRsCb->connCb = connCb;
    gRsCb->heterogTcpFdList = list;

    mocker((stub_fn_t)RsEpollCtl, 5, 0);
    ret = RsEpollCtlMod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_mod_03()
{
    struct SocketPeerInfo fd_handle[1];
    int ret;

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    gRsCb = NULL;

    mocker((stub_fn_t)RsEpollCtl, 5, 0);
    ret = RsEpollCtlAdd((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -22);

    gRsCb = NULL;

    ret = RsEpollCtlMod((const void *)fd_handle, RA_EPOLLIN);
    EXPECT_INT_EQ(ret, -22);

    gRsCb = NULL;
    ret = RsEpollCtlDel(0);
    EXPECT_INT_EQ(ret, -22);

    RsSetCtx(0);
    mocker_clean();
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_del()
{
    int ret;
    struct RsListHead list = {0};

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));

    gRsCb->heterogTcpFdList = list;
    gRsCb->heterogTcpFdList.next = &(gRsCb->heterogTcpFdList);
    gRsCb->heterogTcpFdList.prev = &(gRsCb->heterogTcpFdList);
    ret = RsEpollCtlDel(0);
    EXPECT_INT_EQ(ret, -1);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_ctl_del_01()
{
    int ret;
    struct RsListHead list = {0};

    RS_INIT_LIST_HEAD(&list);
    gRsCb = malloc(sizeof(struct rs_cb));

    gRsCb->heterogTcpFdList = list;
    gRsCb->heterogTcpFdList.next = &(gRsCb->heterogTcpFdList);
    gRsCb->heterogTcpFdList.prev = &(gRsCb->heterogTcpFdList);

    mocker((stub_fn_t)RsEpollCtl, 5, 0);
    ret = RsEpollCtlDel(0);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_set_tcp_recv_callback()
{
	(void)RsSetTcpRecvCallback(NULL);

    gRsCb = malloc(sizeof(struct rs_cb));
    gRsCb->hccpMode = 1;

    (void)RsSetTcpRecvCallback(NULL);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_event_in_handle()
{
    int ret;
    struct rs_cb rs_cb = {0};
    struct epoll_event events = {0};
    rs_cb.sslEnable = 1;

    mocker((stub_fn_t)RsEpollEventListenInHandle, 1, -ENODEV);
    mocker((stub_fn_t)RsEpollEventSslAcceptInHandle, 5, 0);
    (void)RsEpollEventInHandle(&rs_cb, &events);
    mocker_clean();

    return;
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
    struct RsConnCb connCb;
    struct SocketListenInfo conn;
    struct RsListenInfo listen_info;
    conn.localIp.addr.s_addr = 1;

	ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(-ESYSFUNC, ret);

	mocker(setsockopt, 20, 0);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

	mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

    mocker(setsockopt, 20, 0);
    mocker(bind, 20, 1);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
	mocker_clean();

	mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
    mocker(listen, 20, 1);
	mocker_invoke(__errno_location, stub__errno_location, 1);
    ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EAGAIN, ret);
    mocker_clean();

	mocker(setsockopt, 20, 0);
    mocker(bind, 20, 0);
    mocker(listen, 20, 1);
	mocker_invoke(__errno_location, stub__errno_locations, 1);
    ret = RsSocketListenBindListen(-1, &connCb, &conn, &listen_info, 0);
    EXPECT_INT_EQ(EADDRINUSE, ret);
    mocker_clean();
}

void tc_rs_epoll_event_in_handle_01()
{
    int ret;
    struct rs_cb rs_cb = {0};
    struct epoll_event events = {0};
    rs_cb.sslEnable = 0;

    mocker((stub_fn_t)RsEpollEventListenInHandle, 1, -ENODEV);
    mocker((stub_fn_t)RsEpollEventQpMrInHandle, 1, -ENODEV);
    mocker((stub_fn_t)RsEpollEventHeterogTcpRecvInHandle, 5, 0);
    (void)RsEpollEventInHandle(&rs_cb, &events);
    mocker_clean();

    return;
}

void tc_rs_epoll_tcp_recv()
{
    int ret;
    struct rs_cb rs_cb = {0};
    struct SocketPeerInfo fd_handle[1];
    int callback = 0;

    fd_handle[0].phyId = 0;
    fd_handle[0].fd = 0;

    gRsCb = malloc(sizeof(struct rs_cb));
    rs_cb.fdMap = (const void **)&fd_handle;

    rs_cb.tcpRecvCallback = RsSetTcpRecvCallback;
    ret =RsEpollTcpRecv(&rs_cb, 0);
    EXPECT_INT_EQ(ret, 0);

    rs_cb.tcpRecvCallback = NULL;
    ret =RsEpollTcpRecv(&rs_cb, 0);
    EXPECT_INT_EQ(ret, -EINVAL);

    free(gRsCb);
    gRsCb = NULL;
    return;
}

void tc_rs_epoll_event_ssl_accept_in_handle()
{
    int ret;
    struct RsListHead list = {0};
    struct rs_cb *rs_cb = NULL;
    struct RsConnCb connCb = {0};

    RS_INIT_LIST_HEAD(&list);

    rs_cb = malloc(sizeof(struct rs_cb));
    rs_cb->connCb = connCb;
    rs_cb->connCb.serverAcceptList = list;
    rs_cb->connCb.serverAcceptList.next = &(rs_cb->connCb.serverAcceptList);
    rs_cb->connCb.serverAcceptList.prev = &(rs_cb->connCb.serverAcceptList);
    ret = RsEpollEventSslAcceptInHandle(rs_cb, 0);
    EXPECT_INT_EQ(ret, -ENODEV);

    free(rs_cb);
    rs_cb = NULL;
    return;
}
struct RsMrCb *g_mr_cb;
struct ibv_mr *g_ib_mr;

struct RsMrCb *g_mr_cb;
struct ibv_mr *g_ib_mr;

int stub_rs_get_mrcb(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb,
    struct RsListHead *mr_list)
{
	*mr_cb = g_mr_cb;
	return 0;
}

int stub_RsIbvExpPostSend(struct ibv_qp *qp, struct ibv_send_wr *wr, struct ibv_send_wr **bad_wr,
    struct wr_exp_rsp *exp_rsp)
{
	int wqe_index = 2;
	exp_rsp->wqe_index = wqe_index;
	exp_rsp->db_info = 1;
	return 0;
}

void tc_rs_send_exp_wrlist()
{
	DlHalInit();
	struct RsQpCb qp_cb = {0};
	struct WrInfo wrlist[1];
	unsigned int send_num = 1;
	struct SendWrRsp rs_wr_info[1];
	unsigned int complete_num = 0;
	struct DbInfo db = {0};
	struct ibv_qp ibQp = {0};
	struct WqeInfoT wqeTmp = {0};
	struct SendWrRsp wr_rsp = {0};
	struct wr_exp_rsp exp_rsp = {0};

	int ret;

	g_mr_cb = malloc(sizeof(struct RsMrCb));
	g_ib_mr = malloc(sizeof(struct ibv_mr));
	g_mr_cb->ibMr = g_ib_mr;

	wrlist[0].memList.len = RS_TEST_MEM_SIZE;
	wrlist[0].memList.addr = 0x15;
	wrlist[0].op = 1;
	qp_cb.sendWrNum = 0;
	ibQp.qp_num = 1;
	qp_cb.ibQp= &ibQp;
	qp_cb.sqIndex = 1;
	mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb, 2);
	mocker_invoke(RsIbvExpPostSend, stub_RsIbvExpPostSend, 1);
	qp_cb.qpMode = 3;
	rs_wr_info[0].db = db;
	rs_wr_info[0].wqeTmp = wqeTmp;
	ret = RsSendExpWrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
	mocker_clean();

    mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb, 2);
    mocker(RsIbvExpPostSend, 1, -12);
    ret = RsSendExpWrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb, 2);
    mocker(RsIbvExpPostSend, 1, -1);
    ret = RsSendExpWrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

	mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb, 2);
	mocker_invoke(RsIbvExpPostSend, stub_RsIbvExpPostSend, 1);
	wrlist[0].op = 0xf6;
	ret = RsSendExpWrlist(&qp_cb, &wrlist, send_num, &rs_wr_info, &complete_num);
	mocker_clean();

	free(g_ib_mr);
	free(g_mr_cb);

	EXPECT_INT_EQ(ret, 0);

	DlHalDeinit();
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
	uint32_t qpMode = 1;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int i;
	struct RsInitConfig cfg = {0};
    struct RsQpCb *qp_cb = NULL;
	struct ibv_cq *ib_send_cq_t, *ib_recv_cq_t;
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
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

    ret = RsQpn2qpcb(0, rdevIndex, resp.qpn, &qp_cb);

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

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, 0);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, -1);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	qp_cb->ibSendCq = ib_send_cq_t;

	qp_cb->rdevCb->rs_cb->hccpMode = NETWORK_PEER_ONLINE;
	struct RsCqContext srq_context = {0};
	qp_cb->srqContext = &srq_context;
	mocker((stub_fn_t)RsIbvGetCqEvent, 10, 0);
	RsDrvPollSrqCqHandle(qp_cb);
	mocker_clean();

	ret = RsQpDestroy(dev_id, rdevIndex, resp.qpn);

	ret = RsRdevDeinit(dev_id, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_qp_create_with_attrs_v2()
{
	int ret;
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	struct RsQpNormWithAttrs  qp_norm = {0};
	struct RsQpRespWithAttrs qp_resp = {0};
	qp_norm.isExp = 1;

	ret = RsQpCreateWithAttrs(15, rdevIndex, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, NULL);
	EXPECT_INT_NE(ret, 0);
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, NULL, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.extAttrs.version = -1;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
	qp_norm.extAttrs.qpMode = -1;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
	qp_norm.extAttrs.qpMode = RA_RS_OP_QP_MODE_EXT;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp);
	EXPECT_INT_NE(ret, 0);
}

void tc_rs_qp_create_with_attrs()
{
	tc_rs_qp_create_with_attrs_v1();
	tc_rs_qp_create_with_attrs_v2();
}

void tc_rs_normal_qp_create()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	int qpMode = 1;
	uint32_t qpn, qpn2, qpn3;
	int i;
	int try_num = 10;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_PEER_ONLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

	rs_ut_msg("___________________after listen:\n");
    strcpy(white_list.tag, "1234");
	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	rs_ut_msg("___________________after connect:\n");

	usleep(SLEEP_TIME);

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

    struct ibv_cq *ibSendCq;
    struct ibv_cq *ibRecvCq;
    void* context;
    struct CqAttr attr;
    attr.qpContext = &context;
    attr.ibSendCq = &ibSendCq;
    attr.ibRecvCq = &ibRecvCq;
    attr.sendCqDepth = 16384;
    attr.recvCqDepth = 16384;
    attr.sendCqEventId = 1;
    attr.recvCqEventId = 2;
	attr.sendChannel = NULL;
	attr.recvChannel = NULL;
    ret = RsCqCreate(phyId, rdevIndex, &attr);
    EXPECT_INT_EQ(ret, 0);
	struct RsQpResp qp_resp = {0};
	struct RsQpResp qp_resp2 = {0};

    struct ibv_qp_init_attr qp_init_attr;
    qp_init_attr.qp_context = context;
    qp_init_attr.send_cq = ibSendCq;
    qp_init_attr.recv_cq = ibRecvCq;
    qp_init_attr.qp_type = 2;
    qp_init_attr.cap.max_inline_data = 32;
    qp_init_attr.cap.max_send_wr = 4096;
    qp_init_attr.cap.max_send_sge = 4096;
    qp_init_attr.cap.max_recv_wr = 4096;
    qp_init_attr.cap.max_recv_sge = 1;
    struct ibv_qp* qp;

    mocker((stub_fn_t)RsDrvNormalQpCreate, 10, -ENOMEM);
    ret = RsNormalQpCreate(phyId, rdevIndex, &qp_init_attr, &qp_resp, &qp);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    ret = RsNormalQpCreate(phyId, rdevIndex, &qp_init_attr, &qp_resp, &qp);
    EXPECT_INT_EQ(0, ret);

    qp_init_attr.qp_context = NULL;
    ret = RsNormalQpCreate(phyId, rdevIndex, &qp_init_attr, &qp_resp, &qp);
    EXPECT_INT_EQ(-22, ret);

	rs_ut_msg("___________________after qp create:\n");

	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = 0xabcdef;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;
	ret = RsMrReg(phyId, rdevIndex, qp_resp.qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0);
	/* >>>>>>> RsQpConnectAsync test case end <<<<<<<<<<< */

	rs_ut_msg("___________________after mr reg:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, qp_resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	rs_ut_msg("___________________after qp connect async:\n");
	usleep(SLEEP_TIME);
	rs_ut_msg("___________________after qp connect async & sleep:\n");

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
		usleep(30000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

    struct ibv_cq *ib_send_cq2;
    struct ibv_cq *ib_recv_cq2;
    void* context2;
    struct CqAttr attr2;
    attr2.qpContext = &context2;
    attr2.ibSendCq = &ib_send_cq2;
    attr2.ibRecvCq = &ib_recv_cq2;
    attr2.sendCqDepth = 16384;
    attr2.recvCqDepth = 16384;
    attr2.sendCqEventId = -1;
    attr2.recvCqEventId = 0;
	attr2.sendChannel = NULL;
	attr2.recvChannel = NULL;
    ret = RsCqCreate(phyId, rdevIndex, &attr2);
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
	struct ibv_qp* qp2;
    ret = RsNormalQpCreate(phyId, rdevIndex, &qp_init_attr2, &qp_resp2, &qp2);
    EXPECT_INT_EQ(0, ret);

	rs_ut_msg("___________________after qp2 create:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, qp_resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");

	struct ibv_cq *ib_send_cq3;
    struct ibv_cq *ib_recv_cq3;
    void* context3;
    struct CqAttr attr3;
    attr3.qpContext = &context3;
    attr3.ibSendCq = &ib_send_cq3;
    attr3.ibRecvCq = &ib_send_cq3;
    attr3.sendCqDepth = 16384;
    attr3.recvCqDepth = 16384;
    attr3.sendCqEventId = 1;
    attr3.recvCqEventId = 2;
	attr3.sendChannel = (void*)0xabcd;
	attr3.recvChannel = (void*)0xabcd;

    ret = RsCqCreate(phyId, rdevIndex, &attr3);
    EXPECT_INT_EQ(ret, 0);

	struct ibv_cq *ib_send_cq4;
    struct ibv_cq *ib_recv_cq4;
    void* context4;
    struct CqAttr attr4;
    attr4.qpContext = &context4;
    attr4.ibSendCq = &ib_send_cq4;
    attr4.ibRecvCq = &ib_send_cq4;
    attr4.sendCqDepth = 16384;
    attr4.recvCqDepth = 16384;
    attr4.sendCqEventId = 1;
    attr4.recvCqEventId = 2;
	attr4.sendChannel = NULL;
	attr4.recvChannel = (void*)0xabcd;

    ret = RsCqCreate(phyId, rdevIndex, &attr4);
    EXPECT_INT_EQ(ret, -1);

	usleep(SLEEP_TIME);

	ret = RsNormalQpDestroy(phyId, rdevIndex, qp_resp2.qpn);
	ret = RsNormalQpDestroy(phyId, rdevIndex, qp_resp.qpn);

	rs_ut_msg("___________________after qp1&2 destroy:\n");

	ret = RsCqDestroy(phyId, rdevIndex, &attr3);
	ret = RsCqDestroy(phyId, rdevIndex, &attr2);
	ret = RsCqDestroy(phyId, rdevIndex, &attr);

	rs_ut_msg("___________________after cq1&2 destroy:\n");

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	rs_ut_msg("___________________after close socket 0:\n");

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);

	rs_ut_msg("___________________after close socket 1:\n");

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	rs_ut_msg("___________________after stop listen:\n");

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("___________________after deinit:\n");

    return;
}

void tc_rs_query_event()
{
	int ret;
	int event_id = 1;
	struct event_summary *sendEvent;

    mocker((stub_fn_t)calloc, 10, NULL);
	ret = RsQueryEvent(event_id, &sendEvent);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();
}

void tc_rs_create_cq()
{
	int ret;

	struct RsRdevCb rdevCb;
	struct RsCqContext cq_context = {0};
	struct CqAttr attr = {0};
	cq_context.rdevCb = &rdevCb;
	cq_context.cqCreateMode = RS_NORMAL_CQ_CREATE;

    mocker((stub_fn_t)RsQueryEvent, 10, -1);
	ret = RsDrvCreateCqEvent(&cq_context, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker((stub_fn_t)RsIbvCreateCq, 10, NULL);
	ret = RsDrvCreateCqEvent(&cq_context, &attr);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

    mocker((stub_fn_t)ibv_req_notify_cq, 10, -1);
	ret = RsDrvCreateCqEvent(&cq_context, &attr);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

	cq_context.recvEvent = NULL;
	cq_context.sendEvent = NULL;
    mocker((stub_fn_t)RsIbvDestroyCq, 10, -1);
	ret = RsDrvDestroyCqEvent(&cq_context);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

}

void tc_rs_create_normal_qp()
{
	int ret;
	struct RsQpCb qp_cb;
	struct ibv_qp_init_attr qp_init_attr;

    mocker((stub_fn_t)RsIbvCreateQp, 10, NULL);
	ret = RsDrvNormalQpCreate(&qp_cb, &qp_init_attr);
    EXPECT_INT_EQ(-ENOMEM, ret);
    mocker_clean();

    mocker((stub_fn_t)RsIbvQueryQp, 10, -1);
	mocker((stub_fn_t)RsIbvCreateQp, 10, 1);
	mocker((stub_fn_t)RsIbvDestroyQp, 10, 0);
	ret = RsDrvNormalQpCreate(&qp_cb, &qp_init_attr);
    EXPECT_INT_EQ(-EOPENSRC, ret);
    mocker_clean();

    mocker((stub_fn_t)RsIbvQueryQp, 10, 0);
	mocker((stub_fn_t)RsIbvCreateQp, 10, 1);
	mocker((stub_fn_t)RsIbvDestroyQp, 10, 0);
    mocker((stub_fn_t)RsDrvQpInfoRelated, 10, -1);
	ret = RsDrvNormalQpCreate(&qp_cb, &qp_init_attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_rs_create_comp_channel()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;

	struct RsInitConfig cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;

	gRsCb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = gRsCb;

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	void *comp_channel = NULL;
	void *comp_channel1 = NULL;
	ret =  RsCreateCompChannel(phyId, rdevIndex, &comp_channel);
	EXPECT_INT_EQ(0, ret);

	mocker(rsGetLocalDevIDByHostDevID, 1, -19);
	ret =  RsCreateCompChannel(phyId, rdevIndex, &comp_channel1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(RsRdev2rdevCb, 1, -19);
	ret =  RsCreateCompChannel(phyId, rdevIndex, &comp_channel1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(RsIbvCreateCompChannel, 1, NULL);
	ret =  RsCreateCompChannel(phyId, rdevIndex, &comp_channel1);
	EXPECT_INT_EQ(-259, ret);
	mocker_clean();

	mocker(RsIbvDestroyCompChannel, 1, -1);
	ret =  RsDestroyCompChannel(comp_channel1);
	EXPECT_INT_EQ(-1, ret);
	mocker_clean();

	ret =  RsDestroyCompChannel(comp_channel);
	EXPECT_INT_EQ(0, ret);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}

void tc_rs_get_cqe_err_info()
{
	int ret;
    struct RsCqeErrInfo *err_info = &gRsCqeErr;
    struct CqeErrInfo *temp_info = &err_info->info;
    struct CqeErrInfo cqe_info;

	temp_info->status = 0;
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
	mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
    ret = RsDrvGetCqeErrInfo(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)memcpy_s, 1, 1);
    ret = RsDrvGetCqeErrInfo(&cqe_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)memset_s, 1, 1);
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
    mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
	ret = RsDrvGetCqeErrInfo(&cqe_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();

	temp_info->status = 1;
    mocker((stub_fn_t)pthread_mutex_lock, 1, 0);
    mocker((stub_fn_t)pthread_mutex_unlock, 1, 0);
	ret = RsDrvGetCqeErrInfo(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker((stub_fn_t)RsDrvGetCqeErrInfo, 1, 0);
    ret = RsGetCqeErrInfo(&cqe_info);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_get_cqe_err_info_num()
{
    unsigned int num = 0;
    unsigned int phyId;
    int ret;

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)RsRdev2rdevCb, 10, 0);
    phyId = 128;
    ret = RsGetCqeErrInfoNum(phyId, 0, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)RsRdev2rdevCb, 10, 0);
    phyId = 0;
    ret = RsGetCqeErrInfoNum(0, 0, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    return;
}

int stub_RsRdev2rdevCb(unsigned int chipId, unsigned int rdevIndex, struct RsRdevCb **rdevCb)
{
    *rdevCb = &g_rdev_cb;
    return 0;
}

void tc_rs_get_cqe_err_info_list()
{
    struct CqeErrInfo info = {0};
    struct RsQpCb qp_cb = {0};
    unsigned int num = 1;
    unsigned int phyId;
    int ret;

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker((stub_fn_t)RsRdev2rdevCb, 10, 0);
    phyId = 128;
    ret = RsGetCqeErrInfoList(phyId, 0, &info, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    phyId = 0;
    RS_INIT_LIST_HEAD(&g_rdev_cb.qpList);
    ret = RsGetCqeErrInfoList(phyId, 0, &info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    RsListAddTail(&qp_cb.list, &g_rdev_cb.qpList);
    qp_cb.rdevCb = &g_rdev_cb;
    qp_cb.cqeErrInfo.info.status = 1;
    mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 10, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    ret = RsGetCqeErrInfoList(phyId, 0, &info, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    return;
}

void tc_rs_save_cqe_err_info()
{
	int ret;
    struct RsCqeErrInfo *err_info = &gRsCqeErr;
    struct CqeErrInfo *temp_info = &err_info->info;
	struct RsQpCb qp_cb;

	temp_info->status = 0;
    RsDrvSaveCqeErrInfo(0x15, &qp_cb);
    mocker_clean();

	temp_info->status = 1;
    RsDrvSaveCqeErrInfo(0x15, &qp_cb);
    mocker_clean();
}

void tc_rs_cqe_callback_process()
{
	struct RsQpCb qpcb_tmp = {0};
	struct ibv_wc wc = {0};
	struct ibv_cq ev_cq_sq = {0};
	struct ibv_cq ev_cq_rq = {0};
	struct RsRdevCb rdevCb = {0};

	qpcb_tmp.ibSendCq = &ev_cq_sq;
	qpcb_tmp.ibRecvCq = &ev_cq_rq;
	qpcb_tmp.rdevCb = &rdevCb;

	wc.status = IBV_WC_MW_BIND_ERR;
    mocker((stub_fn_t)RsDrvSaveCqeErrInfo, 1, 0);
	RsCqeCallbackProcess(&qpcb_tmp, &wc, ev_cq_rq);
    mocker_clean();
}

void tc_rs_create_srq()
{
	int ret;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;

	struct RsInitConfig cfg = {0};

	rs_ut_msg("resource prepare begin..................\n");

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;

	gRsCb = malloc(sizeof(struct rs_cb));
	struct rs_cb *temPtr = gRsCb;

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RS INIT, ret:%d !\n", ret);

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct SrqAttr attr = {0};
 	struct ibv_srq *ib_srq = NULL;
    struct ibv_cq *ibRecvCq = NULL;
	void *context = NULL;
    attr.ibSrq = &ib_srq;
    attr.ibRecvCq = &ibRecvCq;
    attr.maxSge = 1;
    attr.context = &context;
    attr.srqEventId = 1;
    attr.srqDepth = 63;
    attr.cqDepth = 64;
	struct SrqAttr attr1 = {0};
	struct ibv_srq *ib_srq1 = 0xab;
    struct ibv_cq *ib_recv_cq1 = 0xab;
	struct RsCqContext cq_context1 = {0};
	cq_context1.ibSrqCq =  &ib_recv_cq1;
    attr1.ibSrq = &ib_srq1;
    attr1.ibRecvCq = &ib_recv_cq1;
    attr1.maxSge = 1;
	void *context1 = &cq_context1;
    attr1.context = &context1;
    attr1.srqEventId = 1;
    attr1.srqDepth = 63;
    attr1.cqDepth = 64;

	mocker(rsGetLocalDevIDByHostDevID, 1, -19);
	ret =  RsCreateSrq(phyId, rdevIndex, &attr1);
	EXPECT_INT_EQ(-19, ret);
	mocker_clean();

	mocker(RsIbvCreateCompChannel, 1, NULL);
	ret =  RsCreateSrq(phyId, rdevIndex, &attr1);
	EXPECT_INT_EQ(-22, ret);
	mocker_clean();

	mocker(calloc, 1, NULL);
	ret =  RsCreateSrq(phyId, rdevIndex, &attr1);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();

	mocker(RsIbvCreateSrq, 1, NULL);
	ret =  RsCreateSrq(phyId, rdevIndex, &attr1);
	EXPECT_INT_EQ(-EOPENSRC, ret);
	mocker_clean();

	ret =  RsCreateSrq(phyId, rdevIndex, &attr);
	EXPECT_INT_EQ(0, ret);

	struct ibv_cq *ibSendCq;
    struct ibv_cq *ib_recv_cq3;
    void* context2;
    struct CqAttr attr2;
    attr2.qpContext = &context2;
    attr2.ibSendCq = &ibSendCq;
    attr2.ibRecvCq = &ibRecvCq;
    attr2.sendCqDepth = 16384;
    attr2.recvCqDepth = 16384;
    attr2.sendCqEventId = 1;
    attr2.recvCqEventId = 2;
	attr2.sendChannel = NULL;
	attr2.recvChannel = NULL;

	mocker(RsEpollCtl, 5, 0);
    ret = RsCqCreate(phyId, rdevIndex, &attr2);
    EXPECT_INT_EQ(ret, 0);

	ret = RsCqDestroy(phyId, rdevIndex, &attr2);
    EXPECT_INT_EQ(ret, 0);

	mocker(RsIbvAckCqEvents, 1, NULL);
	ret =  RsDestroySrq(phyId, rdevIndex, &attr);
	EXPECT_INT_EQ(0, ret);
	mocker_clean();

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(temPtr);
	return;
}

void tc_rs_get_ipv6_scope_id()
{
	int ret;
	struct in6_addr localIp;

	ret = RsGetIpv6ScopeId(localIp);
	EXPECT_INT_EQ(-EINVAL, ret);

	mocker(getifaddrs, 1, -1);
	ret = RsGetIpv6ScopeId(localIp);
	EXPECT_INT_EQ(-ESYSFUNC, ret);
}

void tc_rs_create_event_handle()
{
	int ret;
	int fd;

	ret = RsCreateEventHandle(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCreateEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = RsDestroyEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_ctl_event_handle()
{
	struct SocketPeerInfo fd_handle[1];
	int ret;
	int fd;

	ret = RsCtlEventHandle(-1, NULL, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCreateEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = RsCtlEventHandle(fd, NULL, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	fd_handle[0].phyId = 0;
	fd_handle[0].fd = 0;
	ret = RsCtlEventHandle(fd, (const void *)fd_handle, 0, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCtlEventHandle(fd, (const void *)fd_handle, EPOLL_CTL_ADD, 100);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCtlEventHandle(fd, (const void *)fd_handle, EPOLL_CTL_ADD, RA_EPOLLONESHOT);

	ret = RsDestroyEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_wait_event_handle()
{
	struct SocketEventInfoT event_info;
	unsigned int events_num = 0;
	int ret;
	int fd;

	ret = RsWaitEventHandle(-1, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCreateEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = RsWaitEventHandle(fd, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsWaitEventHandle(fd, &event_info, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsWaitEventHandle(fd, &event_info, 0, 1, &events_num);
	EXPECT_INT_EQ(0, ret);

	ret = RsDestroyEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_destroy_event_handle()
{
	int ret;

	ret = RsDestroyEventHandle(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_epoll_create_epollfd()
{
	int ret;

	ret = RsEpollCreateEpollfd(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);
}

void tc_rs_epoll_destroy_fd()
{
	int fd;
	int ret;

	ret = RsEpollDestroyFd(NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsEpollCreateEpollfd(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = RsEpollDestroyFd(&fd);
	EXPECT_INT_EQ(-1, fd);
	EXPECT_INT_EQ(0, ret);
}

void tc_rs_epoll_wait_handle()
{
	int fd;
	int ret;

	ret = RsEpollWaitHandle(-1, NULL, 0, -1, 0);
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
    struct LiteMemAttrResp mem_resp = {0};
    struct RsRdevCb rdevCb = {0};
    struct RsQpCb qp_cb = {0};
    struct rs_cb rs_cb = {0};
    int ret;

    qp_cb.qpMode = RA_RS_OP_QP_MODE;
    qp_cb.memAlign = LITE_ALIGN_4KB;
    ret = RsInitMemPool(&qp_cb);
    EXPECT_INT_EQ(ret, 0);
    RsDeinitMemPool(&qp_cb);

    qp_cb.memAlign = LITE_ALIGN_2MB;
    qp_cb.memResp = mem_resp;
    qp_cb.rdevCb = &rdevCb;
    rdevCb.rs_cb = &rs_cb;
    mocker(RsRoceInitMemPool, 100, -EINVAL);
    ret = RsInitMemPool(&qp_cb);
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
    mocker(RsRoceDeinitMemPool, 100, -EINVAL);
    RsDeinitMemPool(&qp_cb);
    mocker_clean();
}

void tc_rs_get_vnic_ip_info()
{
	int ret;
	unsigned int ids[1] = {0};
	struct IpInfo info[1] = {0};

	DlHalInit();

	ret = RsGetVnicIpInfos(0, 0, NULL, 0, NULL);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetVnicIpInfos(0, 0, ids, 1, NULL);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetVnicIpInfos(0, 2, ids, 1, info);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetVnicIpInfo(0, 0, 0, &info[0]);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetVnicIpInfo(0, 0, 1, &info[0]);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetVnicIpInfo(0, 0, 2, &info[0]);
	EXPECT_INT_NE(ret, 0);

	DlHalDeinit();
}

void tc_rs_typical_register_mr()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	void *addr;
	struct RdmaMrRegInfo mr_reg_info = {0};
	struct ibv_mr *ra_rs_mr_handle = NULL;
	struct RsInitConfig cfg = {0};

	addr = malloc(RS_TEST_MEM_SIZE);
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsTypicalRegisterMrV1(phyId, rdevIndex, &mr_reg_info, &ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = RsTypicalDeregisterMr(phyId, rdevIndex, (uint64_t)addr);
	EXPECT_INT_EQ(ret, 0);

	ret = RsTypicalRegisterMr(phyId, rdevIndex, &mr_reg_info, &ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = RsTypicalDeregisterMr(phyId, rdevIndex, (uint64_t)ra_rs_mr_handle);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
}

int stub_RsIbvQueryQp_init(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	if (attr == NULL) {
		return -EINVAL;
	}
	attr->qp_state = 1;
	return 0;
}

void tc_rs_typical_qp_modify()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	void *addr;
	struct RdmaMrRegInfo mr_reg_info = {0};
	struct ibv_mr *ra_rs_mr_handle = NULL;
	struct RsInitConfig cfg = {0};

	addr = malloc(RS_TEST_MEM_SIZE);
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	struct TypicalQp local_qp_info = {0};
	struct TypicalQp remote_qp_info = {0};
	unsigned int udp_sport;

	struct RsQpNorm qp_norm = {0};
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
	int flag = 0; /* RC */
	int qpMode = 4;
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	qp_norm.isExt = 1;
	int batch_modify_qpn[2];

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);

	local_qp_info.qpn = resp.qpn;
	local_qp_info.psn = resp.psn;
	local_qp_info.gidIdx = resp.gidIdx;
	(void)memcpy_s(local_qp_info.gid, HCCP_GID_RAW_LEN, resp.gid.raw, HCCP_GID_RAW_LEN);
	remote_qp_info.qpn = resp2.qpn;
	remote_qp_info.psn = resp2.psn;
	remote_qp_info.gidIdx = resp2.gidIdx;
	(void)memcpy_s(remote_qp_info.gid, HCCP_GID_RAW_LEN, resp2.gid.raw, HCCP_GID_RAW_LEN);

	mocker_invoke(RsIbvQueryQp, stub_RsIbvQueryQp_init, 10);
    mocker(RsRoceQueryQpc, 10, 1);
	ret = RsTypicalQpModify(phyId, rdevIndex, local_qp_info, remote_qp_info, &udp_sport);
	EXPECT_INT_EQ(ret, 0);
	ret = RsTypicalQpModify(phyId, rdevIndex, remote_qp_info, local_qp_info, &udp_sport);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	batch_modify_qpn[0] = resp.qpn;
	batch_modify_qpn[1] = resp2.qpn;
 	ret = RsQpBatchModify(phyId, rdevIndex, 5, batch_modify_qpn, 2);
	EXPECT_INT_EQ(ret, 0);

 	ret = RsQpBatchModify(phyId, rdevIndex, 1, batch_modify_qpn, 2);
	EXPECT_INT_EQ(ret, 0);

 	ret = RsQpBatchModify(phyId, rdevIndex, 1, batch_modify_qpn, 2);
	EXPECT_INT_NE(ret, 0);

	ret = RsQpDestroy(phyId, rdevIndex, resp.qpn);
	EXPECT_INT_EQ(ret, 0);
	ret = RsQpDestroy(phyId, rdevIndex, resp2.qpn);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	free(addr);
}

void tc_rs_ssl_get_cert() {
	int ret;
	struct tls_cert_mng_info mng_info = {0};
	struct rs_cb rscb = {0};
    struct RsCertSkidSubjectCb skidSubjectCb = {0};
	struct RsCerts certs = {0};
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
	rscb.hccpMode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret(tls_get_user_config, 0, -2, -1);
	rscb.hccpMode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret(tls_get_user_config, 0, -1, -1);
	rscb.hccpMode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -1, 0);
	rscb.hccpMode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -2, -1);
	rscb.hccpMode = NETWORK_OFFLINE;
	ret = rs_ssl_get_cert(&rscb, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker_ret_2(tls_get_user_config, 0, 0, 0, -2, -2);
	rscb.hccpMode = NETWORK_OFFLINE;
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
	struct RsCerts certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	mocker(tls_get_cert_chain, 10, -1);
	ret = rs_ssl_x509_store_init(&store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(tls_get_cert_chain, 10, 0);
	ret = rs_ssl_x509_store_init(&store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	new_certs[0].ncert_count = 1;
	strcpy(new_certs[0].certs[0].ncert_info ,"pub cert");
	ret = rs_ssl_x509_store_init(&store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	new_certs[0].ncert_count = 2;
	strcpy(new_certs[0].certs[1].ncert_info ,"root ca cert");
	mocker(tls_load_cert, 10, NULL);
	ret = rs_ssl_x509_store_init(&store, &certs, &mng_info, &new_certs);
	EXPECT_INT_EQ(ret, -22);
	mocker_clean();

	return;
}

void tc_rs_ssl_skids_subjects_get()
{
	int ret;
	struct tls_cert_mng_info mng_info = {0};
	struct RsCerts certs = {0};
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
	struct RsCerts certs = {0};
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
	struct RsCerts certs = {0};
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

void tc_rs_ssl_check_mng_and_cert_chain()
{
	int ret;
	struct rs_cb rscb = {0};
	struct RsCerts certs = {0};
	struct tls_cert_mng_info mng_info = {0};
	struct CertFile file_name = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};

	mng_info.cert_count = 1;
	mng_info.total_cert_len = 0;
	strcpy(certs.certs[0].certInfo, "pub cert");
	ret = rs_ssl_check_mng_and_cert_chain(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(ret, -22);

	new_certs[0].ncert_count = 2;
	mocker(rs_remove_certs, 20, -1);
	ret = rs_ssl_check_mng_and_cert_chain(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(rs_remove_certs, 20, 0);
	mocker(rs_ssl_check_cert_chain, 20, -1);
	ret = rs_ssl_check_mng_and_cert_chain(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

	mocker(rs_remove_certs, 20, 0);
	mocker(rs_ssl_check_cert_chain, 20, 0);
	mocker(rs_ssl_skid_get_from_chain, 20, -1);
	ret = rs_ssl_check_mng_and_cert_chain(&rscb, &mng_info, &certs, &new_certs, &file_name);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();
	return;
}

void tc_rs_remove_certs()
{
	int ret;
	char end_file[20];
	char ca_file[20];

	mocker(remove, 10, -1);
	ret = rs_remove_certs(&end_file, &ca_file);
	EXPECT_INT_EQ(ret, -EFILEOPER);
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

void tc_rs_peer_socket_recv()
{
    struct SocketConnectInfo conn[10] = {0};
    struct SocketErrInfo  err[10] = {0};
    int ret = 0;

    mocker_clean();

    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    mocker(RsGetConnInfo, 1, -1);
    ret = RsSocketGetClientSocketErrInfo(conn, err, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    conn[0].family = AF_INET;
    mocker(RsSocketConnectCheckPara, 1, 0);
    mocker(RsSocketNodeid2vnic, 1, 0);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(RsDev2conncb, 1, 0);
    mocker(RsGetConnInfo, 1, 0);
    mocker(memcpy_s, 1, 0);
    mocker(memset_s, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = RsSocketGetClientSocketErrInfo(conn, err, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    return;
}

void tc_rs_socket_get_server_socket_err_info()
{
    struct SocketListenInfo conn[10] = {0};
    struct ServerSocketErrInfo err[10] = {0};
    int ret = 0;

    mocker_clean();

    mocker(RsConvertIpAddr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(RsFindListenNode, 1, -1);
    mocker(pthread_mutex_unlock, 1, 0);
    ret = RsSocketGetServerSocketErrInfo(conn, err, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    conn[0].family = AF_INET;
    mocker(RsSocketNodeid2vnic, 1, 0);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker(RsDev2conncb, 1, 0);
    mocker(RsConvertIpAddr, 1, 0);
    mocker(RsFindListenNode, 1, 0);
    mocker(memcpy_s, 2, 0);
    mocker(memset_s, 1, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = RsSocketGetServerSocketErrInfo(conn, err, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    return;
}

int stub_rs_find_listen_node(struct RsConnCb *connCb, struct RsIpAddrInfo *ip_addr, uint32_t server_port,
    struct RsListenInfo **listen_info)
{
    *listen_info = g_plisten_info;

    return 0;
}

void tc_rs_socket_accept_credit_add()
{
    struct SocketListenInfo conn[10] = {0};
    struct RsConnCb connCb = {0};
    int ret = 0;

    mocker_clean();

    mocker(RsConvertIpAddr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    mocker(RsFindListenNode, 1, -1);
    ret = RsSocketAcceptCreditAdd(conn, 1, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(RsConvertIpAddr, 1, 0);
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker_invoke(RsFindListenNode, stub_rs_find_listen_node, 1);
    mocker(RsSocketListenAddToEpoll, 1, 0);
    ret = RsSocketAcceptCreditAdd(conn, 1, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(RsEpollCtl, 1, 0);
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    g_listen_info.fdState = LISTEN_FD_STATE_DELETED;
    ret = RsSocketListenAddToEpoll(&connCb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    g_listen_info.fdState = LISTEN_FD_STATE_DELETED;
    ret = RsSocketListenDelFromEpoll(&connCb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker(RsEpollCtl, 1, -1);
    g_listen_info.fdState = LISTEN_FD_STATE_ADDED;
    ret = RsSocketListenDelFromEpoll(&connCb, g_plisten_info);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    g_listen_info.acceptCreditFlag = 1;
    g_listen_info.acceptCreditLimit = 1;
    mocker(pthread_mutex_lock, 3, 0);
    mocker(pthread_mutex_unlock, 3, 0);
    mocker(RsEpollCtl, 1, 0);
    ret = RsSocketCheckCredit(&connCb, g_plisten_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_epoll_event_ssl_recv_tag_in_handle()
{
    struct rs_cb rs_cb = {0};
    struct RsAcceptInfo *accept_info = NULL;
    struct RsListHead list = {0};

    RS_INIT_LIST_HEAD(&list);
    accept_info = malloc(sizeof(struct RsAcceptInfo));
    accept_info->list = list;
    mocker_clean();
    mocker(RsSslRecvTagInHandle, 1, 0);
    mocker(RsWlistCheckConnAdd, 1, -1);
    mocker(SSL_free, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RsEpollEventSslRecvTagInHandle(&rs_cb, accept_info);
    mocker_clean();
}

void tc_rs_remap_mr()
{
    struct MemRemapInfo memList[1] = {0};
    struct RsMrCb mr_cb = {0};
    struct ibv_mr ibMr = {0};
    int ret;

    mocker_clean();
    RS_INIT_LIST_HEAD(&g_rdev_cb.typicalMrList);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    mocker(RsRoceRemapMr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ret = RsRemapMr(0, 0, memList, 1);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    RS_INIT_LIST_HEAD(&g_rdev_cb.typicalMrList);
    ibMr.length = 100;
    mr_cb.ibMr = &ibMr;
    RsListAddTail(&mr_cb.list, &g_rdev_cb.typicalMrList);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    mocker(RsRoceRemapMr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ret = RsRemapMr(0, 0, memList, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    RS_INIT_LIST_HEAD(&g_rdev_cb.typicalMrList);
    ibMr.length = 100;
    mr_cb.ibMr = &ibMr;
    memList[0].size = (unsigned long long)-1;
    RsListAddTail(&mr_cb.list, &g_rdev_cb.typicalMrList);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    mocker(RsRoceRemapMr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ret = RsRemapMr(0, 0, memList, 1);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();

    RS_INIT_LIST_HEAD(&g_rdev_cb.typicalMrList);
    ibMr.length = 100;
    mr_cb.ibMr = &ibMr;
	memList[0].size = 100;
    RsListAddTail(&mr_cb.list, &g_rdev_cb.typicalMrList);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    mocker(RsRoceRemapMr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    ret = RsRemapMr(0, 0, memList, 1);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_RsRoceGetApiVersion()
{
    unsigned int api_version = 0;

    api_version = RsRoceGetApiVersion();
    EXPECT_INT_EQ(api_version, 0);
}

void tc_rs_get_tls_enable()
{
    unsigned int phyId;
    bool tls_enable;
    int ret;

    mocker(RsGetRsCb, 1, 1);
    ret = RsGetTlsEnable(phyId, &tls_enable);
    EXPECT_INT_EQ(ret, 1);

    ret = RsGetTlsEnable(phyId, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);
	mocker_clean();
}

int rs_get_linux_version_stub(struct RsLinuxVersionInfo *ver_info)
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
	ret = RsGetSecRandom(&value);
    EXPECT_INT_EQ(ret, -257);

    mocker(strstr, 2, NULL);
    ret = RsGetSecRandom(&value);
    EXPECT_INT_NE(0, ret);

    mocker(read, 2, -1);
    ret = RsGetSecRandom(&value);
    EXPECT_INT_NE(0, ret);

    mocker(open, 2, -1);
    ret = RsGetSecRandom(&value);
    EXPECT_INT_NE(0, ret);

	mocker_invoke(RsGetLinuxVersion, rs_get_linux_version_stub, 10);

	ret = RsGetSecRandom(&value);
    EXPECT_INT_NE(ret, 0);
}

void tc_rs_get_hccn_cfg()
{
	char *value[2048] = {0};
	unsigned int value_len = 2048;

    int ret;

	ret = RsGetHccnCfg(0, HCCN_CFG_UDP_PORT_MODE, value, &value_len);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();
}

void tc_rs_free_dev_list(void)
{
    struct rs_cb rscb = {0};

    RS_INIT_LIST_HEAD(&rscb.rdevList);
    RsFreeUdevList(&rscb);

    rscb.protocol = PROTOCOL_UNSUPPORT;
    RsFreeDevList(&rscb);
}

void tc_rs_free_rdev_list(void)
{
    struct RsRdevCb rdevCb = {0};
    struct rs_cb rscb = {0};

    rscb.protocol = PROTOCOL_RDMA;
    mocker(rsGetDevIDByLocalDevID, 1, -1);
    RsFreeRdevList(&rscb);
    mocker_clean();

	RS_INIT_LIST_HEAD(&rscb.rdevList);
    RsListAddTail(&rdevCb.list, &rscb.rdevList);
    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(RsRdevDeinit, 1, -1);
    RsFreeRdevList(&rscb);
    mocker_clean();

    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(RsRdevDeinit, 1, 0);
    RsFreeRdevList(&rscb);
    mocker_clean();
}

void tc_rs_free_udev_list(void)
{
    struct rs_ub_dev_cb udev_cb = {0};
    struct rs_cb rscb = {0};

	RS_INIT_LIST_HEAD(&rscb.rdevList);
    mocker(rs_ub_ctx_deinit, 1, -1);
    RsListAddTail(&udev_cb.list, &rscb.rdevList);
    RsFreeUdevList(&rscb);
    mocker_clean();

    mocker_clean();
    mocker(rs_ub_ctx_deinit, 1, 0);
    RsFreeUdevList(&rscb);
    mocker_clean();
}

void tc_rs_retry_timeout_exception_check()
{
    struct SensorNode sensorNode = {0};
	struct ibv_wc wc = {0};
    int ret = 0;

    sensorNode.sensorHandle = 0;
    ret = RsRetryTimeoutExceptionCheck(&sensorNode);
    EXPECT_INT_EQ(ret, 0);

    sensorNode.sensorHandle = 1;
    mocker_clean();
    mocker(DlHalSensorNodeUpdateState, 1, -1);
    ret = RsRetryTimeoutExceptionCheck(&sensorNode);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(DlHalSensorNodeUpdateState, 1, 0);
    ret = RsRetryTimeoutExceptionCheck(&sensorNode);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

	wc.status = IBV_WC_RETRY_EXC_ERR;
    mocker(RsRetryTimeoutExceptionCheck, 1, 0);
    RsRdmaRetryTimeoutExceptionCheck(&sensorNode, &wc);
    mocker_clean();
}
