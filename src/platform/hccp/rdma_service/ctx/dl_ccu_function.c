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
#include "dl_ccu_function.h"

void *g_ccu_api_handle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_ccu_ops g_ccu_ops;
#else
struct rs_ccu_ops g_ccu_ops = {
    .rs_ccu_init = ccu_init,
    .rs_ccu_uninit = ccu_uninit,
    .rs_ccu_custom_channel = ccu_custom_channel,
    .rs_ccu_get_cqe_base_addr = ccu_get_cqe_base_addr,
};
#endif

STATIC int rs_ccu_device_api_init(void)
{
#ifndef CA_CONFIG_LLT
    g_ccu_ops.rs_ccu_init = (int (*)(void)) HccpDlsym(g_ccu_api_handle, "ccu_init");
    DL_API_RET_IS_NULL_CHECK(g_ccu_ops.rs_ccu_init, "ccu_init");

    g_ccu_ops.rs_ccu_uninit = (int (*)(void)) HccpDlsym(g_ccu_api_handle, "ccu_uninit");
    DL_API_RET_IS_NULL_CHECK(g_ccu_ops.rs_ccu_uninit, "ccu_uninit");

    g_ccu_ops.rs_ccu_custom_channel = (int (*)(const struct channel_info_in *in, struct channel_info_out *out))
        HccpDlsym(g_ccu_api_handle, "ccu_custom_channel");
    DL_API_RET_IS_NULL_CHECK(g_ccu_ops.rs_ccu_custom_channel, "ccu_custom_channel");

    g_ccu_ops.rs_ccu_get_cqe_base_addr = (unsigned long long (*)(unsigned int die_id))
        HccpDlsym(g_ccu_api_handle, "ccu_get_cqe_base_addr");
    DL_API_RET_IS_NULL_CHECK(g_ccu_ops.rs_ccu_get_cqe_base_addr, "ccu_get_cqe_base_addr");
#endif
    return 0;
}

STATIC int rs_open_ccu_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_ccu_api_handle == NULL) {
        g_ccu_api_handle = HccpDlopen("libccu-user-drv.so", RTLD_NOW);
        if (g_ccu_api_handle != NULL) {
            return 0;
        }
        return -EINVAL;
    } else {
        hccp_run_info("ccu_api dlopen again!");
    }
#endif
    return 0;
}

STATIC void rs_close_ccu_so(void)
{
#ifndef CA_CONFIG_LLT
    if (g_ccu_api_handle != NULL) {
        (void)HccpDlclose(g_ccu_api_handle);
        g_ccu_api_handle = NULL;
    }
#endif
    return;
}

int rs_ccu_api_init(void)
{
    int ret;

    ret = rs_open_ccu_so();
    CHK_PRT_RETURN(ret, hccp_err("HccpDlopen[libccu-user-drv.so] failed! ret=[%d],"\
    "Please check network adapter driver has been installed", ret), ret);

    ret = rs_ccu_device_api_init();
    if (ret != 0) {
        hccp_err("[rs_ccu_device_api_init]HccpDlopen failed! ret=[%d]", ret);
        rs_close_ccu_so();
        return ret;
    }
    return 0;
}

void rs_ccu_api_deinit(void)
{
    rs_close_ccu_so();
    return;
}

int rs_ccu_init(void)
{
    if (g_ccu_api_handle == NULL || g_ccu_ops.rs_ccu_init == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_ccu_api_handle is NULL or rs_ccu_init is NULL");
        return -EINVAL;
#endif
    }
    return g_ccu_ops.rs_ccu_init();
}

int rs_ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    if (g_ccu_api_handle == NULL || g_ccu_ops.rs_ccu_custom_channel == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_ccu_api_handle is NULL or rs_ccu_custom_channel is NULL");
        return -EINVAL;
#endif
    }
    return g_ccu_ops.rs_ccu_custom_channel(in, out);
}

int rs_ccu_get_cqe_base_addr(unsigned int die_id, unsigned long long *cqe_base_addr)
{
    if (g_ccu_api_handle == NULL || g_ccu_ops.rs_ccu_get_cqe_base_addr == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_ccu_api_handle is NULL or rs_ccu_get_cqe_base_addr is NULL");
        return -EINVAL;
#endif
    }
    CHK_PRT_RETURN(cqe_base_addr == NULL, hccp_err("cqe_base_addr is null, die_id:%u", die_id), -EINVAL);
    *cqe_base_addr = g_ccu_ops.rs_ccu_get_cqe_base_addr(die_id);
    return 0;
}

int rs_ccu_uninit(void)
{
    if (g_ccu_api_handle == NULL || g_ccu_ops.rs_ccu_uninit == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("g_ccu_api_handle is NULL or rs_ccu_uninit is NULL");
        return -EINVAL;
#endif
    }
    return g_ccu_ops.rs_ccu_uninit();
}
