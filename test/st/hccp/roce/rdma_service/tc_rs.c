
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
#include "hccp_common.h"
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
struct RsQpCb *qp_cb_ab2;
struct RsRdevCb g_rdev_cb = {0};
int g_qp_cb_state = 0;
struct RsConnInfo *g_conn_info;
struct RsListenInfo g_listen_info = {0};
struct RsListenInfo *g_plisten_info = &g_listen_info;

#define SLEEP_TIME 30000

int RsDev2rscb(uint32_t chipId, struct rs_cb **rs, bool init_flag_cb);
enum RsHardwareType RsGetDeviceType(unsigned int phyId);
int RsGetRsCb(unsigned int phyId, struct rs_cb **rs_cb);
void RsFreeAcceptOneNode(struct rs_cb *rscb, struct RsAcceptInfo *accept);
int RsEpollCtl(int epollfd, int op, int fd, unsigned int state);
int RsGetVnicIp(unsigned int phyId, unsigned int *vnic_ip);
void rs_ssl_deinit(struct rs_cb *rscb);
int rs_ssl_crl_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb);
int tls_get_cert_chain(X509_STORE *store, struct RsCerts *certs,
    struct tls_cert_mng_info *mng_info);
int rs_ssl_skid_get_from_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct tls_ca_new_certs *new_certs);
int rs_ssl_get_cert(struct rs_cb *rscb, struct RsCerts *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs);
int rs_check_pridata(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info *mng_info);
int RsPeerGetIfaddrs(struct InterfaceInfo interface_infos[], unsigned int *num, unsigned int phyId);
int RsPeerGetIfnum(unsigned int phyId, unsigned int *num);
int RsCheckDstInterface(unsigned int phyId, const char *ifa_name, enum RsHardwareType type, bool is_all);
int RsSocketWhiteListNodeDestroy(struct RsConnCb *connCb,
    struct SocketWlistInfoT *white_list, unsigned int server_ip, unsigned int server_port);
int RsSocketNodeid2vnic(uint32_t node_id, uint32_t *ip_addr);
int RsServerValidAsyncInit(unsigned int chipId, struct RsConnInfo *conn, struct SocketWlistInfoT *white_list_expect);
int RsServerSendWlistCheckResult(struct RsConnInfo *conn, bool flag);
int RsSetFdNonblock(int connfd);
void RsEpollEventSslListenInHandle(struct rs_cb *rs_cb, struct RsListenInfo *listen_info, int connfd, struct sockaddr_in remote_ip);
void RsSslRecvTagInHandle(struct RsAcceptInfo *accept_info, struct RsConnInfo *conn_tmp);
int rs_socket_fill_wlist_by_phyID(unsigned int chipId, struct SocketWlistInfoT *white_list_node, struct RsConnInfo *rs_conn);
uint32_t RsSocketVnic2nodeid(uint32_t ip_addr);
int RsSocketListenBindListen(int listen_fd, struct RsConnCb *connCb,
    struct SocketListenInfo *conn, struct RsListenInfo *listen_info, uint32_t server_port);
extern int RsDrvPostRecv(struct RsQpCb *qp_cb, struct RecvWrlistData *wr, unsigned int recv_num,
    unsigned int *complete_num);
extern __thread struct rs_cb *gRsCb;
extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int RsDrvNormalQpCreateInit(struct ibv_qp_init_attr *qp_init_attr, struct RsQpCb *qp_cb, struct ibv_port_attr *attr);
extern int RsDrvExpQpCreate(struct RsQpCb *qp_cb, int qpMode);
extern int RsDrvExpQpCreateInit(struct ibv_exp_qp_init_attr *qp_init_attr, struct RsQpCb *qp_cb, struct ibv_port_attr *attr);
extern int RsQpcbInit(struct RsRdevCb *rdevCb, struct RsQpCb *qp_cb, struct RsQpNorm *qp_norm);
extern int RsQpQueryInfo(unsigned int phyId, unsigned int rdevIndex, struct RsRdevCb **rdevCb, int qpMode);
extern int RsSendExpWrlist(struct RsQpCb *qp_cb, struct WrInfo *wr_list, unsigned int send_num, struct SendWrRsp *wr_rsp, unsigned int *complete_num);
extern int RsGetMrcb(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb, struct RsListHead *mr_list);
extern void RsWirteAndReadBuildUpWr(struct RsMrCb *mr_cb, struct RsMrCb *rem_mr_cb, struct WrInfo *wr, struct ibv_sge *list, struct ibv_send_wr *ib_wr);
extern struct ibv_mr* RsDrvMrReg(struct ibv_pd *pd, char *addr, size_t length, int access);
extern int rs_ssl_ca_ky_init(SSL_CTX *ssl_ctx, struct rs_cb *rscb);
extern int rs_ssl_load_ca(SSL_CTX *ssl_ctx, struct rs_cb *rscb, struct tls_cert_mng_info* mng_info);

int RsEpollEventListenInHandle(struct rs_cb *rs_cb, int fd);
int RsEpollEventQpMrInHandle(struct rs_cb *rs_cb, int fd);
int RsEpollEventHeterogTcpRecvInHandle(struct rs_cb *rs_cb, int fd);
extern void RsEpollEventSslAcceptInHandle(struct rs_cb *rs_cb, int fd);
extern struct RsCqeErrInfo gRsCqeErr;
extern void RsDrvSaveCqeErrInfo(uint32_t status, struct RsQpCb *qp_cb);
extern int RsDrvNormalQpCreate(struct RsQpCb *qp_cb, struct ibv_qp_init_attr *qp_init_attr);
extern int RsGetIpv6ScopeId(struct in6_addr localIp);
extern int RsConnectBindClient(int fd, struct RsConnInfo *conn);
extern int DlDrvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
extern int DlHalGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
extern void RsSocketGetBindByChip(unsigned int chipId, bool *bind_ip);

extern int rs_ssl_x509_store_init(X509_STORE *store, struct RsCerts *certs,
	struct tls_cert_mng_info *mng_info, struct tls_ca_new_certs *new_certs);
extern int tls_get_user_config(unsigned int save_mode, unsigned int chipId, const char *name,
    unsigned char *buf, unsigned int *buf_size);
extern X509 *tls_load_cert(const char *inbuf, uint32_t buf_len, int type);
extern int rs_ssl_skid_get_from_chain(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct tls_ca_new_certs *new_certs);
extern int rs_ssl_put_cert_ca_pem(struct RsCerts *certs, struct tls_cert_mng_info* mng_info,
    struct tls_ca_new_certs *new_certs, const char *ca_file);
extern int rs_ssl_put_cert_end_pem(struct RsCerts *certs, struct tls_ca_new_certs *new_certs, const char *end_file);
extern int rs_ssl_put_end_cert(struct RsCerts *certs, const char *end_file);
extern int rs_ssl_skids_subjects_get(struct rs_cb *rscb, struct tls_cert_mng_info *mng_info,
    struct RsCerts *certs, struct rs_new_certs *new_certs);
extern int rs_ssl_X509_store_add_cert(char *cert_info, X509_STORE *store);
extern int RsSocketCloseFd(int fd);
int RsGetConnInfo(struct RsConnCb *connCb, 
			struct SocketConnectInfo *conn, 
			struct RsConnInfo **conn_info);
extern int RsSocketConnectCheckPara(struct SocketConnectInfo *conn_info);
extern int rsGetLocalDevIDByHostDevID(unsigned int phyId, unsigned int *chipId);
extern int RsDev2conncb(uint32_t chipId, struct RsConnCb **connCb);
extern int RsConvertIpAddr(int family, union HccpIpAddr *ip_addr, struct RsIpAddrInfo *ip);
extern int RsFindListenNode(struct RsConnCb *connCb, struct RsIpAddrInfo *ip_addr, uint32_t server_port,
    struct RsListenInfo **listen_info);
extern void RsSensorNodeUnregister(struct SensorNode *sensorNode);
extern int RsSocketListenAddToEpoll(struct RsConnCb *connCb, struct RsListenInfo *listen_info);
extern int RsFindWhiteList(struct RsConnCb *connCb, struct RsIpAddrInfo *server_ip, unsigned int server_port,
    struct RsWhiteList **white_list);
extern int RsFindWhiteListNode(struct RsWhiteList *rs_socket_white_list,
    struct SocketWlistInfoT *white_list_expect, int family, struct RsWhiteListInfo **white_list_node);
extern int RsServerSendWlistCheckResult(struct RsConnInfo *conn, bool flag);
extern bool RsSocketIsVnicIp(unsigned int chipId, unsigned int ip_addr);
extern int RsGetLinuxVersion(struct RsLinuxVersionInfo *ver_info);
extern int FileReadCfg(const char *filePath, int devId, const char *confName, char *confValue, unsigned int len);
extern int DlHalSensorNodeUpdateState(uint32_t devid, uint64_t handle, int val, halGeneralEventType_t assertion);

int replace_rs_qpn2qpcb(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct RsQpCb **qp_cb)
{
	static struct RsQpCb a_qp_cb;
    a_qp_cb.state = g_qp_cb_state;
	*qp_cb = &a_qp_cb;
	return 0;
}

struct RsWhiteListInfo g_white_list_node_tmp = {0};
int stub_rs_find_white_list_node(struct RsWhiteList *rs_socket_white_list,
    struct SocketWlistInfoT *white_list_expect, int family, struct RsWhiteListInfo **white_list_node)
{
	*white_list_node = &g_white_list_node_tmp;
	return 0;
}

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

	//RsEpollHandle();
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
	char pid_sign[] = "ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a";
	struct RsRdevCb rdev_tmp = {0};
	struct RsInitConfig cfg = {0};
	int ret;

	RsSensorNodeUnregister(&rdev_tmp.sensorNode);
	rdev_tmp.sensorNode.sensorHandle = 1;
	RsSensorNodeUnregister(&rdev_tmp.sensorNode);

	rs_ut_msg("\n%s+++++++++ABNORMAL TC Start++++++++\n", __func__);
	ret = RsInit(NULL);
	EXPECT_INT_NE(ret, 0);

	cfg.chipId = 0;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("\n%s---------ABNORMAL TC End----------\n", __func__);

	RsSetHostPid(cfg.chipId, 0, pid_sign);
	/* ------Resource CLEAN-------- */
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	struct RsInitConfig cfg_peer_online = {0};
	cfg_peer_online.chipId = 1;
	cfg_peer_online.hccpMode = NETWORK_PEER_ONLINE;
	ret = RsInit(&cfg_peer_online);

	ret = RsDeinit(&cfg_peer_online);

	struct RsInitConfig cfg_online = {0};
	cfg_online.chipId = 1;
	cfg_online.hccpMode = NETWORK_ONLINE;
	ret = RsInit(&cfg_online);

	ret = RsDeinit(&cfg_online);

	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	ret = RsGetDeviceType(10);

	ret = RsGetDeviceType(1);
	EXPECT_INT_EQ(ret, RS_HARDWARE_UNKNOWN);
	free(gRsCb);
	gRsCb = NULL;

	unsigned int vnic_ip = 0x10;
	ret = RsGetVnicIp(129, &vnic_ip);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetVnicIp(0, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);
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

	struct rs_cb *rscb = NULL;
	rscb = calloc(1, sizeof(struct rs_cb));
	rscb->hccpMode = NETWORK_OFFLINE;
	RS_INIT_LIST_HEAD(&rscb->connCb.clientConnList);
	RsInitRscbCfg(rscb);
	RsDeinitRscbCfg(rscb);
	free(rscb);
	rscb = NULL;
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
	conn[0].port = 16666;
	strcpy(conn[0].tag, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET;
	conn[1].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[1].port = 16666;
	strcpy(conn[1].tag, "5678");
	ret = RsSocketBatchConnect(&conn[0], 2);

	usleep(SLEEP_TIME); //wait for epoll thread start up..


	/* ------Resource CLEAN-------- */
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

extern __thread struct rs_cb *gRsCb;
void tc_rs_socket_deinit()
{
	int ret;
    struct rdev rdev_info = {0};
	struct rs_cb g_rs_cb_tmp = {0};
	g_rs_cb_tmp.hccpMode = NETWORK_PEER_ONLINE;
	gRsCb = &g_rs_cb_tmp;

	rdev_info.phyId = 8;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");
	ret = RsSocketDeinit(rdev_info);
	gRsCb = NULL;
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
	EXPECT_INT_EQ(ret, 0);
	mocker(RsGetIpv6ScopeId, 10, if_nametoindex("lo"));
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

	listen[0].phyId = 10;
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);
	EXPECT_INT_EQ(ret, -EINVAL);

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

	/* stop a non-exist node */
	listen[1].port = 16666;
	ret = RsSocketListenStop(&listen[1], 1);

	/* ------Resource CLEAN-------- */
	struct RsConnInfo conn_send_inc;
	mocker((stub_fn_t)send, 10, -1);
	RsSocketTagSync(&conn_send_inc);
	mocker_clean();

	mocker(RsDrvSocketSend, 10, -EAGAIN);
	RsSocketTagSync(&conn_send_inc);
	mocker_clean();

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
	int try_num;


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

	conn[0].phyId = 10;
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 2);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	strcpy(conn[0].tag, "1234");
	conn[1].phyId = 0;
	conn[1].family = AF_INET6;
	strcpy(conn[1].tag, "5678");
	conn[0].port = 16666;
	conn[1].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 2);
	mocker_clean();

	mocker(RsGetIpv6ScopeId, 10, -1);
	ret = RsSocketBatchConnect(&conn[0], 2);
	mocker_clean();

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

    strcpy(white_list.tag, "1234");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;
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


	usleep(SLEEP_TIME); //wait for epoll thread start up..

#if 1
	i = 0;

	socket_info[i].phyId = 10;
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);

	socket_info[i].phyId = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
#endif

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
#endif

	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	struct rs_cb *rs_cb = NULL;
	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	rs_cb->sslEnable = 1;

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

extern __thread struct rs_cb *gRsCb;
void tc_rs_socket_connect02()
{
	struct RsConnInfo conn_socket_err;
	int ret;

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

	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, -EEXIST);
	
	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	EXPECT_INT_EQ(ret, 0);
	rs_cb->sslEnable = 1;

	accept->list = list;
	accept->ssl = NULL;

	RsFreeAcceptOneNode(rs_cb, accept);

	/* ------Resource CLEAN-------- */
	rs_cb->sslEnable = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

// void tc_rs_socket_deinit2()
// {
// 	int ret;
// 	int i;
// 	struct RsInitConfig cfg = {0};
// 	struct rs_cb *rs_cb = NULL;
// 	struct rdev rdev_info = {0};
// 	struct RsAcceptInfo *accept = calloc(1, sizeof(struct RsAcceptInfo));
// 	struct RsListHead list = {0};

// 	RS_INIT_LIST_HEAD(&list);
// 	rdev_info.phyId = 0;
// 	rdev_info.family = AF_INET;
// 	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

// 	cfg.chipId = 0;
// 	cfg.hccpMode = NETWORK_OFFLINE;
// 	ret = RsInit(&cfg);
// 	EXPECT_INT_EQ(ret, 0);

// 	ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
// 	rs_cb->sslEnable = 1;
// 	accept->ssl = malloc(sizeof(SSL_CTX));
// 	accept->list = list;

// 	RsFreeAcceptOneNode(rs_cb, accept);
// 	ret = RsDeinit(&cfg);
// 	EXPECT_INT_EQ(ret, 0);
// }

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
	int try_num;

	struct SocketFdData conn_info;
	conn_info.localIp.addr.s_addr = 1;
	RsSocketsServeripConverter(&conn_info, 1, 1);

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

	usleep(SLEEP_TIME); //wait for epoll thread start up..

#if 1
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
#endif

#if 1 /* get Client Conn & Close it */
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* param error */
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, NULL, 3);
	EXPECT_INT_NE(ret, 0);

	/* physical id error */
	socket_info[i].phyId = 55555;
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_NE(ret, 0);
	socket_info[i].phyId = 0;	//recover

	/* repeat get */
	ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
	EXPECT_INT_EQ(ret, 0);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	/* close a non-exist fd */
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
	EXPECT_INT_NE(ret, 0);
#endif

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
#endif


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

int stub_rs_epoll_ctl(int epollfd, int op, int fd, int state)
{
	if (op == EPOLL_CTL_ADD) return 0;
	return 1;
}

int stub_RsIbvQueryQp(struct ibv_qp *qp, struct ibv_qp_attr *attr, int attr_mask, struct ibv_qp_init_attr *init_attr)
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
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	int qpMode = 1;
	int i;
	struct RsQpCb qp_cb_abnormal;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

 	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 1;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 3;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

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

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

    RsSocketWhiteListAdd(rdev_info, NULL, 1);
	RsSocketWhiteListDel(rdev_info, NULL, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);

	struct LiteQpCqAttrResp qp_resp;
	ret = RsGetLiteQpCqAttr(phyId, rdevIndex, resp.qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct LiteMemAttrResp mem_resp;
	ret = RsGetLiteMemAttr(rdev_info.phyId, rdevIndex, resp.qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	RsRdevDeinit(phyId, NOTIFY, rdevIndex);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn %d, ret:%d\n", resp.qpn, ret);


	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = RsQpConnectAsync(10, rdevIndex, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	ret = RsQpConnectAsync(phyId, rdevIndex, 4444, -1);
	EXPECT_INT_NE(ret, 0);

	ret = RsQpConnectAsync(phyId, rdevIndex, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, -1);
	EXPECT_INT_NE(ret, 0);

	mocker_invoke(RsEpollCtl, stub_rs_epoll_ctl, 10);
	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, socket_info[i].fd);
	mocker_clean();

	struct RsQpCb *qp_cb;
	ret = RsQpn2qpcb(phyId, rdevIndex, resp.qpn, &qp_cb);
	EXPECT_INT_EQ(ret, 0);
	qp_cb->state = RS_QP_STATUS_DISCONNECT;

	/* >>>>>>> RsQpConnectAsync test case end <<<<<<<<<<< */

	ret = RsQpConnectAsync(phyId, rdevIndex, resp.qpn, socket_info[i].fd);
	rs_ut_msg("***RsQpConnectAsync: %d****\n", ret);

	struct LiteConnectedInfoResp connected_resp;
	qp_cb->state = RS_QP_STATUS_CONNECTED;
	ret = RsGetLiteConnectedInfo(phyId, rdevIndex, resp.qpn, &connected_resp);
	EXPECT_INT_EQ(ret, 0);

	mocker_invoke(RsIbvQueryQp, stub_RsIbvQueryQp, 10);
	ret = RsQpStateModify(&qp_cb_abnormal);
	EXPECT_INT_NE(ret, 0);
	mocker_clean();

	usleep(SLEEP_TIME);

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
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
#endif
	usleep(SLEEP_TIME);

	uint64_t mr_max_addr = 0x654321;
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = mr_max_addr;
	mr_reg_info.len = 1;
	mr_reg_info.access = 0;
	for (i = 0; i < 65; i++) {
		ret = RsMrReg(phyId, rdevIndex, resp.qpn,&mr_reg_info);
		mr_max_addr += 2;
	}

	//ret = RsQpDestroy(dev_id, rdevIndex, qpn2);
	ret = RsQpDestroy(phyId, rdevIndex, resp.qpn);

	/* param error - qpn */
	ret = RsQpDestroy(phyId, rdevIndex, resp2.qpn);
	EXPECT_INT_EQ(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInitWithBackup(rdev_info, rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
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
void QpExtAttrs(int qpMode, struct QpExtAttrs *extAttrs)
{
    extAttrs->qpMode = qpMode;
    // cq attr
    extAttrs->cqAttr.sendCqDepth = RS_DRV_CQ_8K_DEPTH;
    extAttrs->cqAttr.sendCqCompVector = 0;
    extAttrs->cqAttr.recvCqDepth = RS_DRV_CQ_128_DEPTH;
    extAttrs->cqAttr.recvCqCompVector = 0;
    // qp attr
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
    // version control
    extAttrs->version = QP_CREATE_WITH_ATTR_VERSION;
}

void tc_rs_qp_create_with_attrs_v1()
{

	int ret;
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	uint32_t qpn, qpn1, qpn2;
	int qpMode = 1;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num;

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

	struct RsQpNormWithAttrs  qp_norm = {0};
	struct RsQpRespWithAttrs qp_resp_create = {0};

	qp_norm.isExp = 1;
	qp_norm.isExt = 1;
	QpExtAttrs(1, &qp_norm.extAttrs);
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 1;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)halGetDeviceInfo, 10, -1);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 3;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	int support_lite = 0;
	ret = RsGetLiteSupport(rdev_info.phyId, rdevIndex, &support_lite);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(support_lite, 1);

	struct LiteRdevCapResp rdev_resp;
	ret = RsGetLiteRdevCap(rdev_info.phyId, rdevIndex, &rdev_resp);
	EXPECT_INT_EQ(ret, 0);

	listen[0].phyId = 0;
	listen[0].family = AF_INET;
	listen[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);

    strcpy(white_list.tag, "1234");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

    RsSocketWhiteListAdd(rdev_info, NULL, 1);
	RsSocketWhiteListDel(rdev_info, NULL, 1);

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

    qp_norm.extAttrs.dataPlaneFlag.bs.cqCstm = 1;
    qp_norm.aiOpSupport = 1;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);

	struct LiteQpCqAttrResp qp_resp;
	ret = RsGetLiteQpCqAttr(phyId, rdevIndex, qpn, &qp_resp);
	EXPECT_INT_EQ(ret, 0);

	struct LiteMemAttrResp mem_resp;
	ret = RsGetLiteMemAttr(rdev_info.phyId, rdevIndex, qpn, &mem_resp);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	RsRdevDeinit(phyId, NOTIFY, rdevIndex);

	ret = RsRdevInit(rdev_info, NOTIFY, &rdevIndex);
	EXPECT_INT_EQ(ret, 0);

	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn = qp_resp_create.qpn;
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreateWithAttrs: qpn %d, ret:%d\n", qpn, ret);


	/* >>>>>>> RsQpConnectAsync test case begin <<<<<<<<<<< */
	/* param error - qpn */
	ret = RsQpConnectAsync(10, rdevIndex, 4444, socket_info[i].fd);
	EXPECT_INT_NE(ret, 0);

	ret = RsQpConnectAsync(phyId, rdevIndex, 4444, -1);
	EXPECT_INT_NE(ret, 0);

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

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	/* >>>>>>> RsQpCreateWithAttrs test case begin <<<<<<<<<<< */
	/* param error - device id */

	ret = RsQpCreateWithAttrs(15, rdevIndex, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
	EXPECT_INT_NE(ret, 0);

	/* qp number out of boundry */
	struct rs_cb *rs_cb;
	struct RsRdevCb *rdevCb;
        uint32_t chipId = 0;
	ret = RsDev2rscb(chipId, &rs_cb, false);
	EXPECT_INT_EQ(ret, 0);
 	ret = RsGetRdevCb(rs_cb, rdevIndex, &rdevCb);
	EXPECT_INT_EQ(ret, 0);

	int qp_cnt_tmp = rdevCb->qpCnt;
	rdevCb->qpCnt = 44444;
	ret = RsQpCreateWithAttrs(phyId, rdevIndex, &qp_norm, &qp_resp_create);
	qpn2 = qp_resp_create.qpn;
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
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreateWithAttrs: qpn1 %d, ret:%d\n", qpn1, ret);
#endif
	usleep(SLEEP_TIME);

	uint64_t mr_max_addr = 0x654321;
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = mr_max_addr;
	mr_reg_info.len = 1;
	mr_reg_info.access = 0;
	for (i = 0; i < 65; i++) {
		ret = RsMrReg(phyId, rdevIndex, qpn,&mr_reg_info);
		mr_max_addr += 2;
	}

	//ret = RsQpDestroy(dev_id, rdevIndex, qpn2);
	ret = RsQpDestroy(phyId, rdevIndex, qpn);
	ret = RsQpDestroy(phyId, rdevIndex, qpn1);
	/* param error - qpn */
	ret = RsQpDestroy(phyId, rdevIndex, qpn2);
	EXPECT_INT_EQ(ret, 0);

	sock_close[0].fd = socket_info[0].fd;
	ret = RsSocketBatchClose(0, &sock_close[0], 1);

	sock_close[1].fd = socket_info[1].fd;
	ret = RsSocketBatchClose(0, &sock_close[1], 1);


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsRdevDeinit(phyId, NOTIFY, rdevIndex);
	EXPECT_INT_EQ(ret, 0);
	cfg.chipId = 0;
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

void tc_rs_mr_sync()
{
	int ret;
	uint32_t phyId = 0;
	unsigned int rdevIndex = 0;
	int flag = 0; /* RC */
	int qpMode = 1;
	int i;
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
	struct RsQpResp resp = {0};
	struct RsQpResp resp2 = {0};
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num;

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

    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

	conn[0].phyId = 0;
	conn[0].family = AF_INET;
	conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
	conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
	ret = RsSocketBatchConnect(&conn[0], 1);

	rs_ut_msg("___________________after connect:\n");

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

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

#if 1
	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	rs_ut_msg("RsQpCreate: qpn2 %d, ret:%d\n", resp2.qpn, ret);
	rs_ut_msg("___________________after qp2 create:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");
#endif
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
	int try_num;

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

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
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
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn2, socket_info[i].fd);
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
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
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
	int try_num;

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

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	struct RsQpNorm qp_norm = {0};
	qp_norm.flag = flag;
	qp_norm.qpMode = qpMode;
	qp_norm.isExp = 1;
	qp_norm.isExt = 1;

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
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	ret = RsQpCreate(phyId, rdevIndex, qp_norm, &resp2);
	EXPECT_INT_EQ(ret, 0);
	*qpn2 = resp2.qpn;
	rs_ut_msg("RS CREATE QP: QPN:%d, ret:%d\n", *qpn2, ret);

	ret = RsQpConnectAsync(phyId, rdevIndex, *qpn2, socket_info[i].fd);
	*fd2 = socket_info[i].fd;

	usleep(SLEEP_TIME); //wait for epoll thread start up..

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

	cfg.chipId = 0;
	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	rs_ut_msg("resource free done..................\n");

	return ret;
}

struct RsMrCb *g_mr_cb_a;
int stub_rs_get_mrcb_a(struct RsQpCb *qp_cb, uint64_t addr, struct RsMrCb **mr_cb,
    struct RsListHead *mr_list)
{
	*mr_cb = g_mr_cb_a;
	return -1;
}

void tc_rs_mr_create()
{
	int ret;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2, *addr3;
	int try_num = 0;
	struct RdmaMrRegInfo mr_reg_info = {0};

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
	ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
	EXPECT_INT_EQ(ret, 0); //synced, do not need sync again!

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

	usleep(SLEEP_TIME);

	/* free resource */
	rs_ut_msg("RS MR dereg begin...\n");
	ret = RsMrDereg(phyId, rdevIndex, qpn, addr);
	EXPECT_INT_EQ(ret, 0);
	ret = RsMrDereg(phyId, rdevIndex, qpn2, addr2);
	EXPECT_INT_EQ(ret, 0);

	mr_reg_info.addr = addr3;
	mocker(RsQpn2qpcb, 10, 0);
	mocker_invoke((stub_fn_t)RsGetMrcb, stub_rs_get_mrcb_a, 1);
	ret = RsMrDereg(phyId, rdevIndex, 999999, addr2);
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
	attr->qp_state = 1; // IBV_QPS_INIT
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

	int batch_modify_qpn[2];
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

void tc_rs_mr_abnormal()
{
	int ret;
	int flag = 0; /* RC */
	uint32_t qpn, qpn2;
	int fd, fd2;
	void *addr, *addr2;
	unsigned int phyId = 0;
	unsigned int rdevIndex = 0;
	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);


	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);
	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

	ret = RsMrReg(129, rdevIndex, 999999, &mr_reg_info);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsMrReg(phyId, rdevIndex, 999999, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	mr_reg_info.addr = 0;
	ret = RsMrReg(phyId, rdevIndex, qpn, &mr_reg_info);
	EXPECT_INT_NE(ret, 0);

	ret = RsMrDereg(129, rdevIndex, 999999, addr);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsMrDereg(phyId, rdevIndex, 999999, addr);
	EXPECT_INT_NE(ret, 0);

	ret = RsMrDereg(phyId, rdevIndex, qpn2, 0);
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

	mocker(rs_epoll_event_jfc_in_handle, 20, 0);

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

	mocker_clean();

#if 0
	struct RsMrCb mr_cb_3;
	struct RsQpCb qp_cb_3;
	end_wr(10r_cb_3.qp_cb = &sp_cb_3;
	qp_cb_3.conn_info->connfd = RS_FD_INVALID;
	ret = RsMrInfoSync(&mr_cb_3);
	EXPECT_INT_NE(0, ret);

	qp_cb_3.conn_info->connfd = 10;
	mr_cb_3.state = RS_MR_STATE_SYNCED;
	ret = RsMrInfoSync(&mr_cb_3);
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
	RsBufPrint(addr, 1);
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
	struct RsInitConfig cfg = {0};
	struct SocketListenInfo listen[2] = {0};
	struct SocketConnectInfo conn[2] = {0};
	struct RsSocketCloseInfoT sock_close[2] = {0};
	struct SocketFdData socket_info[3] = {0};
    struct SocketWlistInfoT white_list;
    white_list.remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    white_list.connLimit = 1;
	int try_num;
	uint32_t sslEnable = 1;

	ret = RsGetSslEnable(NULL);
	EXPECT_INT_NE(ret, 0);
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	mocker((stub_fn_t)system, 10, 0);
	mocker((stub_fn_t)access, 10, 0);
    mocker_invoke((stub_fn_t)halGetDeviceInfo, stub_halGetDeviceInfo, 10);
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	ret = RsGetSslEnable(NULL);
	EXPECT_INT_NE(ret, 0);

	ret = RsGetSslEnable(&sslEnable);
	EXPECT_INT_EQ(ret, 0);
	EXPECT_INT_EQ(sslEnable, 0);

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
	ret = RsSocketListenStart(&listen[0], 0);
	EXPECT_INT_NE(ret, 0);

	/* param error - fd */
	listen[0].phyId = 15;
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	listen[0].phyId = 0; //recover

	/* repeat listen */
	listen[0].port = 16666;
	ret = RsSocketListenStart(&listen[0], 1);
	/* >>>>>>> RsSocketListenStart test case end <<<<<<<<<<< */
    strcpy(white_list.tag, "1234");

	struct rdev rdev_info = {0};
	rdev_info.phyId = 0;
	rdev_info.family = AF_INET;
	rdev_info.localIp.addr.s_addr = inet_addr("127.0.0.1");

    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;
    strcpy(white_list.tag, "5678");
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

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

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	/* ===RsEpollEventInHandle ut begin --- accept fail=== */
	struct epoll_event events;
	struct rs_cb *rs_cb_t;
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
	/* ===RsEpollEventInHandle ut end --- accept fail=== */

#if 1
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [client]socket_info[0].fd:%d, status:%d\n", __func__, socket_info[i].fd, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);

	i = 1;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "5678");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
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
	EXPECT_INT_NE(ret, 0);
#endif

	usleep(1000);

#if 1 /* get Server Conn & Close it */
	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_SERVER, &socket_info[i], 1);
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[0].fd:%d, client if:0x%x, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].remoteIp.addr.s_addr, socket_info[i].status);

	sock_close[i].fd = socket_info[i].fd;
	ret = RsSocketBatchClose(0, &sock_close[i], 1);
#endif


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
	listen[0].phyId = 0; //recover
	/* >>>>>>> RsSocketListenStop test case end <<<<<<<<<<< */


	/* ------Resource CLEAN-------- */
	listen[0].port = 16666;
	ret = RsSocketListenStop(&listen[0], 1);

	ret = RsDeinit(&cfg);
	EXPECT_INT_EQ(ret, 0);

	return;
}

void tc_rs_get_qp_status()
{
    struct RsQpStatusInfo qp_info= { 0 };
    unsigned int rdevIndex = 0;
    unsigned int phyId = 0;
    uint32_t qpn = 0;
    int ret;

    ret = RsGetQpStatus(129, rdevIndex, qpn, &qp_info);
    EXPECT_INT_EQ(ret, -EINVAL);

    /* param error - status */
    ret = RsGetQpStatus(phyId, rdevIndex, qpn, NULL);
    EXPECT_INT_NE(ret, 0);

    /* param error - status */
    ret = RsGetQpStatus(phyId, rdevIndex, 444444, &qp_info);
    EXPECT_INT_NE(ret, 0);

    mocker_invoke(RsQpn2qpcb, replace_rs_qpn2qpcb, 1);
    g_qp_cb_state = 1;
    mocker(RsRoceQueryQpc, 10, 1);
    ret = RsGetQpStatus(phyId, rdevIndex, qpn, &qp_info);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

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
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

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
	} while(try_num && ret == -2);
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
	struct SendWrRsp rs_wr_info;
	uint32_t phyId = 0;
	uint32_t rdevIndex = 0;

	/* +++++Resource Prepare+++++ */
	ret = tc_rs_sock_qp_create(&fd, &qpn, &fd2, &qpn2);

	addr = malloc(RS_TEST_MEM_SIZE);
	addr2 = malloc(RS_TEST_MEM_SIZE);

	struct RdmaMrRegInfo mr_reg_info = {0};
	mr_reg_info.addr = addr;
	mr_reg_info.len = RS_TEST_MEM_SIZE;
	mr_reg_info.access = RS_ACCESS_LOCAL_WRITE;

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

	list[0].addr = addr;
	list[0].len = RS_TEST_MEM_SIZE;
	list[1].addr = addr;
	list[1].len = RS_TEST_MEM_SIZE;
	wr.bufList = list;
	wr.bufNum = 2;
	wr.dstAddr = addr2;
	wr.op = 0;
	wr.sendFlag = RS_SEND_FENCE;

	ret =RsSendWr(129, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_EQ(ret, -EINVAL);

	try_num = 3;
	do {
		ret =RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
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
		ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
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
                ret = RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
                if (ret == 0)
                        break;
                usleep(SLEEP_TIME);
				try_num--;
        } while(try_num && ret == -2);
        EXPECT_INT_EQ(ret, 0);

	/* qpn error */
	ret =RsSendWr(phyId, rdevIndex, 44444, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	list[0].len = 2147483649;
    list[1].len = 2147483649;
	ret =RsSendWr(phyId, rdevIndex, qpn, &wr,  &rs_wr_info);
	EXPECT_INT_NE(ret, 0);

	list[0].len = RS_TEST_MEM_SIZE;
	list[1].len = RS_TEST_MEM_SIZE;

	/* addr error, cannot find mrcb */
	list[0].addr = 5555;
	ret =RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
	EXPECT_INT_NE(ret, 0);
	list[0].addr = addr;

	/* addr error, cannot find remote mrcb */
	wr.dstAddr = 5555;
	ret =RsSendWr(phyId, rdevIndex, qpn, &wr, &rs_wr_info);
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

void tc_rs_ssl_test1()
{
    int ret;
    uint32_t phyId = 0;
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
	int try_num;

    cfg.chipId = 0;
    cfg.hccpMode = NETWORK_OFFLINE;
    ret = RsInit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    listen[0].phyId = 10;
	listen[0].port = 16666;
    ret = RsSocketListenStart(&listen[0], 1);
    EXPECT_INT_NE(ret, 0);

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
    RsSocketWhiteListAdd(rdev_info, &white_list, 1);;

    conn[0].phyId = 0;
	conn[0].family = AF_INET;
    conn[0].localIp.addr.s_addr = inet_addr("127.0.0.1");
    conn[0].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
    strcpy(conn[0].tag, "1234");
	conn[0].port = 16666;
    ret = RsSocketBatchConnect(&conn[0], 1);
	usleep(SLEEP_TIME); //wait for epoll thread start up..

	i = 0;
	socket_info[i].family = AF_INET;
	socket_info[i].remoteIp.addr.s_addr = inet_addr("127.0.0.1");
	socket_info[i].localIp.addr.s_addr = inet_addr("127.0.0.1");
	strcpy(socket_info[i].tag, "1234");
	try_num = 10;
	do {
		ret = RsGetSockets(RS_CONN_ROLE_CLIENT, &socket_info[i], 1);
        usleep(100*1000);
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
        usleep(100*1000);
	} while (ret != 1 && try_num--);
	rs_ut_msg("%s [server]socket_info[1].fd:%d, status:%d\n",
		__func__, socket_info[i].fd, socket_info[i].status);

	usleep(SLEEP_TIME); //wait for epoll thread start up..

	rs_ut_msg("++++++++++++++ RS QP PREPARE DONE ++++++++++++++\n");


    ret = RsDeinit(&cfg);
    EXPECT_INT_EQ(ret, 0);

    return;
}

void tc_rs_get_ifaddrs()
{
	DlHalInit();
	int ret;
	int phyId = 0;
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 0;
	unsigned int ifaddr_num = 4;
	struct IfaddrInfo ifaddr_infos[4] = {0};
	ret = RsGetIfaddrs(ifaddr_infos, &ifaddr_num, phyId);
	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
	return;
}

void tc_rs_get_ifaddrs_v2()
{
	DlHalInit();
	int ret;
	int phyId = 0;
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 0;
	unsigned int ifaddr_num = 4;
	struct InterfaceInfo interface_infos[4] = {0};
	ret = RsGetIfaddrsV2(interface_infos, &ifaddr_num, phyId, 0);
	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
	return;
}

void tc_rs_get_ifnum()
{
	DlHalInit();
	int ret;
	int phyId = 0;
	unsigned int ifnum = 4;
	bool is_all = false;

	ret = RsGetIfnum(phyId, is_all, &ifnum);

	mocker(RsGetDeviceType, 1, 1);
	mocker((stub_fn_t)RsCheckDstInterface, 100, -1);
	ret = RsFillIfnum(phyId, is_all, &ifnum, 0);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	mocker(RsGetDeviceType, 1, 1);
    mocker((stub_fn_t)RsCheckDstInterface, 100, 1);
	ret = RsFillIfnum(phyId, is_all, &ifnum, 0);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();
	DlHalDeinit();
	return;
}

void tc_rs_get_interface_version()
{
	int version;
	int ret = RsGetInterfaceVersion(0, NULL);
	EXPECT_INT_EQ(ret, -EINVAL);

	ret = RsGetInterfaceVersion(0, &version);
	EXPECT_INT_EQ(ret, 0);
}

extern int kmc_dec_data(struct kmc_enc_info *enc_info, unsigned char *outbuf, unsigned int *size_out);
void tc_rs_ssl_test2()
{
	int ret;
	uint32_t chipId = 0;
	struct rs_cb *rs_cb;
	struct RsInitConfig cfg = {0};
	/* +++++Resource Prepare+++++ */
	cfg.chipId = 0;
	cfg.hccpMode = NETWORK_OFFLINE;
	ret = RsInit(&cfg);
	EXPECT_INT_EQ(ret, 0);
	RsDev2rscb(chipId, &rs_cb, false);
	struct RsSecPara *rs_para = (struct RsSecPara *)calloc(1, sizeof(struct RsSecPara));
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

struct RsConnInfo g_conn = {0};
int stub_RsFd2conn(int fd, struct RsConnInfo **conn)
{

    *conn = &g_conn;
    return 0;
}



void tc_rs_tls_inner_enable()
{
    int ret;
    struct rs_cb rscb = {0};
    rscb.sslEnable = 1;

    ret = rs_tls_inner_enable(&rscb, 1);
    EXPECT_INT_NE(0, ret);

	ret = rs_tls_inner_enable(&rscb, 0);


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

void tc_rs_rs_ssl_crl_init()
{
    int ret;
    struct rs_cb rscb = {0};
    rscb.sslEnable = 1;
    struct RsCertSkidSubjectCb *skidSubjectCb = malloc(sizeof(struct RsCertSkidSubjectCb));
    rscb.skidSubjectCb = skidSubjectCb;
    rscb.hccpMode = NETWORK_PEER_ONLINE;
    rscb.chipId = 0;
    struct tls_cert_mng_info mng_info = {0};

    rscb.clientSslCtx = SSL_CTX_new(TLS_client_method());
    ret = rs_ssl_crl_init(rscb.clientSslCtx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    rscb.serverSslCtx = SSL_CTX_new(TLS_client_method());
    ret = rs_ssl_crl_init(rscb.serverSslCtx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    rs_ssl_deinit(&rscb);
}

void tc_rs_tls_peer_cert_verify()
{
    int ret;
    SSL ssl = {0};

    ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)SSL_get_verify_result, 10, X509_V_ERR_CERT_HAS_EXPIRED);
    ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();

    mocker((stub_fn_t)SSL_get_verify_result, 10, 11);
    ret = rs_tls_peer_cert_verify(&ssl, &gRsCb);
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
    struct RsCerts certs = {0};

    ret = tls_get_cert_chain(&store, &certs, &mng_info);
    EXPECT_INT_EQ(0, ret);
}

void tc_rs_ssl_skid_get_from_chain()
{
    int ret;
    struct tls_cert_mng_info mng_info;
    mng_info.cert_count = 2;
    struct RsCerts certs = {0};
	struct tls_ca_new_certs new_certs[RS_SSL_NEW_CERT_CB_NUM] = {{0}};
    struct rs_cb rscb = {0};
    struct RsCertSkidSubjectCb skidSubjectCb = {0};
    rscb.skidSubjectCb = &skidSubjectCb;

    ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
    EXPECT_INT_EQ(0, ret);

    rscb.skidSubjectCb = NULL;
    ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_ssl_skids_subjects_get, 20, -1);
    mocker(memset_s, 20, -1);
	ret = rs_ssl_skid_get_from_chain(&rscb, &mng_info, &certs, &new_certs);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

    free(rscb.skidSubjectCb);
    rscb.skidSubjectCb = NULL;
}

void tc_rs_check_pridata()
{
    int ret;
    struct tls_cert_mng_info mng_info = {0};;
    mng_info.cert_count = 2;
    struct rs_cb rscb = {0};
    struct RsCertSkidSubjectCb skidSubjectCb = {0};
    rscb.skidSubjectCb = &skidSubjectCb;
    rscb.hccpMode = NETWORK_OFFLINE;
    rscb.chipId = 0;
    rscb.serverSslCtx = SSL_CTX_new(TLS_server_method());
	mng_info.ky_enc_len = 1678;
    mng_info.pwd_len = 16;
    ret = rs_check_pridata(rscb.serverSslCtx, &rscb, &mng_info);
    EXPECT_INT_EQ(0, ret);

    SSL_CTX_free(rscb.serverSslCtx);
    rscb.serverSslCtx = NULL;
}

// void tc_rs_peer_get_ifaddrs()
// {
//     bool is_all = false;
//     int ret;

//     ret = rs_peer_get_ifaddrs(NULL, NULL, 0);
//     EXPECT_INT_EQ(-EINVAL, ret);

//     struct interface_info interface_infos[100] = {0};
//     unsigned int num = 100;
// 	unsigned int if_num = 1000;

// 	struct rs_init_config cfg = {0};
// 	ret = rs_init(&cfg);
// 	EXPECT_INT_EQ(ret, 0);
// 	rs_ut_msg("RS INIT, ret:%d !\n", ret);

// 	ret = rs_peer_get_ifnum(0, &if_num);
// 	EXPECT_INT_EQ(ret, 0);

//     ret = rs_peer_get_ifaddrs(interface_infos, &num, 0);
//     EXPECT_INT_EQ(0, ret);
// 	EXPECT_INT_EQ(if_num, num);

// 	ret = rs_deinit(&cfg);
// 	EXPECT_INT_EQ(ret, 0);

//     ret = rs_get_ifaddrs(NULL, NULL, 0);
//     EXPECT_INT_EQ(-EINVAL, ret);

//     char ifa_name = 0;
//     ret = rs_check_dst_interface(0, &ifa_name, RS_HARDWARE_PCIE, is_all);
//     EXPECT_INT_EQ(0, ret);
// }

void tc_rs_peer_get_ifnum()
{
    int ret;
	int num = 0;

    ret = RsPeerGetIfnum(0, &num);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = RsPeerGetIfnum(0, NULL);
    EXPECT_INT_EQ(-EINVAL, ret);
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

void tc_rs_set_fd_nonblock()
{
    int ret;
    ret = RsSetFdNonblock(-1);
    EXPECT_INT_EQ(-EFILEOPER, ret);
}

void tc_rs_epoll_event_ssl_listen_in_handle()
{
    struct rs_cb rs_cb;
    struct RsListenInfo listen_info;
    struct sockaddr_in remote_ip;
    (void)RsEpollEventSslListenInHandle(&rs_cb, &listen_info, 0, remote_ip);
}

void tc_rs_ssl_recv_tag_in_handle()
{
    struct RsAcceptInfo accept_info;
    accept_info.serverIpAddr.family = AF_INET;
    accept_info.serverIpAddr.binAddr.addr.s_addr = 1;
    accept_info.clientIpAddr.family = AF_INET;
    accept_info.clientIpAddr.binAddr.addr.s_addr = 1;
    accept_info.connFd = 1;
    accept_info.sockPort = 0;
    struct RsConnInfo conn_tmp;
    char tag[256] = {0};
    memcpy_s(conn_tmp.tag, 256, tag, 256);
    mocker(SSL_read, 20, 256);
    (void)RsSslRecvTagInHandle(&accept_info, &conn_tmp);
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
	rdevCb.notifyType = EVENTID;

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

    ret = RsConnectHandle(&cfg);
    EXPECT_INT_EQ(ret, NULL);
    return;
}

extern void RsCloseRoceUserSo(void);
void tc_RsRoceUserApiInit()
{
	RsRoceUserApiInit();
	RsCloseRoceUserSo();
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

	ret = RsNotifyCfgSet(129, va, size);
	EXPECT_INT_EQ(-EINVAL, ret);
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

// lkk
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
	qp_cb.rdevCb->rs_cb->hccpMode == NETWORK_PEER_ONLINE;
	ret = RsDrvNormalQpCreateInit(&qp_init_attr, &qp_cb, &attr);
	mocker_clean();

	struct ibv_qp ib_pd = {0};
	struct ibv_qp ibQp = {0};
	struct ibv_cq ibSendCq = {0};
	struct ibv_cq ev_cq = {0};
	struct ibv_wc wc = {0};
	// struct rq_attr attr = {0};


	qp_cb.ibQp= &ibQp;
	qp_cb.ibPd= &ib_pd;
	qp_cb.ibSendCq = &ibSendCq;
	rs_cb.chipId = 0;
	
    mocker(RsIbvQueryQp, 5, -1);
	ret = RsDrvQpNormal(&qp_cb, 0);
	mocker_clean();

	ret = RsDrvPostRecv(NULL, NULL, 0, NULL);
	EXPECT_INT_EQ(ret, -22);

	mocker(calloc, 1, NULL);
	ret = RsDrvPostRecv(NULL, NULL, 1, NULL);
	EXPECT_INT_EQ(ret, -28);
    mocker_clean();

	RsCqeCallbackProcess(&qp_cb, &wc, &ev_cq);

	return;
}

void tc_rs_drv_exp_qp_create()
{
	struct RsQpCb qp_cb = {0};
	struct RsRdevCb rdevCb = {0};
	struct ibv_pd ib_pd = {0};
	struct rs_cb rs_cb = {0};
	int qpMode;

	qp_cb.rdevCb = &rdevCb;
	qp_cb.ibPd = &ib_pd;
	qp_cb.rdevCb->rs_cb = &rs_cb;

	mocker(RsDrvExpQpCreateInit, 20, 0);
	mocker(RsIbvExpCreateQp, 20, &ib_pd);
	mocker(RsIbvQueryQp, 20, 1);
	mocker(RsIbvDestroyQp, 20, 1);
	qpMode = 2;
	RsDrvExpQpCreate(&qp_cb, qpMode);
	mocker_clean();

	mocker(RsDrvExpQpCreateInit, 20, 0);
	mocker(RsIbvExpCreateQp, 20, &ib_pd);
	mocker(RsIbvQueryQp, 20, 1);
	mocker(RsIbvDestroyQp, 20, 1);
	qpMode = 1;
	RsDrvExpQpCreate(&qp_cb, qpMode);
	mocker_clean();
	return;
}

void tc_rs_drv_exp_qp_create_init()
{
	struct ibv_exp_qp_init_attr qp_init_attr = {0};
	struct RsQpCb qp_cb = {0};
	struct ibv_port_attr attr = {0};
	struct rs_cb rs_cb = {0};
	struct RsRdevCb rdevCb = {0};
	int ret;
	qp_cb.rdevCb = &rdevCb;
	qp_cb.rdevCb->rs_cb = &rs_cb;
	qp_cb.rdevCb->rs_cb->hccpMode == NETWORK_ONLINE;

	qp_cb.txDepth = 10;
	qp_cb.rxDepth = 10;

	mocker(memset_s, 10, 0);
	qp_cb.qpMode = 2;
	ret = RsDrvExpQpCreateInit(&qp_init_attr, &qp_cb, &attr);
	mocker_clean();

	return;
}

void tc_rs_qpcb_init()
{
	struct RsRdevCb rdevCb = {0};
	struct RsQpNorm qp_norm = {0};
	struct RsQpCb qp_cb = {0};
	struct rs_cb rs_cb = {0};
	int qpMode = 1;
	int ret = 0;

	qp_norm.qpMode = 1;
	rdevCb.rs_cb = &rs_cb;

	ret = RsQpcbInit(&rdevCb, &qp_cb, &qp_norm);
	
	return;
}

// void tc_rs_qp_query_info()
// {
// 	unsigned int phyId = 15;
// 	unsigned int rdevIndex = 0;
// 	struct RsRdevCb rdevCb = {0};
// 	int qpMode = 1;
// 	int ret;

// 	mocker(rsGetLocalDevIDByHostDevID, 1, 0);
// 	mocker(RsDev2rscb, 1, 0);
// 	mocker(RsGetRdevCb, 1, 0);
// 	ret = RsQpQueryInfo(phyId, rdevIndex, &&rdevCb, qpMode);
//  mocker clean;

// 	return;
// }
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
    struct rs_cb rs_cb = {0};
    struct epoll_event events = {0};
    rs_cb.sslEnable = 1;

    mocker((stub_fn_t)RsEpollEventListenInHandle, 1, -ENODEV);
    mocker((stub_fn_t)RsEpollEventSslAcceptInHandle, 5, 0);
    (void)RsEpollEventInHandle(&rs_cb, &events);
    mocker_clean();

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
	// RsDrvPollCqHandle(qp_cb);


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

	/* callback_flag = 1 */
	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, 0);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	mocker_invoke((stub_fn_t)ibv_get_cq_event, stub_ibv_get_cq_event, 10);
	mocker((stub_fn_t)ibv_poll_cq, 10, -1);
	RsDrvPollCqHandle(qp_cb);
	mocker_clean();

	qp_cb->ibSendCq = ib_send_cq_t;
//rs_poll_cq_handle end

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

	unsigned int result = RsGetHccpMode(0);
	EXPECT_INT_EQ(ret, 0);

	ret = RsSocketInit(NULL, 0);
	EXPECT_INT_EQ(ret, -22);

    unsigned int vnic_ip[8] = {0};
	ret = RsSocketInit(vnic_ip, 8);
	EXPECT_INT_EQ(ret, 0);

	mocker(memcpy_s, 5, -1);
	ret = RsSocketInit(vnic_ip, 8);
	EXPECT_INT_EQ(ret, -256);

    struct rdev rdev_info;
	rdev_info.phyId = 128;
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, -22);

    rdev_info.phyId = 0;
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);

	rdev_info.family = AF_INET6;
	mocker(rsGetLocalDevIDByHostDevID, 5, 0);
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);
	mocker_clean();

	rdev_info.family = AF_INET;
	mocker(rsGetLocalDevIDByHostDevID, 5, 0);
	ret = RsSocketDeinit(rdev_info);
	EXPECT_INT_EQ(ret, -19);
	mocker_clean();

	rdev_info.phyId = 128;
	struct rs_cb rs_cb ={0};
    ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	EXPECT_INT_EQ(ret, -22);

	rdev_info.phyId = 0;
    ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	EXPECT_INT_EQ(ret, -19);

	rdev_info.phyId = 0;
	mocker(rsGetLocalDevIDByHostDevID, 5, -1);
    ret = RsGetRsCb(rdev_info.phyId, &rs_cb);
	EXPECT_INT_EQ(ret, -1);
	mocker_clean();

    ret = RsRdevDeinit(0, 0, 0);
	EXPECT_INT_EQ(ret, -19);

    ret = RsRdevDeinit(128, 0, 0);
	EXPECT_INT_EQ(ret, -22);

	ret = RsGetVnicIp(128, NULL);
	EXPECT_INT_EQ(ret, -22);

	ret = RsGetVnicIp(8, NULL);
	EXPECT_INT_EQ(ret, -22);

	ret = RsGetVnicIp(8, vnic_ip);
	EXPECT_INT_EQ(ret, 0);

	ret = RsGetInterfaceVersion(0, NULL);
	EXPECT_INT_EQ(ret, -22);

	unsigned int version[1] ={0};
	ret = RsGetInterfaceVersion(0, version);
	EXPECT_INT_EQ(ret, 0);


	DlHalDeinit();
    return;
}

void tc_tls_abnormal()
{
	struct RsConnInfo conn = {0};
	SSL ssl = {0};
	conn.ssl = &ssl;

	RsDrvSocketRecv(-1, NULL, 0, 0);
	RsDrvSocketSend(-1, NULL, 0, 0);
	RsDrvSslBindFd(&conn, 0);
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
	struct RsQpResp qp_resp = {0};
	struct RsQpResp qp_resp2 = {0};
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

	usleep(SLEEP_TIME); //wait for epoll thread start up..

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

#if 1
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
    attr2.sendCqEventId = 1;
    attr2.recvCqEventId = 2;
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
	// void **qp2 = NULL;
	struct ibv_qp* qp2;
    ret = RsNormalQpCreate(phyId, rdevIndex, &qp_init_attr2, &qp_resp2, &qp2);
    EXPECT_INT_EQ(0, ret);


	rs_ut_msg("___________________after qp2 create:\n");

	ret = RsQpConnectAsync(phyId, rdevIndex, qp_resp2.qpn, socket_info[i].fd);

	rs_ut_msg("___________________after qp2 connect async:\n");
#endif

#if 1
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
#endif
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

void tc_rs_get_device_type()
{
	DlHalInit();
	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info_pod_16p, 20);
	int ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info_pod, 20);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);

	mocker((stub_fn_t)rsGetLocalDevIDByHostDevID, 20, 0);
	mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, dl_hal_get_device_info_910A, 20);
	ret = RsGetDeviceType(0);
	EXPECT_INT_EQ(ret, 0);

	mocker_clean();
	DlHalDeinit();
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
	struct SocketEventInfoT event_info[1];
	unsigned int events_num = 0;
	int ret;
	int fd;

	ret = RsWaitEventHandle(-1, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsCreateEventHandle(&fd);
	EXPECT_INT_EQ(0, ret);
	ret = RsWaitEventHandle(fd, NULL, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsWaitEventHandle(fd, event_info, -2, -1, NULL);
	EXPECT_INT_EQ(-EINVAL, ret);

	ret = RsWaitEventHandle(fd, event_info, 0, 1, &events_num);
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

int stub_dl_hal_get_chip_info_910_93(unsigned int dev_id, halChipInfo *chip_info)
{
    strcpy(chip_info->name, "910_93xx");

    return 0;
}

void tc_rs_socket_get_bind_by_chip()
{
	unsigned int chipId = 0;
	bool bind_ip = false;

	mocker(DlDrvDeviceGetIndexByPhyId, 1, -1);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker(DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker(DlHalGetDeviceInfo, 1, -2);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker(DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker(DlHalGetDeviceInfo, 1, 0);
	mocker(DlHalGetChipInfo, 1, -2);
	RsSocketGetBindByChip(chipId, &bind_ip);
	mocker_clean();

	mocker(DlDrvDeviceGetIndexByPhyId, 1, 0);
	mocker(DlHalGetDeviceInfo, 1, 0);
	mocker_invoke(DlHalGetChipInfo, stub_dl_hal_get_chip_info_910_93, 100);
	RsSocketGetBindByChip(chipId, &bind_ip);
	EXPECT_INT_EQ(bind_ip, true);
	mocker_clean();
}

void tc_rs_get_ifaddrs1()
{
	DlHalInit();
	gRsCb = malloc(sizeof(struct rs_cb));
	gRsCb->hccpMode = 1;
	int ret;
	int phyId = 0;
	unsigned int ifaddr_num = 4;
	struct IfaddrInfo ifaddr_infos[4] = {0};
	bool is_all = false;

	ifaddr_num = 0;
	ret = RsFillIfaddrInfos(ifaddr_infos, &ifaddr_num, phyId);

	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 1);

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = RsCheckDstInterface(phyId, "eth0", 1, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)strncmp, 2, 2);
	ret = RsCheckDstInterface(phyId, "eth0", 1, true);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = RsCheckDstInterface(phyId, "eth0", 0, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, 2);
	ret = RsCheckDstInterface(phyId, "bond0", 2, is_all);
	EXPECT_INT_EQ(ret, 0);
	mocker_clean();

	mocker((stub_fn_t)snprintf_s, 2, -1);
	ret = RsCheckDstInterface(phyId, "bond0", 2, is_all);
	EXPECT_INT_EQ(ret, -EAGAIN);
	mocker_clean();

	free(gRsCb);
	gRsCb = NULL;
	DlHalDeinit();
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

void tc_rs_ssl_get_cert()
{
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

void tc_rs_peer_socket_recv()
{
    struct SocketConnectInfo conn[10] = {0};
    struct SocketErrInfo  err[10] = {0};
    int ret = 0;

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

void tc_rs_remap_mr()
{
    struct MemRemapInfo memList[1] = {0};
    struct RsMrCb mr_cb = {0};
    struct ibv_mr ibMr = {0};

    mocker_clean();
    RS_INIT_LIST_HEAD(&g_rdev_cb.typicalMrList);
    mocker(rsGetLocalDevIDByHostDevID, 1, 0);
    mocker_invoke((stub_fn_t)RsRdev2rdevCb, stub_RsRdev2rdevCb, 10);
    mocker(RsRoceRemapMr, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RsRemapMr(0, 0, memList, 1);
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
    RsRemapMr(0, 0, memList, 1);
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
    unsigned int phyId = 0;
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

void tc_rs_get_hccn_cfg()
{
	char *value[2048] = {0};
	unsigned int value_len = 2048;

    int ret;

    mocker(FileReadCfg, 2, 0);
	ret = RsGetHccnCfg(0, HCCN_CFG_UDP_PORT_MODE, value, &value_len);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();
}

void tc_dl_hal_set_clear_user_config()
{
    DlHalSetUserConfig(0, NULL, NULL, 1);
    DlHalClearUserConfig(0, NULL);
}

void tc_rs_free_dev_list(void)
{
    struct rs_cb rscb = {0};

    // list empty
    RS_INIT_LIST_HEAD(&rscb.rdevList);
    RsFreeUdevList(&rscb);

    // protocol invalid
    rscb.protocol = PROTOCOL_UNSUPPORT;
    RsFreeDevList(&rscb);
}

void tc_rs_free_rdev_list(void)
{
    struct RsRdevCb rdevCb = {0};
    struct rs_cb rscb = {0};

    // rsGetDevIDByLocalDevID fail
    rscb.protocol = PROTOCOL_RDMA;
    mocker(rsGetDevIDByLocalDevID, 1, -1);
    RsFreeRdevList(&rscb);
    mocker_clean();

    // RsRdevDeinit fail
	RS_INIT_LIST_HEAD(&rscb.rdevList);
    RsListAddTail(&rdevCb.list, &rscb.rdevList);
    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(RsRdevDeinit, 1, -1);
    RsFreeRdevList(&rscb);
    mocker_clean();

    // success
    mocker(rsGetDevIDByLocalDevID, 1, 0);
    mocker(RsRdevDeinit, 1, 0);
    RsFreeRdevList(&rscb);
    mocker_clean();
}

void tc_rs_free_udev_list(void)
{
    struct rs_ub_dev_cb udev_cb = {0};
    struct rs_cb rscb = {0};

    // rs_ub_ctx_deinit fail
	RS_INIT_LIST_HEAD(&rscb.rdevList);
    mocker(rs_ub_ctx_deinit, 1, -1);
    RsListAddTail(&udev_cb.list, &rscb.rdevList);
    RsFreeUdevList(&rscb);
    mocker_clean();

    // success
    mocker_clean();
    mocker(rs_ub_ctx_deinit, 1, 0);
    RsFreeUdevList(&rscb);
    mocker_clean();
}

void tc_rs_drv_create_cq_with_attrs_abnormal()
{
    struct CqExtAttr cqAttr = {0};
    struct RsRdevCb rdevCb = {0};
    struct RsQpCb qp_cb = {0};
    struct ibv_cq scq = {0};
    int isExt = 0;
    int ret;

    qp_cb.rdevCb = &rdevCb;
    mocker_ret(RsIbvCreateCq, &scq, NULL, NULL);
    mocker(RsIbvDestroyCq, 1, 0);
    ret = RsDrvCreateCqWithAttrs(&qp_cb, isExt, &cqAttr);
    EXPECT_INT_EQ(ret, -ENOMEM);
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