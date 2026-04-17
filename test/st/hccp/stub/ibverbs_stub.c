
#include <ccan/list.h>
#include <infiniband/driver.h>
#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define IBV_DEV_NUM_MAX (1)

struct _compat_ibv_port_attr {
	enum ibv_port_state state;
	enum ibv_mtu max_mtu;
	enum ibv_mtu active_mtu;
	int gid_tbl_len;
	uint32_t port_cap_flags;
	uint32_t max_msg_sz;
	uint32_t bad_pkey_cntr;
	uint32_t qkey_viol_cntr;
	uint16_t pkey_tbl_len;
	uint16_t lid;
	uint16_t sm_lid;
	uint8_t lmc;
	uint8_t max_vl_num;
	uint8_t sm_sl;
	uint8_t subnet_timeout;
	uint8_t init_type_reply;
	uint8_t active_width;
	uint8_t active_speed;
	uint8_t phys_state;
	uint8_t link_layer;
	uint8_t flags;
};

static struct ibv_context_ops g_ibv_context_ops;

static int query_device(struct ibv_context *context,
			     struct ibv_device_attr *device_attr)
{
    return 0;
}

static int query_port(struct ibv_context *context,
			   uint8_t port_num,
			   struct _compat_ibv_port_attr *port_attr)
{
    return 0;
}

static void *alloc_pd(struct ibv_context *context)
{
    return NULL;
}

static int dealloc_pd(struct ibv_pd *pd)
{
    return 0;
}

static struct ibv_mr *reg_mr(struct ibv_pd *pd, void *addr,
				  size_t length, int access)
{
    return NULL;
}

static int rereg_mr(struct ibv_mr *mr, int flags, void *addr,
			 size_t length, int access,
			 struct ibv_pd *pd)
{
    return 0;
}

static int dereg_mr(struct ibv_mr *mr)
{
    return 0;
}

static struct ibv_mw *alloc_mw(struct ibv_pd *pd, enum ibv_mw_type type)
{
    return NULL;
}

static int bind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
			 struct ibv_mw_bind *mw_bind)
{
    return 0;
}

static int dealloc_mw(struct ibv_mw *mw)
{
    return 0;
}

static struct ibv_cq *create_cq(struct ibv_context *context, int cqe,
				      void *cq_context,
				      struct ibv_comp_channel *channel,
				      int comp_vector)
{
    return NULL;
}

static int poll_cq(struct ibv_cq *cq, int num_entries, struct ibv_wc *wc)
{
    return 0;
}

static int req_notify_cq(struct ibv_cq *cq, int solicited_only)
{
    return 0;
}

static int cq_event(struct ibv_comp_channel *channel,
			  struct ibv_cq **cq, void **cq_context)
{
    return 0;
}

static int resize_cq(struct ibv_cq *cq, int cqe)
{
    return 0;
}

static int destroy_cq(struct ibv_cq *cq)
{
    return 0;
}

static struct ibv_srq *create_srq(struct ibv_pd *pd,
					struct ibv_srq_init_attr *attr)
{
    return NULL;
}

static int modify_srq(struct ibv_srq *srq, struct ibv_srq_attr *attr,
			   int attr_mask)
{
    return 0;
}

static int query_srq(struct ibv_srq *srq, struct ibv_srq_attr *attr)
{
    return 0;
}

static int destroy_srq(struct ibv_srq *srq)
{
    return 0;
}

static int post_srq_recv(struct ibv_srq *srq,
			      struct ibv_recv_wr *recv_wr,
			      struct ibv_recv_wr **bad_recv_wr)
{
    return 0;
}

static struct ibv_qp *create_qp(struct ibv_pd *pd,
				     struct ibv_qp_init_attr *attr)
{
    return NULL;
}

static int query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
			 int attr_mask, struct ibv_qp_init_attr *init_attr)
{
    return 0;
}

static int modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
			  int attr_mask)
{
    return 0;
}

static int destroy_qp(struct ibv_qp *qp)
{
    return 0;
}

static int post_send(struct ibv_qp *qp, struct ibv_send_wr *wr,
			  struct ibv_send_wr **bad_wr)
{
    return 0;
}

static int post_recv(struct ibv_qp *qp, struct ibv_recv_wr *wr,
			  struct ibv_recv_wr **bad_wr)
{
    return 0;
}

static struct ibv_ah *create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
    return NULL;
}

static int destroy_ah(struct ibv_ah *ah)
{
    return 0;
}

static int attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid,
			      uint16_t lid)
{
    return 0;
}

static int detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid,
			      uint16_t lid)
{
    return 0;
}

static int async_event(struct ibv_async_event *event)
{
    return 0;
}

static void __attribute__((constructor)) init_dev_list(void)
{
    g_ibv_context_ops._compat_query_device = query_device;
    g_ibv_context_ops._compat_query_port = query_port;
    g_ibv_context_ops._compat_alloc_pd = (void *(*)(void))alloc_pd;
    g_ibv_context_ops._compat_dealloc_pd = (void *(*)(void))dealloc_pd;
    g_ibv_context_ops._compat_reg_mr = (void *(*)(void))reg_mr;
    g_ibv_context_ops._compat_rereg_mr = (void *(*)(void))rereg_mr;
    g_ibv_context_ops._compat_dereg_mr = (void *(*)(void))dereg_mr;
    g_ibv_context_ops.alloc_mw = alloc_mw;
    g_ibv_context_ops.bind_mw = bind_mw;
    g_ibv_context_ops.dealloc_mw = dealloc_mw;
    g_ibv_context_ops._compat_create_cq = (void *(*)(void))create_cq;
    g_ibv_context_ops.poll_cq = poll_cq;
    g_ibv_context_ops.req_notify_cq = req_notify_cq;
    g_ibv_context_ops._compat_cq_event = (void *(*)(void))cq_event;
    g_ibv_context_ops._compat_resize_cq = (void *(*)(void))resize_cq;
    g_ibv_context_ops._compat_destroy_cq = (void *(*)(void))destroy_cq;
    g_ibv_context_ops._compat_create_srq = (void *(*)(void))create_srq;
    g_ibv_context_ops._compat_modify_srq = (void *(*)(void))modify_srq;
    g_ibv_context_ops._compat_query_srq = (void *(*)(void))query_srq;
    g_ibv_context_ops._compat_destroy_srq = (void *(*)(void))destroy_srq;
    g_ibv_context_ops.post_srq_recv = post_srq_recv;
    g_ibv_context_ops._compat_create_qp = (void *(*)(void))create_qp;
    g_ibv_context_ops._compat_query_qp = (void *(*)(void))query_qp;
    g_ibv_context_ops._compat_modify_qp = (void *(*)(void))modify_qp;
    g_ibv_context_ops._compat_destroy_qp = (void *(*)(void))destroy_qp;
    g_ibv_context_ops.post_send = post_send;
    g_ibv_context_ops.post_recv = post_recv;
    g_ibv_context_ops._compat_create_ah = (void *(*)(void))create_ah;
    g_ibv_context_ops._compat_destroy_ah = (void *(*)(void))destroy_ah;
    g_ibv_context_ops._compat_attach_mcast = (void *(*)(void))attach_mcast;
    g_ibv_context_ops._compat_detach_mcast = (void *(*)(void))detach_mcast;
    g_ibv_context_ops._compat_async_event = (void *(*)(void))async_event;
}
int ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr)
{
    return 0;
};

#undef ibv_query_port
int ibv_query_port(struct ibv_context *context, uint8_t port_num,
		   struct _compat_ibv_port_attr *port_attr)
{
    if (!context || !port_attr || port_num == 0) {
        errno = EINVAL;
        return -1;
    }

    memset(port_attr, 0, sizeof(*port_attr));

    port_attr->state           = IBV_PORT_ACTIVE;          // 端口逻辑状态：已激活
    port_attr->max_mtu         = IBV_MTU_4096;             // 硬件支持最大MTU
    port_attr->active_mtu      = IBV_MTU_2048;             // 当前生效MTU
    port_attr->gid_tbl_len     = 128;                      // GID表长度（默认128）
    port_attr->port_cap_flags  = IBV_PORT_IP_BASED_GIDS;    // 端口能力标志
    port_attr->max_msg_sz      = 1073741824U;              // 最大消息大小(1GB)
    port_attr->bad_pkey_cntr   = 0;                        // 错误分区键计数器
    port_attr->qkey_viol_cntr  = 0;                        // QKey违规计数器
    port_attr->pkey_tbl_len    = 128;                      // 分区键表长度
    port_attr->lid             = 0x0001;                   // 本地标识符
    port_attr->sm_lid          = 0x0001;                   // 子网管理器LID
    port_attr->lmc             = 0;                        // 链路掩码计数
    port_attr->max_vl_num      = 1;                        // 最大虚拟通道数
    port_attr->sm_sl           = 0;                        // 子网管理器服务级别
    port_attr->subnet_timeout  = 0;                        // 子网超时参数
    port_attr->init_type_reply = 0;                        // 初始化类型响应
    port_attr->active_width    = 4;                        // 链路宽度：4x
    port_attr->active_speed    = 1;                        // 链路速率：100Gbps
    port_attr->phys_state      = 5;                        // 物理状态：已链接
    port_attr->link_layer      = IBV_LINK_LAYER_ETHERNET;   // 链路层：以太网
    port_attr->flags           = 0;                        // 扩展标志


    return 0;
}

int ibv_query_gid(struct ibv_context* context, uint8_t port_num, int index, union ibv_gid* gid)
{
    if (!context || !gid || port_num < 1) {
        errno = EINVAL;
        return -1;
    }

    uint32_t* p = (uint32_t*)gid;
    p[0] = 0;
    p[1] = 0;
    p[2] = htonl(0x0000FFFF);
    p[3] = 16777343;

    return 0;
}

struct ibv_pd* ibv_alloc_pd(struct ibv_context* context)
{
    if (!context) {
        errno = EINVAL;
        return NULL;
    }

    struct ibv_pd *pd = (struct ibv_pd *)malloc(sizeof(struct ibv_pd));
    if (!pd) {
        errno = ENOMEM;
        return NULL;
    }
    pd->context = context;
    return pd;
}

int ibv_dealloc_pd(struct ibv_pd* pd)
{
    free(pd);
    return 0;
};

struct ibv_cq* ibv_create_cq(struct ibv_context* context, int cqe, void* cq_context,
struct ibv_comp_channel* channel, int comp_vector)
{
    return NULL;
};

int ibv_destroy_cq(struct ibv_cq* cq)
{
    return 0;
};

struct ibv_comp_channel* ibv_create_comp_channel(struct ibv_context* context)
{
    // 1. 参数校验
    if (!context) {
        errno = EINVAL;
        return NULL;
    }

    // 2. 分配内存
    struct ibv_comp_channel* channel = (struct ibv_comp_channel*)malloc(sizeof(struct ibv_comp_channel));
    if (!channel) {
        errno = ENOMEM;
        return NULL;
    }

    // 3. 绑定设备上下文（核心，和你之前的写法完全一致）
    channel->context = context;

    // 4. 返回合法指针
    return channel;
};

int ibv_destroy_comp_channel(struct ibv_comp_channel* channel)
{
    free(channel);
    return 0;
};

struct ibv_srq* ibv_create_srq(struct ibv_pd *pd, struct ibv_srq_init_attr *srqInitAttr)
{
    return NULL;
};

int ibv_destroy_srq(struct ibv_srq* srq)
{
    return 0;
};

int ibv_query_gid_type(struct ibv_context* context, uint8_t port_num, unsigned int index,
    enum ibv_gid_type_sysfs* gid_type)
{
        *gid_type = IBV_GID_TYPE_SYSFS_ROCE_V2;
    return 0;
};

struct ibv_ah* ibv_create_ah(struct ibv_pd* pd, struct ibv_ah_attr* attr)
{
    return NULL;
};

int ibv_destroy_ah(struct ibv_ah* ah)
{
    return 0;
};

int ibv_query_qp(struct ibv_qp* qp, struct ibv_qp_attr* attr, int attr_mask, struct ibv_qp_init_attr* init_attr)
{
    return 0;
};

int ibv_get_cq_event(struct ibv_comp_channel* channel, struct ibv_cq** cq, void** cq_context)
{
    return 0;
};

void ibv_ack_cq_events(struct ibv_cq* cq, unsigned int num_events)
{
    return;
};

int ibv_modify_qp(struct ibv_qp* qp, struct ibv_qp_attr* attr, int attr_mask)
{
    return 0;
};

#undef ibv_reg_mr
struct ibv_mr* ibv_reg_mr(struct ibv_pd *pd, void *addr,size_t length, int access)
{
    // 1. 严格参数校验（真实驱动的校验逻辑）
    if (!pd || !addr || length == 0) {
        errno = EINVAL;  // 非法参数
        return NULL;
    }

    // 2. 分配 MR 结构体内存
    struct ibv_mr *mr = (struct ibv_mr *)malloc(sizeof(struct ibv_mr));
    if (!mr) {
        errno = ENOMEM;  // 内存不足
        return NULL;
    }

    // ===================== 核心：填充所有必选字段 =====================
    mr->context = pd->context;  // 继承PD的设备上下文（必须）
    mr->pd      = pd;           // 绑定归属的保护域PD（必须）
    mr->addr    = addr;         // 注册的内存起始地址
    mr->length  = length;       // 注册的内存长度
    
    // 模拟生成 lkey/rkey（RDMA 必备，随便给固定有效值即可）
    mr->lkey    = 0x11223344;
    mr->rkey    = 0x11223344;

    // 3. 返回合法的 MR 指针
    return mr;
};

int ibv_dereg_mr(struct ibv_mr* mr)
{
    free(mr);
    return 0;
};

struct ibv_qp* ibv_create_qp(struct ibv_pd* pd, struct ibv_qp_init_attr* init_attr)
{
    if (!pd || !init_attr) {
        errno = EINVAL;
        return NULL;
    }

    if (!init_attr->send_cq || !init_attr->recv_cq) {
        errno = EINVAL;
        return NULL;
    }

    struct ibv_qp* qp = (struct ibv_qp*)malloc(sizeof(struct ibv_qp));
    if (!qp) {
        errno = ENOMEM;
        return NULL;
    }

    // ===================== 核心：填充必选字段 =====================
    qp->context      = pd->context;       // 绑定设备上下文
    qp->pd           = pd;                // 绑定保护域
    qp->qp_type      = init_attr->qp_type;// QP类型(RC/UD/UC等)
    qp->send_cq      = init_attr->send_cq;// 绑定发送CQ
    qp->recv_cq      = init_attr->recv_cq;// 绑定接收CQ
    
    qp->qp_num       = 0x00012345;

    return qp;
};

int ibv_destroy_qp(struct ibv_qp* qp)
{
    free(qp);
    return 0;
};

struct ibv_device** ibv_get_device_list (int *num)
{
    static struct ibv_device **g_dev_list = NULL;
    if(g_dev_list == NULL) {
        g_dev_list = calloc(IBV_DEV_NUM_MAX + 1,sizeof(struct ibv_device *));
    }
    *num = IBV_DEV_NUM_MAX;
    for(int i = 0; i < IBV_DEV_NUM_MAX; i++) {
        if(g_dev_list[i] != NULL) continue;
        g_dev_list[i] = calloc(1,sizeof(struct ibv_device));
        g_dev_list[i]->node_type = IBV_NODE_RNIC;
        g_dev_list[i]->transport_type = IBV_TRANSPORT_IB;
        sprintf(g_dev_list[i]->dev_name, "uverbs%d", i);
        sprintf(g_dev_list[i]->name, "rdma%d", i);
        sprintf(g_dev_list[i]->dev_path, "/sys/class/infiniband_verbs/mock%d", i);
        sprintf(g_dev_list[i]->ibdev_path, "/sys/class/infiniband/mock%d", i);
    }
    return g_dev_list;
};

void ibv_free_device_list(struct ibv_device** list)
{
    if (!list) return;

    for (int i = 0; list[i] != NULL; i++) {
        free(list[i]);
    }
    free(list);
    return;
}

const char* ibv_get_device_name(struct ibv_device* device)
{
    return NULL;
};

int ibv_close_device(struct ibv_context* context)
{
    free(context);
    return 0;
};

struct ibv_context* ibv_open_device(struct ibv_device* device)
{
    struct ibv_context *ctx = calloc(1, sizeof(struct ibv_context));
    ctx->device = device;
    ctx->ops = g_ibv_context_ops;
    return ctx;
}

const char* ibv_wc_status_str(enum ibv_wc_status status)
{
    return NULL;
};
