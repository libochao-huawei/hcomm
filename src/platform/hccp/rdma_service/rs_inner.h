/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_INNER_H
#define RS_INNER_H

#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <infiniband/verbs.h>
#include <semaphore.h>
#ifndef CA_CONFIG_LLT
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <crypto/x509.h>
#include <crypto/evp.h>
#include <crypto/asn1.h>
#include <openssl/x509v3.h>
#else
#include "stub_ssl.h"
#endif
#include "ascend_hal_external.h"
#ifndef HNS_ROCE_LLT
#include "dlog_pub.h"
#endif
#include "tls.h"
#include "hccp_common.h"
#include "hccp_ping.h"
#include "rs_common_inner.h"
#include "rs_ping_inner.h"
#include "rs.h"
#include "rs_list.h"

/* priority of algos and forbid unsafety algos */
#define CHIPER_LIST "ECDHE-RSA-AES256-GCM-SHA384:\
    !RC2:!RC4:!MD2:!MD4:!MD5:!DES:!3DES:!SHA1:!BLOWFISH:!CBC:!ECB:!ADH:!LOW:!PSK:!SRP:!DSS:!eNULL:!aNULL:!EXP:@STRENGTH"

#define RS_WC_NUM 16384
#define RS_S6_ADDR32 2
#define RS_QP_ATTR_MIN_RNR_TIMER 12
#define RS_QP_ATTR_TIMEOUT 16
#define RS_QP_ATTR_RETRY_CNT 7
#define RS_QP_ATTR_RNR_RETRY 7
#define RS_QP_ATTR_MAX_SEND_SGE 8
#define RS_QP_ATTR_GID_LEN 16
#define RS_QP_TX_DEPTH_OFFLINE 128
#define RS_QP_RX_DEPTH_OFFLINE 128
#define RS_QP_TX_DEPTH_ONLINE 4096
#define RS_QP_RX_DEPTH_ONLINE 4096
#define RS_MR_NUM_MAX 256
#define RS_USLEEP_TIME 20000
#define RS_CONN_USLEEP_TIME 200000
#define RS_PROMOTE_CONN_USLEEP_TIME 5000
#define RS_RECV_MAX_TIME (1000.0 * 5) // ms
#define RS_RECV_TAG_MAX_TIME (1000.0 * 90) // ms
#define RS_TLS_CONN_TIME 1
#define RS_DEVICE_NUM 0x3
#define RS_HOSTID2DEVID(dev_id) ((dev_id) & RS_DEVICE_NUM)
#define SOCK_CONN_DEV_ID_SIZE 64
#define RS_VNIC_MAX 128
#define RS_VNIC_IP_LEN 14
#define RS_IB_NAME_LEN 10

#define RS_VNIC_FIRST 192
#define RS_VNIC_SECOND 168
#ifndef CA_CONFIG_LLT
#define RS_VNIC_THIRD 2
#else
#define RS_VNIC_THIRD 1
#endif
#define RS_VNIC_FOUTH 199
#define RS_VNIC_FLAG 1

#define RS_TCP_DSCP_0      0
#define RS_ROCE_DSCP_33    33
#define RS_DSCP_MASK       0x3f
#define RS_DSCP_OFF        2

#define RS_ROCE_4_SL    4

#define RS_MAX_FD_NUM 65536

#define IB_NAME "hns_0"
#define RS_CONN_EXIT_FLAG 2
#define RS_TRY_TIME 200
#define RS_WLIST_VALID_FLAG_SIZE   6
#define RS_SSL_CERT_LEN 2048
#define RS_SSL_MIN_CERT_NUM 2
#define RS_SSL_MAX_CERT_NUM 15
#define RS_SSL_MAX_ALL_CERT_NUM (RS_SSL_MAX_CERT_NUM + (TLS_CA_SSL_MAX_NEW_CERT_NUM * RS_SSL_NEW_CERT_CB_NUM))
#define RS_SSL_MIN_CERT_LEN (RS_SSL_CERT_LEN * RS_SSL_MIN_CERT_NUM)
#define RS_SSL_MAX_CERT_LEN (RS_SSL_CERT_LEN * RS_SSL_MAX_CERT_NUM)
#define RS_SSL_PRI_LEN 5120
#define RS_SSL_REVOKE_LEN 20480
#define RS_SSL_FALSH_HEAD_LEN 8
#define RS_SSL_ENC_MODE 1
#define RS_SSL_VERSION 2
#define HCCP_CERTS_STATR "-----BEGIN CERTIFICATE-----"
#define HCCP_CERTS_END "-----END CERTIFICATE-----"
#define RS_KID_MAX_LENGTH 512
#define RS_KID_MIN_LENGTH 8
#define RS_RSA_KY_BITS_MIN_LEN 2048
#define RS_DSA_KY_BITS_MIN_LEN 2048
#define RS_DH_KY_BITS_MIN_LEN 2048
#define RS_EC_KY_BITS_MIN_LEN 256
#define RS_SSL_ERR_MSG_LEN 256
#define RS_SOCKET_MAXLEN 2048
#define RS_INTERFACE_LEN    5
#define RS_INTERFACE_BOND_LEN    6
#define RS_INTERFACE_ETH_PREFIX_LEN 3
#define RS_INTERFACE_BOND_PREFIX_LEN 4

/* pcie card boardid rule: GPIO[75:73]=0x000 */
#define RS_BOARDID_PCIE_CARD_MASK        0xE00
#define RS_BOARDID_PCIE_CARD_MASK_VALUE  0x0
#define RS_BOARDID_AI_SERVER_MODULE  0x0
#define RS_BOARDID_ARM_SERVER_AG     0x20
#define RS_BOARDID_ARM_POD     0x30
#define RS_BOARDID_X86_16P     0x50
#define RS_BOARDID_ARM_SERVER_2DIE    0xB0

#define RS_MAX_RD_ATOMIC_NUM                128
#define RS_QP_TX_DEPTH                      8191
#define RS_QP_32K_DEPTH                     32767
#define RS_MAX_RD_ATOMIC_NUM_PEER_ONLINE    16      // host RDMA adapt
#define RS_QP_TX_DEPTH_PEER_ONLINE          4096    // host RDMA adapt

#define RS_CLOSE_TIMEOUT    5

enum ca_ptye {
    RS_EQPT_CA = 0,
    RS_ROOT_CA
};

#define RS_SSL_DISABLE 0
#define RS_SSL_ENABLE 1

#define RS_EQPT_CERTS_PATH_LEN 256
#define RS_CA_CERTS_PATH_LEN 256

struct rs_cert_info {
    char cert_info[RS_SSL_CERT_LEN];
};

struct rs_certs {
    struct rs_cert_info certs[RS_SSL_MAX_CERT_NUM];
};

#define HCCP_NEW_CERTS_CB1_INDEX    0
#define HCCP_NEW_CERTS_CB2_INDEX    1
#define HCCP_NEW_CERTS_CB3_INDEX    2
#define HCCP_NEW_CERTS_CB4_INDEX    3
#define MAX_CERT_NUM_IN_CB          8
#define RS_SSL_NEW_CERT_CB_NUM 4

struct cert_file {
    const char *end_file;
    const char *ca_file;
};

#define TLS_MAGIC_WORDS_LEN 8
#define TLS_SALT_MAX_LEN 48
#define TLS_ENC_DEC_DIV_LEN 16
#define TLS_MAGIC_WORDS "1234567"
#define X509_VERIFY_SUCC 1

struct rs_sec_para {
    unsigned char in_buf[RS_SSL_PRI_LEN];
    unsigned int in_buf_size;
    unsigned char in_salt[TLS_SALT_MAX_LEN];
    unsigned int in_salt_size;
    unsigned char out_buf[RS_SSL_PRI_LEN];
    unsigned int out_buf_size;
};

#define RS_CLOSE_RETRY_FOR_EINTR(ret, fd) do { \
    do { \
        (ret) = close((fd)); \
    } while (((ret) < 0) && (errno == EINTR)); \
} while (0)

#define RS_CHECK_RET_WITHOUT_RETURN(ret, fmt, val...) do { \
    if (ret) { \
        hccp_warn(fmt, ##val); \
    } \
} while (0)


#define RS_CHECK_POINTER_NULL_WITH_RET(ptr) do { \
        if ((ptr) == NULL) { \
            hccp_err("pointer is NULL!"); \
            return (-EINVAL); \
        } \
} while (0)

#define RS_CHECK_POINTER_NULL_RETURN_VOID(ptr) do { \
        if ((ptr) == NULL) { \
            hccp_err("pointer is NULL!"); \
            return; \
        } \
} while (0)

#define RS_CHECK_POINTER_NULL_RETURN_NULL(ptr) do { \
        if ((ptr) == NULL) { \
            hccp_err("null pointer exception!"); \
            return NULL; \
        } \
} while (0)

#define RS_CHECK_POINTER_NULL_RETURN_INT(ptr) do { \
        if ((ptr) == NULL) { \
            hccp_err("null pointer exception!"); \
            return (-EINVAL); \
        } \
} while (0)

#define RS_PTHREAD_MUTEX_LOCK(conn_mutex) do { \
    int ret_lock = pthread_mutex_lock(conn_mutex); \
    if (ret_lock) { \
        hccp_warn("pthread_mutex_lock unsuccessful, ret[%d]", ret_lock); \
    }\
} while (0)

#define RS_PTHREAD_MUTEX_ULOCK(conn_mutex) do { \
    int ret_ulock = pthread_mutex_unlock(conn_mutex); \
    if (ret_ulock) { \
        hccp_warn("pthread_mutex_unlock unsuccessful, ret[%d]", ret_ulock); \
    } \
} while (0)

#define RS_MAX_SOCKET_NUM      16

#define RS_MAX_DEV_NUM         64

#define RS_MAX_WLIST_NUM       16

#define RS_SOCKET_PARA_CHECK(num, conn) do { \
    if (((num) <= 0) || ((num) > RS_MAX_SOCKET_NUM) || ((conn) == NULL)) { \
        hccp_err("rs socket param error ! number:%d", num); \
        return (-EINVAL); \
    } \
} while (0)

#define RS_QP_PARA_CHECK(phy_id) do { \
    if ((phy_id) >= RS_MAX_DEV_NUM) { \
        hccp_err("rs qp param error ! physical_id:%d", phy_id); \
        return (-EINVAL); \
    } \
} while (0)

struct rs_vnic_info {
    uint32_t vnic_flag;
    uint32_t role;
};

#define RS_FD_INVALID       (-1)
#define RS_EPOLL_EVENT      64
#define RS_SGLIST_MAX       16
#define RS_SGLIST_LEN_MAX   2147483648
#define RS_QP_NUM_MAX       8192
#define RS_MIN_TEMPTH_DEPTH 8
#define RS_MAX_TEMPTH_DEPTH 4096

enum rs_cmd_opcode {
    RS_CMD_QP_INFO = 0x12345678,
    RS_CMD_MR_INFO = 0x12345687,
    RS_CMD_LEN_INFO = 0x12345867,
};

struct rs_mr_info {
    uint32_t cmd;    /* MUST be the first element */

    uint32_t rkey;
    uint64_t addr;
    uint64_t len;
};

#define RS_MR_STATE_SYNCED  1

/*
 * mr_cb also used to sync to remote
 */
struct rs_mr_cb {
    struct rs_mr_info mr_info;   /* MUST be the first element */

    uint64_t wr_id;
    uint32_t state;

    struct ibv_mr *ib_mr;
    struct rs_rdev_cb *dev_cb;
    struct rs_qp_cb *qp_cb;
    struct rs_list_head list;
};

struct rs_qp_info {
    uint32_t cmd;    /* MUST be the first element */

    int lid;
    int qpn;
    int psn;
    int gid_idx;
    union ibv_gid gid;
    struct rs_mr_info notify_mr;
};

struct rs_qp_len_info {
    uint32_t cmd;
    uint32_t len;
};

enum rs_conn_state {
    RS_CONN_STATE_RESET,
    RS_CONN_STATE_INIT,
    RS_CONN_STATE_BIND,
    RS_CONN_STATE_LISTENING,
    RS_CONN_STATE_CONNECTED,
    RS_CONN_STATE_SSL_BIND_FD,
    RS_CONN_STATE_SSL_CONNECTED,
    RS_CONN_STATE_TAG_SYNC,
    RS_CONN_STATE_VALID_SYNC,
    RS_CONN_STATE_TX_TO_HCCL,

    RS_CONN_STATE_TIMEOUT,

    RS_CONN_STATE_ERR,

    RS_CONN_STATE_MAX,
};

#define WELL_KNOWN_PORT_MAX 1024

#define FD_USED_FOR_QP_EXCHANGE 1

struct rs_conn_info {
    struct rs_ip_addr_info server_ip;
    struct rs_ip_addr_info client_ip;
    uint16_t port;
    int scope_id;

    int connfd;
    SSL *ssl;
    uint32_t state;  /* refer to enum rs_conn_state */
    struct timeval start_time;
    struct timeval end_time;
    bool is_got;

    /*
     * HCCL need classify the connection according by the tag.
     * when a client connects successfully, it need send the tag to Server,
     * Server return the tag to HCCL
     */
    char tag[SOCK_CONN_TAG_SIZE + SOCK_CONN_DEV_ID_SIZE];
    uint32_t tag_sync_time;
    uint32_t tag_eintr_time;

    struct socket_err_info err_info;

    struct rs_list_head list;
};

#define RS_BUF_SIZE 2048
#define RS_SOCK_LISTEN_PARALLEL_NUM 16384

enum listen_fd_state {
    LISTEN_FD_STATE_ADDED = 0,
    LISTEN_FD_STATE_DELETED = 1,
};

struct rs_listen_info {
    struct rs_ip_addr_info server_ip_addr;
    struct rs_ip_addr_info client_ip_addr;
    uint16_t sock_port;

    int listen_fd;
    uint32_t state;  /* refer to enum rs_conn_state */
    int counter;

    int last_accept_errno; /* last accept errno, avoid log flush */
    struct socket_err_info err_info;

    bool accept_credit_flag;
    pthread_mutex_t accept_credit_mutex;
    enum listen_fd_state fd_state;
    unsigned int accept_credit_limit;

    struct rs_list_head list;
};

struct rs_accept_info {
    struct rs_ip_addr_info server_ip_addr;
    struct rs_ip_addr_info client_ip_addr;
    uint16_t sock_port;
    int conn_fd;
    SSL *ssl;
    uint32_t state;

    struct rs_list_head list;
};

struct rs_white_list {
    struct rs_ip_addr_info server_ip;
    struct rs_list_head white_list;
    struct rs_list_head list;
};

struct rs_white_list_info {
    struct rs_ip_addr_info client_ip;
    unsigned int conn_limit;
    char tag[SOCK_CONN_TAG_SIZE];
    struct rs_list_head list;
};

struct rs_heterog_tcp_fd_info {
    int fd;
    struct rs_list_head list;
};

struct rs_cqe_err_info {
    pthread_mutex_t mutex;
    struct cqe_err_info info;
};

struct rs_conn_cb {
    struct rs_ip_addr_info local_ip_addr;
    unsigned int wlist_enable;
    int eventfd;
    int epollfd;
    int scope_id;

    pthread_mutex_t conn_mutex;
    struct rs_cb *rscb;
    struct socket_err_info epoll_err_info;

    struct rs_list_head listen_list;
    struct rs_list_head server_accept_list;
    struct rs_list_head server_conn_list;
    struct rs_list_head client_conn_list;
    struct rs_list_head white_list;
};

struct rs_qp_cb {
    struct rs_rdev_cb *rdev_cb;
    struct ibv_pd *ib_pd;
    struct ibv_qp *ib_qp;

    int eq_num;
    struct ibv_comp_channel *channel;
    struct ibv_cq *ib_send_cq;
    int send_cq_depth;
    struct ibv_cq *ib_recv_cq;
    int recv_cq_depth;
    struct rs_cq_context *srq_context;
    int num_recv_cq_events;
    int num_send_cq_events;

    unsigned int tx_depth;
    unsigned int send_sge_num;
    unsigned int rx_depth;
    unsigned int recv_sge_num;

    unsigned int send_wr_num;
    unsigned int recv_wr_num;

    int sq_index;
    int db_index;
    int qp_mode;
    struct rs_qp_info qp_info_lo;
    struct rs_qp_info qp_info_rem;

    struct rs_conn_info *conn_info;
    int state;
    struct timeval start_time;
    struct timeval end_time;
    char qp_mr_buf[RS_BUF_SIZE];
    unsigned int remain_size;

    pthread_mutex_t qp_mutex;

    int mr_num;
    struct rs_list_head list;
    struct rs_list_head mr_list;
    struct rs_list_head rem_mr_list;
    int is_exp;

    uint32_t send_len;
    uint32_t recv_len;
    uint32_t expect_len;

    struct event_summary *send_event;
    struct event_summary *recv_event;

    struct qos_attr qos_attr;

    unsigned int timeout;
    unsigned int retry_cnt;

    struct lite_qp_cq_attr_resp qp_resp;

    struct lite_mem_attr_resp mem_resp;
    int mem_align; // 0,1:4KB, 2:2MB
    uint32_t udp_sport;

    unsigned int ai_op_support;
    unsigned int grp_id;
    unsigned int cq_cstm_flag;

    struct rs_cqe_err_info cqe_err_info;
};

enum rs_cq_create_mode {
    RS_NORMAL_CQ_CREATE = 0,
    RS_SRQ_CQ_CREATE,
    RS_SQ_CQ_CREATE,
};

struct rs_cq_create_attr {
    struct rs_rdev_cb *rdev_cb;
    int eq_num;
    int cq_depth;
    int cq_event_id;
    struct ibv_cq *ib_cq;
    struct ibv_comp_channel *channel;
    struct event_summary *event;
};

struct rs_cq_context {
    struct rs_rdev_cb *rdev_cb;
    int eq_num;
    int cq_create_mode;
    struct ibv_cq *ib_send_cq;
    struct ibv_cq *ib_recv_cq;
    struct ibv_cq *ib_srq_cq;
    struct ibv_comp_channel *channel;
    struct event_summary *send_event;
    struct event_summary *recv_event;
    struct rs_cq_context *srq_context;
    int num_recv_cq_events;
};

#define RS_PORT_DEF     1

/* rs_cb->state enum */
#define RS_STATE_RESET 0
#define RS_STATE_READY 2
#define RS_STATE_HALT 4

struct rs_akid {
    char akid_name[RS_KID_MAX_LENGTH];
};

struct rs_issuer {
    char issuer_name[RS_KID_MAX_LENGTH];
};

struct rs_cert_akid_issuer_cb {
    struct rs_akid akids[RS_SSL_MAX_ALL_CERT_NUM];
    struct rs_issuer issers[RS_SSL_MAX_ALL_CERT_NUM];
};

struct rs_skid {
    char skid_name[RS_KID_MAX_LENGTH];
};

struct rs_subject {
    char subject_name[RS_KID_MAX_LENGTH];
};

struct rs_cert_skid_subject_cb {
    struct rs_skid skids[RS_SSL_MAX_ALL_CERT_NUM];
    struct rs_subject subjects[RS_SSL_MAX_ALL_CERT_NUM];
};

struct rs_rdev_cb {
    struct rs_cb *rs_cb;
    unsigned int rdev_index;
    struct rs_ip_addr_info local_ip;
    int dev_num;
    const char *dev_name;
    int poll_cqe_num;
    unsigned char ib_port;
    unsigned int qp_cnt;
    unsigned int qp_max_num;
    unsigned int tx_depth;
    unsigned int rx_depth;
    unsigned int notify_type;
    unsigned long long notify_pa_base;
    unsigned long long notify_va_base;
    unsigned long long notify_size;
    int notify_access;
    unsigned int logic_devid;
    int sensor_update_cnt;
    uint64_t sensor_handle;
    unsigned int cqe_err_cnt;
    pthread_mutex_t cqe_err_cnt_mutex;

    pthread_mutex_t rdev_mutex;

    struct ibv_mr *notify_mr;
    struct ibv_pd *ib_pd;
    struct ibv_context *ib_ctx;
    struct ibv_device **dev_list;

    struct rs_list_head qp_list;
    struct rs_list_head typical_mr_list;
    struct rs_list_head list;

    int support_lite;
    struct {
        bool backup_flag;
        struct rdev rdev_info;
        struct ibv_context *ib_ctx;
    } backup_info;
};

struct tlv_buf_info {
    unsigned int buffer_size;
    char *buf;
};

struct rs_nslb_cb {
    struct rs_cb *rscb;
    unsigned int phy_id;
    pthread_mutex_t mutex;
    struct tlv_buf_info buf_info;
    void *netco_cb;
    bool netco_init_flag;
};

/*
 * Main Control block for device
 * for multi processor(device) in SMP system, each device have it's own rs_cb
 */
struct rs_cb {
    uint32_t chip_id;
    uint32_t hccp_mode;
    unsigned int logic_id;
    enum protocol_type protocol;

    pthread_mutex_t mutex;

    sem_t connect_trig_sem;
    uint32_t state;
    uint32_t ssl_enable;
    SSL_CTX *server_ssl_ctx;
    SSL_CTX *client_ssl_ctx;
    struct rs_cert_skid_subject_cb *skid_subject_cb;

    int conn_flag;
    struct rs_conn_cb conn_cb;

    unsigned int dev_cnt;
    struct rs_list_head rdev_list;
    struct rs_list_head heterog_tcp_fd_list;

    struct rs_ping_ctx_cb ping_cb;

    struct rs_nslb_cb nslb_cb;

    char buf[RS_BUF_SIZE];
    struct process_rs_sign p_rs_sign;

    unsigned long long notify_va_base;
    unsigned long long notify_size;

    void (*tcp_recv_callback)(const void *fdHandle);
    const void **fd_map;

    struct ifaddrs *ifaddr_list;

    pid_t aicpu_pid;
    unsigned int grp_id;
    pid_t host_pid;
    bool grp_setup_flag;
};

enum rs_hardware_type {
    RS_HARDWARE_SERVER,
    RS_HARDWARE_PCIE,
    RS_HARDWARE_2DIE,
    RS_HARDWARE_UNKNOWN,
};

union rs_socketaddr {
    struct sockaddr_in s_addr;
    struct sockaddr_in6 s_addr6;
};

struct rs_socketaddr_info {
    int family;
    union rs_socketaddr addr;
};

int RsQpInfoSync(struct rs_qp_cb *qpCb);
int RsSetDevice(int devId);
int RsSocketConnectAsync(struct rs_conn_info *conn, struct rs_cb *rscb);
int RsGetSocketConnectState(struct rs_conn_info *conn);
int RsAllocConnNode(struct rs_conn_info **conn, unsigned short serverPort);

extern __thread struct rs_cb *gRsCb;

int RsSocketNodeid2vnic(uint32_t nodeId, uint32_t *ipAddr);
int RsGetHccpMode(unsigned int chipId);
int RsDev2conncb(uint32_t chipId, struct rs_conn_cb **connCb);
int RsFd2conn(int fd, struct rs_conn_info **conn);
int RsDev2rscb(uint32_t chipId, struct rs_cb **rsCb, bool initFlag);
int RsQpn2qpcb(unsigned int phyId, unsigned int rdevIndex, uint32_t qpn, struct rs_qp_cb **qpCb);
int RsRdev2rdevCb(unsigned int chipId, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb);
int RsGetRdevCb(struct rs_cb *rsCb, unsigned int rdevIndex, struct rs_rdev_cb **rdevCb);
void RsAccpetListNodeFree(struct rs_cb *rscb);
int RsWlistCheckConnAdd(struct rs_cb *rsCb, struct rs_conn_info* connTmp);
void ShowConnNode(struct rs_list_head *listHead);
enum ibv_mtu RsDrvSetMtu(struct rs_qp_cb *qpCb);
enum rs_hardware_type RsGetDeviceType(unsigned int phyId);

int RsConvertIpAddr(int family, union hccp_ip_addr *ipAddr, struct rs_ip_addr_info *ip);
bool RsCompareIpAddr(struct rs_ip_addr_info *a, struct rs_ip_addr_info *b);
#ifdef CUSTOM_INTERFACE
int RsSetupSharemem(struct rs_cb *rsCb, bool backupFlag, unsigned int backupPhyid);
#endif
int RsQueryMrCb(struct rs_rdev_cb *devCb, uint64_t addr, struct rs_mr_cb **mrCb, struct rs_list_head *mrList);
int RsGetRsCb(unsigned int phyId, struct rs_cb **rsCb);
int RsQueryGid(struct rdev rdevInfo, struct ibv_context *ibCtxTmp, uint8_t ibPort, int *gidIdx);
int RsEpollEventPingHandle(struct rs_cb *rsCb, int fd);
#endif // RS_INNER_H
