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
#include "ra_rs_comm.h"
#include "ra_rs_err.h"
#include "hccp_tlv.h"
#include "ra_hdc_tlv.h"

int RaHdcTlvInit(struct RaTlvHandle *tlvHandle)
{
    unsigned int phyId = tlvHandle->initInfo.phyId;
    unsigned int opCode = RA_RS_TLV_INIT_V1;
    union OpTlvInitData tlvData = { 0 };
    unsigned int interfaceVersion = 0;
    int ret = 0;

    tlvData.txData.phyId = phyId;

    ret = RaHdcGetInterfaceVersion(phyId, RA_RS_TLV_INIT, &interfaceVersion);
    if (ret == 0 && interfaceVersion >= RA_RS_OPCODE_BASE_VERSION) {
        opCode = RA_RS_TLV_INIT;
    } else {
        ret = -ENOTSUPP;
        hccp_warn("[init][ra_hdc_tlv]ra tlv init version not support, phy_id(%u)", phyId);
        return ret;
    }

    ret = RaHdcProcessMsg(opCode, phyId, (char *)&tlvData, sizeof(union OpTlvInitData));
    CHK_PRT_RETURN(ret == -ENOTSUPP, hccp_warn("[init][ra_hdc_tlv]ra hdc message process unsuccessful ret(%d) phy_id(%u)",
        ret, phyId), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[init][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    tlvHandle->bufferSize = tlvData.rxData.bufferSize;
    return ret;
}

int RaHdcTlvDeinit(struct RaTlvHandle *tlvHandle)
{
    unsigned int phyId = tlvHandle->initInfo.phyId;
    union OpTlvDeinitData tlvData = { 0 };
    int ret;

    tlvData.txData.phyId = phyId;

    ret = RaHdcProcessMsg(RA_RS_TLV_DEINIT, phyId, (char *)&tlvData, sizeof(union OpTlvDeinitData));
    CHK_PRT_RETURN(ret != 0, hccp_err("[deinit][ra_hdc_tlv]ra hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    return 0;
}

STATIC void RaHdcTlvRequestHeadInit(struct RaTlvHandle *tlvHandle, unsigned int moduleType,
    struct TlvMsg *sendMsg, struct TlvRequestMsgHead *head)
{
    head->moduleType = moduleType;
    head->phyId = tlvHandle->initInfo.phyId;
    head->totalBytes = sendMsg->length;
    head->type = sendMsg->type;
    head->offset = 0;
}

STATIC int RaHdTlvRequestForSendNullMsg(unsigned int phyId, union OpTlvRequestData *tlvData,
    struct TlvRequestMsgHead *head, struct TlvMsg *recvMsg)
{
    int ret = 0;

    (void)memcpy_s(&(tlvData->txData.head), sizeof(struct TlvRequestMsgHead),
        head, sizeof(struct TlvRequestMsgHead));

    ret = RaHdcProcessMsg(RA_RS_TLV_REQUEST, phyId, (char *)tlvData, sizeof(union OpTlvRequestData));
    CHK_PRT_RETURN(ret == -EUSERS, hccp_warn("[request][ra_hdc_tlv]hdc message process unsuccessful ret(%d) phy_id(%u)",
        ret, phyId), ret);
    CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]hdc message process failed ret(%d) phy_id(%u)",
        ret, phyId), ret);

    recvMsg->type = head->type;
    recvMsg->length = tlvData->rxData.recvBytes;
    return ret;
}

STATIC int RaTlvRequestGetTlvMsg(struct TlvRequestMsgHead *head, union OpTlvRequestData *tlvData, struct TlvMsg *recvMsg)
{
    int ret = 0;

    if (recvMsg->data == NULL || recvMsg->length == 0) {
        tlvData->rxData.recvBytes = 0;
        goto out;
    }

    CHK_PRT_RETURN(tlvData->rxData.recvBytes > recvMsg->length,
        hccp_err("[request][ra_hdc_tlv]rxData.recvBytes(%u) > recvLen(%u), phyId(%u)", tlvData->rxData.recvBytes,
        recvMsg->length, head->phyId), -EINVAL);

    ret = memcpy_s(recvMsg->data, recvMsg->length, tlvData->rxData.recvData, tlvData->rxData.recvBytes);
    CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]memcpy_s recvData failed, ret(%d) rxData.recvBytes(%u)"
        " recvLen(%u) phyId(%u)", ret, tlvData->rxData.recvBytes, recvMsg->length, head->phyId), -ESAFEFUNC);

out:
    recvMsg->type = head->type;
    recvMsg->length = tlvData->rxData.recvBytes;
    return ret;
}

int RaHdcTlvRequest(struct RaTlvHandle *tlvHandle, unsigned int moduleType,
    struct TlvMsg *sendMsg, struct TlvMsg *recvMsg)
{
    unsigned int phyId = tlvHandle->initInfo.phyId;
    union OpTlvRequestData tlvData = { 0 };
    struct TlvRequestMsgHead head = { 0 };
    int ret = 0;

    RaHdcTlvRequestHeadInit(tlvHandle, moduleType, sendMsg, &head);
    if (sendMsg->length == 0) {
        return RaHdTlvRequestForSendNullMsg(phyId, &tlvData, &head, recvMsg);
    }

    while (head.offset < sendMsg->length) {
        head.sendBytes = (head.totalBytes - head.offset) >= MAX_TLV_MSG_DATA_LEN ?
            MAX_TLV_MSG_DATA_LEN : (head.totalBytes - head.offset);
        ret = memcpy_s(&(tlvData.txData.head), sizeof(struct TlvRequestMsgHead),
            &head, sizeof(struct TlvRequestMsgHead));

        (void)memset_s(tlvData.txData.data, MAX_TLV_MSG_DATA_LEN, 0, MAX_TLV_MSG_DATA_LEN);
        ret = memcpy_s(&(tlvData.txData.data), MAX_TLV_MSG_DATA_LEN, (sendMsg->data + head.offset), head.sendBytes);
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]memcpy_s data failed ret(%d) phy_id(%u) send_bytes(%u)",
            ret, phyId, head.sendBytes), -ESAFEFUNC);

        ret = RaHdcProcessMsg(RA_RS_TLV_REQUEST, phyId, (char *)&tlvData, sizeof(union OpTlvRequestData));
        CHK_PRT_RETURN(ret == -EUSERS, hccp_warn("[request][ra_hdc_tlv]hdc message process unsuccessful ret(%d) phy_id(%u)",
            ret, phyId), ret);
        CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]hdc message process failed ret(%d) phy_id(%u)",
            ret, phyId), ret);
        head.offset += head.sendBytes;
    }

    ret = RaTlvRequestGetTlvMsg(&head, &tlvData, recvMsg);
    CHK_PRT_RETURN(ret != 0, hccp_err("[request][ra_hdc_tlv]RaTlvRequestGetTlvMsg failed ret(%d) phy_id(%u)",
        ret, phyId), ret);
    return ret;
}
