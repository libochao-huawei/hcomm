#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <infiniband/verbs.h>
#include "verbs_exp.h"
#include "hns_roce_u_cmd.h"

struct ibv_mr* ibv_exp_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
    int access, struct roce_process_sign roceSign)
{
    struct ibv_mr *mr = calloc(1, sizeof(struct ibv_mr));
    if (!mr) {
        return NULL;
    }

    // 2. 初始化标准字段（绑定输入参数，模拟真实行为）
    mr->pd = pd;          // 绑定 PD
    mr->addr = addr;      // 绑定用户态地址
    mr->length = length;  // 绑定长度
    mr->lkey = 0x12345678;// 模拟本地密钥（打桩随便填）
    mr->rkey = 0x87654321;// 模拟远程密钥（打桩随便填）

    return mr;
}


int ibv_exp_query_notify(struct ibv_context *context,
    unsigned long long *notifyVa, unsigned long long *size)
{
    return 0;
}

int ibv_exp_post_send(struct ibv_qp *qp, struct ibv_send_wr *wr,
    struct ibv_send_wr **badWr, struct wr_exp_rsp *expRsp)
{
    return 0;
}

int ibv_exp_query_device(struct ibv_context*, struct dev_cap_info*)
{
    return 0;
}

struct ibv_ah* ibv_exp_create_ah(struct ibv_pd *pd, struct ibv_exp_ah_attr *attrx)
{
    return NULL;
}

struct ibv_qp *ibv_exp_create_qp(struct ibv_pd *pd, struct ibv_exp_qp_init_attr *qp_init_attr,
                                 struct rdma_lite_device_qp_attr *qp_resp)
{
    return NULL;
}

struct ibv_cq* ibv_exp_create_cq(
        struct ibv_context* context,
        int cqe,
        void* cq_context,
        struct ibv_comp_channel* comp_channel,
        int comp_vector,
        struct rdma_lite_device_cq_init_attr* init_attr,
        struct rdma_lite_device_cq_attr* cq_attr)
{
    if (!context || cqe <= 0) {
        errno = EINVAL;
        return NULL;
    }

    struct ibv_cq* cq = (struct ibv_cq*)malloc(sizeof(struct ibv_cq));
    if (!cq) {
        errno = ENOMEM;
        return NULL;
    }

    cq->context       = context;
    cq->cqe           = cqe;
    cq->cq_context    = cq_context;
    cq->channel       = comp_channel;

    return cq;
}

int roce_get_roce_dev_data(const char *devName, struct roce_dev_data *rdevData)
{
    return 0;
}

int roce_get_tsqp_depth(const char *devName, unsigned int rdevIndex,
    unsigned int *tempDepth, unsigned int *qpNum, unsigned int *sqDepth)
{
    return 0;
}

int roce_set_tsqp_depth(const char *devName, unsigned int rdevIndex,
    unsigned int tempDepth, unsigned int *qpNum, unsigned int *sqDepth)
{
    return 0;
}

int roce_init_mem_pool(const struct roce_mem_cq_qp_attr *attr,
    struct rdma_lite_device_mem_attr *memAttr, unsigned int num)
{
    return 0;
}

int roce_deinit_mem_pool(unsigned int memIdx)
{
    return 0;
}

int roce_query_qpc(struct ibv_qp *qp, struct hns_roce_qpc_attr_val *attrVal,
    unsigned int attrMask)
{
    return 0;
}

int roce_mmap_ai_db_reg(struct ibv_context *context, unsigned int tgid)
{
    return 0;
}

int roce_unmmap_ai_db_reg(struct ibv_context *context)
{
    return 0;
}

int roce_get_cq_data_plane_info(struct ibv_cq *cq,
    struct hns_roce_cq_data_plane_info *info)
{
    return 0;
}

int roce_get_qp_data_plane_info(struct ibv_qp *qp,
    struct hns_roce_qp_data_plane_info *info)
{
    return 0;
}

int roce_remap_mr(struct ibv_mr *mr, struct hns_roce_mr_remap_info info[],
    unsigned int num)
{
    return 0;
}

unsigned int roce_get_api_version(void)
{
    return 0;
}