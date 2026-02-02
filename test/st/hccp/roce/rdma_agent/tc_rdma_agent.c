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
extern STATIC int RaHdcNotifyBaseAddrInit(unsigned int notifyType, unsigned int phyId, unsigned long long **notifyVa);
extern int RaHdcLitePostSend(struct RaQpHandle *qp_hdc, struct LiteMrInfo *local_mr,
    struct LiteMrInfo *rem_mr, struct LiteSendWr *wr, struct SendWrRsp *wr_rsp);
extern void RaHdcGetOpcodeLiteSupport(unsigned int phyId, unsigned int supportFeature, int *support);
extern void RsSetCtx(unsigned int phyId);
extern int RsSocketGetClientSocketErrInfo(struct SocketConnectInfo conn[], struct SocketErrInfo err[],
    unsigned int num);
extern int RsSocketGetServerSocketErrInfo(struct SocketListenInfo conn[], struct ServerSocketErrInfo err[],
    unsigned int num);
extern int ra_hdc_get_tlv_recv_msg(struct TlvMsg *recv_msg, char *recv_data);
extern struct RaSocketOps gRaPeerSocketOps;
extern int RaRdevInitCheckIp(int mode, struct rdev rdevInfo, char localIp[]);
extern int HdcSendRecvPkt(void *session, void *p_send_rcv_buf, unsigned int in_buf_len, unsigned int out_data_len);
extern int RaHdcGetLiteSupport(struct RaRdmaHandle *rdmaHandle, unsigned int phyId);
extern int RaPeerLoopbackQpCreate(struct RaRdmaHandle *rdma_handle, struct LoopbackQpPair *qp_pair,
    void **qp_handle);
extern int RaPeerLoopbackSingleQpCreate(struct RaRdmaHandle *rdma_handle, struct RaQpHandle **qp_handle,
    struct ibv_qp **qp);
extern int RaPeerLoopbackQpModify(struct RaQpHandle *qp_handle0, struct RaQpHandle *qp_handle1);
extern void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offsize);
extern int open(const char *pathname, int flags);

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
extern int HostNotifyBaseAddrInit(unsigned int phyId);
extern int HostNotifyBaseAddrUninit(unsigned int phyId);
extern int RaPeerSetConnParam(struct SocketInfoT conn[],
    struct SocketFdData rs_conn[], unsigned int i, int buf_size);
extern int DlDrvDeviceGetIndexByPhyId(uint32_t phyId, uint32_t *devIndex);
extern int DlHalGetDeviceInfo(uint32_t devId, int32_t moduleType, int32_t infoType, int64_t *value);
extern int DlHalMemCtl(int type, void *paramValue, size_t paramValueSize, void *outValue, size_t *outSizeRet);
extern void RaHdcLiteMutexDeinit(struct RaRdmaHandle *rdmaHandle);
extern void RaRdmaLiteFreeCtx(struct rdma_lite_context *liteCtx);
extern int DlHalSensorNodeUnregister(uint32_t devid, uint64_t handle);
extern int RaSensorNodeRegister(unsigned int phyId, struct RaRdmaHandle *rdmaHandle);
extern int RaHdcLiteMutexInit(struct RaRdmaHandle *rdmaHandle, unsigned int phyId);
extern int RaHdcLiteGetCqQpAttr(struct RaQpHandle *qpHdc, struct rdma_lite_cq_attr *liteSendCqAttr,
    struct rdma_lite_cq_attr *liteRecvCqAttr, struct rdma_lite_qp_attr *liteQpAttr);
extern int RaHdcLiteInitMemPool(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc,
    struct rdma_lite_cq_attr *liteSendCqAttr, struct rdma_lite_cq_attr *liteRecvCqAttr,
    struct rdma_lite_qp_attr *liteQpAttr);
extern int RaRdmaLiteDestroyCq(struct rdma_lite_cq *liteCq);
extern void RaHdcLiteDeinitMemPool(struct RaRdmaHandle *rdmaHandle, struct RaQpHandle *qpHdc);
extern void RaHdcLiteQpAttrInit(struct RaQpHandle *qpHdc, struct rdma_lite_qp_attr *liteQpAttr,
    struct rdma_lite_qp_cap *cap);
extern int RaRdmaLiteInitMemPool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr);
extern int RaRdmaLiteDeinitMemPool(struct rdma_lite_context *lite_ctx, u32 mem_idx);
extern int RaHdcGetCqeErrInfoNum(struct RaRdmaHandle *rdmaHandle, unsigned int *num);
extern struct rdma_lite_context *RaRdmaLiteAllocCtx(u8 phyId, struct dev_cap_info *cap);
extern struct rdma_lite_cq *RaRdmaLiteCreateCq(struct rdma_lite_context *lite_ctx, 
    struct rdma_lite_cq_attr *lite_cq_attr);
extern int RaRdmaLiteDestroyQp(struct rdma_lite_qp *liteQp);
extern struct rdma_lite_qp *RaRdmaLiteCreateQp(struct rdma_lite_context *lite_ctx,
    struct rdma_lite_qp_attr *lite_qp_attr);
extern int RsSocketAcceptCreditAdd(struct SocketListenInfo conn[], uint32_t num, unsigned int credit_limit);
extern int RaPeerSocketAcceptCreditAdd(unsigned int phyId, struct SocketListenInfoT conn[], unsigned int num,
    unsigned int creditLimit);
extern hdcError_t DlHalHdcRecv(HDC_SESSION session, struct drvHdcMsg *pMsg, int bufLen, UINT64 flag,
    int *recvBufCount, UINT32 timeout);
extern hdcError_t DlDrvHdcAllocMsg(HDC_SESSION session, struct drvHdcMsg **ppMsg, int count);
extern hdcError_t DlDrvHdcFreeMsg(struct drvHdcMsg *msg);
extern hdcError_t DlDrvHdcSessionClose(HDC_SESSION session);
extern int RsRemapMr(unsigned int phyId, unsigned int rdev_index, struct MemRemapInfo mem_list[],
    unsigned int mem_num);

int stub_ra_hdc_process_msg_wrlist(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 1;
    return 0;
}

int stub_ra_hdc_process_msg_wrlist_3(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpSendWrlistData *send_wrlist = (union OpSendWrlistData *)data;
    send_wrlist->rxData.completeNum = 5;
    return 0;
}

int stub_ra_get_interface_version(unsigned int phyId, unsigned int interface_opcode, unsigned int* interface_version)
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
	struct RaInitConfig online_config = {
		.phyId = 10,
		.nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
	};

	ret = RaInit(&online_config);
	return;
}

static int ra_get_interface_version_stub(unsigned int phyId, unsigned int interface_opcode, unsigned int* interface_version)
{
    *interface_version = g_interface_version;
    return 0;
}

int stub_rs_get_lite_support(unsigned int phyId, unsigned int rdevIndex, int *support_lite)
{
	return 0;
}

int stub_ra_hdc_process_rdev_init(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    union OpRdevInitData *rdev_init_data;
    union OpLiteSupportData *lite_support_data;

    if (opcode == RA_RS_RDEV_INIT) {
        rdev_init_data = (union rdev_init_data *)data;
        rdev_init_data->rxData.rdevIndex = 0;
    } else if (opcode == RA_RS_GET_LITE_SUPPORT) {
        lite_support_data = (union OpLiteSupportData *)data;
        lite_support_data->rxData.supportLite = 1;
    }
    return 0;
}

void stub_ra_hdc_get_opcode_lite_support(unsigned int phyId, unsigned int support_feature, int *support)
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
    struct RaInitConfig offline_hdc_config = {
        .phyId = 0,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
        .enableHdcAsync = false,
    };
    struct ProcessRaSign p_ra_sign;
    p_ra_sign.tgid = 0;

    mocker_clean();
    mocker((stub_fn_t)drvHdcClientCreate, 1, 0);
    mocker_invoke((stub_fn_t)drvHdcSessionConnect, (stub_fn_t)stub_session_connect_hdc, 1);
    mocker((stub_fn_t)drvHdcSetSessionReference, 1, 0);
    mocker((stub_fn_t)halHdcRecv, 10, 0);
    int ret = RaHdcInit(&offline_hdc_config, p_ra_sign);
    EXPECT_INT_EQ(ret, 0);
}

void tc_hdc_env_deinit()
{
    struct RaInitConfig offline_hdc_config = {
        .phyId = 0,
        .nicPosition = NETWORK_OFFLINE,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
        .enableHdcAsync = false,
    };

    mocker((stub_fn_t)halHdcRecv, 10, 0);
    mocker((stub_fn_t)drvHdcSessionClose, 1, 0);
    mocker((stub_fn_t)drvHdcClientDestroy, 1, 0);
    int ret = RaHdcDeinit(&offline_hdc_config);
    EXPECT_INT_EQ(ret, 0);
	mocker_clean();
}

int ra_hdc_get_lite_support_stub(struct RaRdmaHandle *rdma_handle, unsigned int phyId)
{
    rdma_handle->supportLite = 1;
    return 0;
}

extern int RaRdmaLitePollCq(struct rdma_lite_cq *liteCq, int numEntries, struct rdma_lite_wc *liteWc);
int stub_RaRdmaLitePollCq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
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

int stub_ra_hdc_send_wr_v2(struct RaQpHandle *qp_hdc, struct SendWrV2 *wr, struct SendWrRsp *op_rsp)
{
    return 0;
}

int ra_peer_loopback_single_qp_create_stub(struct RaRdmaHandle *rdma_handle, struct RaQpHandle **qp_handle,
    struct ibv_qp **qp)
{
    static int call_num = 0;
    int ret = 0;
    call_num++;

    if (call_num == 1) {
        return RaPeerLoopbackSingleQpCreate(rdma_handle, qp_handle, qp);
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
	int qpMode = 1;
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
	struct RaInitConfig offline_config = {
		.phyId = 0,
		.nicPosition = 1,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
	};

    struct RaGetIfattr offline_ifattr_config = {
		.phyId = 0,
		.nicPosition = 1,
        .isAll = 0,
	};

	long size_notify = 0;
	struct SendWr wr;
	struct SgList mem;
	u32 wqe_index;

	mem.addr = addr1;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
    struct SendWrRsp op_rsp;

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
	HccpInit(dev_id, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
	sleep(2);
	RaInit(&offline_config);
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;

    unsigned int ip = inet_addr("127.0.0.1");
    unsigned int remote_ip[] = {inet_addr("127.0.0.1")};
    int num = 1;
	struct MrInfoT mr_info;
    RaSocketListenStart(NULL, 0);
    RaSocketListenStop(NULL, 0);
    RaGetSockets(0, NULL, 0, NULL);
    RaSocketSend(NULL, NULL, 0, NULL);
    RaSocketRecv(NULL, NULL, 0, NULL);
    RaGetQpStatus(NULL, NULL);
    RaMrReg(NULL, NULL);
    RaMrDereg(NULL, NULL);
    RaSendWr(NULL, NULL, NULL);
	RaSendWrlist(NULL, NULL, NULL, 0, NULL);
    RaGetNotifyBaseAddr(NULL, NULL, NULL);
    RaGetNotifyMrInfo(NULL, NULL);
    RaQpDestroy(NULL);
    RaQpConnectAsync(NULL, NULL);
    RaRdevDeinit(NULL, NOTIFY);
	RaSocketDeinit(NULL);
    RaRegisterMr(NULL, NULL, NULL);
    RaDeregisterMr(NULL, NULL);
    RaSocketInit(0, rdev_info, NULL);
    RaSendWrlistExt(NULL, NULL, NULL, 0, NULL);
    RaRecvWrlist(NULL, NULL, 0, NULL);

    ret = RaSendWrV2(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct RaQpHandle ra_qp_handle_v2 = {0};
    struct SendWrV2 wr_v2 = {0};
    struct SendWrRsp op_rsp_v2 = {0};
    struct SgList list_v2 = {0};
    list_v2.len = 0xFFFFFFFF;
    wr_v2.bufList = &list_v2;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    ret = RaQpBatchModify(NULL, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    list_v2.len = 0x1;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaOps rdma_ops_v2 = {0};
    rdma_ops_v2.raSendWrV2 = stub_ra_hdc_send_wr_v2;
    ra_qp_handle_v2.rdmaOps = &rdma_ops_v2;
    ret = RaSendWrV2(&ra_qp_handle_v2, &wr_v2, &op_rsp_v2);
    EXPECT_INT_EQ(0, ret);

    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;

    struct RaSocketHandle *handle1 = NULL;
    ret = RaSocketInit(5, rdev_info, &handle1);
    EXPECT_INT_NE(0, ret);

    ret = RaRdevInitWithBackup(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaHandle *handle2 = NULL;
    ret =  RaRdevInit(5, NOTIFY, rdev_info, &handle2);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaHandle *rdma_handle = NULL;
    struct RaRdmaHandle *rdma_handle2 = NULL;
    ret =  RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaRdevGetHandle(rdev_info.phyId, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);

    struct RaSocketHandle *socket_handle = NULL;
    ret = RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    EXPECT_INT_EQ(0, ret);
    struct SocketHdcInfo *hdc_socket_handle = calloc(1, sizeof(struct SocketHdcInfo));

    conn[0].socketHandle = socket_handle;
    listen[0].socketHandle = socket_handle;
    close[0].socketHandle = socket_handle;
    close[0].fdHandle = hdc_socket_handle;
    socket_info[0].socketHandle = socket_handle;
    socket_info[0].fdHandle = hdc_socket_handle;

    ret = RaSocketSetWhiteListStatus(0);
    EXPECT_INT_EQ(0, ret);
    ret = RaSocketSetWhiteListStatus(3);
    EXPECT_INT_EQ(128203, ret);
    ret = RaSocketGetWhiteListStatus(NULL);
    EXPECT_INT_EQ(128203, ret);
    unsigned int enable;
    ret = RaSocketGetWhiteListStatus(&enable);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetIfnum(NULL, NULL);
    EXPECT_INT_EQ(128303, ret);

    int ifnum = 0;
	offline_ifattr_config.nicPosition = NETWORK_PEER_ONLINE;
    ret = RaGetIfnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);

    offline_ifattr_config.phyId = 129;
    offline_ifattr_config.nicPosition = 1;
    ret = RaGetIfnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(128303, ret);

	ret = RaGetIfaddrs(NULL, NULL, NULL);
	EXPECT_INT_EQ(128303, ret);

	struct RaInitConfig peer_config = {
		.phyId = 0,
		.nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
	};

    struct RaGetIfattr peer_ifattr_config = {
		.phyId = 0,
		.nicPosition = 0,
        .isAll = true,
	};

    unsigned int ifaddr_num = 4;
    offline_ifattr_config.phyId = 0;
    struct InterfaceInfo interface_infos[4] = {0};
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    g_interface_version = 0;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    g_interface_version = 1;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);
    offline_ifattr_config.isAll = true;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_NE(0, ret);
    offline_ifattr_config.isAll = false;
    mocker(memcpy_s, 10 , -1);
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(328306, ret);
    mocker(calloc, 10 , NULL);
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);

    mocker_clean();
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    g_interface_version = 2;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(0, ret);

    g_interface_version = 3;
    ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
    EXPECT_INT_EQ(128303, ret);
    mocker_clean();

    ret = RaGetIfnum(&offline_ifattr_config, &ifnum);
    EXPECT_INT_EQ(0, ret);


	ret = RaGetIfaddrs(&peer_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(0, ret);

	ifaddr_num = 0;
	ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

	ifaddr_num = 9;
	ret = RaGetIfaddrs(&offline_ifattr_config, interface_infos, &ifaddr_num);
	EXPECT_INT_EQ(128303, ret);

	unsigned int interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 0, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 12, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 24, &interface_version);
    EXPECT_INT_EQ(0, ret);

    interface_version = 0;
    ret  = RaGetInterfaceVersion (0, 25, NULL);
    EXPECT_INT_EQ(128303, ret);

    struct SocketWlistInfoT white_list[1];
    ret = RaSocketWhiteListDel(socket_handle, white_list, 1);
	EXPECT_INT_EQ(0, ret);

    ret = RaSocketWhiteListAdd(socket_handle, white_list, 1);
	EXPECT_INT_EQ(0, ret);

	ret = RaSocketListenStart(listen, 1);
	EXPECT_INT_EQ(0, ret);

    ret = RaSocketBatchConnect(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = RaSocketBatchConnect(&conn, 1);
    EXPECT_INT_EQ(0, ret);

    unsigned int connected_num = 0;
	ret = RaGetSockets(server, socket_info, 1, &connected_num);
	EXPECT_INT_EQ(0, ret);

    unsigned long long sent_size = 0;
	ret = RaSocketSend(socket_info[0].fdHandle, data, size, &sent_size);
	EXPECT_INT_EQ(0, ret);

    unsigned long long received_size = 0;
	ret = RaSocketRecv(socket_info[0].fdHandle, data, size, &received_size);
	EXPECT_INT_EQ(0, ret);

    ret = RaSocketSend(hdc_socket_handle, data, 1, &sent_size);
    EXPECT_INT_EQ(0, ret);

    ret = RaSocketRecv(hdc_socket_handle, data, 1, &received_size);
    EXPECT_INT_EQ(0, ret);

    mocker(RaHdcGetSockets, 10 , -1);
    ret = RaGetSockets(server, socket_info, 1, &connected_num);

    mocker(RaHdcSocketSend, 10 , -1);
    ret = RaSocketSend(socket_info[0].fdHandle, data, size, &sent_size);

    mocker(RaHdcSocketRecv, 10 , -1);
    ret = RaSocketRecv(socket_info[0].fdHandle, data, size, &received_size);

	ret = RaSocketListenStop(listen, 1);
	EXPECT_INT_EQ(0, ret);

    ret = RaQpCreate(rdma_handle, 3, 0, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaQpCreateWithAttrs(rdma_handle, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    struct RaQpHandle *qp_handle = NULL;
    struct RaQpHandle *qp_handle_with_attr = NULL;
    struct RaQpHandle *ai_qp_handle = NULL;
    struct RaQpHandle *typical_qp_handle = NULL;
    struct AiQpInfo info;
    ret = RaQpCreateWithAttrs(rdma_handle, NULL, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, NULL, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    struct QpExtAttrs extAttrs;
    extAttrs.version = 0;
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
    extAttrs.qpMode = -1;
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &ai_qp_handle);
    EXPECT_INT_NE(0, ret);
    extAttrs.qpMode = RA_RS_OP_QP_MODE;
    ret = RaQpCreate(rdma_handle, 0, 0, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = RaAiQpCreate(rdma_handle, &extAttrs, &info, &ai_qp_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpDestroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    ret = RaQpDestroy(ai_qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpBatchModify(NULL, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    struct RaRdmaHandle rdma_handle_err = {0};
    rdma_handle_err = *rdma_handle;
    rdma_handle_err.rdevInfo.phyId = 32;

    ret = RaQpBatchModify(&rdma_handle_err, NULL, 0, 0);
    EXPECT_INT_NE(0, ret);

    struct RaQpHandle *batch_modify_qp_hdc[1];
    batch_modify_qp_hdc[0] = qp_handle;

    ret = RaQpBatchModify(rdma_handle, batch_modify_qp_hdc, 1, 5);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpBatchModify(rdma_handle, batch_modify_qp_hdc, 1, 1);
    EXPECT_INT_EQ(0, ret);

    struct QosAttr QosAttr= {0};
    QosAttr.tc = 110;
    QosAttr.sl = 3;
    ret = RaSetQpAttrQos(qp_handle, &QosAttr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrQos(qp_handle, &QosAttr);
    EXPECT_INT_EQ(0, ret);

    unsigned int timeout = 6;
    ret = RaSetQpAttrTimeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrTimeout(qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = RaSetQpAttrRetryCnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSetQpAttrRetryCnt(qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

	unsigned int temp_depth, qp_num;
	ret = RaGetTsqpDepth(NULL, &temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaGetTsqpDepth(rdma_handle, NULL, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

	ret = RaSetTsqpDepth(NULL, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	ret = RaSetTsqpDepth(rdma_handle, temp_depth, NULL);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 1;
	ret = RaSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(128103, ret);

	temp_depth = 8;
	ret = RaSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
	EXPECT_INT_EQ(0, ret);

    ASSERT_ADDR_NE(qp_handle, NULL);

    mr_info.addr = addr;
	mr_info.size = size;
	mr_info.access = access;
    ret = RaMrReg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetNotifyBaseAddr(rdma_handle, &va, &size);
    EXPECT_INT_EQ(0, ret);
    struct MrInfoT mr_info2;
    ret = RaGetNotifyMrInfo(rdma_handle, &mr_info2);
    EXPECT_INT_NE(0, ret);

    ret = RaGetQpStatus(qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    ret = RaSendWr(qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

	unsigned int complete_num = 2;
	unsigned int send_num = 2;
	struct SendWrlistData wrlist[send_num];
	struct SendWrRsp op_rsp_list[send_num];
	wrlist[0].memList = mem;
	wrlist[0].dstAddr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = 0;
	wrlist[1].memList = mem;
	wrlist[1].dstAddr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].sendFlags = 0;
    ret = RaSendWrlist(qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
    EXPECT_INT_EQ(0, ret);

	mem.len = 2147483649;
	wrlist[0].memList = mem;
	ret = RaSendWrlist(qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
	//EXPECT_INT_EQ(-EINVAL, ret);

    ret = RaQpConnectAsync(qp_handle, hdc_socket_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaMrDereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    // typical qp create
    struct TypicalQp qp_info = {0};
    ret = RaTypicalQpCreate(NULL, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, NULL, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 3, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, -1, &qp_info, &typical_qp_handle);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(rdma_handle, 0, RA_RS_OP_QP_MODE_EXT, &qp_info, &typical_qp_handle);
    EXPECT_INT_EQ(0, ret);

    // typical modify qp
    struct TypicalQp remote_qp_info = {0};
    ret = RaTypicalQpModify(typical_qp_handle, NULL, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpModify(NULL, &qp_info, &remote_qp_info);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpModify(typical_qp_handle, &qp_info, &remote_qp_info);
    EXPECT_INT_EQ(0, ret);

    int out_len;
    int op_result;
    int rcv_buf_len = 300;

    char in_buf[512];
    char out_buf[512];
    ret = RaRsTypicalQpModify(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    // typical send wr
    mem.addr = addr1;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
    ret = RaTypicalSendWr(NULL, &wr, &op_rsp);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalSendWr(typical_qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpDestroy(typical_qp_handle);

    ret = RaSocketBatchClose(NULL, 1);
    EXPECT_INT_NE(0, ret);
    ret = RaSocketBatchClose(&close, 1);
    EXPECT_INT_EQ(0, ret);
	mocker_clean();

    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);

	mocker(RaHdcSocketDeinit, 1, -1);
    ret = RaSocketDeinit(socket_handle);
    EXPECT_INT_EQ(128000, ret);
	mocker_clean();

/* start 单算子lite */
    unsigned int support = 0;
    mocker(DlDrvDeviceGetIndexByPhyId, 10 , 0);
    mocker(DlHalGetDeviceInfo, 10 , 0);
    mocker(DlHalMemCtl, 10 , 0);
    ret = RaHdcGetDrvLiteSupport(0, true, &support);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker(DlDrvDeviceGetIndexByPhyId, 10 , 0);
    mocker_invoke((stub_fn_t)DlHalGetDeviceInfo, stub_dl_hal_get_device_info, 10);
    mocker(DlHalMemCtl, 10 , 0);
    ret = RaHdcGetDrvLiteSupport(0, false, &support);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(0, support);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 10);
    mocker_invoke((stub_fn_t)RsGetLiteSupport, stub_rs_get_lite_support, 10);
    struct RaRdmaHandle *lite_rdma_handle = NULL;
    ret =  RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &lite_rdma_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    struct RaQpHandle *lite_qp_handle = NULL;
    ret = RaQpCreate(lite_rdma_handle, 0, RA_RS_OP_QP_MODE, &lite_qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaGetQpStatus(lite_qp_handle, &qp_status);
    EXPECT_INT_EQ(0, ret);

    mem.addr = addr1;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
    lite_qp_handle->localMr[0].addr = wr.bufList[0].addr;
    lite_qp_handle->localMr[0].len = 0x10000;
    lite_qp_handle->remMr[0].addr = wr.dstAddr;
    lite_qp_handle->remMr[0].len = 0x10000;
    lite_qp_handle->sendWrNum = 999;
    mocker_invoke((stub_fn_t)RaRdmaLitePollCq, stub_RaRdmaLitePollCq, 10);
    ret = RaSendWr(lite_qp_handle, &wr, &op_rsp);
    EXPECT_INT_EQ(0, ret);

    struct CqeErrInfo out_info;
    struct CqeErrInfo info0;
    struct CqeErrInfo info1 = { 0 };
    RaHdcLiteGetCqeErrInfo(rdev_info.phyId, &info0);
    RaHdcGetValidCqeErrInfo(&out_info, info0, info1);
    EXPECT_INT_EQ(out_info.status, info0.status);

    send_num = 2;
	wrlist[0].memList = mem;
	wrlist[0].dstAddr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = 0;
	wrlist[1].memList = mem;
	wrlist[1].dstAddr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].sendFlags = 0;
    ret = RaSendWrlist(lite_qp_handle, wrlist, op_rsp_list, send_num, &complete_num);
    EXPECT_INT_EQ(0, ret);

    struct SendWrlistDataExt data_ext[2];
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 2;
    wrlist_num.completeNum = &complete_num;
    data_ext[0].memList = mem;
	data_ext[0].dstAddr = 0x111;
	data_ext[0].op = 0;
	data_ext[0].sendFlags = 0;
	data_ext[1].memList = mem;
	data_ext[1].dstAddr = 0x111;
	data_ext[1].op = 0;
	data_ext[1].sendFlags = 0;
    ret = RaHdcSendWrlistExt(lite_qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaQpDestroy(lite_qp_handle);

    ret = RaRdevDeinit(lite_rdma_handle, NOTIFY);
    EXPECT_INT_EQ(0, ret);
/* end 单算子lite */

    ret = RaDeinit(&offline_config);
    EXPECT_INT_EQ(0, ret);

    mocker(RaHdcDeinit, 10, 1);
    ret = RaDeinit(&offline_config);

	ret = HccpDeinit(dev_id);
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
	struct RaInitConfig online_config = {
		.phyId = 0,
		.nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
	};

	struct MrInfoT mr_info;
	int sock_fd = 0;
	struct SocketListenInfoT listen[2];
	struct SocketConnectInfoT conn[2];
	struct SocketCloseInfoT close[2] = {0};
	struct SocketInfoT socket_info[2];
	socket_info[0].fdHandle = &sock_fd;
	int qp_status = 0;
	int access = 1<<1;
	int server = 0;
	int client = 1;
        char pid_sign[] = {"ec4cd587f827e128c66e01f257b8c2f97b52508e97efe36a"};

	mocker_invoke((stub_fn_t)dlopen, dlopen_stub, 1);
	mocker_invoke((stub_fn_t)dlsym, dlsym_stub, 19);
	mocker_invoke((stub_fn_t)dlclose, dlclose_stub, 1);

	dl_handle = malloc(1);
	HccpInit(dev_id, host_tgid, HDC_SERVICE_TYPE_RDMA, WHITE_LIST_ENABLE);
	sleep(2);
	RaInit(&online_config);

	ret = RaSocketListenStart(listen, 2);
	EXPECT_INT_EQ(0, ret);

	ret = RaSocketBatchConnect(conn, 2);
	EXPECT_INT_EQ(0, ret);

    unsigned int connected_num = 0;
	ret = RaGetSockets(server, socket_info, 2, &connected_num);
	EXPECT_INT_EQ(2, ret);

    unsigned long long sent_size = 0;
	ret = RaSocketSend(socket_info[0].fdHandle, data, size, &sent_size);
	EXPECT_INT_EQ(size, ret);

    unsigned long long received_size = 0;
	ret = RaSocketRecv(socket_info[0].fdHandle, data, size, &received_size);
	EXPECT_INT_EQ(size, ret);

	ret = RaQpCreate(dev_id, flag, qp_mode, &qp_handle1);

    struct QosAttr QosAttr= {0};
    QosAttr.tc = 100;
    QosAttr.sl = 3;
    ret = RaSetQpAttrQos(qp_handle1, &QosAttr);
    EXPECT_INT_EQ(0, ret);

    unsigned int timeout = 6;
    ret = RaSetQpAttrTimeout(qp_handle1, &timeout);
    EXPECT_INT_EQ(0, ret);

    unsigned int retry_cnt = 5;
    ret = RaSetQpAttrRetryCnt(qp_handle1, &retry_cnt);
    EXPECT_INT_EQ(0, ret);


	ret = RaQpCreate(dev_id, flag, qp_mode, &qp_handle2);
	EXPECT_INT_EQ(0, ret);

	ret = RaQpConnectAsync(qp_handle1, socket_info[0].fdHandle);
	EXPECT_INT_EQ(-1, ret);

	ret = RaQpConnectAsync(qp_handle2, socket_info[0].fdHandle);
	EXPECT_INT_EQ(-1, ret);

	ret = RaGetQpStatus(qp_handle1, &qp_status);
	EXPECT_INT_EQ(-1, ret);

	ret = RaGetQpStatus(qp_handle2, &qp_status);
	EXPECT_INT_EQ(-1, ret);

    mr_info.addr = addr1;
	mr_info.size = size;
	mr_info.access = access;
	ret = RaMrReg(qp_handle1, &mr_info);
	EXPECT_INT_EQ(-1, ret);

    mr_info.addr = addr2;
	mr_info.size = size;
	mr_info.access = access;
	ret = RaMrReg(qp_handle2, &mr_info);
	EXPECT_INT_EQ(-1, ret);

	struct SendWr wr;
	struct SgList mem;
	u32 wqe_index;

	mem.addr = addr1;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
        struct SendWrRsp *op_rsp;
	ret = RaSendWr(qp_handle1, &wr, &op_rsp);
	EXPECT_INT_EQ(-1, ret);

	long va = 0;
	long pa = 0;
	long size_notify = 0;
	ret = RaGetNotifyBaseAddr(qp_handle1, &va, &size_notify);
	EXPECT_INT_EQ(-1, ret);
    struct MrInfoT mr_info2;
	ret = RaGetNotifyMrInfo(qp_handle1, &mr_info2);
	EXPECT_INT_EQ(-1, ret);

	ret = RaSocketListenStop(listen, 2);
	EXPECT_INT_EQ(-1, ret);

	close[0].fdHandle = socket_info[0].fdHandle;
	close[1].fdHandle = socket_info[1].fdHandle;
	ret = RaSocketBatchClose(close, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = RaMrDereg(qp_handle1, addr1);
	EXPECT_INT_EQ(-1, ret);

	ret = RaMrDereg(qp_handle2, addr2);
	EXPECT_INT_EQ(-1, ret);

	ret =RaQpDestroy(qp_handle1);
	EXPECT_INT_EQ(-1, ret);

	ret =RaQpDestroy(qp_handle2);
	EXPECT_INT_EQ(-1, ret);

	free(dl_handle);
	free(addr1);
	free(addr2);
	RaDeinit(&online_config);

	mocker_clean();



	ret = RaSocketListenStart(listen, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = RaSocketBatchConnect(conn, 2);
	EXPECT_INT_EQ(-1, ret);

	ret = RaGetSockets(server, socket_info, 2, &connected_num);
	EXPECT_INT_EQ(2, ret);

	ret = RaSocketSend(socket_info[0].fdHandle, data, size, &sent_size);
	EXPECT_INT_EQ(1, ret);

	ret = RaSocketRecv(socket_info[0].fdHandle, data, size, &received_size);
	EXPECT_INT_EQ(1, ret);

	free(socket_info[0].fdHandle);
	free(socket_info[1].fdHandle);
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
    struct SendWr *wr = NULL;
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
    struct RaInitConfig config = {
        .phyId = dev_id,
        .nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };

    config.phyId = 0;
    int ip_addr;
    unsigned int host_tgid = 0;
    int qpMode = 0;
	unsigned int rdevIndex;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct RaSocketHandle socket_handle_tmp ={
        .rdevInfo = rdev_info,
    };
    struct RaRdmaHandle *rdma_handle = &rdma_handle_tmp;
    struct RaSocketHandle *socket_handle = &socket_handle_tmp;
    struct SocketHdcInfo *hdc_socket_handle = calloc(1, sizeof(struct SocketHdcInfo));
    struct QpExtAttrs extAttrs;
    extAttrs.version = QP_CREATE_WITH_ATTR_VERSION;
    extAttrs.qpMode = RA_RS_NOR_QP_MODE;
    unsigned int temp_depth = 128;
    unsigned int qp_num = 0;
    ret = RaPeerSetTsqpDepth(rdma_handle, temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerGetTsqpDepth(rdma_handle, &temp_depth, &qp_num);
    EXPECT_INT_EQ(0, ret);
    ret = RaPeerQpCreateWithAttrs(rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);
    EXPECT_ADDR_NE(NULL, qp_handle_with_attr);

    listen[0].socketHandle = socket_handle;
    conn[0].socketHandle = socket_handle;
    close[0].socketHandle = socket_handle;
    info[0].socketHandle = socket_handle;
	info[0].fdHandle = hdc_socket_handle;
    ret = RaPeerInit(&config, 0);
    EXPECT_INT_EQ(-1, ret);
	ret = RaInit(&config);
	EXPECT_INT_EQ(128000, ret);
	ret = RaDeinit(&config);
	EXPECT_INT_EQ(128000, ret);

	mocker(HostNotifyBaseAddrInit, 1, 0);
	ret = RaPeerRdevInit(rdma_handle, NOTIFY, rdev_info, &rdevIndex);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);

	mocker(HostNotifyBaseAddrUninit, 1, 0);
	ret = RaPeerRdevDeinit(rdma_handle, NOTIFY);
	mocker_clean();
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSocketBatchConnect(0, conn, 1);
    EXPECT_INT_EQ(0, ret);

    listen[0].port = 1;
    ret = RaPeerSocketListenStart(0, listen, 1);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerSocketListenStop(0, listen, 1);
    EXPECT_INT_NE(0, ret);

    ret = RaPeerGetSockets(0, 0, info, 1);
    EXPECT_INT_EQ(1, ret);

    ret = RaPeerSocketSend(0, info[0].fdHandle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = RaPeerSocketSend(0, info[0].fdHandle, data, max_size);
    EXPECT_INT_EQ(1, ret);

    ret = RaPeerSocketRecv(0, info[0].fdHandle, data, size);
    EXPECT_INT_EQ(0, size);

    ret = RaPeerSocketRecv(0, info[0].fdHandle, data, max_size);
    EXPECT_INT_EQ(1, ret);

	struct SocketWlistInfoT white_list[1];
	ret = RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
    EXPECT_INT_EQ(0, ret);
	ret = RaPeerSocketWhiteListDel(rdev_info, white_list, 1);
    EXPECT_INT_EQ(0, ret);

    struct RaQpHandle *qp_handle = (struct RaQpHandle *)calloc(1, sizeof(struct RaQpHandle));
    void *mr_handle = NULL;


    ret = RaPeerNotifyBaseAddrInit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerNotifyBaseAddrInit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = NotifyBaseAddrUninit(EVENTID, 0);
    EXPECT_INT_EQ(0, ret);

    ret = NotifyBaseAddrUninit(NO_USE, 0);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerQpConnectAsync(qp_handle, info[0].fdHandle);
    EXPECT_INT_EQ(0, size);

    close[0].fdHandle = info[0].fdHandle;
    ret = RaPeerSocketBatchClose(0, close, 1);
    EXPECT_INT_EQ(0, size);

    // close[0].fdHandle = info[0].fdHandle;
    // mocker(memset_s, 5, 0);
    // ret = RaPeerSocketBatchClose(0, close, 1);
    // EXPECT_INT_EQ(0, size);
    // mocker_clean();

    ret = RaPeerGetQpStatus(qp_handle, &status);
    EXPECT_INT_EQ(0, ret);

    struct MrInfoT mr_info;
    mr_info.addr = addr;
    mr_info.size = size;
    mr_info.access = access;

    ret = RaPeerMrReg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerMrDereg(qp_handle, &mr_info);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerRegisterMr(rdma_handle, &mr_info, &mr_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDeregisterMr(mr_handle, NULL);
    EXPECT_INT_EQ(0, ret);

    void *comp_channel = NULL;
    ret = RaPeerCreateCompChannel(rdma_handle, &comp_channel);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDestroyCompChannel(comp_channel);
    EXPECT_INT_EQ(0, ret);

    struct SrqAttr attr = {0};
    ret = RaPeerCreateSrq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDestroySrq(rdma_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    struct SendWrRsp op_rsp;
    ret = RaPeerSendWr(qp_handle, wr, &op_rsp);
    EXPECT_INT_NE(0, ret);

    struct RecvWrlistData rev_wr = {0};
    rev_wr.wrId = 100;
    rev_wr.memList.lkey = 0xff;
    rev_wr.memList.addr = addr;
    rev_wr.memList.len = size;
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    ret = RaPeerRecvWrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerQpDestroy(qp_handle);
    ret = RaPeerQpDestroy(qp_handle_with_attr);
    EXPECT_INT_EQ(0, ret);

    ret = RaPeerDeinit(&config);
    EXPECT_INT_NE(0, ret);

	// free(hdc_socket_handle);
    return;
}


extern int RaHdcProcessMsg(unsigned int opcode, unsigned int phyId, char *data, unsigned int data_size);
extern int RaHdcGetAllVnic(unsigned int curPhyId, unsigned int *vnicIp, unsigned int num);
extern int RaHdcInitApart(unsigned int phyId, unsigned int *logicId);
extern int RaHdcSendPid(unsigned int phyId, struct ProcessRaSign pRaSign);
extern int strcpy_s(void * dest, size_t destMax, const void * src, size_t count);
extern void RaHdcSendWrlistInit(union OpSendWrlistData *send_wrlist, struct RaQpHandle *qp_hdc,
    unsigned int complete_cnt, struct WrlistSendCompleteNum wrlist_num);

void tc_hdc_fail()
{
    struct RaQpHandle qp_hdc;
    struct SendWr swr;
    qp_hdc.phyId = 0;
    qp_hdc.rdevIndex = 0;
    qp_hdc.qpn = 0;
    swr.bufNum = 0;
    swr.dstAddr = 0;
    swr.op = 0;
    swr.sendFlag = 0;
    struct SendWrlistData wr[1] = {0};
    struct SendWrRsp op_rsp[1] = {0};
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 1;
    unsigned int complete_num = 0;
    wrlist_num.completeNum = &complete_num;
    unsigned int interface_version = 0;
    struct RaRdmaHandle rdma_handle = { 0 };

    RaHdcGetInterfaceVersion(0, 0, &interface_version);

    struct IfaddrInfo ifaddr_infos[1] = {{{{0}}}};
    unsigned int num = 1;
    RaHdcGetIfnum(0, 0, &num);

    RaHdcGetIfaddrs(0, ifaddr_infos, &num);
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;

    mocker(memset_s, 5, -1);
    RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);

    RaHdcSocketInit(rdev_info);
    RaHdcSocketDeinit(rdev_info);
    mocker_clean();

    mocker(memcpy_s, 10, -1);
    RaHdcGetIfnum(0, 0, &num);

    mocker(memcpy_s, 10, -1);
    RaHdcGetIfaddrs(0, ifaddr_infos, &num);

    RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);

    RaHdcSocketInit(rdev_info);

    RaHdcSendWrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    RaHdcSendWr(&qp_hdc, &swr, op_rsp);

    mocker(RaHdcGetAllVnic, 5, 0);
    RaHdcSocketInit(rdev_info);
    RaHdcSocketDeinit(rdev_info);
    mocker_clean();

    mocker(RaHdcProcessMsg, 5, 0);
    RaHdcGetIfnum(0, 0, &num);

    mocker(RaHdcProcessMsg, 5, 0);
    RaHdcGetIfaddrs(0, ifaddr_infos, &num);

    qp_hdc.qpMode = 1;
    RaHdcSendWr(&qp_hdc, &swr, op_rsp);

    qp_hdc.qpMode = 2;
    RaHdcSendWr(&qp_hdc, &swr, op_rsp);
    mocker_clean();

    unsigned int vnic_ip = 0;
    mocker(RaHdcProcessMsg, 10, -1);

    unsigned int temp_depth = 0;
    unsigned int qp_num = 0;
    rdma_handle.rdevInfo.phyId = 0;
    rdma_handle.rdevIndex = 0;

    RaHdcGetTsqpDepth(&rdma_handle, &temp_depth, &qp_num);
    RaHdcSetTsqpDepth(&rdma_handle, 0, &qp_num);

    RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);

    RaHdcGetAllVnic(0, &vnic_ip, 0);

    RaHdcSessionClose(0);

    unsigned long long va = 0;
    unsigned long long size = 0;
    RaHdcNotifyCfgGet(0, &va, &size);
    RaHdcNotifyCfgSet(0, 0, 0);

    mocker(RaHdcGetAllVnic, 5, 0);
    RaHdcSocketInit(rdev_info);
    RaHdcSocketDeinit(rdev_info);
    mocker_clean();

    mocker(drvGetDevNum, 5, -1);
    RaHdcGetAllVnic(0, &vnic_ip, 0);
    mocker_clean();

    mocker(drvDeviceGetPhyIdByIndex, 5, -1);
    RaHdcGetAllVnic(0, &vnic_ip, 0);
    mocker_clean();

    struct RaInitConfig cfg = {0};
    cfg.phyId = 0;
    mocker(RaHdcSessionClose, 5, -1);
    RaHdcDeinit(&cfg);
    mocker_clean();

    mocker(pthread_mutex_destroy, 5, -1);
    RaHdcDeinit(&cfg);
    mocker_clean();

    struct ProcessRaSign p_ra_sign;
    mocker(RaHdcInitApart, 5, -1);
    RaHdcInit(&cfg, p_ra_sign);
    mocker_clean();


    unsigned int logic_id = 0;
    mocker(drvDeviceGetIndexByPhyId, 5, -1);
    RaHdcInitApart(0, &logic_id);
    mocker_clean();

    mocker(pthread_mutex_init, 5, -1);
    RaHdcInitApart(0, &logic_id);
    mocker_clean();

    mocker(strcpy_s, 5, -1);
    p_ra_sign.tgid = 0;
    RaHdcSendPid(0, p_ra_sign);
    mocker_clean();

    mocker(RaHdcProcessMsg, 10, -1);
    RaHdcSendPid(0, p_ra_sign);

    RaHdcGetNotifyBaseAddr(&rdma_handle, &va, &size);

    RaHdcGetIfaddrs(0, ifaddr_infos, &num);

    RaHdcSendWrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    struct MrInfoT info;
    char addr = 0;
    info.addr = &addr;
    info.size = 0;
    info.access = 0;
    RaHdcMrReg(&qp_hdc, &info);
    RaHdcMrDereg(&qp_hdc, &info);

    struct QosAttr QosAttr= {0};
    QosAttr.tc = 110;
    QosAttr.sl = 3;
    RaHdcSetQpAttrQos(&qp_hdc, &QosAttr);

    struct SocketHdcInfo sock_handle;
    sock_handle.fd = 0;
    RaHdcQpConnectAsync(&qp_hdc, &sock_handle);

    int status = 0;
    RaHdcGetQpStatus(&qp_hdc, &status);

    g_interface_version = RA_RS_OPCODE_BASE_VERSION;
    status = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 10);
    RaHdcGetQpStatus(&qp_hdc, &status);

    struct RaQpHandle *qp_handle = &qp_hdc;
    RaHdcQpCreate(&rdma_handle, 0, 1, &qp_handle);

    mocker_clean();

    mocker(calloc, 5, NULL);
    RaHdcSendWrlist(&qp_hdc, wr, op_rsp, wrlist_num);

    RaHdcQpCreate(&rdma_handle, 0, 1, &qp_handle);
    mocker_clean();

    RaHdcSendWr(&qp_hdc, &swr, op_rsp);

    return;
}


void tc_peer_fail()
{
    //int server_devid = 0;
    //mocker(drvDeviceGetPhyIdByIndex, 5, -1);
    //ra_peer_get_server_devid(0, &server_devid);
    //mocker_clean();
	struct MrInfoT mr_info;
    void *mr_handle = NULL;
    void *comp_channel = NULL;
    int ret;

    struct SocketConnectInfoT conn[1] = {0};
    mocker(RaGetSocketConnectInfo, 5, -1);
    RaPeerSocketBatchConnect(0, conn, 0);
    mocker_clean();

    struct RaSocketHandle socket_handle;
    socket_handle.rdevInfo.phyId = 0;
    socket_handle.rdevInfo.family = AF_INET;
    socket_handle.rdevInfo.localIp.addr.s_addr = 0;
    conn[0].socketHandle = &socket_handle;
    mocker(RaGetSocketConnectInfo, 5, 0);
    mocker(RsSocketBatchConnect, 5, -1);
    RaPeerSocketBatchConnect(0, conn, 0);
    mocker(RsSocketSetScopeId, 5, -2);
    ret = RaPeerSocketBatchConnect(0, conn, 0);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    struct SocketCloseInfoT con[1] = {0};
    mocker(memset_s, 5, -1);
    RaPeerSocketBatchClose(0, con, 0);
    mocker_clean();

    mocker(memset_s, 5, 0);
    RaPeerSocketBatchClose(0, con, 0);
    mocker_clean();

    struct SocketHdcInfo *hdc_socket_handle = calloc(1, sizeof(struct SocketHdcInfo));
    con[0].fdHandle = hdc_socket_handle;
    mocker(memset_s, 5, 0);
    ret = RaPeerSocketBatchClose(0, con, 1);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct SocketListenInfoT conn_listen[1] = {0};
    conn_listen[0].port = 65536;
    RaPeerSocketListenStart(0, conn_listen, 1);
    RaPeerSocketListenStop(0, conn_listen, 1);

    conn_listen[0].port = 0;
    mocker(RaGetSocketListenInfo, 5, -1);
    RaPeerSocketListenStart(0, conn_listen, 1);
    RaPeerSocketListenStop(0, conn_listen, 1);
    mocker_clean();

    struct RaSocketHandle socket_handles;
    socket_handles.rdevInfo.phyId = 0;
    socket_handles.rdevInfo.family = AF_INET;
    conn_listen[0].socketHandle = &socket_handles;
    mocker(RaGetSocketListenInfo, 5, 0);
    RaPeerSocketListenStart(0, conn_listen, 1);
    RaPeerSocketListenStop(0, conn_listen, 1);
    mocker_clean();

    mocker(RaGetSocketListenInfo, 5, 0);
    mocker(RsSocketListenStart, 5, -1);
    RaPeerSocketListenStart(0, conn_listen, 1);
    mocker((stub_fn_t)RsSocketSetScopeId, 10, -2);
    ret = RaPeerSocketListenStart(0, conn_listen, 1);
    EXPECT_INT_EQ(-2, ret);
    mocker_clean();

    mocker(RaGetSocketListenInfo, 5, 0);
    mocker(RsSocketListenStart, 5, 0);
    mocker(RaGetSocketListenResult, 5, -1);
    RaPeerSocketListenStart(0, conn_listen, 1);
    mocker_clean();

    struct SocketInfoT conn_p[1];
    struct SocketFdData rs_conn[1];
    // struct ra_socket_handle socket_handle;
    // socket_handle.rdevInfo.phyId = 0;
    // socket_handle.rdevInfo.family = AF_INET;
    // socket_handle.rdevInfo.localIp.addr.s_addr = 0;
    conn_p[0].status = 0;
    RaPeerSetRsConnParam(conn_p, 1, rs_conn, 0);

    rs_conn[0].phyId = 0;
    rs_conn[0].fd = 0;
    conn_p[0].socketHandle = &socket_handle;
    mocker(memcpy_s, 5, -1);
    RaPeerSetRsConnParam(conn_p, 1, rs_conn, 2);
    mocker_clean();

    conn_p[0].fdHandle = calloc(1, sizeof(struct SocketPeerInfo));
    RaPeerSetConnParam(conn_p, rs_conn, 0, 0);
    free(conn_p[0].fdHandle);
    conn_p[0].fdHandle = NULL;


    struct SocketInfoT conn_s[1] = {0};
    mocker(RaPeerSetRsConnParam, 5, -1);
    RaPeerGetSockets(0, 0, conn_s, 1);
    mocker_clean();

    conn_p[0].status = 1;
    RaPeerGetSockets(0, 0, conn_p, 1);
    free(conn_p[0].fdHandle);
    conn_p[0].fdHandle = NULL;

    mocker(RaPeerSetRsConnParam, 5, 0);
    mocker(RaPeerSetConnParam, 1, 1);
    RaPeerGetSockets(0, 0, conn_s, 1);
    mocker_clean();

    struct SocketPeerInfo peer_socket_handle = {0};

    ret = RaPeerSocketSend(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 1;
    ret = RaPeerSocketSend(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 0;
    ret = RaPeerSocketRecv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    peer_socket_handle.sslEnable = 1;
    ret = RaPeerSocketRecv(0, &peer_socket_handle, NULL, 0);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerSetRsConnParam, 5, 0);
    mocker(RsGetSockets, 5, -1);
    RaPeerGetSockets(0, 0, conn_s, 1);
    mocker_clean();

    mocker(RaPeerSetRsConnParam, 5, 0);
    mocker(RsGetSockets, 5, 0);
    mocker(RsGetSslEnable, 5, -1);
    RaPeerGetSockets(0, 0, conn_s, 1);
    mocker_clean();

    struct rdev rdev_info = {0};
    rdev_info.localIp.addr.s_addr = 0;
    struct SocketWlistInfoT white_list[1];
    white_list[0].remoteIp.addr.s_addr = 0;

    mocker(inet_ntoa, 5, NULL);
    RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, 1);
    mocker(RsSocketWhiteListAdd, 5, -1);
    RaPeerSocketWhiteListAdd(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, NULL);
    RaPeerSocketWhiteListDel(rdev_info, white_list, 1);
    mocker_clean();

    mocker(inet_ntoa, 5, 1);
    mocker(RsSocketWhiteListDel, 5, -1);
    RaPeerSocketWhiteListDel(rdev_info, white_list, 1);
    mocker_clean();

    mocker(RsSocketDeinit, 5, -1);
    RaPeerSocketDeinit(rdev_info);
    mocker_clean();

    struct RaRdmaHandle rdma_handle;
    rdma_handle.rdevInfo.phyId = 0;

    mocker(calloc, 5, NULL);
    void *qp_handle;
    void *qp_handle_with_attr;
    qp_handle_with_attr = NULL;
    struct QpExtAttrs extAttrs;
    RaPeerQpCreate(&rdma_handle, 0, 0, &qp_handle);
    ret  = RaPeerQpCreateWithAttrs(&rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(-ENOMEM, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    mocker(RsQpCreate, 5, -1);
    mocker((stub_fn_t)RsQpCreateWithAttrs, 10, 1);
    RaPeerQpCreate(&rdma_handle, 0, 0, &qp_handle);
    ret  = RaPeerQpCreateWithAttrs(&rdma_handle, &extAttrs, &qp_handle_with_attr);
    EXPECT_INT_EQ(1, ret);
    EXPECT_ADDR_EQ(NULL, qp_handle_with_attr);
    mocker_clean();

    unsigned long long notify_size;
    unsigned long long va = 0;
    ret = RaPeerGetNotifyBaseAddr(&rdma_handle, &va, &notify_size);
    EXPECT_INT_NE(0, ret);
    mocker((stub_fn_t)RsGetNotifyMrInfo, 10, -1);
    ret = RaPeerGetNotifyBaseAddr(&rdma_handle, &va, &notify_size);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct RaQpHandle *qp_peer = NULL;
    qp_peer = (struct RaQpHandle *)calloc(1, sizeof(struct RaQpHandle));

    mocker(RsQpDestroy, 5, -1);
    RaPeerQpDestroy(qp_peer);
    mocker_clean();

    struct RaQpHandle qp_peer_p = {0};
    int status = 0;
    mocker(RsGetQpStatus, 5, -1);
    RaPeerGetQpStatus(&qp_peer_p, &status);
    mocker_clean();

    int addr = 0;
	mr_info.addr = addr;
	mr_info.size = 0;
	mr_info.access = 0;
    mocker(RsMrReg, 5, -1);
    RaPeerMrReg(&qp_peer_p, &mr_info);
    mocker_clean();

    mocker(RsMrDereg, 5, -1);
    RaPeerMrDereg(&qp_peer_p, &mr_info);
    mocker_clean();

    mocker(RsRegisterMr, 5, -1);
    RaPeerRegisterMr(&rdma_handle, &mr_info, &mr_handle);
    mocker_clean();

    mocker(RsDeregisterMr, 5, -1);
    RaPeerDeregisterMr(&rdma_handle, mr_handle);
    mocker_clean();

    mocker(RsCreateCompChannel, 5, -1);
    ret = RaPeerCreateCompChannel(&rdma_handle, &comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker(RsDestroyCompChannel, 5, -1);
    ret = RaPeerDestroyCompChannel(comp_channel);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsPeerGetIfnum, 10, -1);
    unsigned int num = 0;
    RaPeerGetIfnum(0, &num);
    mocker_clean();

    struct SrqAttr attr = {0};
    mocker((stub_fn_t)RsCreateSrq, 10, -1);
    ret = RaPeerCreateSrq(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);

    mocker((stub_fn_t)RsDestroySrq, 10, -1);
    ret = RaPeerDestroySrq(&rdma_handle, &attr);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    struct InterfaceInfo interface_infos[1];
    mocker(RsPeerGetIfaddrs, 5, -1);
    RaPeerGetIfaddrs(0, interface_infos, &num);
    mocker_clean();

    unsigned int rdevIndex = 0;
    //mocker(ra_peer_get_server_devid, 5, -1);
    RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, 0);
    mocker(RsRdevInit, 5, -1);
    RaPeerRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, -1);
    RaPeerRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();

    //mocker(ra_peer_get_server_devid, 5, 0);
    mocker(RsRdevDeinit, 5, -1);
    RaPeerRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();



    return;
}

void tc_comm_fail()
{
    struct RaSocketHandle socket_handle;
    struct SocketConnectInfoT conn[1];
    struct SocketConnectInfo rs_conn[1] ={0};
    char tag[SOCK_CONN_TAG_SIZE] = {0};
    socket_handle.rdevInfo.phyId = 0;
    socket_handle.rdevInfo.family = AF_INET;
    socket_handle.rdevInfo.localIp.addr.s_addr = 0;
    conn[0].socketHandle = &socket_handle;
    conn[0].port = 0;
    conn[0].remoteIp.addr.s_addr = 0;
    memcpy_s(conn[0].tag, SOCK_CONN_TAG_SIZE, tag, SOCK_CONN_TAG_SIZE);


    struct SocketListenInfoT conn_listen[1];
    struct SocketListenInfo rs_conn_listen[1] = {0};
    conn_listen[0].socketHandle = &socket_handle;
    conn_listen[0].port = 0;


    RaGetSocketConnectInfo(NULL, 0, NULL, 0);
    RaGetSocketListenInfo(NULL, 0, NULL, 0);
    RaGetSocketListenResult(NULL, 0, NULL, 0);


    struct SocketListenInfo rs_conn_result[1];
    struct SocketListenInfoT conn_result[1] = {0};
    conn_result[0].socketHandle = &socket_handle;
    rs_conn_result[0].phase = 0;
    rs_conn_result[0].err = 0;
    rs_conn_result[0].phyId = 0;
    rs_conn_result[0].family = 0;
    rs_conn_result[0].localIp.addr.s_addr = 0;

    mocker(memcpy_s, 5, -1);
    RaGetSocketConnectInfo(conn, 1, rs_conn, 2);

    RaGetSocketListenInfo(conn_listen, 1, rs_conn_listen, 2);

    RaGetSocketListenResult(rs_conn_result, 1, conn_result, 2);
    mocker_clean();

    return;
}

extern int RaAssembleSockets(union OpSocketInfoData *socketInfoData, struct SocketInfoT *conn,
    unsigned int num, const int sockFd[], size_t sockFdLen);
void tc_hdc_fail_01()
{
    int data = 0;

    mocker(calloc, 5, NULL);
    RaHdcProcessMsg(0, 0, &data, 0);
    mocker_clean();

    struct SocketConnectInfoT conn[1];
    mocker(calloc, 5, NULL);
    RaHdcSocketBatchConnect(0, conn, 0);
    mocker_clean();

    mocker(RaGetSocketConnectInfo, 5, -1);
    RaHdcSocketBatchConnect(0, conn, 0);
    mocker_clean();

    mocker(RaGetSocketConnectInfo, 5, 0);
    mocker(RaHdcProcessMsg, 5, -1);
    RaHdcSocketBatchConnect(0, conn, 0);
    mocker_clean();

    struct SocketListenInfoT conn_listen[1];

    mocker(RaGetSocketListenInfo, 5, -1);
    RaHdcSocketListenStart(0, conn_listen, 0);
    RaHdcSocketListenStop(0, conn_listen, 0);
    mocker_clean();

    mocker(RaGetSocketListenInfo, 5, 0);
    mocker(RaHdcProcessMsg, 5, -1);
    RaHdcSocketListenStart(0, conn_listen, 0);
    RaHdcSocketListenStop(0, conn_listen, 0);
    mocker_clean();

    mocker(RaGetSocketListenInfo, 5, 0);
    mocker(RaHdcProcessMsg, 5, 0);
    mocker(RaGetSocketListenResult, 5, -1);
    RaHdcSocketListenStart(0, conn_listen, 0);
    mocker_clean();


    struct SocketInfoT conn_info[1] = {0};
    mocker(memcpy_s, 5, -1);
    RaGetIpAndTagInfo(NULL, NULL, conn_info, 0);
    mocker_clean();

    struct SocketInfoT conn_s[1];
    mocker(calloc, 5, NULL);
    RaHdcGetSockets(0, 0, conn_s, 0);
    mocker_clean();

    mocker(RaAssembleSockets, 5, -1);
    RaHdcGetSockets(0, 0, conn_s, 0);
    mocker_clean();

    struct SocketHdcInfo handle;
    handle.fd = 0;

    mocker(calloc, 5, NULL);
    RaHdcSocketSend(0, &handle, &data, 1);
    RaHdcSocketRecv(0, &handle, &data, 1);
    mocker_clean();

    mocker(memcpy_s, 5, -1);
    RaHdcSocketSend(0, &handle, &data, 1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 5, -1);
    RaHdcSocketSend(0, &handle, &data, 1);
    RaHdcSocketRecv(0, &handle, &data, 1);
    mocker_clean();


    struct RaQpHandle *qp_hdc = NULL;
    qp_hdc = (struct RaQpHandle *)calloc(1, sizeof(struct RaQpHandle));
    mocker(RaHdcProcessMsg, 5, -1);
    RaHdcQpDestroy(qp_hdc);
    mocker_clean();


    return;
}

extern int strncmp(const char *cs, const char *ct, size_t count);
void tc_host_fail()
{
    union HccpIpAddr ip;
    char net_addr[1];
    int ret;
	unsigned long long *notify_va = NULL;
    mocker(inet_ntop, 5, NULL);
    RaInetPton(AF_INET, ip, net_addr, 1);
    mocker_clean();

    RaInetPton(AF_INET6, ip, net_addr, 1);

    mocker(drvDeviceGetIndexByPhyId, 5, -1);
    RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halNotifyGetInfo, 5, -1);
    RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halMemAlloc, 5, -1);
    RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(RaHdcNotifyCfgSet, 5, -1);
    RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    mocker_clean();

    mocker(halMemFree, 5, -1);
    RaHdcNotifyBaseAddrInit(NOTIFY, 0, &notify_va);
    mocker_clean();

    struct RaInitConfig config = {0};
    config.phyId = 0;
    RaInit(NULL);
    RaDeinit(NULL);

    config.phyId = 17;
    RaInit(&config);
    RaDeinit(&config);

    config.phyId = 0;
    config.nicPosition  = NETWORK_OFFLINE;

    mocker(drvGetProcessSign, 5, -1);
    RaInit(&config);
    mocker_clean();

    mocker(strcpy_s, 5, -1);
    RaInit(&config);
    mocker_clean();

    mocker(RaHdcInit, 5, -1);
    RaInit(&config);
    RaDeinit(&config);
    mocker_clean();

    config.nicPosition  = 5;
    RaInit(&config);
    RaDeinit(&config);

    struct rdev rdev_info;
    struct RaSocketHandle *socket_handle = NULL;
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;

    RaSocketInit(NETWORK_OFFLINE, rdev_info, NULL);

    struct RaSocketHandle socket_handle_tmp;
    socket_handle_tmp.rdevInfo.phyId = 17;
    RaSocketDeinit(&socket_handle_tmp);

    socket_handle_tmp.rdevInfo.phyId = 0;
    mocker(RaInetPton, 10, -1);
    RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    RaSocketDeinit(&socket_handle_tmp);
    mocker_clean();

    mocker(calloc, 10, NULL);
    RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(memcpy_s, 10, -1);
    RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    mocker(RaHdcSocketInit, 10, -1);
    RaSocketInit(NETWORK_OFFLINE, rdev_info, &socket_handle);
    mocker_clean();

    char localIp[1];

    mocker(RaGetIfaddrs, 10, 0);
    mocker(RaInetPton, 10, -1);
    RaRdevInitCheckIp(NETWORK_OFFLINE, rdev_info, localIp);
    mocker_clean();

    mocker(RaGetIfaddrs, 10, 0);
    mocker(RaInetPton, 10, 0);
    mocker(strncmp, 10, -1);
    RaRdevInitCheckIp(NETWORK_OFFLINE, rdev_info, localIp);
    mocker_clean();

    mocker(RaHdcProcessMsg, 100, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    g_interface_version = 2;
    RaRdevInitCheckIp(NETWORK_OFFLINE, rdev_info, localIp);
    mocker_clean();

    struct RaRdmaHandle rdma_handle = {0};
    rdev_info.phyId = 17;
    RaRdevInitCheck(0, rdev_info, localIp, 0, &rdma_handle);

    rdev_info.phyId = 0;
    RaRdevInitCheck(0, rdev_info, localIp, 0, NULL);

    mocker(RaInetPton, 10, -1);
    RaRdevInitCheck(0, rdev_info, localIp, 0, &rdma_handle);
    mocker_clean();

    struct RaRdmaHandle *rdma_handle_init = NULL;
    mocker(RaRdevInitCheck, 10, 0);
    mocker(RaHdcNotifyBaseAddrInit, 10, -1);
    RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(RaRdevInitCheck, 10, 0);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    mocker(calloc, 10, NULL);
    RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(RaRdevInitCheck, 10, 0);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    RaRdevInit(5, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(RaRdevInitCheck, 10, 0);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    mocker(memcpy_s, 10, -1);
    RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    mocker(RaRdevInitCheck, 10, 0);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    mocker(RaHdcRdevInit, 10, -1);
    RaRdevInit(NETWORK_OFFLINE, NOTIFY, rdev_info, &rdma_handle_init);
    mocker_clean();

    struct RaRdmaHandle *rdma_handle_t = NULL;
    rdma_handle_t = calloc(1, sizeof(struct RaRdmaHandle));

    rdma_handle_t->rdevInfo.phyId = 17;
    RaRdevDeinit(rdma_handle_t, NOTIFY);

    rdma_handle_t->rdevInfo.phyId = 0;
    mocker(RaInetPton, 10, -1);
    RaRdevDeinit(rdma_handle_t, NOTIFY);
    mocker_clean();

    free(rdma_handle_t);
    rdma_handle_t = NULL;


    struct SocketConnectInfoT conn_connect[1] = {0};
    struct RaSocketHandle socket_handle_connect = {0};
    conn_connect[0].socketHandle = &socket_handle_connect;
    RaSocketBatchConnect(conn_connect, 1);

    struct SocketCloseInfoT conn_close[1] = {0};
    conn_close[0].socketHandle = &socket_handle_connect;
    RaSocketBatchClose(conn_close, 1);

    struct SocketListenInfoT conn_listen[1] = {0};
    conn_listen[0].socketHandle = &socket_handle_connect;
    RaSocketListenStart(conn_listen, 1);
    RaSocketListenStop(conn_listen, 1);

    struct SocketInfoT conn[1] = {0};
    conn[0].socketHandle = &socket_handle_connect;
    unsigned int connected_num = 0;
    RaGetSockets(0, conn, 1, &connected_num);

    struct SocketHdcInfo fd_handle = {0};
    fd_handle.socketHandle = NULL;
    int data = 0;
    unsigned long long sent_size = 0;
    unsigned long long received_size = 0;
    RaSocketSend(&fd_handle, &data, 1, &sent_size);
    RaSocketRecv(&fd_handle, &data, 1, &received_size);

    struct RaQpHandle *qp_handle_create = NULL;
    ret = RaQpCreate(NULL, 1, -1, &qp_handle_create);
    EXPECT_INT_NE(0, ret);
    ret = RaQpCreateWithAttrs(NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaTypicalQpCreate(NULL, 0, 0, NULL, NULL);
    EXPECT_INT_NE(0, ret);
    ret = RaAiQpCreate(NULL, NULL, NULL, NULL);
    EXPECT_INT_NE(0, ret);

    struct RaQpHandle qp_handle = {0};
    qp_handle.rdmaOps = NULL;
    RaQpDestroy(&qp_handle);

    RaQpConnectAsync(&qp_handle, &fd_handle);

    int status = 0;
    RaGetQpStatus(&qp_handle, &status);

    struct MrInfoT info = {0};
    RaMrReg(&qp_handle, &info);

    // RaMrDereg(void *qp_handle, struct mrInfo*info);

    struct SocketInitInfoT socket_init = {0};
    void* socket_handle1 = NULL;

    socket_init.scopeId = 0;
    socket_init.rdevInfo.phyId = 0;
    socket_init.rdevInfo.family = AF_INET;
    socket_init.rdevInfo.localIp.addr.s_addr = 0;
    RaSocketInitV1(NETWORK_OFFLINE, socket_init, &socket_handle1);

    socket_init.scopeId = 0;
    socket_init.rdevInfo.phyId = 0;
    socket_init.rdevInfo.family = AF_INET6;
    socket_init.rdevInfo.localIp.addr.s_addr = 0;
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    RaSocketDeinit(socket_handle1);

    RaSocketInitV1(NETWORK_ONLINE, socket_init, &socket_handle1);

    mocker(calloc, 1, NULL);
    RaSocketInitV1(NETWORK_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    mocker(RaInetPton, 1, 99);
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    mocker(memcpy_s, 1, 1);
    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, &socket_handle1);
    mocker_clean();

    RaSocketInitV1(NETWORK_PEER_ONLINE, socket_init, NULL);
    return;
}

void tc_ra_rdev_get_port_status()
{
    enum PortStatus status = PORT_STATUS_DOWN;
    struct RaRdmaHandle rdma_handle = { 0 };
    struct RaRdmaOps ops = {0};
    int ret;

    ret = RaRdevGetPortStatus(NULL, NULL);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdevInfo.phyId = 100000;
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    rdma_handle.rdevInfo.phyId = 0;
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);

    ops.raRdevGetPortStatus = RaHdcRdevGetPortStatus;
    rdma_handle.rdmaOps = &ops;
    mocker(RaHdcProcessMsg, 5, -1);
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker(RaHdcProcessMsg, 5, 0);
    ret = RaRdevGetPortStatus(&rdma_handle, &status);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    int out_len;
    int op_result;
    int rcv_buf_len = 300;
 
    char in_buf[512];
    char out_buf[512];
 
    ret = RaRsRdevGetPortStatus(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

int ra_recv_wrlist_stub(struct RaQpHandle *handle, struct RecvWrlistData *wr, unsigned int recv_num,
        unsigned int *complete_num)
{
    return 0;
}
void tc_ra_recv_wrlist(void)
{
    int ret;
    struct RaQpHandle qp_handle = {0};
    struct RecvWrlistData wr = {0};
    unsigned int recv_num = 1;
    unsigned int complete_num = 0;

    ret = RaRecvWrlist(NULL, NULL, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.memList.len = 0xffffffff;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr.memList.len = 100;
    qp_handle.rdmaOps = NULL;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raRecvWrlist = ra_recv_wrlist_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaRecvWrlist(&qp_handle, &wr, recv_num, &complete_num);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_ra_peer_epoll_ctl_add()
{
    int ret;

    ret = RaPeerEpollCtlAdd(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlAdd, 3, -1);
    ret = RaPeerEpollCtlAdd(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_set_tcp_recv_callback()
{
    RaSetTcpRecvCallback(NULL, NULL);
    (void)RaPeerSetTcpRecvCallback(0, NULL);

    struct RaSocketHandle abc = {0};
    int cb = 0;
    RaSetTcpRecvCallback(&abc, &cb);

    abc.rdevInfo.phyId = 10000;
    RaSetTcpRecvCallback(&abc, &cb);
    return;
}

void tc_ra_peer_epoll_ctl_mod()
{
    int ret;

    ret = RaPeerEpollCtlMod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlMod, 3, -1);
    ret = RaPeerEpollCtlMod(NULL, RA_EPOLLIN);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_peer_epoll_ctl_del()
{
    int ret;
    struct SocketPeerInfo fd_handle = {0};

    ret = RaPeerEpollCtlDel((const void *)&fd_handle);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RsEpollCtlDel, 3, -1);
    ret = RaPeerEpollCtlDel((const void *)&fd_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    return;
}

void tc_ra_get_qp_context()
{
    struct RaQpHandle RaQpHandle;
    void *qp_handle = (void *)&RaQpHandle;
    void *qp = NULL;
    void *send_cq= NULL;
    void *recv_cq = NULL;
    struct RaRdmaOps ops;
    RaQpHandle.rdmaOps = NULL;
    RaGetQpContext(qp_handle, &qp, &send_cq, &recv_cq);
    RaGetQpContext(NULL, &qp, &send_cq, &recv_cq);
    ops.raGetQpContext = RaPeerGetQpContext;
    RaQpHandle.rdmaOps = &ops;
    RaGetQpContext(qp_handle, &qp, &send_cq, &recv_cq);
}

void tc_peer_log_test()
{
    struct RaInitConfig config = {
        .phyId = 0,
        .nicPosition = 0,
        .hdcType = HDC_SERVICE_TYPE_RDMA,
    };
    mocker(RsInit, 1, 0);
    RaPeerInit(&config, 1);
    int ret = RaPeerInit(&config, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    struct InterfaceInfo interface_infos[1];
    unsigned int num = 0;
    mocker(RsPeerGetIfaddrs, 5, 0);
    RaPeerGetIfaddrs(0, interface_infos, &num);
    mocker_clean();
    RaPeerDeinit(&config);

    mocker((stub_fn_t)RsDeinit, 10, -11);
    ret = RaPeerDeinit(&config);
    EXPECT_INT_EQ(-11, ret);
    mocker_clean();
    return;
}

void tc_ra_rs_send_wr_list_v2()
{
    union OpSendWrlistDataV2 send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;

    union OpSendWrlistDataV2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListV2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list()
{
    union OpSendWrlistData send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;

    union OpSendWrlistData send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrList(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);

    tc_ra_rs_send_wr_list_v2();
}

void tc_ra_rs_send_wr_list_ext_v2()
{
    union OpSendWrlistDataExtV2 send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;
    send_wrlist.txData.wrlist[0].aux.dataType = 0;
    send_wrlist.txData.wrlist[0].aux.reduceType = 0;
    send_wrlist.txData.wrlist[0].aux.notifyOffset = 0;

    union OpSendWrlistDataExtV2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListExtV2(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
}

void tc_ra_rs_send_wr_list_ext()
{
    union OpSendWrlistDataExt send_wrlist;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;
    send_wrlist.txData.wrlist[0].aux.dataType = 0;
    send_wrlist.txData.wrlist[0].aux.reduceType = 0;
    send_wrlist.txData.wrlist[0].aux.notifyOffset = 0;

    union OpSendWrlistDataExt send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    RaRsSendWrListExt(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    tc_ra_rs_send_wr_list_ext_v2();
}

void tc_ra_rs_send_normal_wrlist()
{
    union OpSendWrlistDataExtV2 send_wrlist;
    int ret = 0;

    send_wrlist.txData.phyId = 0;
    send_wrlist.txData.rdevIndex = 0;
    send_wrlist.txData.qpn = 0;
    send_wrlist.txData.sendNum = 1;
    send_wrlist.txData.wrlist[0].op = 0;
    send_wrlist.txData.wrlist[0].sendFlags = 0;
    send_wrlist.txData.wrlist[0].dstAddr = 0;
    send_wrlist.txData.wrlist[0].memList.addr = 0;
    send_wrlist.txData.wrlist[0].memList.len = 0;
    send_wrlist.txData.wrlist[0].memList.lkey = 0;
    send_wrlist.txData.wrlist[0].aux.dataType = 0;
    send_wrlist.txData.wrlist[0].aux.reduceType = 0;
    send_wrlist.txData.wrlist[0].aux.notifyOffset = 0;

    union OpSendWrlistDataExtV2 send_wrlist_out;

    char* in_buf = (char*)(&send_wrlist);
    char* out_buf = (char*)(&send_wrlist_out);

    int out_len;
    int op_result;
    int rcv_buf_len = 0;

    ret = RaRsSendNormalWrlist(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_hdc_send_wrlist_ext_init_v2()
{
    union OpSendWrlistDataExtV2 send_wrlist;
    struct RaQpHandle qp_hdc;
    unsigned int complete_cnt;
    struct WrlistSendCompleteNum wrlist_num;
    RaHdcSendWrlistExtInitV2(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);
}

void tc_ra_hdc_send_wrlist_ext_init()
{
    union OpSendWrlistDataExt send_wrlist;
    struct RaQpHandle qp_hdc;
    unsigned int complete_cnt;
    struct WrlistSendCompleteNum wrlist_num;
    RaHdcSendWrlistExtInit(&send_wrlist, &qp_hdc, complete_cnt, wrlist_num);
    tc_ra_hdc_send_wrlist_ext_init_v2();
}

void tc_ra_hdc_send_wrlist_ext()
{
    mocker_clean();

    struct RaQpHandle qp_handle;

    struct SendWrlistDataExt wr[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num;
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;

    struct RaQpHandle handle;
    handle.qpMode = 1;

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    int ret = RaHdcSendWrlist(&handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();

    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist_3, 1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(&qp_handle, wr, op_rsp, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -EINVAL);
    mocker_clean();
}

void tc_host_ra_send_wrlist_ext()
{
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    struct SendWrlistDataExt wr[1];
    struct SendWrRsp op_rsp[1];

    unsigned int complete_num;

    RaSendWrlistExt(&qp_handle, wr, op_rsp, 1, &complete_num);
}

void tc_ra_hdc_send_normal_wrlist()
{
    struct RaQpHandle qp_handle;
    struct WrInfo wr[1];
    struct SendWrRsp op_rsp[1];
    struct WrlistSendCompleteNum wrlist_num = { 0 };
    unsigned int complete_num = 0;
    wrlist_num.sendNum = 1;
    wrlist_num.completeNum = &complete_num;
    int ret = 0;

    mocker_clean();

    qp_handle.qpMode = RA_RS_OP_QP_MODE;
    qp_handle.supportLite = 1;
    RaHdcSendNormalWrlist(&qp_handle, wr, op_rsp, wrlist_num);

    qp_handle.qpMode = RA_RS_NOR_QP_MODE;
    wrlist_num.completeNum = &complete_num;
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_msg_wrlist, 1);
    ret = RaHdcSendNormalWrlist(&qp_handle, wr, op_rsp, wrlist_num);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

int ra_send_normal_wrlist_stub(void *qp_handle, struct WrInfo wr[], struct SendWrRsp op_rsp[], unsigned int send_num,
    unsigned int *complete_num)
{
    return 0;
}

void tc_host_ra_send_normal_wrlist()
{
    struct RaRdmaOps rdma_ops = {0};
    struct RaQpHandle qp_handle;
    struct SendWrRsp op_rsp[1];
    qp_handle.rdmaOps = NULL;
    unsigned int complete_num;
    struct WrInfo wr[1];
    int ret = 0;

    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    rdma_ops.raSendNormalWrlist = ra_send_normal_wrlist_stub;
    qp_handle.rdmaOps = &rdma_ops;
    wr[0].memList.len = MAX_SG_LIST_LEN_MAX + 1;
    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(128103, ret);

    wr[0].memList.len = 1;
    ret = RaSendNormalWrlist(&qp_handle, wr, op_rsp, 1, &complete_num);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_create_cq()
{
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

    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);

    struct RaRdmaOps ops;
    ops.raCqCreate = RaPeerCqCreate;
    ops.raCqDestroy = RaPeerCqDestroy;
    RaRdmaHandle.rdmaOps = &ops;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCqCreate(rdma_handle, &attr);
    RaCqDestroy(rdma_handle, &attr);
}

void tc_ra_create_notmal_qp()
{
    struct ibv_cq *ibSendCq;
    struct ibv_cq *ibRecvCq;
    void* context;
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
    struct RaQpHandle *qp_handle = NULL;

    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaNormalQpDestroy(qp_handle);

    struct RaRdmaOps ops;
    ops.raNormalQpCreate = RaPeerNormalQpCreate;
    ops.raNormalQpDestroy = RaPeerNormalQpDestroy;
    RaRdmaHandle.rdmaOps = &ops;
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaNormalQpDestroy(qp_handle);

    RaNormalQpCreate(rdma_handle, &qp_init_attr, NULL, &qp);
    RaNormalQpDestroy(NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    RaNormalQpDestroy(qp_handle);

    mocker((stub_fn_t)RaPeerNormalQpCreate, 10, -1);
    RaNormalQpCreate(rdma_handle, &qp_init_attr, &qp_handle, &qp);
    mocker_clean();
}
int ra_set_qp_attr_qos_stub(struct RaQpHandle *qp_stub, struct QosAttr *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_qos()
{
    int ret;
    struct QosAttr attr = {0};
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrQos(NULL, &attr);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrQos(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 256;
    attr.sl = 8;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.sl = 4;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    attr.tc = 33 * 4;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrQos = ra_set_qp_attr_qos_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrQos(&qp_handle, &attr);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_timeout_stub(struct RaQpHandle *qp_stub, unsigned int *attr)
{
    return 0;
}

void tc_ra_set_qp_attr_timeout()
{
    int ret;
    unsigned int timeout = 0;
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrTimeout(NULL, &timeout);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrTimeout(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 4;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    timeout = 23;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrTimeout = ra_set_qp_attr_timeout_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrTimeout(&qp_handle, &timeout);
    EXPECT_INT_EQ(0, ret);

    return;
}

int ra_set_qp_attr_retry_cnt_stub(struct RaQpHandle *qp_stub, unsigned int *retry_cnt)
{
    return 0;
}

void tc_ra_set_qp_attr_retry_cnt()
{
    int ret;
    unsigned int retry_cnt = 0;
    struct RaQpHandle qp_handle;
    qp_handle.rdmaOps = NULL;

    ret = RaSetQpAttrRetryCnt(NULL, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    ret = RaSetQpAttrRetryCnt(&qp_handle, NULL);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 8;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 4;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    retry_cnt = 7;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(128103, ret);

    struct RaRdmaOps rdma_ops = {0};
    rdma_ops.raSetQpAttrRetryCnt = ra_set_qp_attr_retry_cnt_stub;
    qp_handle.rdmaOps = &rdma_ops;
    ret = RaSetQpAttrRetryCnt(&qp_handle, &retry_cnt);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_ra_create_comp_channel()
{
    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;

    void *comp_channel = NULL;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    comp_channel = (void *)0xabcd;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    struct RaRdmaOps ops;
    ops.raCreateCompChannel = RaPeerCreateCompChannel;
    ops.raDestroyCompChannel = RaPeerDestroyCompChannel;
    RaRdmaHandle.rdmaOps = &ops;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);

    RaCreateCompChannel(rdma_handle, NULL);
    RaDestroyCompChannel(rdma_handle, NULL);
    RaCreateCompChannel(NULL, NULL);
    RaDestroyCompChannel(NULL, NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCreateCompChannel(rdma_handle, &comp_channel);
    RaDestroyCompChannel(rdma_handle, comp_channel);
}

void tc_ra_get_cqe_err_info()
{
    int ret;
    struct CqeErrInfo info = {0};

    ret = RaGetCqeErrInfo(0, NULL);
    EXPECT_INT_EQ(128103, ret);

    mocker(RaHdcProcessMsg, 1, 0);
    ret = RaGetCqeErrInfo(0, &info);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_ra_rdev_get_cqe_err_info_list()
{
    struct RaRdmaHandle RaRdmaHandle;
    struct CqeErrInfo info[128] = {0};
    unsigned int num = 128;
    int ret;

    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;

    mocker(RaHdcGetCqeErrInfoList, 10, 0);
    ret = RaRdevGetCqeErrInfoList((void *)&RaRdmaHandle, info, &num);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevGetCqeErrInfoList((void *)&RaRdmaHandle, info, NULL);
    EXPECT_INT_EQ(128103, ret);

    num = 129;
    ret = RaRdevGetCqeErrInfoList((void *)&RaRdmaHandle, info, &num);
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

    ret = RaRsGetIfnum(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
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

    ret = RaRsPingInit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingInit, 1, 12);
    ret = RaRsPingInit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingTargetAdd(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingTargetAdd, 1, 12);
    ret = RaRsPingTargetAdd(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingTaskStart(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingTaskStart, 1, 12);
    ret = RaRsPingTaskStart(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingGetResults(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingGetResults, 1, 12);
    ret = RaRsPingGetResults(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingTaskStop(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingTaskStop, 1, 12);
    ret = RaRsPingTaskStop(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingTargetDel(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingTargetDel, 1, 12);
    ret = RaRsPingTargetDel(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    ret = RaRsPingDeinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker(RsPingDeinit, 1, 12);
    ret = RaRsPingDeinit(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    return;
}

void tc_ra_get_qp_attr()
{
    struct QpAttr qpn = {0};
    struct RaQpHandle qp_handle = {0};
    struct RaRdmaHandle rdma_handle = {0};
    int ret;

    ret = RaGetQpAttr(NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    qp_handle.rdmaHandle = &rdma_handle;
    ret = RaGetQpAttr(&qp_handle, &qpn);
    EXPECT_INT_EQ(0, ret);
    return;
}

void tc_ra_create_srq()
{
    struct RaRdmaHandle RaRdmaHandle;
    void *rdma_handle = (void *)&RaRdmaHandle;
    RaRdmaHandle.rdevIndex = 0;
    RaRdmaHandle.rdevInfo.phyId = 32767;
    RaRdmaHandle.rdmaOps = NULL;
    struct SrqAttr attr = {0};

    RaCreateSrq(rdma_handle, NULL);
    RaDestroySrq(rdma_handle, NULL);

    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);

    struct RaRdmaOps ops;
    ops.raCreateSrq = RaPeerCreateSrq;
    ops.raDestroySrq = RaPeerDestroySrq;
    RaRdmaHandle.rdmaOps = &ops;
    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);

    RaCreateSrq(NULL, NULL);
    RaDestroySrq(NULL, NULL);

    RaRdmaHandle.rdevInfo.phyId = 0;
    RaCreateSrq(rdma_handle, &attr);
    RaDestroySrq(rdma_handle, &attr);
}

void tc_hdc_send_wr_op()
{
    mocker_clean();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

	ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);


    struct SendWr wr = {0};
    struct SendWrRsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct SgList mem;
    mem.addr = addr;
	mem.len = 10;
	wr.bufList = &mem;
	wr.dstAddr = 0x111;
	wr.bufNum = 1;
	wr.op = 0;
	wr.sendFlag = 0;
    qp_handle->localMr[0].addr = wr.bufList[0].addr;
    qp_handle->localMr[0].len = 0x10000;
    qp_handle->remMr[0].addr = wr.dstAddr;
    qp_handle->remMr[0].len = 0x10000;
    qp_handle->sendWrNum = 999;
    mocker_invoke((stub_fn_t)RaRdmaLitePollCq, stub_RaRdmaLitePollCq, 10);
    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 2;
    wrlist_num.completeNum = &complete_num;
    struct SendWrlistData wrlist[wrlist_num.sendNum];
	struct SendWrRsp op_rsp_list[wrlist_num.sendNum];
	wrlist[0].memList = mem;
	wrlist[0].dstAddr = 0x111;
	wrlist[0].op = 0;
	wrlist[0].sendFlags = 0;
	wrlist[1].memList = mem;
	wrlist[1].dstAddr = 0x111;
	wrlist[1].op = 0;
	wrlist[1].sendFlags = 0;
    qp_handle->rdmaOps = rdma_handle.rdmaOps;
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    struct SendWrlistDataExt data_ext[wrlist_num.sendNum];
    data_ext[0].memList = mem;
	data_ext[0].dstAddr = 0x111;
	data_ext[0].op = 0;
	data_ext[0].sendFlags = 0;
	data_ext[1].memList = mem;
	data_ext[1].dstAddr = 0x111;
	data_ext[1].op = 0;
	data_ext[1].sendFlags = 0;
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -1);
    g_interface_version = RA_RS_SEND_WRLIST_EXT_V2_VERSION;
    mocker_invoke((stub_fn_t)RaGetInterfaceVersion, (stub_fn_t)stub_ra_get_interface_version, 1);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    g_interface_version = 0;
    EXPECT_INT_EQ(ret, -1);

    RaHdcLitePeriodPollCqe(&rdma_handle);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    RaHdcRdevDeinit(&rdma_handle, NOTIFY);

    free(addr);
    mocker_clean();
}

void tc_hdc_lite_send_wr_op()
{
    mocker_clean();
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;

    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    int ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);

    struct SendWr wr = {0};
    struct SendWrRsp rsp = {0};
    int i = 0;

    ASSERT_ADDR_NE(qp_handle, NULL);

    void *addr = malloc(10);
    struct SgList mem;
    mem.addr = addr;
    mem.len = 10;
    wr.bufList = &mem;
    wr.dstAddr = 0x111;
    wr.bufNum = 1;
    wr.op = 0;
    wr.sendFlag = 0;
    qp_handle->localMr[0].addr = wr.bufList[0].addr;
    qp_handle->localMr[0].len = 0x10000;
    qp_handle->remMr[0].addr = wr.dstAddr;
    qp_handle->remMr[0].len = 0x10000;
    qp_handle->sendWrNum = 999;
    qp_handle->supportLite = 1;
    mocker_invoke((stub_fn_t)RaRdmaLitePollCq, stub_RaRdmaLitePollCq, 10);
    ret = RaHdcSendWr(qp_handle, &wr, &rsp);
    EXPECT_INT_EQ(ret, 0);

    unsigned int complete_num = 0;
    struct WrlistSendCompleteNum wrlist_num;
    wrlist_num.sendNum = 2;
    wrlist_num.completeNum = &complete_num;
    struct SendWrlistData wrlist[wrlist_num.sendNum];
    struct SendWrRsp op_rsp_list[wrlist_num.sendNum];
    wrlist[0].memList = mem;
    wrlist[0].dstAddr = 0x111;
    wrlist[0].op = 0;
    wrlist[0].sendFlags = 0;
    wrlist[1].memList = mem;
    wrlist[1].dstAddr = 0x111;
    wrlist[1].op = 0;
    wrlist[1].sendFlags = 0;
    qp_handle->rdmaOps = rdma_handle.rdmaOps;
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    struct SendWrlistDataExt data_ext[wrlist_num.sendNum];
    data_ext[0].memList = mem;
    data_ext[0].dstAddr = 0x111;
    data_ext[0].op = 0;
    data_ext[0].sendFlags = 0;
    data_ext[1].memList = mem;
    data_ext[1].dstAddr = 0x111;
    data_ext[1].op = 0;
    data_ext[1].sendFlags = 0;
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(0, ret);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -12);
    ret = RaHdcSendWrlist(qp_handle, wrlist, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    mocker((stub_fn_t)RaHdcLitePostSend, 10, -12);
    ret = RaHdcSendWrlistExt(qp_handle, data_ext, op_rsp_list, wrlist_num);
    EXPECT_INT_EQ(ret, -12);

    RaHdcLitePeriodPollCqe(&rdma_handle);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);

    RaHdcRdevDeinit(&rdma_handle, NOTIFY);

    free(addr);
    mocker_clean();
}

void tc_ra_create_event_handle(void)
{
    int ret;
    int fd;

    ret = RaCreateEventHandle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaCreateEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerCreateEventHandle, 1024, -EINVAL);
    ret = RaCreateEventHandle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_ctl_event_handle(void)
{
    int ret;
    int fd = 0;
    int fd_handle;

    ret = RaCtlEventHandle(-1, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, NULL, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 4, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 1, 100);
    EXPECT_INT_NE(0, ret);

    ret = RaCtlEventHandle(fd, &fd_handle, 1, 0);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerCtlEventHandle, 1024, -EINVAL);
    ret = RaCtlEventHandle(fd, &fd_handle, 1, 0);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_wait_event_handle(void)
{
    int ret;
    int fd = 0;
    unsigned int events_num = 0;
    struct SocketEventInfoT event_info = {};

    ret = RaWaitEventHandle(-1, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, NULL, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, -2, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, -1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, 1, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaWaitEventHandle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerWaitEventHandle, 1024, -EINVAL);
    ret = RaWaitEventHandle(fd, &event_info, 0, 1, &events_num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_destroy_event_handle(void)
{
    int ret;
    int fd;

    ret = RaDestroyEventHandle(NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaDestroyEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);

    mocker(RaPeerDestroyEventHandle, 1024, -EINVAL);
    ret = RaDestroyEventHandle(&fd);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_peer_create_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerCreateEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_ctl_event_handle()
{
    int ret;
    int fd_handle;

    ret = RaPeerCtlEventHandle(0, NULL, 0, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(-EINVAL, ret);

    ret = RaPeerCtlEventHandle(0, &fd_handle, 1, RA_EPOLLONESHOT);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_wait_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerWaitEventHandle(0, NULL, 0, -1, 0);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_destroy_event_handle()
{
    int ret;
    int fd;

    ret = RaPeerDestroyEventHandle(&fd);
    EXPECT_INT_EQ(0, ret);
}

void tc_ra_peer_socket_batch_abort()
{
    unsigned int dev_id;
    struct SocketConnectInfoT conn[4] = {0};
    int ret = 0;
 
    mocker(RaGetSocketConnectInfo, 20, 1);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_NE(ret, 0);
    mocker_clean();
 
    mocker(RaGetSocketConnectInfo, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(RsSocketBatchAbort, 10, 1);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 1);
    mocker_clean();
 
    mocker(RaGetSocketConnectInfo, 20, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(RsSocketBatchAbort, 10, 0);
    ret = RaPeerSocketBatchAbort(dev_id, conn, 5);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

int ra_hdc_poll_cq_stub(struct RaQpHandle *qp_hdc, bool is_send_cq, unsigned int num_entries, void *wc)
{
    if (is_send_cq) {
        return -1;
    }
    return 0;
}

void tc_ra_poll_cq(void)
{
    int ret;
    struct RaQpHandle qp_handle = {0};
    struct RaRdmaOps rdma_ops = {0};
    struct rdma_lite_wc_v2 lite_wc = {0};

    ret = RaPollCq(NULL, true, 0, NULL);
    EXPECT_INT_NE(0, ret);

    qp_handle.rdmaOps = &rdma_ops;
    rdma_ops.raPollCq = NULL;
    ret = RaPollCq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    rdma_ops.raPollCq = ra_hdc_poll_cq_stub;
    ret = RaPollCq(&qp_handle, true, 1, &lite_wc);
    EXPECT_INT_NE(0, ret);

    ret = RaPollCq(&qp_handle, false, 1, &lite_wc);
    EXPECT_INT_EQ(0, ret);
}


void tc_hdc_recv_wrlist()
{
    mocker_clean();
    void *addr = NULL;
    int size = 0;
    int ret = 0;
    struct RecvWrlistData rev_wr = {0};
    unsigned int recv_num = 1;
    unsigned int rev_complete_num = 0;
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;
    struct RaQpHandle qp_handle_tmp = { 0 };

    rev_wr.wrId = 100;
    rev_wr.memList.lkey = 0xff;
    rev_wr.memList.addr = addr;
    rev_wr.memList.len = size;

    // abnormal case
    qp_handle_tmp.qpMode = 0;
    ret = RaHdcRecvWrlist(&qp_handle_tmp, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_NE(ret, 0);

    // normal case
    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->supportLite = 1;

    ret = RaHdcRecvWrlist(qp_handle, &rev_wr, recv_num, &rev_complete_num);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_hdc_poll_cq()
{
    mocker_clean();
    int ret = 0;
    unsigned int num_entries = 1;
    struct rdma_lite_wc_v2 lite_wc = {0};
    struct rdev rdev_info = {0};
    unsigned int rdevIndex = 0;
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle* qp_handle = NULL;
    struct RaQpHandle qp_handle_tmp = { 0 };

    // abnormal case
    qp_handle_tmp.qpMode = 0;
    ret = RaHdcPollCq(&qp_handle_tmp, true, num_entries, &lite_wc);
    EXPECT_INT_NE(ret, 0);

    // normal case
    mocker_invoke((stub_fn_t)RaHdcGetOpcodeLiteSupport, (stub_fn_t)stub_ra_hdc_get_opcode_lite_support, 100);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_process_rdev_init, 100);
    rdma_handle.disabledLiteThread = true;
    ret = RaHdcRdevInit(&rdma_handle, NOTIFY, rdev_info, &rdevIndex);
    EXPECT_INT_EQ(ret, 0);
    ret = RaHdcQpCreate(&rdma_handle, 0, 2, &qp_handle);
    EXPECT_INT_EQ(ret, 0);
    ASSERT_ADDR_NE(qp_handle, NULL);
    qp_handle->supportLite = 1;
    qp_handle->recvWrNum = 1;

    ret = RaHdcPollCq(qp_handle, false, num_entries, &lite_wc);
    EXPECT_INT_EQ(ret, 0);

    ret = RaHdcQpDestroy(qp_handle);
    EXPECT_INT_EQ(ret, 0);
    RaHdcRdevDeinit(&rdma_handle, NOTIFY);
    mocker_clean();
}

void tc_hdc_get_lite_support()
{
    int support;

    g_interface_version = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x3, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x3, &support);
    EXPECT_INT_EQ(support, 1);
    mocker_clean();

    g_interface_version = 2;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x2, &support);
    EXPECT_INT_EQ(support, 2);
    mocker_clean();

    g_interface_version = 1;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    RaHdcGetOpcodeLiteSupport(0, 0x2, &support);
    EXPECT_INT_EQ(support, 0);
    mocker_clean();
}

void tc_ra_rdev_get_support_lite()
{
    struct RaRdmaHandle rdma_handle = {0};
    int support_lite = 1;
    int ret;

    ret = RaRdevGetSupportLite(NULL, NULL);
    EXPECT_INT_NE(ret, 0);

    ret = RaRdevGetSupportLite(&rdma_handle, &support_lite);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(support_lite, rdma_handle.supportLite);
}

void tc_ra_rdev_get_handle()
{
    void *rdma_handle = NULL;
    int ret;

    ret = RaRdevGetHandle(1024, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaRdevGetHandle(0, NULL);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaRdevGetHandle(0, &rdma_handle);
    EXPECT_INT_EQ(ret, -ENODEV);
}

void tc_ra_is_first_or_last_used()
{
    s32 ret = 0;

    ret = RaIsFirstUsed(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsFirstUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(-1);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 0);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsLastUsed(0);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsLastUsed(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(128);
    EXPECT_INT_EQ(ret, -EINVAL);

    ret = RaIsFirstUsed(15);
    EXPECT_INT_EQ(ret, 1);

    ret = RaIsLastUsed(15);
    EXPECT_INT_EQ(ret, 1);
}

void tc_ra_rs_socket_port_is_use()
{
    unsigned int size = sizeof(union OpSocketConnectData) + sizeof(struct MsgHead);
    union OpSocketConnectData socket_connect_data = {{0}};
    unsigned int port = 0x16;

    socket_connect_data.txData.conn[0].port = port;
    socket_connect_data.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    RaRsSocketBatchConnect(in_buf, out_buf, &out_len, &op_result, size);

    socket_connect_data.txData.num = 1U | (1U << 31U);
    socket_connect_data.txData.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_connect_data, sizeof(union OpSocketConnectData));
    RaRsSocketBatchConnect(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;

    size = sizeof(union OpSocketListenData) + sizeof(struct MsgHead);
    union OpSocketListenData socket_listen_data = {{0}};
    socket_listen_data.txData.conn[0].port = port;
    socket_listen_data.txData.num = 1;

    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    RaRsSocketListenStart(in_buf, out_buf, &out_len, &op_result, size);
    RaRsSocketListenStop(in_buf, out_buf, &out_len, &op_result, size);

    socket_listen_data.txData.num = 1U | (1U << 31U);
    socket_listen_data.txData.conn[0].port = 0xFFFFFFFF;
    memcpy(in_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_listen_data, sizeof(union OpSocketListenData));
    RaRsSocketListenStart(in_buf, out_buf, &out_len, &op_result, size);
    RaRsSocketListenStop(in_buf, out_buf, &out_len, &op_result, size);

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

    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, NULL, 0, NULL);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0xFFFF, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    ret = RaSocketGetVnicIpInfos(0, 0xFFFF, ids, 1, infos);
    EXPECT_INT_NE(0, ret);

    g_interface_version = 0;
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_get_interface_version, 100);
    ret = RaSocketGetVnicIpInfos(0, PHY_ID_VNIC_IP, ids, 1, infos);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}

void tc_ra_rs_get_vnic_ip_infos_v1()
{
    unsigned int size = sizeof(union OpGetVnicIpInfosDataV1) + sizeof(struct MsgHead);
    union OpGetVnicIpInfosDataV1 vnic_infos = {{0}};

    vnic_infos.txData.phyId = 0;
    vnic_infos.txData.type = 0;
    vnic_infos.txData.ids[0] = 3232235521;
    vnic_infos.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosDataV1));
    memcpy(out_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosDataV1));
    RaRsGetVnicIpInfosV1(in_buf, out_buf, &out_len, &op_result, size);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_vnic_ip_infos()
{
    unsigned int size = sizeof(union OpGetVnicIpInfosData) + sizeof(struct MsgHead);
    union OpGetVnicIpInfosData vnic_infos = {{0}};
    int ret;

    vnic_infos.txData.phyId = 0;
    vnic_infos.txData.type = 0;
    vnic_infos.txData.ids[0] = 3232235521;
    vnic_infos.txData.num = 1;

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;

    memcpy(in_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosData));
    memcpy(out_buf + sizeof(struct MsgHead), &vnic_infos, sizeof(union OpGetVnicIpInfosData));
    RaRsGetVnicIpInfos(in_buf, out_buf, &out_len, &op_result, size);

    mocker(RsGetVnicIp, 1, 12);
    ret = RaRsRdevInitWithBackup(in_buf, out_buf, &size, &op_result, size);
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

    ret = RaRsTypicalMrRegV1(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = RaRsTypicalMrDereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);

    ret = RaRsTypicalMrReg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
    ret = RaRsTypicalMrDereg(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_rs_get_cqe_err_info()
{
    char *cqe_list_buf = NULL;
    char *cqe_num_buf = NULL;
    int op_result;
    int size;
    int ret;

    size = sizeof(union OpGetCqeErrInfoListData) + sizeof(struct MsgHead);
    cqe_num_buf = calloc(1, size);
    ret = RaRsGetCqeErrInfoNum(cqe_num_buf, cqe_num_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    size = sizeof(union OpGetCqeErrInfoListData) + sizeof(struct MsgHead);
    cqe_list_buf = calloc(1, size);
    ret = RaRsGetCqeErrInfoList(cqe_list_buf, cqe_list_buf, &size, &op_result, size);
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

    ret = RaRsRdevInitWithBackup(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_rs_get_qp_status()
{
    union OpQpStatusData qp_status_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpQpStatusData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    qp_status_data.txData.phyId = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct MsgHead), &qp_status_data, sizeof(union OpQpStatusData));
    memcpy(out_buf + sizeof(struct MsgHead), &qp_status_data, sizeof(union OpQpStatusData));
    ret = RaRsGetQpStatus(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_qp_info()
{
    union OpQpInfoData qp_info_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpQpInfoData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    qp_info_data.txData.phyId = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct MsgHead), &qp_info_data, sizeof(union OpQpInfoData));
    memcpy(out_buf + sizeof(struct MsgHead), &qp_info_data, sizeof(union OpQpInfoData));
    ret = RaRsGetQpInfo(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_sockets()
{
    union OpSocketInfoData socket_info_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpSocketInfoData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    socket_info_data.txData.num = 1U | (1U << 31U);
    memcpy(in_buf + sizeof(struct MsgHead), &socket_info_data, sizeof(union OpSocketInfoData));
    memcpy(out_buf + sizeof(struct MsgHead), &socket_info_data, sizeof(union OpSocketInfoData));
    ret = RaRsGetSockets(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_rdev_init_with_backup()
{
    union OpRdevInitWithBackupData rdev_init_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpRdevInitWithBackupData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &rdev_init_data, sizeof(union OpRdevInitWithBackupData));
    memcpy(out_buf + sizeof(struct MsgHead), &rdev_init_data, sizeof(union OpRdevInitWithBackupData));
    mocker(RsRdevInitWithBackup, 1, 12);
    ret = RaRsRdevInitWithBackup(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_cqe_err_info_01()
{
    union OpGetCqeErrInfoData cqe_err_info_ret = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpGetCqeErrInfoData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &cqe_err_info_ret, sizeof(union OpGetCqeErrInfoData));
    memcpy(out_buf + sizeof(struct MsgHead), &cqe_err_info_ret, sizeof(union OpGetCqeErrInfoData));
    mocker(RsGetCqeErrInfo, 1, 12);
    ret = RaRsGetCqeErrInfo(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_ai_qp_create()
{
    union OpAiQpCreateData create_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpAiQpCreateData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &create_data, sizeof(union OpAiQpCreateData));
    memcpy(out_buf + sizeof(struct MsgHead), &create_data, sizeof(union OpAiQpCreateData));
    mocker(RsQpCreateWithAttrs, 1, 12);
    ret = RaRsAiQpCreate(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_typical_qp_modify()
{
    union OpTypicalQpModifyData qp_modify_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpTypicalQpModifyData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &qp_modify_data, sizeof(union OpTypicalQpModifyData));
    memcpy(out_buf + sizeof(struct MsgHead), &qp_modify_data, sizeof(union OpTypicalQpModifyData));
    mocker(RsTypicalQpModify, 1, 12);
    ret = RaRsTypicalQpModify(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

int ra_socket_batch_abort_stub(unsigned int phyId, struct SocketConnectInfoT conn[], unsigned int num)
{
    return 0;
}
 
void tc_ra_socket_batch_abort(void)
{
    int ret;
    unsigned int phyId;
    struct SocketConnectInfoT conn = {0};
    struct RaSocketHandle socket_handle = {0};
    struct RaSocketOps socket_ops = {0};
 
    ret = RaSocketBatchAbort(NULL, 0);
    EXPECT_INT_EQ(128203, ret);
 
    conn.socketHandle = NULL;
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);
 
    socket_ops.raSocketBatchAbort = ra_socket_batch_abort_stub;
    socket_handle.socketOps = &socket_ops;
    socket_handle.rdevInfo.phyId = 16;
    conn.socketHandle = &socket_handle;
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(128203, ret);
 
    socket_handle.rdevInfo.phyId = 0;
    mocker(RaInetPton, 5, -1);
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
 
    mocker(RaInetPton, 5, 0);
    ret = RaSocketBatchAbort(&conn, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

int stub_ra_hdc_get_cqe_err_num(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    if (data_size == sizeof(union OpGetCqeErrInfoNumData)) {
        union OpGetCqeErrInfoNumData *cqe_err_info_num_data = (union OpGetCqeErrInfoNumData *)data;
        cqe_err_info_num_data->rxData.num = 10;
    } else if (data_size == sizeof(union OpGetCqeErrInfoListData)) {
        union OpGetCqeErrInfoListData *cqe_err_info_list =
            (union op_get_cqe_eop_get_cqe_err_info_list_datarr_info_num_data *)data;
        cqe_err_info_list->rxData.num = 1;
    }
    return 0;
}

int stub_ra_hdc_get_cqe_err_num_v2(unsigned int opcode, int device_id, char *data, unsigned int data_size)
{
    stub_ra_hdc_get_cqe_err_num(opcode, device_id, data, data_size);
    ((union OpGetCqeErrInfoListData *)data)->rxData.infoList[0].qpn = 12345;
    return 0;
}

int stub_ra_hdc_get_cqe_err_info_num(struct RaRdmaHandle *rdma_handle, unsigned int *num)
{
    *num = 10;
    return 0;
}

int stub_ra_hdc_get_interface_version(unsigned int phyId, unsigned int interface_opcode, unsigned int *interface_version)
{
    *interface_version = 1;
    return 0;
}

int stub_ra_hdc_lite_get_cqe_err_info_list(struct RaRdmaHandle *rdma_handle, struct CqeErrInfo *info_list,
    unsigned int *num)
{
    *num = 10;
    return 0;
}

void tc_ra_hdc_get_cqe_err_info_list()
{
    struct RaRdmaHandle rdma_handle = {0};
    struct RaQpHandle qp_hdc = {0};
    struct CqeErrInfo info[130] = {0};
    int num = 11;
    int ret = 0;

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker((stub_fn_t)RaHdcProcessMsg, 10, 0);
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 10);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    num = 11;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num_v2, 10);
    num = 129;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(info[10].qpn, 12345);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 11);

    mocker_clean();
    mocker_invoke(RaHdcLiteGetCqeErrInfoList, stub_ra_hdc_lite_get_cqe_err_info_list, 1);
    mocker_invoke(RaHdcGetCqeErrInfoNum, stub_ra_hdc_get_cqe_err_info_num, 1);
    mocker_invoke((stub_fn_t)RaHdcProcessMsg, (stub_fn_t)stub_ra_hdc_get_cqe_err_num, 10);
    mocker((stub_fn_t)memcpy_s, 1, -1);
    num = 11;
    ret = RaHdcGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, -1);

    mocker_clean();
    rdma_handle.supportLite = 0;
    ret = RaHdcLiteGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    rdma_handle.supportLite = 1;
    rdma_handle.cqeErrCnt = 0;
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    ret = RaHdcLiteGetCqeErrInfoList(&rdma_handle, info, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(RaHdcGetInterfaceVersion, 10, -1);
    ret = RaHdcGetCqeErrInfoNum(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    EXPECT_INT_EQ(num, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, stub_ra_hdc_get_interface_version, 1);
    ret = RaHdcGetCqeErrInfoNum(&rdma_handle, &num);
    EXPECT_INT_EQ(ret, 0);
    return;
}

void tc_ra_hdc_lite_ctx_init()
{
    struct RaRdmaHandle rdma_handle = {0};
    unsigned int phyId = 0;
    unsigned int rdevIndex = 0;
    struct rdma_lite_context rdma_lite_context = {0};
    int ret = 0;

    rdma_handle.supportLite = 2 * 1024 * 1024;
    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 1, NULL);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 10, &rdma_lite_context);
    mocker(RaHdcLiteMutexInit, 10, 0);
    rdma_handle.disabledLiteThread = true;
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    rdma_handle.disabledLiteThread = false;
    mocker(pthread_create, 10, -1);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, -258);

    mocker_clean();
    mocker(RaHdcLiteMutexDeinit, 10, 0);
    mocker(RaRdmaLiteFreeCtx, 10, 0);
    mocker(DlHalSensorNodeUnregister, 10, 0);
    mocker(DlDrvDeviceGetIndexByPhyId, 10, 0);
    mocker(RaSensorNodeRegister, 10, 0);
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteAllocCtx, 10, &rdma_lite_context);
    mocker(RaHdcLiteMutexInit, 10, 0);
    mocker(pthread_create, 10, 0);
    ret = RaHdcLiteCtxInit(&rdma_handle, phyId, rdevIndex);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 10, 0);
    ret = RaHdcLiteMutexInit(&rdma_handle, phyId);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(pthread_mutex_init, 1, -1);
    ret = RaHdcLiteMutexInit(&rdma_handle, phyId);
    EXPECT_INT_EQ(ret, -258);
}

struct rdma_lite_cq *stub_RaRdmaLiteCreateCq(struct rdma_lite_context *lite_ctx,
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
    struct RaQpHandle qp_hdc = {0};
    struct rdma_lite_qp_cap cap = {0};
    struct rdma_lite_cq lite_cq = {0};
    struct rdma_lite_qp lite_qp = {0};
    struct RaRdmaHandle rdma_handle = {0};
    struct rdma_lite_cq_attr lite_send_cq_attr = {0};
    struct rdma_lite_cq_attr lite_recv_cq_attr = {0};
    struct rdma_lite_qp_attr lite_qp_attr = {0};
    struct rdma_lite_wc lite_wc = {0};
    unsigned int api_version = 0;
    int ret = 0;

    qp_hdc.list.next = &(qp_hdc.list);
    qp_hdc.list.prev = &(qp_hdc.list);
    rdma_handle.qpList.next = &(rdma_handle.qpList);
    rdma_handle.qpList.prev = &(rdma_handle.qpList);
    qp_hdc.supportLite = 1;
    qp_hdc.qpMode = 2;
    rdma_handle.supportLite = 1;
    cap.max_inline_data = QP_DEFAULT_MAX_CAP_INLINE_DATA;
    cap.max_send_sge = QP_DEFAULT_MIN_CAP_SEND_SGE;
    cap.max_recv_sge = 1;
    cap.max_send_wr = RA_QP_TX_DEPTH;
    cap.max_recv_wr = RA_QP_TX_DEPTH;

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 1, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker_invoke(RaRdmaLiteCreateCq, stub_RaRdmaLiteCreateCq, 2);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 2, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 1, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -14);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyQp, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 10, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, NULL);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, -12);

    mocker_clean();
    mocker(RaHdcLiteGetCqQpAttr, 10, 0);
    mocker(RaHdcLiteInitMemPool, 10, 0);
    mocker(RaRdmaLiteDestroyCq, 10, 0);
    mocker(RaHdcLiteDeinitMemPool, 10, 0);
    mocker(RaRdmaLiteCreateCq, 10, &lite_cq);
    mocker(RaRdmaLiteCreateQp, 10, &lite_qp);
    mocker(pthread_mutex_init, 10, 0);
    mocker(pthread_mutex_lock, 10, 0);
    mocker(pthread_mutex_unlock, 10, 0);
    mocker(calloc, 10, &lite_wc);
    ret = RaHdcLiteQpCreate(&rdma_handle, &qp_hdc, &cap);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaHdcLiteQpAttrInit, 10, 0);
    mocker(memcpy_s, 10, 0);
    ret = RaHdcLiteGetCqQpAttr(&qp_hdc, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaRdmaLiteInitMemPool, 10, 0);
    ret = RaHdcLiteInitMemPool(&qp_hdc, &cap, &lite_send_cq_attr, &lite_recv_cq_attr, &lite_qp_attr);
    EXPECT_INT_EQ(ret, 0);

    mocker_clean();
    mocker(RaRdmaLiteDeinitMemPool, 10, 0);
    RaHdcLiteDeinitMemPool(&rdma_handle, &qp_hdc);

    mocker_clean();
    ret = RaRdmaLiteGetApiVersion();
    EXPECT_INT_EQ(ret, 0);
}

void tc_ra_get_client_socket_err_info()
{
    int ret = 0;
    struct SocketConnectInfoT conn[10] = {0};
    struct SocketErrInfo err[10] = {0};
    unsigned int num = 1;
    struct RaSocketHandle *socket_handle = NULL;
    struct rdev rdev_info = {0};
 
    socket_handle = malloc(sizeof(struct RaSocketHandle));
    socket_handle->rdevInfo = rdev_info;
    mocker_clean();
 
    conn[0].socketHandle = NULL;
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(128203, ret);
 
    conn[0].socketHandle = socket_handle;
    socket_handle->socketOps = &gRaPeerSocketOps;
    rdev_info.phyId = 0;
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketConnectInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetClientSocketErrInfo, 1, 1);
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();
 
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketConnectInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetClientSocketErrInfo, 1, 0);
    ret = RaGetClientSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
 
    free(socket_handle);
    socket_handle = NULL;
}
 
void tc_ra_get_server_socket_err_info()
{
    int ret = 0;
    struct SocketListenInfoT conn[10] = {0};
    struct ServerSocketErrInfo err[10] = {0};
    unsigned int num = 1;
    struct RaSocketHandle *socket_handle = NULL;
    struct rdev rdev_info = {0};
 
    socket_handle = malloc(sizeof(struct RaSocketHandle));
    socket_handle->rdevInfo = rdev_info;
    mocker_clean();
 
    conn[0].socketHandle = NULL;
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(128203, ret);
 
    conn[0].socketHandle = socket_handle;
    socket_handle->socketOps = &gRaPeerSocketOps;
    rdev_info.phyId = 0;
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetServerSocketErrInfo, 1, 1);
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(328207, ret);
    mocker_clean();
 
    mocker(RaInetPton, 5, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RsSocketGetServerSocketErrInfo, 1, 0);
    ret = RaGetServerSocketErrInfo(conn, err, num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
 
    free(socket_handle);
    socket_handle = NULL;
}

void tc_ra_socket_accept_credit_add(void)
{
    struct SocketListenInfoT conn[10] = {0};
    struct RaSocketHandle socket_handle = {0};
    struct RaSocketOps socket_ops = {0};
    int ret = 0;

    conn[0].socketHandle = &socket_handle;
    socket_handle.socketOps = &socket_ops;
    socket_handle.socketOps->raSocketAcceptCreditAdd = RaPeerSocketAcceptCreditAdd;
    mocker(RaInetPton, 1, 0);
    mocker(RaGetSocketListenInfo, 1, 0);
    mocker(RsSetCtx, 1, 0);
    mocker(RaPeerMutexLock, 1, 0);
    mocker(RaPeerMutexUnlock, 1, 0);
    mocker(RsSocketAcceptCreditAdd, 1, 0);
    ret = RaSocketAcceptCreditAdd(conn, 1, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_hdc_recv_handle_send_pkt_unsuccess()
{
    mocker_clean();
    mocker(DlHalHdcRecv, 1, 1);
    mocker(DlDrvHdcAllocMsg, 1, 0);
    mocker(DlDrvHdcFreeMsg, 1, 1);
    mocker(DlDrvHdcSessionClose, 1, 1);
    mocker(RsSetCtx, 1, 0);
    mocker(pthread_mutex_lock, 1, 0);
    mocker(pthread_mutex_unlock, 1, 0);
    RaHdcRecvHandleSendPkt(0);
    mocker_clean();
}

void tc_ra_hdc_tlv_request()
{
    struct RaTlvHandle tlv_handle_tmp = {0};
    struct TlvMsg send_msg = {0};
    struct TlvMsg recv_msg = {0};
    unsigned int moduleType;
    int ret = 0;

    // ccu init/uninit
    moduleType = TLV_MODULE_TYPE_CCU;
    send_msg.length = 0;
    send_msg.type = 0;
    mocker(RaHdcProcessMsg, 100, 0);
    ret = RaHdcTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_EQ(ret, 0);

    // data split
    tlv_handle_tmp.initInfo.phyId = 0;
    moduleType = TLV_MODULE_TYPE_NSLB;
    send_msg.length = TC_TLV_HDC_MSG_SIZE;
    send_msg.type = 0;
    send_msg.data = (char *)malloc(TC_TLV_HDC_MSG_SIZE);
    int i = 0;
    for (i = 0; i < TC_TLV_HDC_MSG_SIZE; i++) {
        send_msg.data[i] = (char)(i % 256);
    }

    mocker(RaHdcProcessMsg, 100, 0);
    ret = RaHdcTlvRequest(&tlv_handle_tmp, moduleType, &send_msg, &recv_msg);
    EXPECT_INT_EQ(ret, 0);
    mocker_clean();

    free(send_msg.data);
    send_msg.data = NULL;
}

void tc_ra_remap_mr(void)
{
    struct RaRdmaHandle rdma_handle = {0};
    struct MemRemapInfo info[1] = {0};
    struct RaRdmaOps rdma_ops = {0};
    int ret = 0;
 
    mocker_clean();
    rdma_handle.rdmaOps = &rdma_ops;
    rdma_handle.rdmaOps->raRemapMr = RaHdcRemapMr;
    mocker(RaHdcProcessMsg, 1, 0);
    ret = RaRemapMr((void *)&rdma_handle, info, 1);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}
 
void tc_ra_register_mr(void)
{
    struct RaRdmaHandle rdma_handle = {0};
    struct RaMrHandle *mr_handle = NULL;
    struct RaRdmaOps rdma_ops = {0};
    struct MrInfoT info = {0};
    int ret = 0;
 
    mocker_clean();
    mocker(RaHdcProcessMsg, 2, 0);
    rdma_handle.rdmaOps = &rdma_ops;
    rdma_handle.rdmaOps->raRegisterMr = RaHdcTypicalMrReg;
    rdma_handle.rdmaOps->raDeregisterMr = RaHdcTypicalMrDereg;
    ret = RaRegisterMr((void *)&rdma_handle, &info, (void **)&mr_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaDeregisterMr((void *)&rdma_handle, (void *)mr_handle);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_rs_remap_mr(void)
{
    union OpRemapMrData op_data = {{0}};
    char* out_buf;
    int op_result;
    char* in_buf;
    int size;
    int ret;

    size = sizeof(union OpRemapMrData) + sizeof(struct MsgHead);
    in_buf = calloc(1, size);
    out_buf = calloc(1, size);
    memcpy(in_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpRemapMrData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpRemapMrData));
    mocker(RsRemapMr, 2, 1);
    ret = RaRsRemapMr(in_buf, out_buf, &size, &op_result, size);
    EXPECT_INT_EQ(ret, 0);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_get_tls_enable()
{
    struct RaInfo info = {0};
    bool tls_enable;
    int ret;

    info.mode = NETWORK_PEER_ONLINE;
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    mocker(RaHdcProcessMsg, 1, 0);
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(0, ret);

    info.phyId = RA_MAX_PHY_ID_NUM;
    ret = RaGetTlsEnable(&info, &tls_enable);
    EXPECT_INT_EQ(128303, ret);

}

void tc_ra_get_sec_random()
{
    struct RaInfo info = {0};
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
    mocker(RaHdcProcessMsg, 10, 0);
    mocker(RaPeerGetSecRandom, 10, -1);
    ret = ra_get_sec_random(&info, &value);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_rs_get_tls_enable()
{
    unsigned int size = sizeof(union OpGetTlsEnableData) + sizeof(struct MsgHead);
    union OpGetTlsEnableData op_data  = {{0}};


    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetTlsEnableData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetTlsEnableData));
    ret = RaRsGetTlsEnable(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_rs_get_sec_random()
{
    unsigned int size = sizeof(union OpGetSecRandomData) + sizeof(struct MsgHead);
    union OpGetSecRandomData op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetSecRandomData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetSecRandomData));
    ret = RaRsGetSecRandom(in_buf, out_buf, &out_len, &op_result, size);
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

    ret = RaRsSocketCreditAdd(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
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

    ret = RaRsSocketBatchAbort(in_buf, out_buf, &out_len, &op_result, rcv_buf_len);
    EXPECT_INT_EQ(0, ret);

    return;
}

void tc_hdc_socket_batch_abort()
{
    struct SocketListenInfoT conn[1];
    int ret;
    mocker_clean();
    mocker((stub_fn_t)calloc, 1, NULL);
    ret = RaHdcSocketBatchAbort(1, conn, 1);
    EXPECT_INT_EQ(ret, -ENOMEM);

    mocker_clean();
    mocker((stub_fn_t)RaGetSocketConnectInfo, 1, -1);
    ret = RaHdcSocketBatchAbort(1, conn, 1);
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();
}

void tc_ra_hdc_socket_accept_credit_add()
{
    struct SocketListenInfoT conn[1];
    int ret;
    mocker(RaGetSocketListenInfo, 1, -1);
    ret = RaHdcSocketAcceptCreditAdd(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();

    mocker(RaHdcProcessMsg, 1, -1);
    ret = RaHdcSocketAcceptCreditAdd(1, conn, 1, 1);
    EXPECT_INT_EQ(1, 1);
    mocker_clean();
}

void tc_ra_get_hccn_cfg()
{
    struct RaInfo info = {0};
    char *value = calloc(1, 2048);
    unsigned int val_len = 2048;
    int ret;

    mocker_clean();
    info.mode = NETWORK_OFFLINE;
    ret = RaGetHccnCfg(NULL, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    val_len = 1024;
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phyId = 64;
    info.mode = NETWORK_OFFLINE;
    val_len = 2048;
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(128303, ret);

    info.phyId = 0;
    mocker(RaHdcProcessMsg, 10, 0);
    ret = RaGetHccnCfg(&info, HCCN_CFG_UDP_PORT_MODE, value, &val_len);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free(value);
}

void tc_ra_rs_get_hccn_cfg()
{
    unsigned int size = sizeof(union OpGetHccnCfgData) + sizeof(struct MsgHead);
    union OpGetHccnCfgData op_data  = {{0}};

    char* in_buf = calloc(1, size);
    char* out_buf = calloc(1, size);
    int out_len;
    int op_result;
    int ret;

    memcpy(in_buf + sizeof(struct MsgHead), &op_data , sizeof(union OpGetHccnCfgData));
    memcpy(out_buf + sizeof(struct MsgHead), &op_data, sizeof(union OpGetHccnCfgData));
    ret = RaRsGetHccnCfg(in_buf, out_buf, &out_len, &op_result, size);
    EXPECT_INT_EQ(0, ret);

    free(in_buf);
    free(out_buf);
    in_buf = NULL;
    out_buf = NULL;
}

void tc_ra_save_snapshot_input()
{
    enum SaveSnapshotAction action;
    struct RaInfo *info = NULL;
    int ret;

    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_NE(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_NE(0, ret);

    info = calloc(1,sizeof(struct RaInfo));
    info->phyId = RA_MAX_PHY_ID_NUM;
    info->mode = NETWORK_PEER_ONLINE;
    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->phyId = 0;
    action = SAVE_SNAPSHOT_ACTION_POST_PROCESSING + 1;
    ret = RaSaveSnapshot(info, action);
    EXPECT_INT_NE(0, ret);

    info->mode = NETWORK_PEER_ONLINE;
    info->phyId = 0;
    ret = RaSaveSnapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_EQ(0, ret);

    info->mode = NETWORK_OFFLINE + 1;
    ret = RaSaveSnapshot(info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = RaRestoreSnapshot(info);
    EXPECT_INT_NE(0, ret);

    free(info);
    info = NULL;
}

void tc_ra_save_snapshot_pre()
{
    struct RaInfo info = {0};
    struct RaRdmaHandle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabledLiteThread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notifyType = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(RaRdevInitCheckIp, 10, 0);
    mocker((stub_fn_t)HdcSendRecvPkt, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    mocker_invoke(RaHdcGetLiteSupport, ra_hdc_get_lite_support_stub, 10);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    g_interface_version = 1;
    ret = RaRdevInitV2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(ret, 0);

    info.mode = NETWORK_OFFLINE;
    rdma_handle->supportLite = LITE_NOT_SUPPORT;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->supportLite = LITE_ALIGN_4KB;
    rdma_handle->threadStatus = LITE_THREAD_STATUS_RUNNING;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(128300, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_POST_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_ra_save_snapshot_post()
{
    struct RaInfo info = {0};
    struct RaRdmaHandle *rdma_handle = NULL;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RdevInitInfo init_info = {0};
    init_info.disabledLiteThread = false;
    init_info.mode = NETWORK_OFFLINE;
    init_info.notifyType = NOTIFY;
    int ret;

    tc_hdc_env_init();

    mocker(RaRdevInitCheckIp, 10, 0);
    mocker((stub_fn_t)HdcSendRecvPkt, 10, 0);
    mocker_invoke(RaHdcGetInterfaceVersion, ra_get_interface_version_stub, 10);
    mocker_invoke(RaHdcGetLiteSupport, ra_hdc_get_lite_support_stub, 10);
    mocker(RaHdcNotifyBaseAddrInit, 10, 0);
    g_interface_version = 1;
    ret = RaRdevInitV2(init_info, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);

    info.mode = NETWORK_OFFLINE;
    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(128300, ret);

    rdma_handle->supportLite = LITE_NOT_SUPPORT;
    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    rdma_handle->threadStatus = LITE_THREAD_STATUS_RUNNING;
    rdma_handle->supportLite = LITE_ALIGN_4KB;
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_EQ(0, ret);

    // delay do not cover SUSPEND
    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0, ret);

    ret = RaSaveSnapshot(&info, SAVE_SNAPSHOT_ACTION_PRE_PROCESSING);
    EXPECT_INT_NE(0 ,ret);

    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = RaRestoreSnapshot(&info);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit(rdma_handle, NOTIFY);
    tc_hdc_env_deinit();
}

void tc_hdc_async_del_req_handle()
{
    pthread_mutex_t req_mutex;
    pthread_mutex_init(&req_mutex, NULL);

    struct RaListHead list1 = {0};

    RA_INIT_LIST_HEAD(&list1);
    RaHwAsyncDelList(&list1, &req_mutex);
}

void tc_ra_loopback_qp_create()
{
    struct RaRdmaHandle *rdma_handle2;
    struct RaRdmaHandle *rdma_handle;
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    int ret = 0;

    mocker(RaRdevInitCheckIp, 10, 0);
    ret = RaRdevInit(NETWORK_PEER_ONLINE, NO_USE, rdev_info, &rdma_handle);
    EXPECT_INT_EQ(0, ret);
    ret = RaRdevGetHandle(rdev_info.phyId, &rdma_handle2);
    EXPECT_INT_EQ(0, ret);
    EXPECT_INT_EQ(rdma_handle, rdma_handle2);
    mocker_clean();

    struct LoopbackQpPair qp_pair;
    void *qp_handle = NULL;

    ret = RaLoopbackQpCreate(NULL, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = RaLoopbackQpCreate(rdma_handle, NULL, NULL);
    EXPECT_INT_EQ(128103, ret);

    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, NULL);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdevInfo.phyId = 128;
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128103, ret);

    rdma_handle->rdevInfo.phyId = 0;
    mocker(RaPeerLoopbackQpCreate, 10, -1);
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(128100, ret);

    mocker_clean();
    // 创建flush qp主成功场景
    ret = RaLoopbackQpCreate(rdma_handle, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(0, ret);
    // 销毁flush qp
    ret = RaQpDestroy(qp_handle);
    EXPECT_INT_EQ(0, ret);

    ret = RaRdevDeinit(rdma_handle, NO_USE);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct LoopbackQpPair qp_pair;
    void *qp_handle = NULL;
    int ret = 0;

    mocker(RaPeerLoopbackSingleQpCreate, 10, -1);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker_invoke(RaPeerLoopbackSingleQpCreate, ra_peer_loopback_single_qp_create_stub, 10);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaPeerLoopbackQpModify, 10, -1);
    ret = RaPeerLoopbackQpCreate(&rdma_handle_tmp, &qp_pair, &qp_handle);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_ra_peer_loopback_single_qp_create()
{
    struct rdev rdev_info = {0};
    rdev_info.phyId = 0;
    rdev_info.family = AF_INET;
    rdev_info.localIp.addr.s_addr = 0;
    struct RaRdmaHandle rdma_handle_tmp = {
        .rdevInfo = rdev_info,
        .rdevIndex = 0,
    };
    struct RaQpHandle *qp_handle = NULL;
    struct ibv_qp *qp = NULL;
    int ret = 0;

    mocker(RaPeerCqCreate, 10, -1);
    ret = RaPeerLoopbackSingleQpCreate(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();

    mocker(RaPeerNormalQpCreate, 10, -1);
    ret = RaPeerLoopbackSingleQpCreate(&rdma_handle_tmp, &qp_handle, &qp);
    EXPECT_INT_EQ(-1, ret);
    mocker_clean();
}

void tc_ra_hdc_uninit_async()
{
    RaHdcUninitAsync();
}

void tc_ra_hdc_qp_create_with_attrs()
{
    struct RaRdmaHandle rdma_handle = {0};
    struct QpExtAttrs extAttrs = {0};
    struct AiQpInfo info = {0};
    void *qp_handle = NULL;
    int ret = 0;

    mocker(memcpy_s, 1, -1);
    ret = RaHdcQpCreateWithAttrs(&rdma_handle, &extAttrs, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = RaHdcAiQpCreate(&rdma_handle, &extAttrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();

    mocker(memcpy_s, 1, -1);
    ret = RaHdcAiQpCreateWithAttrs(&rdma_handle, &extAttrs, &info, &qp_handle);
    EXPECT_INT_EQ(ret, -256);
    mocker_clean();
}

void *stub_mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset)
{
	errno = 1;
	return (void*)-1;
};
 
void tc_host_notify_base_addr_init()
{
	int ret;
 
    mocker_clean();
	mocker(drvDeviceGetIndexByPhyId, 1, 0);
	mocker(halNotifyGetInfo, 1, 0);
	mocker(open, 1, 1);
	mocker_u64_invoke(mmap, stub_mmap, 20);
	ret = HostNotifyBaseAddrInit(0);
	EXPECT_INT_EQ(-ENOMEM, ret);
	mocker_clean();
}