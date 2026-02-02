#define _GNU_SOURCE
#define SOCK_CONN_TAG_SIZE 192
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ifaddrs.h>

#include "ut_dispatch.h"
#include "dl_ccu_function.h"
#include "dl_net_function.h"
#include "dl_hal_function.h"
#include "rs.h"
#include "rs_ub.h"
#include "rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "tc_ut_rs_ub.h"
#include "ra_rs_err.h"

extern int rs_ccu_jetty_db_reg(struct rs_ctx_jetty_cb *jettyCb);
extern int rs_set_jetty_opt(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg);
extern int rs_get_jetty_opt(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg);
extern int rs_jetty_attr_init(struct rs_ctx_jetty_cb *jettyCb, urma_jetty_cfg_t *jettyCfg);
extern int rs_mmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb);
extern void rs_munmap_jetty_va(struct rs_ctx_jetty_cb *jettyCb);
extern void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg);
extern uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id);
extern uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id);
extern void RsEpollCtl(int epollfd, int op, int fd, int state);

void tc_rs_ub_ctx_ext_jetty_create()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_jetty_cfg_t jetty_cfg = {0};
    urma_device_t urma_dev = {0};
    struct rs_cb rscb = {0};

    dev_cb.rscb = &rscb;
    dev_cb.urma_dev = &urma_dev;
    jetty_cb.dev_cb = &dev_cb;

    // stars jetty: rs_jetty_attr_init fail
    jetty_cb.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
    mocker(rs_jetty_attr_init, 1, -1);
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    mocker_clean();

    // stars jetty: rs_set_jetty_opt fail
    mocker(rs_set_jetty_opt, 1, -1);
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    mocker_clean();

    // stars jetty: rs_get_jetty_opt fail
    mocker(rs_get_jetty_opt, 1, -1);
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    mocker_clean();

    // stars jetty: rs_ccu_jetty_db_reg fail
    mocker(rs_ccu_jetty_db_reg, 1, -1);
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    mocker_clean();

    // user ctl normal
    mocker(rs_mmap_jetty_va, 10, 0);
    mocker(rs_munmap_jetty_va, 10, 0);
    jetty_cb.jetty_mode = JETTY_MODE_USER_CTL_NORMAL;
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    EXPECT_INT_NE(NULL, jetty_cb.jetty);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    mocker_clean();
}

void tc_rs_ub_ctx_ext_jetty_delete()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jetty_t jetty = {0};
    struct rs_cb rscb = {0};

    dev_cb.urma_dev = &urma_dev;
    dev_cb.rscb = &rscb;
    jetty_cb.dev_cb = &dev_cb;
    jetty_cb.jetty = &jetty;

    // fail
    jetty_cb.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
    mocker(rs_net_free_jetty_id, 1, -1);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    mocker_clean();
}

// void tc_rs_ub_stars_va_munmap_batch()
// {
//     struct rs_ctx_jetty_cb *jettyCbArr[2];
//     struct rs_ctx_jetty_cb jettycb0 = {0};
//     struct rs_ctx_jetty_cb jettycb1 = {0};
//     struct rs_ub_dev_cb dev_cb = {0};
//     urma_device_t urma_dev = {0};
//     struct rs_cb rscb = {0};
//     unsigned int num = 2;

//     dev_cb.urma_dev = &urma_dev;
//     dev_cb.rscb = &rscb;
//     jettycb0.dev_cb = &dev_cb;
//     jettycb1.dev_cb = &dev_cb;
//     jettycb0.jetty_mode = JETTY_MODE_CCU;
//     jettycb1.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
//     jettyCbArr[0] = &jettycb0;
//     jettyCbArr[1] = &jettycb1;

//     // rs_net_free_jetty_id fail
//     mocker(rs_net_free_jetty_id, 2, -1);
//     mocker(rs_mmap_jetty_va, 10, 0);
//     mocker(rs_munmap_jetty_va, 10, 0);
//     rs_ub_stars_va_munmap_batch(jettyCbArr, num);

//     // success
//     rs_ub_stars_va_munmap_batch(jettyCbArr, num);
// }

int rs_ccu_get_cqe_base_addr_stub_succ(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    *cqe_base_addr = 1;
    return 0;
}

int rs_ccu_get_cqe_base_addr_stub_unsucc(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    *cqe_base_addr = 0;
    return -19;
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_succ()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_succ, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(0, ret);
    free(out_jfc);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_fail_jfc()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_succ, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_fail_addr()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_unsucc, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_fail_id()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_succ, 1);
    mocker(rs_net_alloc_jfc_id, 1, -28);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-28, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_fail_set()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();


    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_succ, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-22, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu_fail_active()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_CCU_POLL;
    mocker_invoke(rs_ccu_get_cqe_base_addr, rs_ccu_get_cqe_base_addr_stub_succ, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-19, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_ccu()
{
    tc_rs_ub_ctx_jfc_create_ext_ccu_succ();
    tc_rs_ub_ctx_jfc_create_ext_ccu_fail_jfc();
    tc_rs_ub_ctx_jfc_create_ext_ccu_fail_addr();
    tc_rs_ub_ctx_jfc_create_ext_ccu_fail_id();
    tc_rs_ub_ctx_jfc_create_ext_ccu_fail_set();
    tc_rs_ub_ctx_jfc_create_ext_ccu_fail_active();
}

int rs_net_get_cqe_base_addr_stub_succ(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    *cqe_base_addr = 1;
    return 0;
}

int rs_net_get_cqe_base_addr_stub_unsucc(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    *cqe_base_addr = 0;
    return -19;
}

void tc_rs_ub_ctx_jfc_create_ext_stars_succ()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();
    jfc_cb.jfc_type = JFC_MODE_STARS_POLL;
    mocker_invoke(rs_net_get_cqe_base_addr, rs_net_get_cqe_base_addr_stub_succ, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(0, ret);
    free(out_jfc);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_stars_fail_addr()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    mocker_clean();

    jfc_cb.jfc_type = JFC_MODE_STARS_POLL;
    mocker_invoke(rs_net_get_cqe_base_addr, rs_net_get_cqe_base_addr_stub_unsucc, 1);
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_jfc_create_ext_stars()
{
    tc_rs_ub_ctx_jfc_create_ext_stars_succ();
    tc_rs_ub_ctx_jfc_create_ext_stars_fail_addr();
}

void tc_rs_ub_ctx_jfc_create_ext()
{
    tc_rs_ub_ctx_jfc_create_ext_ccu();
    tc_rs_ub_ctx_jfc_create_ext_stars();
}

// void tc_rs_ub_delete_jfc_ext()
// {
//     struct rs_ctx_jfc_cb jfc_cb = {0};
//     struct rs_ub_dev_cb dev_cb = {0};
//     urma_device_t urma_dev = {0};
//     urma_jfc_t jfc = {0};
//     int ret = 0;

//     dev_cb.urma_dev = &urma_dev;
//     jfc_cb.dev_cb = &dev_cb;
//     jfc_cb.jfc_addr = (uint64_t)(uintptr_t)&jfc;
//     mocker_clean();

//     ret = rs_ub_delete_jfc_ext(&dev_cb, &jfc_cb);
//     EXPECT_INT_EQ(0, ret);
//     mocker_clean();

//     mocker(rs_net_free_jfc_id, 1 , -22);
//     ret = rs_ub_delete_jfc_ext(&dev_cb, &jfc_cb);
//     EXPECT_INT_EQ(-22, ret);
//     mocker_clean();
// }
