/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#if !defined(RDMA_VERBS_H)
#define RDMA_VERBS_H

#include <assert.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int rdma_seterrno(int ret)
{
	if (ret) {
		errno = ret;
		ret = -1;
	}
	return ret;
}

/*
 * Shared receive queues.
 */
int rdma_create_srq(struct rdma_cm_id *id, struct ibv_pd *pd,
		    struct ibv_srq_init_attr *attr);
int rdma_create_srq_ex(struct rdma_cm_id *id, struct ibv_srq_init_attr_ex *attr);

void rdma_destroy_srq(struct rdma_cm_id *id);


/*
 * Memory registration helpers.
 */
static inline struct ibv_mr *
rdma_reg_msgs(struct rdma_cm_id *id, void *addr, size_t length)
{
	return ibv_reg_mr(id->pd, addr, length, IBV_ACCESS_LOCAL_WRITE);
}

static inline struct ibv_mr *
rdma_reg_read(struct rdma_cm_id *id, void *addr, size_t length)
{
	return ibv_reg_mr(id->pd, addr, length, IBV_ACCESS_LOCAL_WRITE |
						IBV_ACCESS_REMOTE_READ);
}

static inline struct ibv_mr *
rdma_reg_write(struct rdma_cm_id *id, void *addr, size_t length)
{
	return ibv_reg_mr(id->pd, addr, length, IBV_ACCESS_LOCAL_WRITE |
						IBV_ACCESS_REMOTE_WRITE);
}

static inline int
rdma_dereg_mr(struct ibv_mr *mr)
{
	return rdma_seterrno(ibv_dereg_mr(mr));
}


/*
 * Vectored send, receive, and RDMA operations.
 * Support multiple scatter-gather entries.
 */
static inline int
rdma_post_recvv(struct rdma_cm_id *id, void *context, struct ibv_sge *sgl,
		int nsge)
{
	struct ibv_recv_wr wr, *bad;

	wr.wr_id = (uintptr_t) context;
	wr.next = NULL;
	wr.sg_list = sgl;
	wr.num_sge = nsge;

	if (id->srq)
		return rdma_seterrno(ibv_post_srq_recv(id->srq, &wr, &bad));
	else
		return rdma_seterrno(ibv_post_recv(id->qp, &wr, &bad));
}

static inline int
rdma_post_sendv(struct rdma_cm_id *id, void *context, struct ibv_sge *sgl,
		int nsge, int flags)
{
	struct ibv_send_wr wr, *bad;

	wr.wr_id = (uintptr_t) context;
	wr.next = NULL;
	wr.sg_list = sgl;
	wr.num_sge = nsge;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = flags;

	return rdma_seterrno(ibv_post_send(id->qp, &wr, &bad));
}

static inline int
rdma_post_readv(struct rdma_cm_id *id, void *context, struct ibv_sge *sgl,
		int nsge, int flags, uint64_t remote_addr, uint32_t rkey)
{
	struct ibv_send_wr wr, *bad;

	wr.wr_id = (uintptr_t) context;
	wr.next = NULL;
	wr.sg_list = sgl;
	wr.num_sge = nsge;
	wr.opcode = IBV_WR_RDMA_READ;
	wr.send_flags = flags;
	wr.wr.rdma.remote_addr = remote_addr;
	wr.wr.rdma.rkey = rkey;

	return rdma_seterrno(ibv_post_send(id->qp, &wr, &bad));
}

static inline int
rdma_post_writev(struct rdma_cm_id *id, void *context, struct ibv_sge *sgl,
		 int nsge, int flags, uint64_t remote_addr, uint32_t rkey)
{
	struct ibv_send_wr wr, *bad;

	wr.wr_id = (uintptr_t) context;
	wr.next = NULL;
	wr.sg_list = sgl;
	wr.num_sge = nsge;
	wr.opcode = IBV_WR_RDMA_WRITE;
	wr.send_flags = flags;
	wr.wr.rdma.remote_addr = remote_addr;
	wr.wr.rdma.rkey = rkey;

	return rdma_seterrno(ibv_post_send(id->qp, &wr, &bad));
}

/*
 * Simple send, receive, and RDMA calls.
 */
static inline int
rdma_post_recv(struct rdma_cm_id *id, void *context, void *addr,
	       size_t length, struct ibv_mr *mr)
{
	struct ibv_sge sge;

	assert((addr >= mr->addr) &&
		(((uint8_t *) addr + length) <= ((uint8_t *) mr->addr + mr->length)));
	sge.addr = (uint64_t) (uintptr_t) addr;
	sge.length = (uint32_t) length;
	sge.lkey = mr->lkey;

	return rdma_post_recvv(id, context, &sge, 1);
}

static inline int
rdma_post_send(struct rdma_cm_id *id, void *context, void *addr,
	       size_t length, struct ibv_mr *mr, int flags)
{
	struct ibv_sge sge;

	sge.addr = (uint64_t) (uintptr_t) addr;
	sge.length = (uint32_t) length;
	sge.lkey = mr ? mr->lkey : 0;

	return rdma_post_sendv(id, context, &sge, 1, flags);
}

static inline int
rdma_post_read(struct rdma_cm_id *id, void *context, void *addr,
	       size_t length, struct ibv_mr *mr, int flags,
	       uint64_t remote_addr, uint32_t rkey)
{
	struct ibv_sge sge;

	sge.addr = (uint64_t) (uintptr_t) addr;
	sge.length = (uint32_t) length;
	sge.lkey = mr->lkey;

	return rdma_post_readv(id, context, &sge, 1, flags, remote_addr, rkey);
}

static inline int
rdma_post_write(struct rdma_cm_id *id, void *context, void *addr,
		size_t length, struct ibv_mr *mr, int flags,
		uint64_t remote_addr, uint32_t rkey)
{
	struct ibv_sge sge;

	sge.addr = (uint64_t) (uintptr_t) addr;
	sge.length = (uint32_t) length;
	sge.lkey = mr ? mr->lkey : 0;

	return rdma_post_writev(id, context, &sge, 1, flags, remote_addr, rkey);
}

static inline int
rdma_post_ud_send(struct rdma_cm_id *id, void *context, void *addr,
		  size_t length, struct ibv_mr *mr, int flags,
		  struct ibv_ah *ah, uint32_t remote_qpn)
{
	struct ibv_send_wr wr, *bad;
	struct ibv_sge sge;

	sge.addr = (uint64_t) (uintptr_t) addr;
	sge.length = (uint32_t) length;
	sge.lkey = mr ? mr->lkey : 0;

	wr.wr_id = (uintptr_t) context;
	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = flags;
	wr.wr.ud.ah = ah;
	wr.wr.ud.remote_qpn = remote_qpn;
	wr.wr.ud.remote_qkey = RDMA_UDP_QKEY;

	return rdma_seterrno(ibv_post_send(id->qp, &wr, &bad));
}

static inline int
rdma_get_send_comp(struct rdma_cm_id *id, struct ibv_wc *wc)
{
	struct ibv_cq *cq;
	void *context;
	int ret;

	do {
		ret = ibv_poll_cq(id->send_cq, 1, wc);
		if (ret)
			break;

		ret = ibv_req_notify_cq(id->send_cq, 0);
		if (ret)
			return rdma_seterrno(ret);

		ret = ibv_poll_cq(id->send_cq, 1, wc);
		if (ret)
			break;

		ret = ibv_get_cq_event(id->send_cq_channel, &cq, &context);
		if (ret)
			return ret;

		assert(cq == id->send_cq && context == id);
		ibv_ack_cq_events(id->send_cq, 1);
	} while (1);

	return (ret < 0) ? rdma_seterrno(ret) : ret;
}

static inline int
rdma_get_recv_comp(struct rdma_cm_id *id, struct ibv_wc *wc)
{
	struct ibv_cq *cq;
	void *context;
	int ret;

	do {
		ret = ibv_poll_cq(id->recv_cq, 1, wc);
		if (ret)
			break;

		ret = ibv_req_notify_cq(id->recv_cq, 0);
		if (ret)
			return rdma_seterrno(ret);

		ret = ibv_poll_cq(id->recv_cq, 1, wc);
		if (ret)
			break;

		ret = ibv_get_cq_event(id->recv_cq_channel, &cq, &context);
		if (ret)
			return ret;

		assert(cq == id->recv_cq && context == id);
		ibv_ack_cq_events(id->recv_cq, 1);
	} while (1);

	return (ret < 0) ? rdma_seterrno(ret) : ret;
}

#ifdef __cplusplus
}
#endif

#endif /* RDMA_CMA_H */
