#include <stdlib.h>
#include <sys/time.h>
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

struct rs_conn_cb **conn_cb

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

struct rs_list_head {
    struct rs_list_head *next, *prev;
};

struct rs_conn_cb {
    uint32_t local_ip_addr;
    unsigned int wlist_enable;
    int eventfd;
    int epollfd;
    unsigned int server_port;

    pthread_mutex_t conn_mutex;
    struct rs_cb *rscb;

    struct rs_list_head listen_list;
    struct rs_list_head server_accept_list;
    struct rs_list_head server_conn_list;
    struct rs_list_head client_conn_list;
    struct rs_list_head white_list;
};

struct rs_list_head server_conn_list;

// int rs_dev2conncb(uint32_t chip_id, struct rs_conn_cb **conn_cb)
// {
//     int ret;
//     struct rs_cb *rs_cb = NULL;

//     ret = rs_dev2rscb(chip_id, &rs_cb, false);
//     if (ret) {
//         hccp_err("get rs_cb failed(%d)", ret);
//         return ret;
//     }

//     *conn_cb = &(rs_cb->conn_cb);

//     return 0;
// }

// void rs_dev2conncb(struct timeval *end_time, struct timeval *start_time, float *msec)
// {
//     /* if low position is sufficient, then borrow one from the high position */
//     if (end_time->tv_usec < start_time->tv_usec) {
//         end_time->tv_sec -= 1;
//         end_time->tv_usec += MS_PER_SECOND_I * MS_PER_SECOND_I;
//     }

//     *msec = (end_time->tv_sec - start_time->tv_sec) * MS_PER_SECOND_F +
//             (end_time->tv_usec - start_time->tv_usec) / MS_PER_SECOND_F;

//     return;
// }


