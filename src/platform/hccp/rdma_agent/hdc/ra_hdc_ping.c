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

int RaHdcPingInit(struct ra_ping_handle *pingHandle, struct ping_init_attr *initAttr,
    struct ping_init_info *initInfo)
{
    unsigned int phyId = pingHandle->phy_id;
    union op_ping_init_data pingData = { 0 };
    int ret;

    ret = memcpy_s(&(pingData.tx_data.attr), sizeof(struct ping_init_attr), initAttr, sizeof(struct ping_init_attr));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s init_attr failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    ret = RaHdcProcessMsg(RA_RS_PING_INIT, phyId, (char *)&pingData, sizeof(union op_ping_init_data));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    ret = memcpy_s(initInfo, sizeof(struct ping_init_info), &(pingData.rx_data.info), sizeof(struct ping_init_info));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s init_info failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);
    ret = memcpy_s(&(pingHandle->dev), sizeof(union ping_dev), &(initAttr->dev), sizeof(union ping_dev));
    CHK_PRT_RETURN(ret, hccp_err("[init][ra_hdc_ping]memcpy_s dev info failed, ret(%d) phyId(%u)",
        ret, phyId), -ESAFEFUNC);
    pingHandle->dev_index = pingData.rx_data.dev_index;

    return 0;
}

STATIC void RaHdcPingInitRdev(struct ra_rs_dev_info *rdev, unsigned int phyId, unsigned int devIndex)
{
    rdev->phy_id = phyId;
    rdev->dev_index = devIndex;
}

int RaHdcPingTargetAdd(struct ra_ping_handle *pingHandle, struct ping_target_info target[], uint32_t num)
{
    unsigned int phyId = pingHandle->phy_id;
    union op_ping_add_data pingData;
    unsigned int i;
    int ret;

    for (i = 0; i < num; i++) {
        if (pingHandle->protocol == PROTOCOL_RDMA) {
            CHK_PRT_RETURN(target[i].local_info.rdma.udp_sport > MAX_PORT_NUM,
                hccp_err("[add][ra_hdc_ping]udp_sport(%u) invaild, i(%u), phyId(%u)",
                target[i].local_info.rdma.udp_sport, i, phyId), -EINVAL);
        }

        (void)memset_s(&pingData, sizeof(pingData), 0, sizeof(pingData));
        RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);
        ret = memcpy_s(&(pingData.tx_data.target), sizeof(struct ping_target_info),
            &(target[i]), sizeof(struct ping_target_info));
        CHK_PRT_RETURN(ret, hccp_err("[add][ra_hdc_ping]memcpy_s target failed, ret(%d) i(%u) phyId(%u)",
            ret, i, phyId), -ESAFEFUNC);
        ret = RaHdcProcessMsg(RA_RS_PING_ADD, phyId, (char *)&pingData, sizeof(union op_ping_add_data));
        CHK_PRT_RETURN(ret, hccp_err("[add][ra_hdc_ping]ra hdc message process failed ret(%d) i(%u) phy_id(%u)",
            ret, i, phyId), ret);
    }

    return 0;
}

int RaHdcPingTaskStart(struct ra_ping_handle *pingHandle, struct ping_task_attr *attr)
{
    union op_ping_start_data pingData = { 0 };
    unsigned int phyId = pingHandle->phy_id;
    int ret;

    RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);
    ret = memcpy_s(&(pingData.tx_data.attr), sizeof(struct ping_task_attr), attr, sizeof(struct ping_task_attr));
    CHK_PRT_RETURN(ret, hccp_err("[start][ra_hdc_ping]memcpy_s attr failed, ret(%d), phyId(%u)",
        ret, phyId), -ESAFEFUNC);

    ret = RaHdcProcessMsg(RA_RS_PING_START, phyId, (char *)&pingData, sizeof(union op_ping_start_data));
    CHK_PRT_RETURN(ret, hccp_err("[start][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);
    return 0;
}

int RaHdcPingGetResults(struct ra_ping_handle *pingHandle, struct ping_target_result target[], uint32_t *num)
{
    unsigned int phyId = pingHandle->phy_id;
    union op_ping_results_data pingData;
    unsigned int totalNum = *num;
    unsigned int completeCnt = 0;
    unsigned int sendNum = 0;
    unsigned int i = 0;
    unsigned int j = 0;
    int ret = 0;

    while (completeCnt < totalNum) {
        (void)memset_s(&pingData, sizeof(pingData), 0, sizeof(pingData));
        RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);
        sendNum = ((totalNum - completeCnt) >= RA_MAX_PING_TARGET_NUM) ?
            RA_MAX_PING_TARGET_NUM : (totalNum - completeCnt);

        // prepare tx data target
        for (i = 0; i < sendNum; i++) {
            j = i + completeCnt;
            ret = memcpy_s(&(pingData.tx_data.target[i]), sizeof(struct ping_target_comm_info),
                &(target[j].remote_info), sizeof(struct ping_target_comm_info));
            if (ret) {
                hccp_err("[get][ra_hdc_ping]memcpy_s remote_info failed, ret(%d), i(%u), j(%u), phyId(%u)",
                    ret, i, j, phyId);
                goto out;
            }
        }
        pingData.tx_data.num = sendNum;

        ret = RaHdcProcessMsg(RA_RS_PING_GET_RESULTS, phyId, (char *)&pingData,
            sizeof(union op_ping_results_data));
        // caller needs to retry, degrade log level
        if (ret == -EAGAIN) {
            hccp_warn("[get][ra_hdc_ping]ra hdc message process unsuccessful, ret(%d) phyId(%u)", ret, phyId);
            goto out;
        }

        if (pingData.rx_data.num > sendNum) {
            hccp_err("[get][ra_hdc_ping]rx_data.num[%u] is larger than send_num[%u], ret(%d) phyId(%u)",
                pingData.rx_data.num, sendNum, ret, phyId);
            ret = -EINVAL;
            goto out;
        }

        // prepare rx data target
        for (i = 0; i < pingData.rx_data.num; i++) {
            j = i + completeCnt;
            ret = memcpy_s(&(target[j].result), sizeof(struct ping_result_info),
                &(pingData.rx_data.target[i]), sizeof(struct ping_result_info));
            if (ret) {
                hccp_err("[get][ra_hdc_ping]memcpy_s result failed, ret(%d), i(%u), j(%u), phyId(%u)",
                    ret, i, j, phyId);
                ret = -ESAFEFUNC;
                goto out;
            }
        }

        completeCnt += pingData.rx_data.num;
        if (ret) {
            hccp_err("[get][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)", ret, phyId);
            goto out;
        }
    }

out:
    *num = completeCnt;

    return ret;
}

int RaHdcPingTargetDel(struct ra_ping_handle *pingHandle, struct ping_target_comm_info target[], uint32_t num)
{
    unsigned int phyId = pingHandle->phy_id;
    union op_ping_del_data pingData;
    unsigned int completeCnt = 0;
    unsigned int sendNum = 0;
    unsigned int i = 0;
    unsigned int j = 0;
    int ret;

    while (completeCnt < num) {
        (void)memset_s(&pingData, sizeof(pingData), 0, sizeof(pingData));
        RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);
        sendNum = ((num - completeCnt) >= RA_MAX_PING_TARGET_NUM) ? RA_MAX_PING_TARGET_NUM : (num - completeCnt);

        // prepare tx data target
        for (i = 0; i < sendNum; i++) {
            j = i + completeCnt;
            ret = memcpy_s(&(pingData.tx_data.target[i]), sizeof(struct ping_target_comm_info),
                &(target[j]), sizeof(struct ping_target_comm_info));
            CHK_PRT_RETURN(ret, hccp_err("[del][ra_hdc_ping]memcpy_s target failed, ret(%d), i(%u) j(%u), phyId(%u)",
                ret, i, j, phyId), -ESAFEFUNC);
        }
        pingData.tx_data.num = sendNum;

        ret = RaHdcProcessMsg(RA_RS_PING_DEL, phyId, (char *)&pingData, sizeof(union op_ping_del_data));
        CHK_PRT_RETURN(ret, hccp_err("[del][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
            ret, phyId), ret);
        completeCnt += sendNum;
    }

    return 0;
}

int RaHdcPingTaskStop(struct ra_ping_handle *pingHandle)
{
    union op_ping_stop_data pingData = { 0 };
    unsigned int phyId = pingHandle->phy_id;
    int ret;

    RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);

    ret = RaHdcProcessMsg(RA_RS_PING_STOP, phyId, (char *)&pingData, sizeof(union op_ping_stop_data));
    CHK_PRT_RETURN(ret, hccp_err("[stop][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    return 0;
}

int RaHdcPingDeinit(struct ra_ping_handle *pingHandle)
{
    union op_ping_deinit_data pingData = { 0 };
    unsigned int phyId = pingHandle->phy_id;
    int ret;

    RaHdcPingInitRdev(&pingData.tx_data.rdev, phyId, pingHandle->dev_index);

    ret = RaHdcProcessMsg(RA_RS_PING_DEINIT, phyId, (char *)&pingData, sizeof(union op_ping_deinit_data));
    CHK_PRT_RETURN(ret, hccp_err("[deinit][ra_hdc_ping]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    return 0;
}
