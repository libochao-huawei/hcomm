#include "udma_u_ctl.h"
#include "rs_ctx_inner.h"
#include "rs_ub.h"

int rs_set_jetty_opt(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    return 0;
}
int rs_get_jetty_opt(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    return 0;
}

int rs_ccu_jetty_db_reg(struct rs_ctx_jetty_cb *jettyCb)
{
    return 0;
}

int rs_mmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb)
{
    return 0;
}

void rs_munmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb)
{
    return;
}

int rs_jetty_attr_init(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg)
{
    return 0;
}

int RsEpollCtl(int epollfd, int op, int fd, unsigned int state)
{
    return 0;
}

int dlHalBuffFree(void *buff)
{
    return 0;
}

void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg)
{
    return;
}

void rs_ub_ctx_ext_jetty_delete(struct rs_ctx_jetty_cb *jetty_cb)
{
    return;
}

int rs_ub_ctx_jfc_create_ext(struct rs_ctx_jfc_cb *ctx_jfc_cb, urma_jfc_cfg_t jfc_cfg, urma_jfc_t **jfc)
{
    return 0;
}