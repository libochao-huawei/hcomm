/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <errno.h>
#include "hccp_dl.h"
#include "rs.h"
#include "dl_netco_function.h"

void *g_netco_api_handle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_netco_ops g_netco_ops;
#else
struct rs_netco_ops g_netco_ops = {
    .rs_netco_init = Net_CoInitFactory,
    .rs_netco_deinit = NET_CoDestruct,
    .rs_netco_event_dispatch = NET_CoFdEventDispatch,
    .rs_netco_tbl_add_upd = NET_CoTblAddUpd,
};
#endif

STATIC int rs_open_libnet_co_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_netco_api_handle == NULL) {
        g_netco_api_handle = hccp_dlopen("libnet_co.so", RTLD_NOW);
        if (g_netco_api_handle != NULL) {
            return 0;
        }
        hccp_err("hccp_dlopen[libnet_co.so] fail! errno=%d, dlerror: %s", errno, dlerror());
        return -EINVAL;
    } else {
        hccp_run_info("netco_api dlopen again!");
    }
#endif
    return 0;
}

STATIC void rs_close_netco_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_netco_api_handle != NULL) {
        (void)hccp_dlclose(g_netco_api_handle);
        g_netco_api_handle = NULL;
    }
#endif
    return;
}

STATIC int rs_netco_tbl_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_netco_ops.rs_netco_init = (void *(*)(int, NetCoIpPortArg))
        hccp_dlsym(g_netco_api_handle, "Net_CoInitFactory");
    DL_API_RET_IS_NULL_CHECK(g_netco_ops.rs_netco_init, "Net_CoInitFactory");

    g_netco_ops.rs_netco_deinit = (void (*)(void *))
        hccp_dlsym(g_netco_api_handle, "NET_CoDestruct");
    DL_API_RET_IS_NULL_CHECK(g_netco_ops.rs_netco_deinit, "NET_CoDestruct");

    g_netco_ops.rs_netco_event_dispatch = (unsigned int (*)(void *, int, unsigned int))
        hccp_dlsym(g_netco_api_handle, "NET_CoFdEventDispatch");
    DL_API_RET_IS_NULL_CHECK(g_netco_ops.rs_netco_event_dispatch, "NET_CoFdEventDispatch");

    g_netco_ops.rs_netco_tbl_add_upd = (int (*)(void *, unsigned int, char *, unsigned int))
        hccp_dlsym(g_netco_api_handle, "NET_CoTblAddUpd");
    DL_API_RET_IS_NULL_CHECK(g_netco_ops.rs_netco_tbl_add_upd, "NET_CoTblAddUpd");
#endif
    return 0;
}

STATIC int rs_netco_api_init(void)
{
    int ret;
    ret = rs_open_libnet_co_so();
    CHK_PRT_RETURN(ret, hccp_err("hccp_dlopen[libnet_co.so] fail! ret=[%d],"\
    "Please check network adapter driver has been installed", ret), ret);

    ret = rs_netco_tbl_api_init();
    if (ret != 0) {
        hccp_err("[rs_netco_tbl_api_init]hccp_dlopen fail! ret=[%d]", ret);
        rs_close_netco_so();
        return ret;
    }

    return 0;
}

int rs_nslb_api_init(void)
{
    int ret;

    ret = rs_netco_api_init();
    CHK_PRT_RETURN(ret, hccp_err("rs_netco_api_init fail! ret=[%d]", ret), ret);

    return 0;
}

void rs_nslb_api_deinit(void)
{
    if (g_netco_api_handle != NULL) {
        (void)hccp_dlclose(g_netco_api_handle);
        g_netco_api_handle = NULL;
    }
    return;
}

void *rs_netco_init(int epollfd, NetCoIpPortArg ipPortArg)
{
    if (g_netco_ops.rs_netco_init == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_init is null");
        return NULL;
#endif
    }
    return g_netco_ops.rs_netco_init(epollfd, ipPortArg);
}

void rs_netco_deinit(void *co)
{
    if (g_netco_ops.rs_netco_deinit == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_deinit is null");
        return;
#endif
    }
    return g_netco_ops.rs_netco_deinit(co);
}

unsigned int rs_netco_event_dispatch(void *co, int fd, unsigned int curEvents)
{
    if (g_netco_ops.rs_netco_event_dispatch == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_event_dispatch is null");
        return -EINVAL;
#endif
    }
    return g_netco_ops.rs_netco_event_dispatch(co, fd, curEvents);
}

int rs_netco_tbl_add_upd(void *netco_handle, unsigned int type, char *data, unsigned int data_len)
{
    if (g_netco_ops.rs_netco_tbl_add_upd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_tbl_add_upd is null");
        return -EINVAL;
#endif
    }
    return g_netco_ops.rs_netco_tbl_add_upd(netco_handle, type, data, data_len);
}
