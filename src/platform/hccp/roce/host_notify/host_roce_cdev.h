/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef HOST_ROCE_CDEV_H
#define HOST_ROCE_CDEV_H

struct host_roce_notify_info {
    unsigned int logic_id;
    unsigned long long va;
    unsigned long long sz;
};

#define HOST_CDEV_IOC_MAXNR     1
#define HOST_CDEV_DEV_MAJOR     0
#define HOST_CDEV_IOC_MAGIC  '%'

#define HOST_CDEV_IOC_FREE_NOTIFY _IOWR(HOST_CDEV_IOC_MAGIC, 1, struct host_roce_notify_info)


#endif

