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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "securec.h"
#include "user_log.h"
#include "dl_netco_function.h"
#include "hccp_tlv.h"
#include "file_opt.h"
#include "ra_rs_err.h"
#include "network_comm.h"
#include "rs_adp_nslb.h"

STATIC int rs_get_netco_cfg(unsigned int phy_id, NetCoIpPortArg *netco_arg)
{
    char cfg_val[CFG_VAL_LEN] = {0};
    bool nslb_support = false;
    int ret = 0;

    ret = file_read_cfg(NETCO_CFGFILE_PATH, (int)phy_id, "udp_port_mode", cfg_val, CFG_VAL_LEN);
    // file not exist or item not found, degrade log level
    CHK_PRT_RETURN(ret == FILE_OPT_INNER_PARAM_ERR || ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND,
        hccp_run_warn("file_read_cfg udp_port_mode unsuccessful, ret(%d)", ret), -ENOTSUPP);
    CHK_PRT_RETURN(ret != 0, hccp_err("file_read_cfg udp_port_mode failed, ret(%d)", ret), ret);

    nslb_support = (strncmp(cfg_val, "nslb_dp", strlen("nslb_dp") + 1) == 0) ? true : false;
    CHK_PRT_RETURN(!nslb_support, hccp_run_warn("phy_id(%u) not support nslb", phy_id), -ENOTSUPP);

    (void)memset_s(cfg_val, CFG_VAL_LEN, 0, CFG_VAL_LEN);
    ret = file_read_cfg(NETCO_CFGFILE_PATH, (int)phy_id, "nslb_dp_listen_port", cfg_val, CFG_VAL_LEN);
    // file not exist or item not found, degrade log level
    CHK_PRT_RETURN(ret == FILE_OPT_INNER_PARAM_ERR || ret == FILE_OPT_SYS_RD_FILE_NOT_FOUND,
        hccp_run_warn("file_read_cfg nslb_dp_listen_port unsuccessful, ret(%d)", ret), -ENOTSUPP);
    CHK_PRT_RETURN(ret != 0, hccp_err("file_read_cfg nslb_dp_listen_port failed, ret(%d)", ret), ret);

    netco_arg->listenPort = (unsigned short)strtoul(cfg_val, NULL, NETCO_PORT_NUM_BASE);
    netco_arg->gatewayPort = netco_arg->listenPort;
    return 0;
}

STATIC int rs_netco_init_arg(unsigned int phy_id, NetCoIpPortArg *netco_arg)
{
    struct ifaddr_info ifaddr_infos = {0};
    unsigned int gw_addr = 0;
    unsigned int num = 1;
    int ret = 0;

    ret = rs_get_netco_cfg(phy_id, netco_arg);
    CHK_PRT_RETURN(ret == -ENOTSUPP, hccp_run_warn("get netco cfg unsuccessful, ret(%d) phy_id(%u)", ret, phy_id), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("get netco cfg failed, ret(%d) phy_id(%u)", ret, phy_id), ret);

    ret = rs_get_ifaddrs(&ifaddr_infos, &num, phy_id);
    CHK_PRT_RETURN(ret != 0 || num != 1, hccp_err("rs get ifaddr failed, ret(%d) or num(%u) != 1", ret, num), -EINVAL);
    netco_arg->localIp = ntohl(ifaddr_infos.ip.addr.s_addr);

    ret = net_get_gateway_address(phy_id, &gw_addr);
    CHK_PRT_RETURN(ret != 0, hccp_err("rs get gateway failed ret %d", ret), ret);
    netco_arg->gatewayIp = ntohl(gw_addr);

    // 0 indicates port will be assigned randomly
    netco_arg->localNetPort = htons(0);
    return 0;
}

int rs_nslb_netco_init(struct rs_nslb_cb *nslb_cb)
{
    NetCoIpPortArg netco_arg = {0};
    void *netco_cb = NULL;
    int ret = 0;

    ret = rs_netco_init_arg(nslb_cb->phy_id, &netco_arg);
    CHK_PRT_RETURN(ret == -ENOTSUPP, hccp_warn("get netco init arg unsuccessful, ret(%d)", ret), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("get netco init arg failed, ret(%d)", ret), -EINVAL);

    ret = rs_nslb_api_init();
    CHK_PRT_RETURN(ret != 0, hccp_err("rs_nslb_api_init[%d]", ret), ret);

    netco_cb = rs_netco_init(nslb_cb->rscb->conn_cb.epollfd, netco_arg);
    CHK_PRT_RETURN(netco_cb == NULL, hccp_err("netco init failed"), -EINVAL);

    nslb_cb->netco_cb = netco_cb;
    nslb_cb->netco_init_flag = true;
    return 0;
}

void rs_nslb_netco_deinit(struct rs_nslb_cb *nslb_cb)
{
    rs_netco_deinit(nslb_cb->netco_cb);
    nslb_cb->netco_init_flag = false;
    nslb_cb->netco_cb = NULL;

    rs_nslb_api_deinit();
    return;
}

int rs_nslb_netco_request(struct rs_nslb_cb *nslb_cb, unsigned int type, char *data, unsigned int data_len)
{
    int ret = 0;

    ret = rs_netco_tbl_add_upd(nslb_cb->netco_cb, type, data, data_len);
    ret = (ret > 0) ? -ret: ret;
    CHK_PRT_RETURN(ret != 0, hccp_err("netco request failed, type(%u) ret(%d)", type, ret), ret);
    return 0;
}

int rs_epoll_nslb_event_handle(struct rs_nslb_cb *nslb_cb, int fd, unsigned int events)
{
    unsigned int ret = 0;
    int ret_val = 0;

    // netco is not initialized, no epoll event need to handle
    if (!nslb_cb->netco_init_flag) {
        return -ENODEV;
    }

    RS_PTHREAD_MUTEX_LOCK(&nslb_cb->mutex);
    ret = rs_netco_event_dispatch(nslb_cb->netco_cb, fd, events);
    ret_val = (ret == NET_CO_PROCED) ? (int)ret : -ENODEV;
    RS_PTHREAD_MUTEX_ULOCK(&nslb_cb->mutex);
    return ret_val;
}
