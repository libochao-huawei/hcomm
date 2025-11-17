/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rs tlv unit test.
 * Create: 2025-03-21
 */

#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "dl_netco_function.h"
#include "ut_dispatch.h"
#include "hccp_tlv.h"
#include "rs_inner.h"
#include "rs_tlv.h"
#include "rs_epoll.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "file_opt.h"
#include "rs_adp_nslb.h"
#include "network_comm.h"

static struct rs_cb stub_rs_cb;
extern int rs_tlv_assemble_send_data(struct tlv_buf_info *buf_info, struct tlv_request_msg_head *head, char *data,
    unsigned int *send_finish);
extern int rs_get_nslb_cb(uint32_t phy_id, struct rs_nslb_cb **nslb_cb);
extern void rs_epoll_event_handle_one(struct rs_cb *rs_cb, struct epoll_event *events);
extern int rs_nslb_init(unsigned int phy_id, unsigned int *buffer_size);
extern int rs_nslb_deinit(unsigned int phy_id);
extern int rs_nslb_request(struct tlv_request_msg_head *head, char *data);
extern int rs_netco_tbl_api_init(void);

int stub_rs_get_nslb_cb(uint32_t phy_id, struct rs_nslb_cb **nslb_cb)
{
    stub_rs_cb.conn_cb.epollfd = 0;
    stub_rs_cb.nslb_cb.buf_info.buffer_size = RS_NSLB_BUFFER_SIZE;
    stub_rs_cb.nslb_cb.buf_info.buf = (char *)calloc(stub_rs_cb.nslb_cb.buf_info.buffer_size, sizeof(char));
    stub_rs_cb.nslb_cb.netco_init_flag = false;
    pthread_mutex_init(&stub_rs_cb.nslb_cb.mutex, NULL);
    *nslb_cb = &stub_rs_cb.nslb_cb;
    return 0;
}

int stub_rs_get_nslb_cb_deinit(uint32_t phy_id, struct rs_nslb_cb **nslb_cb)
{
    stub_rs_cb.conn_cb.epollfd = 0;
    stub_rs_cb.nslb_cb.buf_info.buf = (char *)calloc(stub_rs_cb.nslb_cb.buf_info.buffer_size, sizeof(char));
    stub_rs_cb.nslb_cb.buf_info.buffer_size = RS_NSLB_BUFFER_SIZE;
    stub_rs_cb.nslb_cb.netco_init_flag = true;
    stub_rs_cb.nslb_cb.netco_cb = NULL;
    pthread_mutex_init(&stub_rs_cb.nslb_cb.mutex, NULL);
    *nslb_cb = &stub_rs_cb.nslb_cb;
    return 0;
}

int stub_rs_get_nslb_cb_after_deinit(uint32_t phy_id, struct rs_nslb_cb **nslb_cb)
{
    *nslb_cb = &stub_rs_cb.nslb_cb;
    return 0;
}

int stub_rs_get_nslb_cb_init(uint32_t phy_id, struct rs_nslb_cb **nslb_cb)
{
    stub_rs_cb.nslb_cb.netco_init_flag = false;
    *nslb_cb = &stub_rs_cb.nslb_cb;
    return 0;
}

int stub_rs_tlv_assemble_send_data(struct tlv_buf_info *buf_info, struct tlv_request_msg_head *head, char *data,
    bool *send_finish)
{
    if (head->offset == 0) {
        *send_finish = false;
    } else {
        *send_finish = true;
    }
    return 0;
}

int stub_rs_get_rs_cb_v2(unsigned int phy_id, struct rs_cb **rs_cb)
{
    stub_rs_cb.nslb_cb.netco_init_flag = false;
    stub_rs_cb.conn_cb.epollfd = 0;
    *rs_cb = &stub_rs_cb;
    return 0;
}

int stub_file_read_cfg(const char *file_path, int dev_id, const char *conf_name, char *conf_value, unsigned int len)
{
    if (strncmp(conf_name, "udp_port_mode", strlen("udp_port_mode") + 1) == 0){
        memcpy_s(conf_value, len, "nslb_dp", strlen("nslb_dp"));
    } else {
        memcpy_s(conf_value, len, "16666", strlen("16666"));
    }
    return 0;
}

void free_rs_cb() {
    pthread_mutex_destroy(&stub_rs_cb.nslb_cb.mutex);
    free(stub_rs_cb.nslb_cb.buf_info.buf);
    stub_rs_cb.nslb_cb.buf_info.buf = NULL;
}

void tc_rs_tlv_init()
{
    unsigned int module_type = TLV_MODULE_TYPE_NSLB;
    unsigned int buffer_size = 0;
    unsigned int phy_id = 0;
    int ret;

    mocker(rs_nslb_init, 10, 0);
    ret = rs_tlv_init(module_type, phy_id, &buffer_size);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    module_type = TLV_MODULE_TYPE_MAX;
    ret = rs_tlv_init(module_type, phy_id, &buffer_size);
    EXPECT_INT_NE(0, ret);
}

void tc_rs_tlv_deinit()
{
    unsigned int module_type = TLV_MODULE_TYPE_NSLB;
    unsigned int phy_id = 0;
    int ret;

    mocker(rs_nslb_deinit, 10, 0);
    ret = rs_tlv_deinit(module_type, phy_id);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    module_type = TLV_MODULE_TYPE_MAX;
    ret = rs_tlv_deinit(module_type, phy_id);
    EXPECT_INT_NE(0, ret);
}

void tc_rs_tlv_request()
{
    struct tlv_request_msg_head head = {0};
    char data[10];
    int ret;

    head.module_type = TLV_MODULE_TYPE_NSLB;
    mocker(rs_nslb_request, 10, 0);
    ret = rs_tlv_request(&head, &data);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    head.module_type = TLV_MODULE_TYPE_MAX;
    ret = rs_tlv_request(&head, &data);
    EXPECT_INT_NE(0, ret);
}

void tc_rs_nslb_init()
{
    void *stub_co = (void *)calloc(1, sizeof(unsigned int));
    unsigned int buffer_size = 0;
    unsigned int phy_id = 0;
    int ret;

    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v2, 10);
    mocker(calloc, 10, NULL);
    ret = rs_nslb_init(phy_id, &buffer_size);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v2, 10);
    mocker_invoke(file_read_cfg, stub_file_read_cfg, 10);
    mocker(rsGetLocalDevIDByHostDevID, 10, 0);
    mocker(net_get_gateway_address, 10, 0);
    mocker(rs_get_ifaddrs, 10, 0);
    mocker(rs_netco_init, 10, stub_co);
    ret = rs_nslb_init(phy_id, &buffer_size);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free_rs_cb();

    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v2, 10);
    mocker(rs_nslb_netco_init, 10, -EINVAL);
    ret = rs_nslb_init(phy_id, &buffer_size);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v2, 10);
    mocker(rs_nslb_netco_init, 10, -ENOTSUPP);
    ret = rs_nslb_init(phy_id, &buffer_size);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    free(stub_co);
    stub_co = NULL;
}

void tc_rs_nslb_deinit()
{
    unsigned int phy_id = 0;
    int ret;

    mocker_invoke(rs_get_nslb_cb, stub_rs_get_nslb_cb_deinit, 10);
    ret = rs_nslb_deinit(phy_id);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();

    mocker_invoke(rs_get_nslb_cb, stub_rs_get_nslb_cb_after_deinit, 10);
    ret = rs_nslb_deinit(phy_id);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_nslb_request()
{
    struct tlv_request_msg_head head = {0};
    char *data;
    int ret;

    head.phy_id = 0;
    head.type = 0;
    data = (char *)calloc(16, sizeof(char));

    mocker_invoke(rs_get_nslb_cb, stub_rs_get_nslb_cb, 10);
    mocker(rs_tlv_assemble_send_data, 10, -EINVAL);
    ret = rs_nslb_request(&head, data);
    EXPECT_INT_NE(0, ret);
    mocker_clean();
    free_rs_cb();

// send_finish = false
    head.offset = 0;
    mocker_invoke(rs_get_nslb_cb, stub_rs_get_nslb_cb, 10); 
    mocker_invoke(rs_tlv_assemble_send_data, stub_rs_tlv_assemble_send_data, 10);
    ret = rs_nslb_request(&head, data);
    EXPECT_INT_EQ(0, ret);
    free_rs_cb();

// send_finish = true
    head.offset = 1U;
    head.total_bytes = 16U;
    ret = rs_nslb_request(&head, data);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
    free_rs_cb();

    free(data);
    data = NULL;
}

void tc_rs_tlv_assemble_send_data()
{
    struct tlv_request_msg_head head;
    struct tlv_buf_info buf_info;
    bool send_finish;
    char data[16] = {0};
    int ret;

    buf_info.buffer_size = RS_NSLB_BUFFER_SIZE;
    buf_info.buf = (char *)calloc(RS_NSLB_BUFFER_SIZE, sizeof(char));
    memset_s(buf_info.buf, buf_info.buffer_size, 0, buf_info.buffer_size);
    head.send_bytes = 16U;
    head.total_bytes = 16U;
    head.offset = 0;
    head.phy_id = 0;
    head.type = 0;

    mocker(memset_s, 10 , 0);
    mocker(memcpy_s, 10 , 0);
    ret = rs_tlv_assemble_send_data(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(0, ret);

    head.offset = 0;
    head.send_bytes = 8U;
    head.total_bytes = 16U;
    ret = rs_tlv_assemble_send_data(&buf_info, &head, data, &send_finish);
    EXPECT_INT_EQ(0, ret);

    head.offset = 16U;
    head.send_bytes = 16U;
    head.total_bytes = 16U;
    ret = rs_tlv_assemble_send_data(&buf_info, &head, data, &send_finish);
    EXPECT_INT_NE(0, ret);
    mocker_clean();

    free(buf_info.buf);
    buf_info.buf = NULL;
}

void tc_rs_epoll_nslb_event_handle()
{
    struct epoll_event test_events;
    struct rs_cb test_rs_cb = {0};
    int ret;

    test_rs_cb.nslb_cb.netco_init_flag = true;
    test_rs_cb.nslb_cb.phy_id = 0;
    test_events.events = 0;

    mocker(rs_epoll_nslb_event_handle, 10, NET_CO_PROCED);
    rs_epoll_event_handle_one(&test_rs_cb, &test_events);
    mocker_clean();

    pthread_mutex_init(&test_rs_cb.nslb_cb.mutex, NULL);
    ret = rs_epoll_nslb_event_handle(&test_rs_cb.nslb_cb, 0, 0);
    EXPECT_INT_EQ(NET_CO_PROCED, ret);
    mocker_clean();

    mocker(rs_netco_event_dispatch, 10, -1);
    ret = rs_epoll_nslb_event_handle(&test_rs_cb.nslb_cb, 0, 0);
    EXPECT_INT_NE(NET_CO_PROCED, ret);
    mocker_clean();
    pthread_mutex_destroy(&test_rs_cb.nslb_cb.mutex);
}

void tc_rs_get_nslb_cb()
{
    struct rs_nslb_cb *nslb_cb = NULL;
    uint32_t phy_id;
    int ret;

    mocker_invoke(rs_get_rs_cb, stub_rs_get_rs_cb_v2, 10);
    ret = rs_get_nslb_cb(phy_id, &nslb_cb);
    EXPECT_INT_EQ(0, ret);
    mocker_clean();
}

void tc_rs_nslb_api_init()
{
    NetCoIpPortArg arg = {0};
    unsigned int data_len = 0;
    unsigned int type = 0;
    void *stub_co;
    char *data;
    int ret;

    rs_netco_init(0, arg);

    mocker(rs_netco_tbl_api_init, 10, -1);
    ret = rs_netco_api_init();
    EXPECT_INT_NE(0, ret);
    mocker_clean();
}
