/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hnae3.h"
#include "hns_roce_device.h"
#include "hns_roce_cmd.h"
#include "hns_roce_hw_v2.h"
#include "hns_roce_sec.h"
#include "hns_roce_common.h"

int hns_roce_v2_query_cqc_info(struct hns_roce_dev *hr_dev, u32 cqn,
                               int *buffer)
{
    struct hns_roce_v2_cq_context *context = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    if (IS_ERR(mailbox)) {
        dev_err(hr_dev->dev, "alloc cmd mailbox error\n");
        return PTR_ERR(mailbox);
    }

    context = mailbox->buf;
    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = cqn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_CQC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "QUERY cqc cmd process error, ret[%d]\n", ret);
        goto err_mailbox;
    }

    ret = memcpy_s(buffer, sizeof(*context), context, sizeof(*context));
    HNS_ROCE_SEC_CHECK_GOTO_MAILBOX_ERR(hr_dev->dev, ret);

err_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}


int hns_roce_v2_query_qpc_info(struct hns_roce_dev *hr_dev, u32 qpn,
                               int *buffer)
{
    struct hns_roce_v2_qp_context *context = NULL;
    struct hns_roce_cmd_mailbox *mailbox;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = qpn;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_QPC;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "QUERY qpc cmd process error\n");
        goto err_mailbox;
    }

    context = mailbox->buf;
    ret = memcpy_s(buffer, sizeof(*context), context, sizeof(*context));
    HNS_ROCE_SEC_CHECK_GOTO_MAILBOX_ERR(hr_dev->dev, ret);

err_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

int hns_roce_v2_query_mpt_info(struct hns_roce_dev *hr_dev, u32 key,
                               int *buffer)
{
    struct hns_roce_v2_mpt_entry *context = NULL;
    struct hns_roce_cmd_mailbox *mailbox = NULL;
    struct hns_roce_mbox_post_params mbox_pst_params = {0};
    int ret;

    mailbox = hns_roce_alloc_cmd_mailbox(hr_dev);
    HNS_ROCE_ALLOC_CMD_MAILBOX_RET_CHECK(mailbox);

    context = mailbox->buf;
    mbox_pst_params.in_param = 0;
    mbox_pst_params.out_param = mailbox->dma;
    mbox_pst_params.in_modifier = key;
    mbox_pst_params.op_modifier = 0;
    mbox_pst_params.op = HNS_ROCE_CMD_QUERY_MPT;
    ret = hns_roce_cmd_mbox(hr_dev, mbox_pst_params, HNS_ROCE_CMD_TIMEOUT_MSECS);
    if (ret) {
        dev_err(hr_dev->dev, "QUERY mpt cmd process error, ret[%d]\n", ret);
        goto err_mailbox;
    }

    ret = memcpy_s(buffer, sizeof(*context), context, sizeof(*context));
    HNS_ROCE_SEC_CHECK_GOTO_MAILBOX_ERR(hr_dev->dev, ret);

err_mailbox:
    hns_roce_free_cmd_mailbox(hr_dev, mailbox);

    return ret;
}

