#include "hccp_ctx.h"
#include "ra_rs_comm.h"
#include "ra_rs_ctx.h"
#include "ra_hdc_ctx.h"

int rs_get_dev_eid_info_num(unsigned int phy_id, unsigned int *num)
{
    return 0;
}

int rs_get_dev_eid_info_list(unsigned int phy_id, struct dev_eid_info info_list[], unsigned int start_index,
    unsigned int count)
{
    return 0;
}

int rs_ctx_init(struct ctx_init_attr *attr, unsigned int *dev_index, struct dev_base_attr *dev_attr)
{
    return 0;
}

int rs_ctx_deinit(struct ra_rs_dev_info *dev_info)
{
    return 0;
}

int rs_ctx_token_id_alloc(struct ra_rs_dev_info *dev_info, unsigned long long *addr, unsigned int *token_id)
{
    return 0;
}

int rs_ctx_token_id_free(struct ra_rs_dev_info *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_lmem_reg(struct ra_rs_dev_info *dev_info, struct mem_reg_attr_t *mem_attr, struct mem_reg_info_t *mem_info)
{
    return 0;
}

int rs_ctx_lmem_unreg(struct ra_rs_dev_info *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_rmem_import(struct ra_rs_dev_info *dev_info, struct mem_import_attr_t *mem_attr,
    struct mem_import_info_t *mem_info)
{
    return 0;
}

int rs_ctx_rmem_unimport(struct ra_rs_dev_info *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_chan_create(struct ra_rs_dev_info *dev_info, unsigned long long *addr)
{
    return 0;
}

int rs_ctx_chan_destroy(struct ra_rs_dev_info *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_cq_create(struct ra_rs_dev_info *dev_info, struct ctx_cq_attr *attr,
    struct ctx_cq_info *info)
{
    return 0;
}

int rs_ctx_cq_destroy(struct ra_rs_dev_info *dev_info, unsigned long long addr)
{
    return 0;
}

int rs_ctx_qp_create(struct ra_rs_dev_info *dev_info, struct ctx_qp_attr *qp_attr,
    struct qp_create_info *qp_info)
{
    return 0;
}

int rs_ctx_qp_destroy(struct ra_rs_dev_info *dev_info, unsigned int id)
{
    return 0;
}

int rs_ctx_qp_import(struct ra_rs_dev_info *dev_info, struct rs_jetty_import_attr *import_attr,
    struct rs_jetty_import_info *import_info)
{
    return 0;
}

int rs_ctx_qp_unimport(struct ra_rs_dev_info *dev_info, unsigned int rem_jetty_id)
{
    return 0;
}

int rs_ctx_qp_bind(struct ra_rs_dev_info *dev_info, struct rs_ctx_qp_info *local_qp_info,
    struct rs_ctx_qp_info *remote_qp_info)
{
    return 0;
}

int rs_ctx_qp_unbind(struct ra_rs_dev_info *dev_info, unsigned int qp_id)
{
    return 0;
}

int rs_ctx_batch_send_wr(struct wrlist_base_info *base_info, struct batch_send_wr_data *wr_data,
    struct send_wr_resp *wr_resp, struct wrlist_send_complete_num *wrlist_num)
{
    return 0;
}

int rs_ctx_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return 0;
}

int rs_ctx_update_ci(struct ra_rs_dev_info *dev_info, unsigned int qp_id, uint16_t ci)
{
    return 0;
}

int rs_get_tp_info_list(struct ra_rs_dev_info *dev_info, struct get_tp_cfg *cfg,
    struct tp_info info_list[], unsigned int *num)
{
    return 0;
}
