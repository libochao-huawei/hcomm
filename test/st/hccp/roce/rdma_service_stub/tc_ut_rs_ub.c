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
#include "rs_ub_jetty.h"
#include "rs_ub_jfc.h"
#include "tc_ut_rs_ub.h"
#include "ra_rs_err.h"

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

    jetty_cb.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
}

void tc_rs_ub_ctx_ext_jetty_delete()
{
    struct rs_ctx_jetty_cb jetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    struct rs_cb rscb = {0};

    dev_cb.urma_dev = &urma_dev;
    dev_cb.rscb = &rscb;
    jetty_cb.dev_cb = &dev_cb;

    jetty_cb.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
}

void tc_rs_ub_stars_va_munmap_batch()
{
    struct rs_ctx_jetty_cb *jettyCbArr[2];
    struct rs_ctx_jetty_cb jettycb0 = {0};
    struct rs_ctx_jetty_cb jettycb1 = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    struct rs_cb rscb = {0};
    unsigned int num = 2;

    dev_cb.urma_dev = &urma_dev;
    dev_cb.rscb = &rscb;
    jettycb0.dev_cb = &dev_cb;
    jettycb1.dev_cb = &dev_cb;
    jettycb0.jetty_mode = JETTY_MODE_CCU;
    jettycb1.jetty_mode = JETTY_MODE_CACHE_LOCK_DWQE;
    jettyCbArr[0] = &jettycb0;
    jettyCbArr[1] = &jettycb1;

    rs_ub_stars_va_munmap_batch(jettyCbArr, num);
}

void tc_rs_ub_ctx_jfc_create_ext()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_cfg_t jfc_cfg = {0};
    urma_jfc_t *out_jfc = NULL;
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    ret = rs_ub_ctx_jfc_create_ext(&jfc_cb, jfc_cfg, &out_jfc);
    EXPECT_INT_EQ(-259, ret);
}

void tc_rs_ub_delete_jfc_ext()
{
    struct rs_ctx_jfc_cb jfc_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_device_t urma_dev = {0};
    urma_jfc_t jfc = {0};
    int ret = 0;

    dev_cb.urma_dev = &urma_dev;
    jfc_cb.dev_cb = &dev_cb;
    jfc_cb.jfc_addr = (uint64_t)(uintptr_t)&jfc;
    ret = rs_ub_delete_jfc_ext(&dev_cb, &jfc_cb);
    EXPECT_INT_EQ(-259, ret);
}
