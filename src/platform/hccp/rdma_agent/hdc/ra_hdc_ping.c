/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "securec.h"
#include "user_log.h"
#include "ra_rs_err.h"
#include "ra_hdc_ping.h"

int ra_hdc_ping_init(struct ra_ping_handle *ping_handle, struct ping_init_attr *init_attr,
    struct ping_init_info *init_info)
{
    unsigned int phy_id = ping_handle->phy_id;
    union op_ping_init_data ping_data = { 0 };
    int ret;

    ret = memcpy_s(&(ping_data.tx_data.attr), sizeof(struct ping_init_attr), init_attr, sizeof(struct ping_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s init_attr failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_PING_INIT, phy_id, (char *)&ping_data, sizeof(union op_ping_init_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    ret = memcpy_s(init_info, sizeof(struct ping_init_info), &(ping_data.rx_data.info), sizeof(struct ping_init_info));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s init_info failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);
    ret = memcpy_s(&(ping_handle->dev), sizeof(union ping_dev), &(init_attr->dev), sizeof(union ping_dev));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s dev info failed, ret(%d) phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);
    ping_handle->dev_index = ping_data.rx_data.dev_index;

    return 0;
}

STATIC void ra_hdc_ping_init_rdev(struct ra_rs_dev_info *rdev, unsigned int phy_id, unsigned int dev_index)
{
    rdev->phy_id = phy_id;
    rdev->dev_index = dev_index;
}

int ra_hdc_ping_target_add(struct ra_ping_handle *ping_handle, struct ping_target_info target[], uint32_t num)
{
    unsigned int phy_id = ping_handle->phy_id;
    union op_ping_add_data ping_data;
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        if (ping_handle->protocol == PROTOCOL_RDMA) {
            CHK_PRT_RETURN(target[i].local_info.rdma.udp_sport > MAX_PORT_NUM,
                hccp_err("[add][ra_hdc_ping]udp_sport(%u) invaild, i(%u), phy_id(%u)",
                target[i].local_info.rdma.udp_sport, i, phy_id), -EINVAL);
        }

        (void)memset_s(&ping_data, sizeof(ping_data), 0, sizeof(ping_data));
        ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);
        ret = memcpy_s(&(ping_data.tx_data.target), sizeof(struct ping_target_info),
            &(target[i]), sizeof(struct ping_target_info));
        CHK_PRT_RETURN(ret, hccp_err("[add][ra_hdc_ping]memcpy_s target failed, ret(%d) i(%u) phy_id(%u)",
            ret, i, phy_id), -ESAFEFUNC);
        ret = ra_hdc_process_msg(RA_RS_PING_ADD, phy_id, (char *)&ping_data, sizeof(union op_ping_add_data));
        CHK_PRT_RETURN(ret, hccp_err("[add][ra_hdc_ping]ra hdc message process failed ret(%d) i(%u) phy_id(%u)",
            ret, i, phy_id), ret);
    }

    return 0;
}

int ra_hdc_ping_task_start(struct ra_ping_handle *ping_handle, struct ping_task_attr *attr)
{
    union op_ping_start_data ping_data = { 0 };
    unsigned int phy_id = ping_handle->phy_id;
    int ret;

    ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);
    ret = memcpy_s(&(ping_data.tx_data.attr), sizeof(struct ping_task_attr), attr, sizeof(struct ping_task_attr));
    CHK_PRT_RETURN(ret, hccp_err("[start][ra_hdc_ping]memcpy_s attr failed, ret(%d), phy_id(%u)",
        ret, phy_id), -ESAFEFUNC);

    ret = ra_hdc_process_msg(RA_RS_PING_START, phy_id, (char *)&ping_data, sizeof(union op_ping_start_data));
    CHK_PRT_RETURN(ret, hccp_err("[start][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);
    return 0;
}

int ra_hdc_ping_get_results(struct ra_ping_handle *ping_handle, struct ping_target_result target[], uint32_t *num)
{
    unsigned int phy_id = ping_handle->phy_id;
    union op_ping_results_data ping_data;
    unsigned int total_num = *num;
    unsigned int complete_cnt = 0;
    unsigned int send_num = 0;
    unsigned int i = 0;
    unsigned int j = 0;
    int ret = 0;

    while (complete_cnt < total_num) {
        (void)memset_s(&ping_data, sizeof(ping_data), 0, sizeof(ping_data));
        ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);
        send_num = ((total_num - complete_cnt) >= RA_MAX_PING_TARGET_NUM) ?
            RA_MAX_PING_TARGET_NUM : (total_num - complete_cnt);

        // prepare tx data target
        for (i = 0; i < send_num; i++) {
            j = i + complete_cnt;
            ret = memcpy_s(&(ping_data.tx_data.target[i]), sizeof(struct ping_target_comm_info),
                &(target[j].remote_info), sizeof(struct ping_target_comm_info));
            if (ret) {
                hccp_err("[get][ra_hdc_ping]memcpy_s remote_info failed, ret(%d), i(%u), j(%u), phy_id(%u)",
                    ret, i, j, phy_id);
                goto out;
            }
        }
        ping_data.tx_data.num = send_num;

        ret = ra_hdc_process_msg(RA_RS_PING_GET_RESULTS, phy_id, (char *)&ping_data,
            sizeof(union op_ping_results_data));
        // caller needs to retry, degrade log level
        if (ret == -EAGAIN) {
            hccp_warn("[get][ra_hdc_ping]ra hdc message process unsuccessful, ret(%d) phy_id(%u)", ret, phy_id);
            goto out;
        }

        if (ping_data.rx_data.num > send_num) {
            hccp_err("[get][ra_hdc_ping]rx_data.num[%u] is larger than send_num[%u], ret(%d) phy_id(%u)",
                ping_data.rx_data.num, send_num, ret, phy_id);
            ret = -EINVAL;
            goto out;
        }

        // prepare rx data target
        for (i = 0; i < ping_data.rx_data.num; i++) {
            j = i + complete_cnt;
            ret = memcpy_s(&(target[j].result), sizeof(struct ping_result_info),
                &(ping_data.rx_data.target[i]), sizeof(struct ping_result_info));
            if (ret) {
                hccp_err("[get][ra_hdc_ping]memcpy_s result failed, ret(%d), i(%u), j(%u), phy_id(%u)",
                    ret, i, j, phy_id);
                ret = -ESAFEFUNC;
                goto out;
            }
        }

        complete_cnt += ping_data.rx_data.num;
        if (ret) {
            hccp_err("[get][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)", ret, phy_id);
            goto out;
        }
    }

out:
    *num = complete_cnt;

    return ret;
}

int ra_hdc_ping_target_del(struct ra_ping_handle *ping_handle, struct ping_target_comm_info target[], uint32_t num)
{
    unsigned int phy_id = ping_handle->phy_id;
    union op_ping_del_data ping_data;
    unsigned int complete_cnt = 0;
    unsigned int send_num = 0;
    unsigned int i = 0;
    unsigned int j = 0;
    int ret;

    while (complete_cnt < num) {
        (void)memset_s(&ping_data, sizeof(ping_data), 0, sizeof(ping_data));
        ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);
        send_num = ((num - complete_cnt) >= RA_MAX_PING_TARGET_NUM) ? RA_MAX_PING_TARGET_NUM : (num - complete_cnt);

        // prepare tx data target
        for (i = 0; i < send_num; i++) {
            j = i + complete_cnt;
            ret = memcpy_s(&(ping_data.tx_data.target[i]), sizeof(struct ping_target_comm_info),
                &(target[j]), sizeof(struct ping_target_comm_info));
            CHK_PRT_RETURN(ret, hccp_err("[del][ra_hdc_ping]memcpy_s target failed, ret(%d), i(%u) j(%u), phy_id(%u)",
                ret, i, j, phy_id), -ESAFEFUNC);
        }
        ping_data.tx_data.num = send_num;

        ret = ra_hdc_process_msg(RA_RS_PING_DEL, phy_id, (char *)&ping_data, sizeof(union op_ping_del_data));
        CHK_PRT_RETURN(ret, hccp_err("[del][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, phy_id), ret);
        complete_cnt += send_num;
    }

    return 0;
}

int ra_hdc_ping_task_stop(struct ra_ping_handle *ping_handle)
{
    union op_ping_stop_data ping_data = { 0 };
    unsigned int phy_id = ping_handle->phy_id;
    int ret;

    ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);

    ret = ra_hdc_process_msg(RA_RS_PING_STOP, phy_id, (char *)&ping_data, sizeof(union op_ping_stop_data));
    CHK_PRT_RETURN(ret, hccp_err("[stop][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}

int ra_hdc_ping_deinit(struct ra_ping_handle *ping_handle)
{
    union op_ping_deinit_data ping_data = { 0 };
    unsigned int phy_id = ping_handle->phy_id;
    int ret;

    ra_hdc_ping_init_rdev(&ping_data.tx_data.rdev, phy_id, ping_handle->dev_index);

    ret = ra_hdc_process_msg(RA_RS_PING_DEINIT, phy_id, (char *)&ping_data, sizeof(union op_ping_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phy_id), ret);

    return 0;
}
