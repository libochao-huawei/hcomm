/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hccp_dl.h"
#include "net_adapt_u_api.h"
#include "dl_net_function.h"

void *g_net_api_handle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_net_ops g_net_ops;
#else
struct rs_net_ops g_net_ops = {
    .rs_net_adapt_init = net_adapt_init,
    .rs_net_adapt_uninit = net_adapt_uninit,
    .rs_net_alloc_jfc_id = net_alloc_jfc_id,
    .rs_net_free_jfc_id = net_free_jfc_id,
    .rs_net_alloc_jetty_id = net_alloc_jetty_id,
    .rs_net_free_jetty_id = net_free_jetty_id,
    .rs_net_get_cqe_base_addr = net_get_cqe_base_addr,
};
#endif

int rs_net_adapt_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_net_ops.rs_net_adapt_init = (int (*)(void)) HccpDlsym(g_net_api_handle, "net_adapt_init");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_adapt_init, "net_adapt_init");

    g_net_ops.rs_net_adapt_uninit = (void (*)(void)) HccpDlsym(g_net_api_handle, "net_adapt_uninit");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_adapt_uninit, "net_adapt_uninit");

    g_net_ops.rs_net_alloc_jfc_id = (int (*)(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id))
        HccpDlsym(g_net_api_handle, "net_alloc_jfc_id");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_alloc_jfc_id, "net_alloc_jfc_id");

    g_net_ops.rs_net_free_jfc_id = (int (*)(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id))
        HccpDlsym(g_net_api_handle, "net_free_jfc_id");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_free_jfc_id, "net_free_jfc_id");

    g_net_ops.rs_net_alloc_jetty_id =
        (int (*)(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id))
        HccpDlsym(g_net_api_handle, "net_alloc_jetty_id");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_alloc_jetty_id, "net_alloc_jetty_id");

    g_net_ops.rs_net_free_jetty_id = (int (*)(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id))
        HccpDlsym(g_net_api_handle, "net_free_jetty_id");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_free_jetty_id, "net_free_jetty_id");

    g_net_ops.rs_net_get_cqe_base_addr = (unsigned long long (*)(unsigned int die_id))
        HccpDlsym(g_net_api_handle, "net_get_cqe_base_addr");
    DL_API_RET_IS_NULL_CHECK(g_net_ops.rs_net_get_cqe_base_addr, "net_get_cqe_base_addr");
#endif
    return 0;
}

int rs_open_net_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_net_api_handle == NULL) {
        g_net_api_handle = HccpDlopen("libnet_adapt.so", RTLD_NOW);
        return ((g_net_api_handle != NULL) ? 0 : -EINVAL);
    }
    hccp_run_info("net_adapt_api HccpDlopen again!");
#endif
    return 0;
}

void rs_close_net_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_net_api_handle != NULL) {
        (void)HccpDlclose(g_net_api_handle);
        g_net_api_handle = NULL;
    }
#endif
    return;
}

int rs_net_api_init(void)
{
    int ret;

    ret = rs_open_net_so();
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_open_net_so[libnet_adapt.so] failed! ret=[%d],"
        "please check network adapter driver has been installed", ret), ret);

    ret = rs_net_adapt_api_init();
    if (ret != 0) {
        hccp_err("rs_net_adapt_api_init failed! ret=[%d]", ret);
        rs_close_net_so();
        return ret;
    }
    return 0;
}

void rs_net_api_deinit(void)
{
    rs_close_net_so();
    return;
}

int rs_net_adapt_init(void)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_adapt_init == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_adapt_init is NULL");
        return -EINVAL;
#endif
    }
    return g_net_ops.rs_net_adapt_init();
}

void rs_net_adapt_uninit(void)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_adapt_uninit == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_adapt_uninit is NULL");
        return;
#endif
    }
    g_net_ops.rs_net_adapt_uninit();
}

int rs_net_alloc_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_alloc_jfc_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_alloc_jfc_id is NULL");
        return -EINVAL;
#endif
    }
    return g_net_ops.rs_net_alloc_jfc_id(udev_name, jfc_mode, jfc_id);
}

int rs_net_free_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_free_jfc_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_free_jfc_id is NULL");
        return -EINVAL;
#endif
    }
    return g_net_ops.rs_net_free_jfc_id(udev_name, jfc_mode, jfc_id);
}

int rs_net_alloc_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_alloc_jetty_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_alloc_jetty_id is NULL");
        return -EINVAL;
#endif
    }
    return g_net_ops.rs_net_alloc_jetty_id(udev_name, jetty_mode, jetty_id);
}

int rs_net_free_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_free_jetty_id == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_free_jetty_id is NULL");
        return -EINVAL;
#endif
    }
    return g_net_ops.rs_net_free_jetty_id(udev_name, jetty_mode, jetty_id);
}

int rs_net_get_cqe_base_addr(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    if (g_net_api_handle == NULL || g_net_ops.rs_net_get_cqe_base_addr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_net_api_handle is NULL or rs_net_get_cqe_base_addr is NULL");
        return -EINVAL;
#endif
    }
    CHK_PRT_RETURN(cqe_base_addr == NULL, hccp_err("cqe_base_addr is null, die_id:%u", die_id), -EINVAL);
    *cqe_base_addr = g_net_ops.rs_net_get_cqe_base_addr(die_id);
    return 0;
}
