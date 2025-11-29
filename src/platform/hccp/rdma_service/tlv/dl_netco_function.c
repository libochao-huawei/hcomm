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

void *gNetcoApiHandle = NULL;
#ifndef CA_CONFIG_LLT
struct rs_netco_ops gNetcoOps;
#else
struct rs_netco_ops g_netco_ops = {
    .rs_netco_init = Net_CoInitFactory,
    .rs_netco_deinit = NET_CoDestruct,
    .rs_netco_event_dispatch = NET_CoFdEventDispatch,
    .rs_netco_tbl_add_upd = NET_CoTblAddUpd,
};
#endif

STATIC int RsOpenLibnetCoSo(void)
{
#ifndef CA_CONFIG_LLT
    if (gNetcoApiHandle == NULL) {
        gNetcoApiHandle = HccpDlopen("libnet_co.so", RTLD_NOW);
        if (gNetcoApiHandle != NULL) {
            return 0;
        }
        hccp_err("hccp_dlopen[libnet_co.so] failed! errno=%d, dlerror: %s", errno, dlerror());
        return -EINVAL;
    } else {
        hccp_run_info("netco_api dlopen again!");
    }
#endif
    return 0;
}

STATIC void RsCloseNetcoSo(void)
{
#ifndef CA_CONFIG_LLT
    if (gNetcoApiHandle != NULL) {
        (void)HccpDlclose(gNetcoApiHandle);
        gNetcoApiHandle = NULL;
    }
#endif
    return;
}

STATIC int RsNetcoTblApiInit(void)
{
#ifndef CA_CONFIG_LLT
    gNetcoOps.rs_netco_init = (void *(*)(int, NetCoIpPortArg))
        HccpDlsym(gNetcoApiHandle, "Net_CoInitFactory");
    DL_API_RET_IS_NULL_CHECK(gNetcoOps.rs_netco_init, "Net_CoInitFactory");

    gNetcoOps.rs_netco_deinit = (void (*)(void *))
        HccpDlsym(gNetcoApiHandle, "NET_CoDestruct");
    DL_API_RET_IS_NULL_CHECK(gNetcoOps.rs_netco_deinit, "NET_CoDestruct");

    gNetcoOps.rs_netco_event_dispatch = (unsigned int (*)(void *, int, unsigned int))
        HccpDlsym(gNetcoApiHandle, "NET_CoFdEventDispatch");
    DL_API_RET_IS_NULL_CHECK(gNetcoOps.rs_netco_event_dispatch, "NET_CoFdEventDispatch");

    gNetcoOps.rs_netco_tbl_add_upd = (int (*)(void *, unsigned int, char *, unsigned int))
        HccpDlsym(gNetcoApiHandle, "NET_CoTblAddUpd");
    DL_API_RET_IS_NULL_CHECK(gNetcoOps.rs_netco_tbl_add_upd, "NET_CoTblAddUpd");
#endif
    return 0;
}

STATIC int RsNetcoApiInit(void)
{
    int ret;
    ret = RsOpenLibnetCoSo();
    CHK_PRT_RETURN(ret, hccp_err("hccp_dlopen[libnet_co.so] failed! ret=[%d],"\
    "Please check network adapter driver has been installed", ret), ret);

    ret = RsNetcoTblApiInit();
    if (ret != 0) {
        hccp_err("[rs_netco_tbl_api_init]hccp_dlopen failed! ret=[%d]", ret);
        RsCloseNetcoSo();
        return ret;
    }

    return 0;
}

int RsNslbApiInit(void)
{
    int ret;

    ret = RsNetcoApiInit();
    CHK_PRT_RETURN(ret, hccp_err("rs_netco_api_init failed! ret=[%d]", ret), ret);

    return 0;
}

void RsNslbApiDeinit(void)
{
    if (gNetcoApiHandle != NULL) {
        (void)HccpDlclose(gNetcoApiHandle);
        gNetcoApiHandle = NULL;
    }
    return;
}

void *RsNetcoInit(int epollfd, NetCoIpPortArg ipPortArg)
{
    if (gNetcoOps.rs_netco_init == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_init is null");
        return NULL;
#endif
    }
    return gNetcoOps.rs_netco_init(epollfd, ipPortArg);
}

void RsNetcoDeinit(void *co)
{
    if (gNetcoOps.rs_netco_deinit == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_deinit is null");
        return;
#endif
    }
    return gNetcoOps.rs_netco_deinit(co);
}

unsigned int RsNetcoEventDispatch(void *co, int fd, unsigned int curEvents)
{
    if (gNetcoOps.rs_netco_event_dispatch == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_event_dispatch is null");
        return -EINVAL;
#endif
    }
    return gNetcoOps.rs_netco_event_dispatch(co, fd, curEvents);
}

int RsNetcoTblAddUpd(void *netcoHandle, unsigned int type, char *data, unsigned int dataLen)
{
    if (gNetcoOps.rs_netco_tbl_add_upd == NULL) {
#ifndef CA_CONFIG_LLT
        hccp_err("rs_netco_tbl_add_upd is null");
        return -EINVAL;
#endif
    }
    return gNetcoOps.rs_netco_tbl_add_upd(netcoHandle, type, data, dataLen);
}
