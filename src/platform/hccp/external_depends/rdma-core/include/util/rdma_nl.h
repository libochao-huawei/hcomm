/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef UTIL_RDMA_NL_H
#define UTIL_RDMA_NL_H

#include <stdbool.h>

#include <rdma/rdma_netlink.h>
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

extern struct nla_policy rdmanl_policy[RDMA_NLDEV_ATTR_MAX];
struct nl_sock *rdmanl_socket_alloc(void);
int rdmanl_get_devices(struct nl_sock *nl, nl_recvmsg_msg_cb_t cb_func,
		       void *data);
int rdmanl_get_chardev(struct nl_sock *nl, int ibidx, const char *name,
		       nl_recvmsg_msg_cb_t cb_func, void *data);
bool get_copy_on_fork(void);
int rdmanl_get_copy_on_fork(struct nl_sock *nl, nl_recvmsg_msg_cb_t cb_func,
			    void *data);

#endif
