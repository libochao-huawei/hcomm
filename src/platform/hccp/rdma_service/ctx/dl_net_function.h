/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: 获取动态库中network 适配层接口函数的适配接口私有头文件
 * Create: 2025-12-9
 */

#ifndef DL_NET_FUNCTION_H
#define DL_NET_FUNCTION_H

struct rs_net_ops {
    int (*rs_net_adapt_init)(void);
    void (*rs_net_adapt_uninit)(void);
    int (*rs_net_alloc_jfc_id)(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id);
    int (*rs_net_free_jfc_id)(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id);
    int (*rs_net_alloc_jetty_id)(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id);
    int (*rs_net_free_jetty_id)(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id);
    unsigned long long (*rs_net_get_cqe_base_addr)(unsigned int die_id);
};

int rs_net_api_init(void);
void rs_net_api_deinit(void);

int rs_net_adapt_init(void);
void rs_net_adapt_uninit(void);
int rs_net_alloc_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id);
int rs_net_free_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id);
int rs_net_alloc_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id);
int rs_net_free_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id);
int rs_net_get_cqe_base_addr(unsigned int die_id, unsigned long long *cqe_base_addr);

#endif // DL_NET_FUNCTION_H
