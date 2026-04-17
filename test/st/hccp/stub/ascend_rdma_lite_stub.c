#include <stddef.h>
#include <stdint.h>
#include "rdma_lite.h"

struct rdma_lite_context* rdma_lite_alloc_context(uint8_t phyId, struct dev_cap_info *cap)
{
    return NULL;
}

void rdma_lite_free_context(struct rdma_lite_context *liteCtx)
{
    return;
}

int rdma_lite_init_mem_pool(struct rdma_lite_context *liteCtx,
    struct rdma_lite_mem_attr *liteMemAttr)
{
    return 0;
}

int rdma_lite_deinit_mem_pool(struct rdma_lite_context *liteCtx, uint32_t memIdx)
{
    return 0;
}

struct rdma_lite_cq* rdma_lite_create_cq(struct rdma_lite_context *liteCtx,
    struct rdma_lite_cq_attr *liteCqAttr)
{
    return NULL;
}

int rdma_lite_destroy_cq(struct rdma_lite_cq *liteCq)
{
    return 0;
}

struct rdma_lite_qp* rdma_lite_create_qp(struct rdma_lite_context *liteCtx,
    struct rdma_lite_qp_attr *liteQpAttr)
{
    return NULL;
}

int rdma_lite_destroy_qp(struct rdma_lite_qp *liteQp)
{
    return 0;
}

int rdma_lite_set_qp_sl(struct rdma_lite_qp *liteQp, int sl)
{
    return 0;
}

int rdma_lite_clean_qp(struct rdma_lite_qp *liteQp)
{
    return 0;
}

int rdma_lite_restore_snapshot(struct rdma_lite_context *liteCtx)
{
    return 0;
}

unsigned int rdma_lite_get_api_version(void)
{
    return 0;
}