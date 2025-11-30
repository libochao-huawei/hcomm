#include "dlfcn.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"
#include "ra_rdma_lite.h"

typedef uint32_t u32;
typedef uint16_t u16;
//typedef uint64_t u64;
typedef unsigned long long u64;
typedef signed int s32;

void *dl_handle;

static int counter = 0;
int (*ra_peer_init_p)(s32 dev_id) = ra_peer_init;
int (*ra_peer_deinit_p)(s32 dev_id) = ra_peer_deinit;
int (*ra_peer_socket_batch_connect_p)(struct SocketConnectInfoT conn[], u32 num) = ra_peer_socket_batch_connect;
int (*ra_peer_socket_batch_close_p)(struct SocketCloseInfoT conn[], u32 num) = ra_peer_socket_batch_close;
int (*ra_peer_socket_listen_start_p)(struct SocketListenInfoT conn[], u32 num) = ra_peer_socket_listen_start;
int (*ra_peer_socket_listen_stop_p)(struct SocketListenInfoT conn[], u32 num) = ra_peer_socket_listen_stop;
int (*ra_peer_get_sockets_p)(u32 role, struct SocketInfoT conn[], u32 num) = ra_peer_get_sockets;
int (*ra_peer_socket_send_p)(int dev_id, void* handle, void* data, u64 size) = ra_peer_socket_send;
int (*ra_peer_socket_recv_p)(int dev_id, void* handle, void* data, u64 size) = ra_peer_socket_recv;
void* (*ra_peer_qp_create_p)(int dev_id, int flag) = ra_peer_qp_create;
int (*ra_peer_qp_destroy_p)(void *handle) = ra_peer_qp_destroy;
int (*ra_peer_qp_connect_async_p)(void* handle, void *sock_handle) = ra_peer_qp_connect_async;
int (*ra_peer_get_qp_status_p)(void* handle, int *status)= ra_peer_get_qp_status;
int (*ra_peer_mr_reg_p)(void* handle, void *addr, u64 size, int access) = ra_peer_mr_reg;
int (*ra_peer_mr_dereg_p)(void* handle, void* addr) = ra_peer_mr_dereg;
int (*ra_peer_send_async_p)(void* handle, struct send_wr* wr, u32* wqe_index) = ra_peer_send_wr;
int (*ra_peer_get_notify_base_addr_p)(void *handle, u64 *va, u64 *size) = ra_peer_get_notify_base_addr;

int (*ra_hdc_init_p)(s32 dev_id) = ra_hdc_init;
int (*ra_hdc_deinit_p)(s32 dev_id) = ra_hdc_deinit;
int (*ra_hdc_socket_batch_connect_p)(struct SocketConnectInfoT conn[], u32 num) = ra_hdc_socket_batch_connect;
int (*ra_hdc_socket_batch_close_p)(struct SocketCloseInfoT conn[], u32 num) = ra_hdc_socket_batch_close;
int (*ra_hdc_socket_listen_start_p)(struct SocketListenInfoT conn[], u32 num) = ra_hdc_socket_listen_start;
int (*ra_hdc_socket_listen_stop_p)(struct SocketListenInfoT conn[], u32 num) = ra_hdc_socket_listen_stop;
int (*ra_hdc_get_sockets_p)(u32 role, struct SocketInfoT conn[], u32 num) = ra_hdc_get_sockets;
int (*ra_hdc_socket_send_p)(void* handle, void* data, u64 size) = ra_hdc_socket_send;
int (*ra_hdc_socket_recv_p)(void* handle, void* data, u64 size) = ra_hdc_socket_recv;
void* (*ra_hdc_qp_create_p)(int dev_id, int flag) = ra_hdc_qp_create;
int (*ra_hdc_qp_destroy_p)(void *handle) = ra_hdc_qp_destroy;
int (*ra_hdc_qp_connect_async_p)(void* handle, void *sock_handle) = ra_hdc_qp_connect_async;
int (*ra_hdc_get_qp_status_p)(void* handle, int *status)= ra_hdc_get_qp_status;
int (*ra_hdc_mr_reg_p)(void* handle, void *addr, u64 size, int access) = ra_hdc_mr_reg;
int (*ra_hdc_mr_dereg_p)(void* handle, void* addr) = ra_hdc_mr_dereg;
int (*ra_hdc_send_async_p)(void* handle, struct send_wr* wr, struct send_wr_rsp *op_rsp, unsigned int host_tgid) = ra_hdc_send_wr;
int (*ra_hdc_get_notify_base_addr_p)(void *handle, u64 *va, u64 *size) = ra_hdc_get_notify_base_addr;

void *dlopen_stub(char* lib_path, int para)
{
	return dl_handle;
}

void *dlsym_stub(void* handle, char* func)
{

	counter ++;
	switch (counter)
	{
		case 1:
			return ra_hdc_init_p;
			break;
		case 2:
			return ra_hdc_deinit_p;
			break;
		case 3:
			return ra_hdc_socket_batch_connect_p;
			break;
		case 4:
			return ra_hdc_socket_batch_close_p;
			break;
		case 5:
			return ra_hdc_socket_listen_start_p;
			break;
		case 6:
			return ra_hdc_socket_listen_stop_p;
			break;
		case 7:
			return ra_hdc_get_sockets_p;
			break;
		case 8:
			return ra_hdc_socket_send_p;
			break;
		case 9:
			return ra_hdc_socket_recv_p;
			break;
		case 10:
			return ra_hdc_qp_create_p;
			break;
		case 11:
			return ra_hdc_qp_destroy_p;
			break;
		case 12:
			return ra_hdc_qp_connect_async_p;
			break;
		case 13:
			return ra_hdc_get_qp_status_p;
			break;
		case 14:
			return ra_hdc_mr_reg_p;
			break;
		case 15:
			return ra_hdc_mr_dereg_p;
			break;
		case 16:
			return ra_hdc_send_async_p;
			break;
		case 17:
			break;
		case 18:
			return ra_hdc_get_notify_base_addr_p;
			break;
		case 25:
			return ra_peer_init_p;
			break;
		case 26:
			return ra_peer_deinit_p;
			break;
		case 27:
			return ra_peer_socket_batch_connect_p;
			break;
		case 28:
			return ra_peer_socket_batch_close_p;
			break;
		case 29:
			return ra_peer_socket_listen_start_p;
			break;
		case 30:
			return ra_peer_socket_listen_stop_p;
			break;
		case 31:
			return ra_peer_get_sockets_p;	
			break;
		case 32:
			return ra_peer_socket_send_p;	
			break;
		case 33:
			return ra_peer_socket_recv_p;	
			break;
		case 34:
			return ra_peer_qp_create_p;	
			break;
		case 35:
			return ra_peer_qp_destroy_p;	
			break;
		case 36:
			return ra_peer_qp_connect_async_p;	
			break;
		case 37:
			return ra_peer_get_qp_status_p;	
			break;
		case 38:
			return ra_peer_mr_reg_p;	
			break;
		case 39:
			return ra_peer_mr_dereg_p;	
			break;
		case 40:
			return ra_peer_send_async_p;	
			break;
		case 41:
			break;
		case 42:
			return ra_peer_get_notify_base_addr_p;	
			break;
	}
	return;
}

int dlclose_stub(void* handle)
{
	return 0;
}

struct rdma_lite_context *rdma_lite_alloc_context(u8 phy_id, struct dev_cap_info *cap)
{
    struct rdma_lite_context *lite_ctx = calloc(1, sizeof(struct rdma_lite_context));
    return lite_ctx;
}

void rdma_lite_free_context(struct rdma_lite_context *lite_ctx)
{
    free(lite_ctx);
}

int rdma_lite_init_mem_pool(struct rdma_lite_context *lite_ctx, struct rdma_lite_mem_attr *lite_mem_attr)
{
    return 0;
}

int rdma_lite_deinit_mem_pool(struct rdma_lite_context *lite_ctx, u32 mem_idx)
{
    return 0;
}

struct rdma_lite_cq *rdma_lite_create_cq(struct rdma_lite_context *lite_ctx, struct rdma_lite_cq_attr *lite_cq_attr)
{
    struct rdma_lite_cq *lite_cq = calloc(1, sizeof(struct rdma_lite_cq));
    return lite_cq;
}

int rdma_lite_destroy_cq(struct rdma_lite_cq *lite_cq)
{
    free(lite_cq);

    return 0;
}

int rdma_lite_poll_cq(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc *lite_wc)
{
    return 0;
}

int rdma_lite_poll_cq_v2(struct rdma_lite_cq *lite_cq, int num_entries, struct rdma_lite_wc_v2 *lite_wc)
{
    return 0;
}

struct rdma_lite_qp *rdma_lite_create_qp(struct rdma_lite_context *lite_ctx, struct rdma_lite_qp_attr *lite_qp_attr)
{
    struct rdma_lite_qp *lite_qp = calloc(1, sizeof(struct rdma_lite_qp));
    return lite_qp;
}

int rdma_lite_destroy_qp(struct rdma_lite_qp *lite_qp)
{
    free(lite_qp);

    return 0;
}

int rdma_lite_post_send(struct rdma_lite_qp *lite_qp, struct rdma_lite_send_wr *wr,
        struct rdma_lite_send_wr **bad_wr, struct rdma_lite_post_send_attr *attr,
        struct rdma_lite_post_send_resp *resp)
{
    return 0;
}

int rdma_lite_post_recv(struct rdma_lite_qp *lite_qp, struct rdma_lite_recv_wr *wr, struct rdma_lite_recv_wr **bad_wr)
{
    return 0;
}

int rdma_lite_set_qp_sl(struct rdma_lite_qp *lite_qp, int sl)
{
    return 0;
}

int rdma_lite_clean_qp(struct rdma_lite_qp *lite_qp)
{
    return 0;
}

int rdma_lite_restore_snapshot(struct rdma_lite_context *lite_ctx)
{
    return 0;
}
