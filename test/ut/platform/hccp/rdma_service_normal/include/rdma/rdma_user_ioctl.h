/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RDMA_USER_IOCTL_H
#define RDMA_USER_IOCTL_H

#include <rdma/ib_user_mad.h>
#include <rdma/hfi/hfi1_ioctl.h>
#include <rdma/rdma_user_ioctl_cmds.h>

/* Legacy name, for user space application which already use it */
#define IB_IOCTL_MAGIC		RDMA_IOCTL_MAGIC

/*
 * General blocks assignments
 * It is closed on purpose do not expose it it user space
 * #define MAD_CMD_BASE		0x00
 * #define HFI1_CMD_BAS		0xE0
 */

/* MAD specific section */
#define IB_USER_MAD_REGISTER_AGENT	_IOWR(RDMA_IOCTL_MAGIC, 0x01, struct ib_user_mad_reg_req)
#define IB_USER_MAD_UNREGISTER_AGENT	_IOW(RDMA_IOCTL_MAGIC,  0x02, __u32)
#define IB_USER_MAD_ENABLE_PKEY		_IO(RDMA_IOCTL_MAGIC,   0x03)
#define IB_USER_MAD_REGISTER_AGENT2	_IOWR(RDMA_IOCTL_MAGIC, 0x04, struct ib_user_mad_reg_req2)

/* HFI specific section */
/* allocate HFI and context */
#define HFI1_IOCTL_ASSIGN_CTXT		_IOWR(RDMA_IOCTL_MAGIC, 0xE1, struct hfi1_user_info)
/* find out what resources we got */
#define HFI1_IOCTL_CTXT_INFO		_IOW(RDMA_IOCTL_MAGIC,  0xE2, struct hfi1_ctxt_info)
/* set up userspace */
#define HFI1_IOCTL_USER_INFO		_IOW(RDMA_IOCTL_MAGIC,  0xE3, struct hfi1_base_info)
/* update expected TID entries */
#define HFI1_IOCTL_TID_UPDATE		_IOWR(RDMA_IOCTL_MAGIC, 0xE4, struct hfi1_tid_info)
/* free expected TID entries */
#define HFI1_IOCTL_TID_FREE		_IOWR(RDMA_IOCTL_MAGIC, 0xE5, struct hfi1_tid_info)
/* force an update of PIO credit */
#define HFI1_IOCTL_CREDIT_UPD		_IO(RDMA_IOCTL_MAGIC,   0xE6)
/* control receipt of packets */
#define HFI1_IOCTL_RECV_CTRL		_IOW(RDMA_IOCTL_MAGIC,  0xE8, int)
/* set the kind of polling we want */
#define HFI1_IOCTL_POLL_TYPE		_IOW(RDMA_IOCTL_MAGIC,  0xE9, int)
/* ack & clear user status bits */
#define HFI1_IOCTL_ACK_EVENT		_IOW(RDMA_IOCTL_MAGIC,  0xEA, unsigned long)
/* set context's pkey */
#define HFI1_IOCTL_SET_PKEY		_IOW(RDMA_IOCTL_MAGIC,  0xEB, __u16)
/* reset context's HW send context */
#define HFI1_IOCTL_CTXT_RESET		_IO(RDMA_IOCTL_MAGIC,   0xEC)
/* read TID cache invalidations */
#define HFI1_IOCTL_TID_INVAL_READ	_IOWR(RDMA_IOCTL_MAGIC, 0xED, struct hfi1_tid_info)
/* get the version of the user cdev */
#define HFI1_IOCTL_GET_VERS		_IOR(RDMA_IOCTL_MAGIC,  0xEE, int)

#endif /* RDMA_USER_IOCTL_H */
