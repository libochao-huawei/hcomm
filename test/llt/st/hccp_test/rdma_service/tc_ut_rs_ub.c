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
#include "stub/ibverbs.h"
#include "rs.h"
#include "rs_ub.h"
#include "rs_ccu.h"
#include "rs_esched.h"
#include "rs_ctx.h"
#include "rs_inner.h"
#include "rs_ctx_inner.h"
#include "tc_ut_rs_ub.h"
#include "rs_drv_rdma.h"
#include "stub/verbs_exp.h"
#include "tls.h"
#include "encrypt.h"
#include "kmc_crypt.h"
#include "rs_epoll.h"
#include "rs_socket.h"
#include "ra_rs_err.h"

extern int rs_ub_get_rdev_cb(struct rs_cb *rs_cb, unsigned int rdev_index, struct rs_ub_dev_cb **dev_cb);
extern int rs_urma_device_api_init(void);
extern int rs_open_urma_so(void);
extern int rs_urma_jetty_api_init(void);
extern int rs_urma_jfc_api_init(void);
extern int rs_urma_segment_api_init(void);
extern int rs_urma_data_api_init(void);
extern urma_device_t **rs_urma_get_device_list(int *num_devices);
extern urma_eid_info_t *rs_urma_get_eid_list(urma_device_t *dev, uint32_t *cnt);
extern void rs_urma_free_device_list(urma_device_t **device_list);
extern void rs_urma_free_eid_list(urma_eid_info_t *eid_list);
extern void rs_ub_ctx_ext_jetty_create(struct rs_ctx_jetty_cb *jetty_cb, urma_jetty_cfg_t *jetty_cfg);
// new p00951995
extern int rs_ub_ctx_reg_jetty_db(struct rs_ctx_jetty_cb *jetty_cb, struct udma_u_jetty_info *jetty_info);
extern int rs_init_rscb_cfg(struct rs_cb *rscb, struct rs_init_config *cfg);
extern int rs_ub_create_ctx(urma_device_t *urma_dev, unsigned int eid_index, urma_context_t **urma_ctx);
extern int rs_ub_get_ue_info(urma_context_t *urma_ctx, struct dev_base_attr *dev_base_attr);
extern uint32_t rs_generate_ue_info(uint32_t die_id, uint32_t func_id);
extern uint32_t rs_generate_dev_index(uint32_t dev_cnt, uint32_t die_id, uint32_t func_id);
char g_rev_buf[RS_BUF_SIZE] = {0};
struct rs_cb stub_rs_cb;
struct rs_ub_dev_cb stub_dev_cb;

int stub_rs_get_rs_cb_v1(unsigned int phy_id, struct rs_cb **rs_cb)
{
    *rs_cb = &stub_rs_cb;
    return 0;
}

int stub_rs_ub_get_dev_cb(struct rs_cb *rscb, unsigned int dev_index, struct rs_ub_dev_cb **dev_cb)
{
    stub_dev_cb.rscb = &stub_rs_cb;
    *dev_cb = &stub_dev_cb;
    return 0;
}

void tc_rs_ub_get_rdev_cb()
{
    struct rs_ub_dev_cb *rdev_cb_out;
    struct rs_ub_dev_cb rdev_cb = {0};
    unsigned int rdev_index = 0;
    struct rs_cb rs_cb;
    int ret;

    RS_INIT_LIST_HEAD(&rs_cb.rdev_list);
    rs_list_add_tail(&rdev_cb.list, &rs_cb.rdev_list);

    ret = rs_ub_get_dev_cb(&rs_cb, rdev_index, &rdev_cb_out);
    EXPECT_INT_EQ(ret, 0);

    rdev_index = 1;
    ret = rs_ub_get_dev_cb(&rs_cb, rdev_index, &rdev_cb_out);
    EXPECT_INT_EQ(ret, -ENODEV);

    return;
}

void tc_rs_urma_api_init_abnormal()
{
    int ret;

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, 0);
    mocker(rs_urma_segment_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    mocker(rs_open_urma_so, 100, 0);
    mocker(rs_urma_device_api_init, 100, 0);
    mocker(rs_urma_jetty_api_init, 100, 0);
    mocker(rs_urma_jfc_api_init, 100, 0);
    mocker(rs_urma_segment_api_init, 100, 0);
    mocker(rs_urma_data_api_init, 100, -1);
    ret = rs_urma_api_init();
    EXPECT_INT_EQ(ret, -1);
    mocker_clean();

    return;
}

void tc_rs_ub_v2()
{
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct dev_base_attr attr = {0};
    struct rs_init_config cfg = {0};
    struct ctx_init_attr info = {0};
    struct rs_cb rscb = {0};
    unsigned long long token_id_addr = 0;
    unsigned int token_id_num = 0;
    unsigned int dev_index;
    int ret = 0;

    struct mem_reg_attr_t lmem_attr = {0};
    struct mem_reg_info_t lmem_info = {0};
    struct mem_import_attr_t rmem_attr = {0};
    struct mem_import_info_t rmem_info = {0};
    void *addr = malloc(1);
    lmem_attr.mem.addr = (uintptr_t)addr;
    lmem_attr.mem.size = 1;
    lmem_attr.ub.flags.bs.token_id_valid = 1;

    cfg.chip_id = 0;
    ret = rs_init(&cfg);
    EXPECT_INT_EQ(ret, 0);

    RS_INIT_LIST_HEAD(&rscb.rdev_list);
    ret = rs_ub_ctx_init(&rscb, &info, &dev_index, &attr);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_get_dev_cb(&rscb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);
    lmem_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_lmem_reg(dev_cb, &lmem_attr, &lmem_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &rmem_attr, &rmem_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_unimport(dev_cb, rmem_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_unreg(dev_cb, lmem_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_deinit(dev_cb);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;

    ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
}

urma_device_t tc_urma_dev = {0};
urma_device_t *tc_urma_device_list[1] = {&tc_urma_dev};
urma_eid_info_t tc_urma_eid_list[1] = {0};
urma_eid_info_t tc_urma_eid_list2[2] = {0};

urma_device_t **tc_rs_urma_get_device_list_stub(int *num_devices)
{
    *num_devices = 1;
    return tc_urma_device_list;
}

void tc_rs_ub_get_dev_eid_info_num()
{
    int ret;
    unsigned int phy_id;
    unsigned int num;
 
    phy_id = RS_MAX_DEV_NUM;
    ret = rs_ub_get_dev_eid_info_num(phy_id, &num);
    EXPECT_INT_EQ(0, ret);
 
    mocker(rs_urma_get_device_list, 10, NULL);
    ret = rs_ub_get_dev_eid_info_num(phy_id, &num);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
 
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, NULL);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_num(phy_id, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
 
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, tc_urma_eid_list);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_num(phy_id, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

int rs_ub_get_dev_eid_info_num_stub(unsigned int phy_id, unsigned int *num)
{
    *num = 1;
    return 0;
}

urma_eid_info_t *rs_urma_get_eid_list_stub(urma_device_t *dev, uint32_t *cnt)
{
    *cnt = 1;
    return tc_urma_eid_list;
}

urma_eid_info_t *rs_urma_get_eid_list_stub2(urma_device_t *dev, uint32_t *cnt)
{
    *cnt = 2;
    return tc_urma_eid_list2;
}

void tc_rs_ub_get_dev_eid_info_list()
{
    int ret;
    unsigned int phy_id;
    unsigned int start_index;
    unsigned int count;
    struct dev_eid_info info_list[1] = {0};
    
    phy_id = 0;
    start_index = 0;
    count = 1;
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_EQ(0, ret);

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker(rs_urma_get_device_list, 10, NULL);
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_urma_get_eid_list, 10, NULL);
    mocker(rs_urma_free_device_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, 0);
    mocker(rs_ub_get_ue_info, 10, 0);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub2, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    mocker(rs_urma_delete_context, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, -ENODEV);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_EQ(-ENODEV, ret);
    mocker_clean();
 
    mocker_invoke(rs_ub_get_dev_eid_info_num, rs_ub_get_dev_eid_info_num_stub, 10);
    mocker_invoke(rs_urma_get_device_list, tc_rs_urma_get_device_list_stub, 10);
    mocker(rs_ub_create_ctx, 10, 0);
    mocker(rs_ub_get_ue_info, 10, -259);
    mocker_invoke(rs_urma_get_eid_list, rs_urma_get_eid_list_stub, 10);
    mocker(rs_urma_free_device_list, 10, 0);
    mocker(rs_urma_free_eid_list, 10, 0);
    mocker(rs_urma_delete_context, 10, 0);
    ret = rs_ub_get_dev_eid_info_list(phy_id, info_list, start_index, count);
    EXPECT_INT_EQ(-259, ret);
    mocker_clean();
}

struct rs_cb *tc_rs_ub_v2_init(int mode, unsigned int *dev_index)
{
    int ret;
    struct dev_base_attr attr = {0};
    struct rs_init_config cfg = {0};
    struct ctx_init_attr info = {0};
    struct rs_cb *rs_cb;
    cfg.hccp_mode = mode;

    ret = rs_init(&cfg);
    EXPECT_INT_EQ(ret, 0);
    ret = rs_get_rs_cb(0, &rs_cb);
    EXPECT_INT_EQ(ret, 0);
    RS_INIT_LIST_HEAD(&rs_cb->rdev_list);

    ret = rs_ub_ctx_init(rs_cb, &info, dev_index, &attr);
    EXPECT_INT_EQ(0, ret);

    return rs_cb;
}

void tc_rs_ub_v2_deinit(struct rs_cb *rs_cb, int mode, unsigned int dev_index)
{
    int ret;
    struct rs_init_config cfg = {0};
    cfg.hccp_mode = mode;
    struct rs_ub_dev_cb *dev_cb = NULL;

    ret = rs_ub_get_dev_cb(rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_deinit(dev_cb);
    EXPECT_INT_EQ(ret, 0);
    ret = rs_deinit(&cfg);
	EXPECT_INT_EQ(ret, 0);
    mocker_clean();
}

void tc_rs_ub_ctx_token_id_alloc()
{
    tc_rs_ub_ctx_token_id_alloc1();
    tc_rs_ub_ctx_token_id_alloc2();
    tc_rs_ub_ctx_token_id_alloc3();
}

void tc_rs_ub_ctx_token_id_alloc1()
{
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_token_id_alloc2()
{
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    mocker(rs_urma_alloc_token_id, 10, NULL);
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_NE(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_token_id_alloc3()
{
    unsigned long long addr = 0;
    unsigned long long addr1 = 0;
    unsigned int dev_index = 0;
    unsigned int token_id = 0;
    unsigned int token_id1 = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    int ret;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr, &token_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &addr1, &token_id1);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr1);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_free(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jfce_create()
{
    unsigned long long addr = 0;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    int ret;
    struct rs_ub_dev_cb *dev_cb = NULL;

    mocker_clean();
    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_chan_create(dev_cb, &addr);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_chan_destroy(dev_cb, addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jfc_create()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_cq_attr attr = {0};
    struct ctx_cq_info info = {0};
    struct rs_ub_dev_cb *dev_cb = NULL;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    attr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, info.addr);
    EXPECT_INT_EQ(0, ret);

    // new case
    attr.ub.mode = JFC_MODE_CCU_POLL;
    attr.ub.ccu_ex_cfg.valid = 1;
    
    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, info.addr);
    EXPECT_INT_EQ(0, ret);

    attr.ub.mode = JFC_MODE_MAX;
    ret = rs_ub_ctx_jfc_create(dev_cb, &attr, &info);
    EXPECT_INT_EQ(-EINVAL, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jetty_create()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qp_attr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_ub_dev_cb *dev_cb = NULL;
    unsigned long long token_id_addr = 0;
    unsigned int token_id_num = 0;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    qp_attr.ub.mode = JETTY_MODE_CCU;
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);
    qp_attr.ub.token_id_addr = token_id_addr;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    // new case
    cq_attr.ub.mode = JFC_MODE_CCU_POLL;
    cq_attr.ub.ccu_ex_cfg.valid = 1;
    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    qp_attr.ub.mode = JETTY_MODE_CCU_TA_CACHE;
    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id_num);
    EXPECT_INT_EQ(0, ret);
    qp_attr.ub.token_id_addr = token_id_addr;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    qp_attr.ub.mode = JETTY_MODE_MAX;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(-EINVAL, ret);

    qp_attr.ub.mode = JETTY_MODE_CCU_TA_CACHE;
    qp_attr.ub.ta_cache_mode.lock_flag = 0;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(-EINVAL, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jetty_import()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct ctx_qp_attr qp_attr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_jetty_bind()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct rs_ub_dev_cb *dev_cb = NULL;
    struct ctx_qp_attr qp_attr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = JFC_MODE_STARS_POLL;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    // abnormal case
    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    // abnormal case
    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_NE(0, ret);

    // abnormal case
    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    // abnormal case
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);

    // new case
    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = 10000;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_NE(0, ret);

    cq_attr.ub.mode = JFC_MODE_STARS_POLL;
    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    qp_attr.ub.mode = JFC_MODE_STARS_POLL;
    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_NE(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    struct rs_ctx_jetty_cb jetty_cb = {0};
    jetty_cb.dev_cb = dev_cb;
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);

    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);

}

void tc_rs_ub_ctx_batch_send_wr()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qp_attr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};
    struct wrlist_base_info base_info = {0};
    struct batch_send_wr_data wr_data_nop[1] = {0};
    struct batch_send_wr_data wr_data[1] = {0};
    struct send_wr_resp wr_resp[1] = {0};
    struct wrlist_send_complete_num wrlist_num= {0};
    struct mem_reg_attr_t mem_reg_attr = {0};
    struct mem_reg_info_t mem_reg_info = {0};
    struct mem_import_attr_t mem_import_attr = {0};
    struct mem_import_info_t mem_import_info = {0};
    unsigned int complete_num = 0;
    unsigned long long token_id_addr = 0;
    unsigned int token_id = 0;
    struct rs_ub_dev_cb *dev_cb = NULL;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = JFC_MODE_STARS_POLL;
    wrlist_num.send_num = 1;
    wrlist_num.complete_num = &complete_num;
    void *addr = malloc(1);
    mem_reg_attr.mem.addr = (uintptr_t)addr;
    mem_reg_attr.mem.size = 1;
    mem_reg_attr.ub.flags.bs.token_id_valid = 1;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id);
    EXPECT_INT_EQ(0, ret);
    mem_reg_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_lmem_reg(dev_cb, &mem_reg_attr, &mem_reg_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &mem_import_attr, &mem_import_info);
    EXPECT_INT_EQ(0, ret);

    wr_data[0].dev_rmem_handle = mem_import_info.ub.target_seg_handle;
    wr_data[0].num_sge = 1;
    wr_data[0].sges[0].addr = addr;
    wr_data[0].sges[0].len = 1;
    wr_data[0].sges[0].dev_lmem_handle = mem_reg_info.ub.target_seg_handle;

    base_info.dev_index = dev_index;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data, wr_resp, &wrlist_num);
    EXPECT_INT_EQ(0, ret);

    wr_data[0].ub.rem_jetty = 0xfffff;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data, wr_resp, &wrlist_num);
    EXPECT_INT_NE(0, ret);

    wr_data_nop[0].ub.opcode = RA_UB_OPC_NOP;
    ret = rs_ub_ctx_batch_send_wr(tc_rs_cb, &base_info, wr_data_nop, wr_resp, &wrlist_num);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_unimport(dev_cb, mem_import_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_update_ci(dev_cb, 10000, 0);
    EXPECT_INT_NE(0, ret);
 
    ret = rs_ub_ctx_jetty_update_ci(dev_cb, 0, 0);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_unreg(dev_cb, mem_reg_info.ub.target_seg_handle);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unbind(dev_cb, rs_qp_info.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_unimport(dev_cb, import_data.rem_jetty_id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_destroy(dev_cb, qp_info.ub.id);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jfc_destroy(dev_cb, cq_info.addr);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;
    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_free_cb_list()
{
    int ret;
    unsigned int dev_index = 0;
    struct rs_cb *tc_rs_cb;
    struct ctx_qp_attr qp_attr = {0};
    struct qp_create_info qp_info = {0};
    struct ctx_cq_attr cq_attr = {0};
    struct ctx_cq_info cq_info = {0};
    struct rs_jetty_import_attr import_attr = {0};
    struct rs_jetty_import_info import_data = {0};
    struct rs_ctx_qp_info rs_qp_info = {0};
    struct mem_reg_attr_t mem_reg_attr = {0};
    struct mem_reg_info_t mem_reg_info = {0};
    struct mem_import_attr_t mem_import_attr = {0};
    struct mem_import_info_t mem_import_info = {0};
    unsigned long long jfce_addr = 0;
    unsigned long long token_id_addr = 0;
    unsigned int token_id = 0;
    unsigned int complete_num = 0;
    struct rs_ub_dev_cb *dev_cb = NULL;

    tc_rs_cb = tc_rs_ub_v2_init(NETWORK_OFFLINE, &dev_index);
    cq_attr.ub.mode = JFC_MODE_STARS_POLL;
    void *addr = malloc(1);
    mem_reg_attr.mem.addr = (uintptr_t)addr;
    mem_reg_attr.mem.size = 1;
    mem_reg_attr.ub.flags.bs.token_id_valid = 1;

    ret = rs_ub_get_dev_cb(tc_rs_cb, dev_index, &dev_cb);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_token_id_alloc(dev_cb, &token_id_addr, &token_id);
    EXPECT_INT_EQ(0, ret);
    mem_reg_attr.ub.token_id_addr = token_id_addr;

    ret = rs_ub_ctx_jfc_create(dev_cb, &cq_attr, &cq_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_create(dev_cb, &qp_attr, &qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_import(dev_cb, &import_attr, &import_data);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_jetty_bind(dev_cb, &rs_qp_info, &rs_qp_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_lmem_reg(dev_cb, &mem_reg_attr, &mem_reg_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_rmem_import(dev_cb, &mem_import_attr, &mem_import_info);
    EXPECT_INT_EQ(0, ret);

    ret = rs_ub_ctx_chan_create(dev_cb, &jfce_addr);
    EXPECT_INT_EQ(0, ret);

    free(addr);
    addr = NULL;
    tc_rs_ub_v2_deinit(tc_rs_cb, NETWORK_OFFLINE, dev_index);
}

void tc_rs_ub_ctx_ext_jetty_create()
{
    struct rs_ctx_jetty_cb jetty_cb = { 0 };
    struct rs_ub_dev_cb dev_cb = { 0 };
    urma_jetty_cfg_t jetty_cfg = { 0 };
    struct rs_cb rscb = { 0 };

    dev_cb.rscb = &rscb;
    jetty_cb.jetty_mode = JETTY_MODE_USER_CTL_NORMAL;
    jetty_cb.dev_cb = &dev_cb;
    rs_ub_ctx_ext_jetty_create(&jetty_cb, &jetty_cfg);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    
    // new case
    mocker_clean();
    mocker(rs_ub_ctx_reg_jetty_db, 1, 0);
    jetty_cb.jetty_mode = JETTY_MODE_CCU_TA_CACHE;
    rs_ub_ctx_ext_jetty_create_ta_cache(&jetty_cb, &jetty_cfg);
    rs_ub_ctx_ext_jetty_delete(&jetty_cb);
    mocker_clean();
}

void tc_rs_ub_ctx_rmem_import()
{
    struct mem_import_attr_t rmem_attr = {0};
    struct mem_import_info_t rmem_info = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    int ret;

    mocker(rs_ub_get_dev_cb, 2, 0);
    mocker(memcpy_s, 2, -1);
    ret = rs_ub_ctx_rmem_import(&dev_cb, &rmem_attr, &rmem_info);
    EXPECT_INT_EQ(-ESAFEFUNC, ret);
    mocker_clean();
}

void tc_rs_get_tp_info_list()
{
    struct ra_rs_dev_info dev_info = {0};
    struct tp_info info_list[2] = {0};
    struct get_tp_cfg cfg = {0};
    unsigned int num = 1;
    int ret = 0;

    mocker(rs_get_rs_cb, 1, -EINVAL);
    ret = rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UNSUPPORT;
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v1, 10);
    ret = rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(-EINVAL, ret);
    mocker_clean();

    stub_rs_cb.protocol = PROTOCOL_UDMA;
    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v1, 10);
    mocker_invoke(rs_ub_get_dev_cb, stub_rs_ub_get_dev_cb, 10);
    ret =  rs_get_tp_info_list(&dev_info, &cfg, info_list, &num);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_ub_ctx_drv_jetty_import()
{
    struct rs_ctx_rem_jetty_cb rjetty_cb = {0};
    struct rs_ub_dev_cb dev_cb = {0};
    urma_context_t urma_ctx = {0};
    int ret = 0;

    rjetty_cb.mode = JETTY_IMPORT_MODE_EXP;
    dev_cb.urma_ctx = &urma_ctx;
    rjetty_cb.dev_cb = &dev_cb;
    ret = rs_ub_ctx_drv_jetty_import(&rjetty_cb);
    EXPECT_INT_EQ(0, ret);

    free(rjetty_cb.tjetty);
    rjetty_cb.tjetty = NULL;
}
