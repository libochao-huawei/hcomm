/**
 * @file hccp_common.h
 * @brief This module provides common structure about sockets and rdma for HCCL
 * @version Copyright (c) Huawei Technologies Co., Ltd. 2019-2025. All rights reserved.
 * @author
 * @date 2019-03-25
 * @defgroup libsocket socket interface
 * @defgroup librdma rdma interface
 * @defgroup libinit init interface
 */

#ifndef __HCCP_COMMON_H__
#define __HCCP_COMMON_H__
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>

#ifdef __cplusplus
extern "C" {
#endif


#define HCCP_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

/**
 * @ingroup libsocket
 * implication of tag : tag is used for HCCL identification group information\n
 * macro : size of tag
 */
#define SOCK_CONN_TAG_SIZE 192
#define MAX_INTERFACE_NUM   8
#define MAX_INTERFACE_NAME_LEN   256
#define RA_QOS_ATTR_RESERVED 6
// 临时方案讲网卡数目提升到1024
#define MAX_INTERFACE_NUM_BAK   1024
#define MAX_SOCKET_EVENT_NUM 1024

enum notify_type {
    NO_USE = 0,
    NOTIFY = 1,
    EVENTID = 2,
};

/**
 * @ingroup libinit
 * white list switch
 */
enum white_list_status {
    WHITE_LIST_DISABLE = 0, /**< enable white list */
    WHITE_LIST_ENABLE, /**< disable white list */
};

/**
 * @ingroup librdma
 */
#define QP_CREATE_WITH_ATTR_VERSION 1
#define RA_QP_CREATE_WITH_ATTR_RESERVED 32
#define CQE_ERR_INFO_MAX_NUM 128
#define HCCP_GID_RAW_LEN 16U
#define REMAP_MR_MAX_NUM 128U

/**
 * @ingroup libcommon
 * others module error code conversion
 */
#define OTHERS_EAGAIN    128301   /* EAGAIN:try again */
#define OTHERS_ENOTSUPP  528302   /* ENOTSUPP: operation not supported */

/**
 * @ingroup libsocket
 * socket module error code conversion
 */
#define SOCK_EAGAIN    128201   /* EAGAIN:no data received by socket */
#define SOCK_CLOSE   128203 /* EINVAL:device异常关闭时作为心跳返回值返回给hccl*/
#define SOCK_ENOENT    228200   /* ENOENT:SOCK_ENOENT means mr async not success right now,revoke the funcion again */
#define SOCK_EADDRINUSE    128205   /* EADDRINUSE：check if IP has been listened when SOCK_EADDRINUSE is returned */
#define SOCK_EADDRNOTAVAIL 128206   /* EADDRNOTAVAIL：check if IP exist when SOCK_EADDRNOTAVAIL is returned */
#define SOCK_ESOCKCLOSED   128207   /* ESOCKCLOSED：socket has been closed */
#define SOCK_ENODEV   228202 /* socket 设备不存在 */

/**
 * @ingroup libinit
 * init module error code conversion
 */
#define HCCP_EAGAIN        128001   /* EAGAIN:try again */
#define HCCP_EINVALIDIPS   328008   /* ranktable中ip和物理网卡的ip不一致 */
#define HCCP_ELINKDOWN     328004   /* 网口down */

/**
 * @ingroup librdma
 * rdma module error code conversion
 */
#define ROCE_EAGAIN    128101   /* EAGAIN:try again */
#define ROCE_ENOMEM    328100   /* ENOMEM: roce module has ENOMEM error */
#define ROCE_EOPENSRC  528101   /* EOPENSRC: open source verbs error */
#define ROCE_ENOENT    228100   /* ENOENT: means mr async not success right now, revoke the funcion again */

/**
 * @ingroup libinit
 * hccp operating environment
 */
enum network_mode {
    NETWORK_PEER_ONLINE = 0, /**< Third-party online mode */
    NETWORK_OFFLINE, /**< offline mode */
    NETWORK_ONLINE, /**< online mode */
};

/**
 * @ingroup libinit
 * hccp support protocol type
 */
enum protocol_type {
    PROTOCOL_RDMA = 0,
    PROTOCOL_UDMA,
    PROTOCOL_UNSUPPORT,
};

/**
 * @ingroup libinit
 * rdma gid, aka infiniband ibv_gid
 */
union hccp_gid {
    uint8_t raw[HCCP_GID_RAW_LEN];
    struct {
        uint64_t subnet_prefix;
        uint64_t interface_id;
    } global;
};

/**
 * @ingroup libinit
 * info need of rdma_agent
 */
struct ra_info {
    int mode; /**< reference to network_mode */
    unsigned int phy_id; /**< physical device id */
};

/**
 * @ingroup libinit
 * ip address
 */
union hccp_ip_addr {
    struct in_addr addr;
    struct in6_addr addr6;
};

/**
 * @ingroup libinit
 * hccp init info
 */
struct rdev {
    unsigned int phy_id; /**< physical device id */
    int family; /**< AF_INET(ipv4) or AF_INET6(ipv6) */
    union hccp_ip_addr local_ip;
};

struct rdev_init_info {
    int mode;
    unsigned int notify_type;
    bool enabled_910a_lite; /**< true will enable 910A lite, invalid if enabled_2mb_lite is false; default is false */
    bool disabled_lite_thread; /**< true will not start lite thread, flag invalid if enabled_910a/2mb_lite is false */
    bool enabled_2mb_lite; /**< true will enable 2MB lite(include 910A & 910B), default is false */
};

struct socket_init_info_t {
    struct rdev rdev_info;
    int scope_id;
};

/**
 * @ingroup libsocket
 * socket listen status
 */
enum listen_phase {
    LISTEN_OK = 0, /**< socket listen ok */
    LISTEN_CREATE_FD_ERR = 1, /**< socket create fd error */
    LISTEN_BIND_ERR = 2, /**< socket bind socket port error */
    LISTEN_BEGIN_ERR = 3, /**< socket listen error */
};

/**
 * @ingroup libsocket
 * struct of the listen info
 */
struct socket_listen_info_t {
    void *socket_handle; /**< socket handle */
    unsigned int port; /**< Socket listening port number */
    unsigned int phase; /**< refer to enum listen_phase */
    unsigned int err; /**< errno */
};

/**
 * @ingroup libsocket
 * struct of the client socket
 */
struct socket_connect_info_t {
    void *socket_handle; /**< socket handle */
    union hccp_ip_addr remote_ip; /**< IP address of remote socket, [0-7] is reserved for vnic */
    unsigned int port; /**< Socket listening port number */
    char tag[SOCK_CONN_TAG_SIZE]; /**< tag must ended by '\0' */
};

/**
 * @ingroup libsocket
 * struct of the socket to be closed
 */
struct socket_close_info_t {
    void *socket_handle; /**< socket handle */
    void *fd_handle; /**< fd handle */
    int disuse_linger; /**< 0:use(default l_linger is RS_CLOSE_TIMEOUT), others:disuse */
};

/**
 * @ingroup libsocket
 * Details about socket after socket is linked
 */
struct socket_info_t {
    void *socket_handle; /**< socket handle */
    void *fd_handle; /**< fd handle */
    union hccp_ip_addr remote_ip; /**< IP address of remote socket */
    int status; /**< socket status:0 not connected 1:connected 2:connect timeout 3:connecting */
    char tag[SOCK_CONN_TAG_SIZE]; /**< tag must ended by '\0' */
};

/**
 * @ingroup libsocket
 * struct of socket event info
 */
#ifdef __x86_64__
#define EPOLL_PACKED __attribute__((packed))
#else
#define EPOLL_PACKED
#endif

struct socket_event_info {
    uint32_t event;
    union {
        void *fd_handle;
        uint64_t u64;
    };
} EPOLL_PACKED;
/**
 * @ingroup libinit
 * Configuration of rdma_agent initializatioin
 */
struct ra_init_config {
    unsigned int phy_id; /**< physical device id */
    unsigned int nic_position; /**< reference to network_mode */
    int hdc_type; /**< reference to drvHdcServiceType */
    bool enable_hdc_async; /**< true will init an extra HDC session for async APIs */
};

struct ra_get_ifattr {
    unsigned int phy_id; /**< physical device id */
    unsigned int nic_position; /**< reference to network_mode */
    bool is_all; /**< valid when nic_position is NETWORK_OFFLINE. false: get specific rnic ip, true: get all rnic ip */
};

/**
 * @ingroup libinit
 * id type to get vnic ip
 */
enum id_type {
    PHY_ID_VNIC_IP,
    SDID_VNIC_IP
};

/**
 * @ingroup libinit
 * struct of ip info
 */
struct ip_info {
    int family;
    union hccp_ip_addr ip;
    uint32_t resv[2U];
};

/**
 * @ingroup librdma
 * Flag of RMDA operations
 */
enum ra_send_flags {
    RA_SEND_FENCE = 1 << 0, /**< RDMA operation with fence */
    RA_SEND_SIGNALED = 1 << 1, /**< RDMA operation with signaled */
    RA_SEND_SOLICITED = 1 << 2, /**< RDMA operation with solicited */
    RA_SEND_INLINE = 1 << 3, /**< RDMA operation with inline */
};

/**
 * @ingroup librdma
 * Scatter and gather element
 */
struct sg_list {
    uint64_t addr; /**< address of buf */
    uint32_t len; /**< len of buf */
    uint32_t lkey; /**< local addr access key */
};

enum ra_wr_opcode {
    RA_WR_RDMA_WRITE,
    RA_WR_RDMA_WRITE_WITH_IMM,
    RA_WR_SEND,
    RA_WR_SEND_WITH_IMM,
    RA_WR_RDMA_READ,
    RA_WR_RDMA_ATOMIC_WRITE = 0xf0,
    RA_WR_RDMA_WRITE_WITH_NOTIFY = 0xf2,
    RA_WR_RDMA_REDUCE_WRITE = 0xf5,
    RA_WR_RDMA_REDUCE_WRITE_WITH_NOTIFY = 0xf6,
};

/**
 * @ingroup librdma
 * port status
 */
enum port_status {
    PORT_STATUS_DOWN = 0,
    PORT_STATUS_ACTIVE = 1,
};

/**
 * @ingroup librdma
 * RDMA work request
 */
struct send_wr {
    struct sg_list *buf_list; /**< list of sg */
    uint16_t buf_num; /**< num of buf_list */
    uint64_t dst_addr; /**< destination address */
    uint32_t rkey;     /**< remote address access key */
    uint32_t op; /**< operations of RDMA supported:RDMA_WRITE:0 */
    int send_flag; /**< reference to ra_send_flags */
};

struct wr_aux_info {
    uint8_t data_type;
    uint8_t reduce_type;
    uint32_t notify_offset;
};

struct wr_ext_info {
    uint32_t imm_data;
    uint16_t resv;
};

struct send_wrlist_data {
    unsigned long long dst_addr; /**< destination address */
    unsigned int op; /**< operations of RDMA supported:RDMA_WRITE:0, RDMA_READ:4 */
    int send_flags; /**< reference to ra_send_flags */
    struct sg_list mem_list; /**< list of sg */
};

struct send_wrlist_data_ext {
    unsigned long long dst_addr; /**< destination address */
    unsigned int op; /**< operations of RDMA supported:RDMA_WRITE:0, RDMA_READ:4 */
    int send_flags; /**< reference to ra_send_flags */
    struct sg_list mem_list; /**< list of sg */
    union {
        struct wr_aux_info aux; /**< aux info */
        struct wr_ext_info ext; /**< ext info */
    };
};

struct send_wr_v2 {
    uint64_t wr_id; /**< user assigned work request ID */
    struct sg_list *buf_list; /**< list of sg */
    uint16_t buf_num; /**< num of buf_list */
    uint64_t dst_addr; /**< destination address */
    uint32_t rkey;     /**< remote address access key */
    uint32_t op; /**< operations of RDMA supported:RDMA_WRITE:0 */
    int send_flag; /**< reference to ra_send_flags */
    union {
        struct wr_aux_info aux; /**< aux info */
        struct wr_ext_info ext; /**< ext info */
    };
};

struct wr_info {
    int send_flags;                 /**< reference to ra_send_flags */
    uint32_t rkey;                  /**< remote address access key */
    uint32_t op;                    /**< operations of RDMA supported:ra_wr_opcode */
    uint32_t imm_data;              /**< imm data */
    uint64_t wr_id;                 /**< user assigned work request ID */
    uint64_t dst_addr;              /**< destination address */
    struct sg_list mem_list;        /**< sg info */
    struct wr_aux_info aux;         /**< aux info */
};

struct recv_wrlist_data {
    uint64_t wr_id; /**< user assigned work request ID */
    struct sg_list mem_list; /**< list of sg */
};

/**
 * @ingroup libsocket
 * Socket whitelist
 */
struct socket_wlist_info_t {
    union hccp_ip_addr remote_ip; /**< IP address of remote */
    unsigned int conn_limit; /**< limit of whilte list */
    char tag[SOCK_CONN_TAG_SIZE]; /**< tag used for whitelist must ended by '\0' */
};

/**
 * @ingroup librdma
 * Flag of mr access
 */
enum ra_access_flags {
    RA_ACCESS_LOCAL_WRITE  = 1, /**< mr local write access */
    RA_ACCESS_REMOTE_WRITE = (1 << 1), /**< mr remote write access */
    RA_ACCESS_REMOTE_READ  = (1 << 2), /**< mr remote read access */
    RA_ACCESS_REMOTE_ATOMIC = (1 << 3), /**< mr remote atomic access */
    RA_ACCESS_REDUCE       = (1 << 8),
};

#define mem_mr_access_flags ra_access_flags

/**
 * @ingroup librdma
 * wqe template info
 */
struct wqe_info {
    unsigned int sq_index; /**< index of sq */
    unsigned int wqe_index; /**< index of wqe */
};

/**
 * @ingroup librdma
 * doorbell info
 */
struct db_info {
    unsigned int db_index; /**< index of db */
    unsigned long db_info; /**< db content */
};

/**
 * @ingroup librdma
 * respond of sending work request
 */
struct send_wr_rsp {
    union {
        struct wqe_info wqe_tmp; /**< wqe template info */
        struct db_info db; /**< doorbell info */
    };
};

struct ifaddr_info {
    union hccp_ip_addr ip; /* Address of interface */
    struct in_addr mask; /* Netmask of interface */
};

struct interface_info {
    int family;
    int scope_id;
    struct ifaddr_info ifaddr; /* Address and netmask of interface */
    char ifname[MAX_INTERFACE_NAME_LEN]; /* Name of interface */
};

struct mr_info {
    void *addr; /**< starting address of mr */
    unsigned long long size; /**< size of mr */
    int access; /**< access of mr, reference to ra_access_flags */
    unsigned int lkey; /**< local addr access key */
    unsigned int rkey; /**< remote addr access key */
};

struct mem_remap_info {
    void *addr; /**< starting address of needed remap memory */
    unsigned long long size; /**< size of needed remap memory */
};

enum ra_wc_opcode {
    RA_WC_SEND,
    RA_WC_RDMA_WRITE,
    RA_WC_RDMA_READ,
    RA_WC_RECV			= 1 << 7,
    RA_WC_RECV_RDMA_WITH_IMM,
};

enum RaEpollEvent {
    RA_EPOLLIN = 0,
    RA_EPOLLOUT,
    RA_EPOLLPRI,
    RA_EPOLLERR,
    RA_EPOLLHUP,
    RA_EPOLLET,
    RA_EPOLLONESHOT,
    RA_EPOLLOUT_LET_ONESHOT,
    RA_EPOLLINVALD
};

struct cq_attr {
    void **qp_context;
    struct ibv_cq **ib_send_cq;
    struct ibv_cq **ib_recv_cq;
    int send_cq_depth;
    int recv_cq_depth;
    int send_cq_event_id;
    int recv_cq_event_id;
    struct ibv_comp_channel *send_channel;
    struct ibv_comp_channel *recv_channel;
    void *srq_context;
};

struct srq_attr {
    void **context;
    struct ibv_srq **ib_srq;
    struct ibv_cq **ib_recv_cq;
    int cq_depth;
    int srq_depth;
    int max_sge;
    int srq_event_id;
};

struct qos_attr {
    unsigned char tc;          // traffic class
    unsigned char sl;          // priority(service level)
    unsigned char reserved[RA_QOS_ATTR_RESERVED];
};

struct cqe_err_info {
    uint32_t status;
    uint32_t qpn;
    struct timeval time;
};

struct cq_ext_attr {
    int send_cq_depth;
    int recv_cq_depth;
    int send_cq_comp_vector;
    int recv_cq_comp_vector;
};

struct ai_data_plane_wq {
    unsigned wqn;
    unsigned long long buf_addr;
    unsigned int wqebb_size;
    unsigned int depth;
    unsigned long long head_addr;
    unsigned long long tail_addr;
    unsigned long long swdb_addr;
    unsigned long long db_reg;
    unsigned int reserved[8U];
};

struct ai_data_plane_cq {
    unsigned int cqn;
    unsigned long long buf_addr;
    unsigned int cqe_size;
    unsigned int depth;
    unsigned long long head_addr;
    unsigned long long tail_addr;
    unsigned long long swdb_addr;
    unsigned long long db_reg;
    unsigned int reserved[2U];
};

struct ai_data_plane_info {
    struct ai_data_plane_wq sq;
    struct ai_data_plane_wq rq;
    struct ai_data_plane_cq scq;
    struct ai_data_plane_cq rcq;
    unsigned int reserved[8U];
};

union ai_data_plane_cstm_flag {
    struct {
        uint32_t cq_cstm  : 1; // 0: hccp poll cq; 1: caller poll cq
        uint32_t reserved : 31;
    } bs;
    uint32_t value;
};

enum {
    HCCP_RDMA_NOR_MODE      = 0,
    HCCP_RDMA_GDR_TMPL_MODE = 1,
    HCCP_RDMA_OP_MODE       = 2,
    HCCP_RDMA_GDR_ASYN_MODE = 3,
    HCCP_RDMA_OP_MODE_EXT   = 4,
    HCCP_RDMA_ERR_MODE      = 5
};

struct qp_ext_attrs {
    int qp_mode;
    // cq attr
    struct cq_ext_attr cq_attr;
    // qp attr
    struct ibv_qp_init_attr qp_attr;
    // version control and reserved
    int version;
    int mem_align; // 0,1:4KB, 2:2MB
    uint32_t udp_sport;
    union ai_data_plane_cstm_flag data_plane_flag; // only valid in ra_ai_qp_create
    uint32_t reserved[29U];
};

struct ai_qp_info {
    unsigned long long ai_qp_addr; // refer to struct ibv_qp *
    unsigned int sq_index; // index of sq
    unsigned int db_index; // index of db

    // below cq related info valid when data_plane_flag.bs.cq_cstm was 1
    unsigned long long ai_scq_addr; // refer to struct ibv_cq *scq
    unsigned long long ai_rcq_addr; // refer to struct ibv_cq *rcq
    struct ai_data_plane_info data_plane_info;
};

struct typical_qp {
    uint32_t qpn;
    uint32_t psn;
    uint32_t gid_idx;
    uint8_t resv1[4U]; // for compatibility issue
    uint8_t gid[HCCP_GID_RAW_LEN];
    uint32_t tc;
    uint32_t sl;
    uint32_t retry_cnt;
    uint32_t retry_time;
    // version control and reserved
    int version;
    uint32_t reserved[32U];
    uint8_t resv2[4U]; // for compatibility issue
};

struct qp_attr {
    unsigned int qpn;
    unsigned int udp_sport;
    unsigned int psn;
    unsigned int gid_idx;
    unsigned char gid[HCCP_GID_RAW_LEN];
};

struct loopback_qp_pair {
    void *ibv_qp0;
    void *ibv_qp1;
};

struct socket_err_info {
    struct timeval time;
    int err_no;
    int action; // refer to enum rs_conn_state
    char resv[32U];
};

struct server_socket_err_info {
    struct socket_err_info epoll_wait;
    struct socket_err_info accept;
};

enum save_snapshot_action {
    SAVE_SNAPSHOT_ACTION_PRE_PROCESSING = 0,
    SAVE_SNAPSHOT_ACTION_POST_PROCESSING = 1,
    SAVE_SNAPSHOT_ACTION_MAX,
};
#ifdef __cplusplus
}
#endif
#endif
