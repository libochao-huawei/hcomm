/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _HNS_ROCE_NOTIFY_H
#define _HNS_ROCE_NOTIFY_H

struct hns_roce_notify_node {
    unsigned long long va;
    unsigned long long pa;
    unsigned long long sz;
    pid_t pid;
    struct list_head node;
};

void *hns_roce_notify_client_init(void);

void hns_roce_notify_client_cleanup(char *priv);

int hns_roce_notify_add(struct hns_roce_notify_node *notify_node);

void hns_roce_notify_del(struct hns_roce_notify_node *notify_node);

#endif
