#include <stdlib.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include "hccp_common.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

typedef uint32_t u32;
typedef uint16_t u16;
//typedef uint64_t u64;
typedef unsigned long long u64;
typedef signed int s32;
static int rs_socket_batch_connect_counter = 0;
static int rs_socket_batch_close_counter = 0;
static int rs_socket_listen_start_counter = 0;
static int rs_socket_listen_stop_counter = 0;
static int rs_get_sockets_counter = 0;
static int rs_socket_send_counter = 0;
static int rs_socket_recv_counter = 0;

static int rs_qp_create_counter = 0;
static int rs_qp_destroy_counter = 0;
static int rs_mr_reg_counter = 0;
static int rs_mr_dereg_counter = 0;
static int rs_qp_connect_async_counter = 0;
static int rs_get_qp_status_counter = 0;
static int rs_send_async_counter = 0;
static int rs_get_sq_index_counter = 0;
static int rs_get_notify_ba_counter = 0;

static int rs_init_counter = 0;
static int rs_deinit_counter = 0;
static struct ibv_mr stub_mr = {0};

struct rs_init_config {
	u32 port;		//[in]
	u32 device_id;	//[in]
	u32 rs_position;	//[in] 0:online, 1:offline
};

struct rs_socket_listen_info_t {
	uint32_t device_id;	//[in] device id
	uint32_t ip_addr;	//[in] Server listen IP
	uint32_t phase;		//[out] refer to enum listen_phase
	uint32_t err;		//[out] errno
};

struct rs_socket_connect_info_t {
	uint32_t device_id;	//[in] device id
	uint32_t ip_addr;	//[in] Server IP
	char tag[128];	//[in] CCL group classification
};

struct rs_socket_close_info_t {
	int fd;		//[in]client socket
};

struct rs_socket_info_t {
	int fd;			//[out] socket fd
	u32 device_id;		//[in] device id
	u32 server_ip_addr;	//[in] server IP
	u32 client_ip_addr;	//[out] for Server: Accpet成功后的client ip
	s32 status;		//[out] 0：未连接，1：连接OK，2：连接超时，3：连接中
	char tag[128];	//[in] HCCL group classification
};

struct rs_qp_norm {
    int flag;
    int qp_mode;
    int is_exp;
    int is_ext;
    int mem_align; // 0,1:4KB, 2:2MB
};

struct rs_wrlist_base_info {
    unsigned int dev_id;
    unsigned int rdev_index;
    unsigned int qpn;
	unsigned int key_flag;
};

// struct socket_fd_data {
//     int fd;
//     unsigned int phy_id;
//     int family; // AF_INET(ipv4) or AF_INET6(ipv6)
//     union hccp_ip_addr local_ip;
//     union hccp_ip_addr remote_ip;
//     int status;
//     char tag[SOCK_CONN_TAG_SIZE];
// };

#define MS_PER_SECOND_F   1000.0
#define MS_PER_SECOND_I   1000

void rs_get_cur_time(struct timeval *time)
{
    int ret;

    ret = gettimeofday(time, NULL);
    if (ret) {
        memset(time, 0, sizeof(struct timeval));
    }

    return;
}

void hccp_time_interval(struct timeval *end_time, struct timeval *start_time, float *msec)
{
    /* if low position is sufficient, then borrow one from the high position */
    if (end_time->tv_usec < start_time->tv_usec) {
        end_time->tv_sec -= 1;
        end_time->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
    }

    *msec = (end_time->tv_sec - start_time->tv_sec) * MS_PER_SECOND_F +
            (end_time->tv_usec - start_time->tv_usec) / MS_PER_SECOND_F;

    return;
}

int rs_socket_batch_connect(struct socket_connect_info conn[], uint32_t num)
{
	if (rs_socket_batch_connect_counter <= 1) {
		rs_socket_batch_connect_counter++;
		return 0;
	} else {
		return -1;
	}
}
int rs_socket_batch_close(int disuse_linger, struct rs_socket_close_info_t conn[], u32 num)
{
	rs_socket_batch_close_counter++;
	if (rs_socket_batch_close_counter <= 1) {
		return 0;
	} else {
		return -1;
	}
}
int rs_socket_set_scope_id(unsigned int dev_id, int scope_id)
{
    return 0;
}
int rs_socket_batch_abort(int disuse_linger, struct rs_socket_close_info_t conn[], u32 num)
{
    return 0;
}
int rs_socket_listen_start(struct socket_listen_info listen[], uint32_t num)
{
	if (rs_socket_listen_start_counter <= 1) {
		rs_socket_listen_start_counter++;
		return 0;
	} else {
		return -1;
	}
}
int rs_socket_listen_stop(struct socket_listen_info conn[], u32 num)
{
	rs_socket_listen_stop_counter++;
	if (rs_socket_listen_stop_counter <= 1) {
		return 0;
	} else {
		return -1;
	}
}

int rs_get_socket_num(unsigned int role, struct rdev rdev_info, unsigned int *socket_num)
{
    // unsigned int num = 0;
    // if (rs_socket_batch_connect_counter < 0) {
    //     *socket_num = num;
    //     return -1;
    // }
    // num = rs_socket_batch_connect_counter;
	// 	*socket_num = num;
		return 0;
}

int rs_get_all_sockets(unsigned int phy_id, uint32_t role, struct socket_fd_data *conn,
    uint32_t *socket_num)
{
	conn->fd = 0;
	conn->local_ip.addr.s_addr = 0;
	conn->remote_ip.addr.s_addr = 0;
	conn->phy_id = 0;
	conn->tag[0] = '0';
	return 0;
}

int rs_get_sockets(uint32_t role, struct socket_fd_data conn[], uint32_t num)
{
	return num;
}

int rs_get_ssl_enable(uint32_t *ssl_enable)
{
    return 0;
}

int rs_peer_socket_send(uint32_t ssl_enable, int fd, const void *data, uint64_t size)
{
    return size;
}

int rs_peer_socket_recv(uint32_t ssl_enable, int fd, void *data, uint64_t size)
{
    return size;
}

int rs_socket_send(int fd, void* data, u64 size)
{
	if (rs_socket_send_counter <= 1) {
		rs_socket_send_counter++;
		return size;
	} else {
		return 1;
	}
}

int rs_socket_recv(int fd, void* data, u64 size)
{
	if (rs_socket_recv_counter <= 1) {
		rs_socket_recv_counter++;
		return size;
	} else {
		return 1;
	}
}

int rs_qp_create(unsigned int phy_id, unsigned int rdev_index, struct rs_qp_norm qp_norm,
    struct rs_qp_resp *qp_resp)
{
	qp_resp->qpn = 0;
	qp_resp->psn = 0;
	qp_resp->gid_idx = 0;
	qp_resp->gid.global.subnet_prefix = 0;
	qp_resp->gid.global.interface_id = 0;

	return 0;

}

int rs_qp_create_with_attrs(unsigned int phy_id, unsigned int rdev_index,
    struct rs_qp_norm_with_attrs *qp_norm, struct rs_qp_resp_with_attrs *qp_resp)
{
	qp_resp->qpn = 0;
	qp_resp->ai_qp_addr = 0;
	return 0;
}

int rs_qp_destroy(unsigned int dev_id, unsigned int rdev_index, unsigned int qpn)
{
	rs_qp_destroy_counter++;
	if (rs_qp_destroy_counter <= 3) {
		return 0;
	} else {
		return -1;
	}
}

int rs_typical_qp_modify(unsigned int phy_id, unsigned int rdev_index,
    struct typical_qp local_qp_info, struct typical_qp remote_qp_info, unsigned int *udp_sport)
{
	return 0;
}

int rs_typical_register_mr_v1(unsigned int phy_id, unsigned int rdev_index, struct rdma_mr_reg_info *mr_reg_info,
    void **mr_handle)
{
	*mr_handle = &stub_mr;
	return 0;
}

int rs_typical_register_mr(unsigned int phy_id, unsigned int rdev_index, struct rdma_mr_reg_info *mr_reg_info,
    void **mr_handle)
{
	*mr_handle = &stub_mr;
	return 0;
}

int rs_typical_deregister_mr(unsigned int phy_id, unsigned int dev_index, unsigned long long addr)
{
	return 0;
}

int rs_remap_mr(unsigned int phy_id, unsigned int rdev_index, struct mem_remap_info mem_list[],
    unsigned int mem_num)
{
    return 0;
}

int rs_qp_connect_async(unsigned int dev_id, unsigned int rdev_index, unsigned int qpn, int fd)
{
	rs_qp_connect_async_counter++;
	if (rs_qp_connect_async_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_get_qp_status(unsigned int dev_id, unsigned int rdev_index, unsigned int qpn, int *status)
{
	rs_get_qp_status_counter++;
	if (rs_get_qp_status_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_mr_reg(unsigned int dev_id, unsigned int rdev_index, unsigned int qpn, struct rdma_mr_reg_info *mr_reg_info)
{
	rs_mr_reg_counter++;
	if (rs_mr_reg_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}
int rs_mr_dereg(unsigned int dev_id, unsigned int rdev_index, unsigned int qpn, char *addr)
{
	rs_mr_dereg_counter++;
	if (rs_mr_dereg_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_register_mr(unsigned int dev_id, unsigned int rdev_index, struct rdma_mr_reg_info *mr_reg_info, void *mr_handler)
{
    rs_mr_reg_counter++;
	if (rs_mr_reg_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_deregister_mr(void *mr_handler)
{
    rs_mr_dereg_counter++;
	if (rs_mr_dereg_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_send_wr(unsigned int dev_id, unsigned int rdev_index, uint32_t qpn, struct send_wr *wr,
    struct send_wr_rsp *wr_rsp)
{
	rs_send_async_counter++;
	if (rs_send_async_counter <= 1) {
		return 0;
	} else {
		return -1;
	}
}

int rs_send_wrlist(struct rs_wrlist_base_info base_info, struct wr_info *wr,
    unsigned int send_num, struct send_wr_rsp *wr_rsp, unsigned int *complete_num)
{
	//(*complete_num)++;
	*complete_num = send_num;
	return 0;
}

int rs_get_notify_mr_info(unsigned int phy_id, unsigned int rdev_index, struct mr_info *info)
{
	rs_get_notify_ba_counter++;
	if (rs_get_notify_ba_counter <= 1) {
		return 0;
	} else {
		return -1;
	}
}
int rs_set_host_pid(void *in_buf, void **out_buf, int *out_len, u32 *close_session)
{
	return 0;
}

int rs_rdev_get_port_status(unsigned int phy_id, unsigned int rdev_index, enum port_status *status)
{
	return 0;
}

int rs_init(struct rs_init_config *cfg)
{
	rs_init_counter++;
	if (rs_init_counter <= 2) {
		return 0;
	} else {
		return -1;
	}
}

int rs_bind_hostpid(unsigned int chip_id, pid_t pid)
{
	return 0;
}

int rs_deinit(struct rs_init_config *cfg)
{
	rs_deinit_counter++;
	if (rs_deinit_counter <= 1) {
		return 0;
	} else {
		return -1;
	}
}

int rs_socket_init(const unsigned int *vnic_ip, unsigned int num)
{
    return 0;
}

int rs_socket_deinit(struct rdev rdev_info)
{
    return 0;
}

int rs_rdev_init(struct rdev rdev_info, unsigned int notify_type,  unsigned int *rdev_index)
{
    return 0;
}

int rs_rdev_init_with_backup(struct rdev rdev_info, struct rdev backup_rdev_info,
    unsigned int notify_type, unsigned int *rdev_index)
{
    return 0;
}

int rs_rdev_deinit(unsigned int dev_id, unsigned int notify_type,  unsigned int rdev_index)
{
    return 0;
}

int rs_socket_white_list_add(struct rdev rdev_info, struct socket_wlist_info_t white_list[],
                                        u32 num)
{
    return 0;
}

int rs_socket_white_list_del(struct rdev rdev_info, struct socket_wlist_info_t white_list[],
                                        u32 num)
{
    return 0;
}
int rs_peer_get_ifaddrs(struct interface_info interface_infos[], unsigned int *num,
    unsigned int phy_id)
{
	return 0;
}

int rs_get_ifaddrs(struct ifaddr_info ifaddr_infos[], unsigned int *num, unsigned int dev_id)
{
	return 0;
}

int rs_get_ifaddrs_v2(struct interface_info interface_infos[], unsigned int *num, unsigned int dev_id, bool is_all)
{
	return 0;
}

int rs_get_ifnum(unsigned int phy_id, bool is_all, unsigned int *num)
{
    return 0;
}

int rs_peer_get_ifnum(unsigned int phy_id, unsigned int *num)
{
    return 0;
}

int rs_get_vnic_ip(unsigned int phy_id, unsigned int *vnic_ip)
{
	return 0;
}

int rs_get_interface_version(unsigned int opcode, unsigned int *version)
{
    *version = 1;
    return 0;
}

int rs_notify_cfg_set(unsigned int dev_id, unsigned long long va, unsigned long long size)
{
	return 0;
}

int rs_notify_cfg_get(unsigned int dev_id, unsigned long long *va, unsigned long long *size)
{
	return 0;
}

int rs_set_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int temp_depth,
    unsigned int *qp_num)
{
	return 0;
}

int rs_get_tsqp_depth(unsigned int phy_id, unsigned int rdev_index, unsigned int *temp_depth,
    unsigned int *qp_num)
{
	return 0;
}

void rs_heartbeat_alive_print(struct rs_pthread_info *pthread_info)
{
	return;
}

int rs_recv_wrlist(struct rs_wrlist_base_info base_info, struct recv_wrlist_data *wr,
    unsigned int recv_num, unsigned int *complete_num)
{
	*complete_num = recv_num;
	return 0;
}

int rs_epoll_ctl_add(const void *fd_handle, enum RaEpollEvent event)
{
    return 0;
}

int rs_epoll_ctl_mod(const void *fd_handle, enum RaEpollEvent event)
{
    return 0;
}

int rs_epoll_ctl_del(int fd)
{
    return 0;
}

void rs_set_tcp_recv_callback(const void *callback)
{
    return;
}

int rs_get_qp_context(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, void** qp,
    void** send_cq, void** recv_cq)
{
    return 0;
}

int rs_cq_create(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr)
{
    return 0;
}

int rs_cq_destroy(unsigned int phy_id, unsigned int rdev_index, struct cq_attr *attr)
{
	return 0;
}

int rs_normal_qp_create(unsigned int phy_id, unsigned int rdev_index,
    struct ibv_qp_init_attr *qp_init_attr, struct rs_qp_resp *qp_resp, void** qp)
{
    return 0;
}

int rs_normal_qp_destroy(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn)
{
	return 0;
}

int rs_set_qp_attr_qos(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    struct qos_attr *attr)
{
    return 0;
}

int rs_set_qp_attr_timeout(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *timeout)
{
    return 0;
}

int rs_set_qp_attr_retry_cnt(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn,
    unsigned int *retry_cnt)
{
    return 0;
}

int rs_get_cqe_err_info(struct cqe_err_info *info)
{
	return 0;
}

int rs_get_cqe_err_info_num(unsigned int phy_id, unsigned int rdev_idx, unsigned int *num)
{
    return 0;
}

int rs_get_cqe_err_info_list(unsigned int phy_id, unsigned int rdev_idx, struct cqe_err_info *info,
    unsigned int *num)
{
    return 0;
}

void tls_get_enable_info(unsigned int save_mode, unsigned int chip_id, unsigned char *buf, unsigned int buf_size)
{
    return 0;
}

int rs_create_comp_channel(unsigned int phy_id, unsigned int rdev_index, void** comp_channel)
{
	return 0;
}

int rs_destroy_comp_channel(void* comp_channel)
{
	return 0;
}

int rs_create_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr)
{
	return 0;
}

int rs_destroy_srq(unsigned int phy_id, unsigned int rdev_index, struct srq_attr *attr)
{
	return 0;
}

int rs_get_lite_support(unsigned int phy_id, unsigned int rdev_index, int *support_lite)
{
	return 0;
}

int rs_get_lite_rdev_cap(unsigned int phy_id, unsigned int rdev_index, struct lite_rdev_cap_resp *resp)
{
	return 0;
}

int rs_get_lite_qp_cq_attr(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_qp_cq_attr_resp *resp)
{
	return 0;
}

int rs_get_lite_connected_info(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_connected_info_resp *resp)
{
	return 0;
}

int rs_get_lite_mem_attr(unsigned int phy_id, unsigned int rdev_index, unsigned int qpn, struct lite_mem_attr_resp *resp)
{
    return 0;
}

void rs_set_ctx(unsigned int phy_id)
{
    return;
}

int rs_create_event_handle(int *event_handle)
{
    return 0;
}

int rs_ctl_event_handle(int event_handle, const void *fd_handle, int opcode,
    enum RaEpollEvent event)
{
    int ret;
    int fd = -1;
    int tmpEvent;

    if (event_handle < 0) {
        hccp_err("[rs_ctl_event_handle]event_handle[%d] is invalid", event_handle);
        return -EINVAL;
    }
    if (fd_handle == NULL) {
        hccp_err("[rs_ctl_event_handle]fd_handle is NULL");
        return -EINVAL;
    }
    if (opcode != EPOLL_CTL_ADD && opcode != EPOLL_CTL_DEL && opcode != EPOLL_CTL_MOD) {
        hccp_err("[rs_ctl_event_handle]opcode[%d] invalid, valid opcode includes {%d, %d, %d}",
            opcode, EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD);
        return -EINVAL;
    }
    if (opcode == EPOLL_CTL_DEL && event != RA_EPOLLIN) {
        hccp_err("[rs_ctl_event_handle]param invalid: opcode[%d], event[%d]", opcode, event);
        return -EINVAL;
    }

    if (event == RA_EPOLLONESHOT) {
        tmpEvent = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT;
    } else if (event == RA_EPOLLIN) {
        tmpEvent = EPOLLIN;
    } else {
        hccp_err("[rs_ctl_event_handle]unkown event[%d]", event);
        return -EINVAL;
    }

    return 0;
}

int rs_wait_event_handle(int event_handle, struct socket_event_info *event_info, int timeout)
{
    return 0;
}

int rs_destroy_event_handle(int *event_handle)
{
    return 0;
}

int rs_get_vnic_ip_infos(unsigned int phy_id, enum id_type type, unsigned int ids[], unsigned int num,
    struct ip_info infos[])
{
    return 0;
}

int rs_qp_batch_modify(unsigned int phy_id, unsigned int rdev_index,
    int status, int qpn[], int qpn_num)
{
    return 0;
}

int rs_socket_get_client_socket_err_info(struct socket_connect_info conn[],
    struct socket_err_info err[], unsigned int num)
{
    return 0;
}

int rs_socket_get_server_socket_err_info(struct socket_listen_info conn[],
    struct server_socket_err_info err[], unsigned int num)
{
    return 0;
}

int rs_socket_accept_credit_add(struct socket_listen_info conn[], uint32_t num, unsigned int credit_limit)
{
    return 0;
}

int rs_get_tls_enable(unsigned int phy_id, bool *tls_enable)
{
	return 0;
}

int rs_get_sec_random(unsigned int *value)
{
	return 0;
}

int rs_drv_get_random_num(int *rand_num)
{
	return 0;
}

int rs_get_hccn_cfg(unsigned int phy_id, enum hccn_cfg_key key, char *value,
    unsigned int *value_len)
{
	return 0;
}
