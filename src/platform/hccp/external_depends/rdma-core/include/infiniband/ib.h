/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#if !defined(_RDMA_IB_H)
#define _RDMA_IB_H

#include <linux/types.h>
#include <endian.h>
#include <string.h>

#ifndef AF_IB
#define AF_IB 27
#endif
#ifndef PF_IB
#define PF_IB AF_IB
#endif

struct ib_addr {
	union {
		__u8		uib_addr8[16];
		__be16		uib_addr16[8];
		__be32		uib_addr32[4];
		__be64		uib_addr64[2];
	} ib_u;
#define sib_addr8		ib_u.uib_addr8
#define sib_addr16		ib_u.uib_addr16
#define sib_addr32		ib_u.uib_addr32
#define sib_addr64		ib_u.uib_addr64
#define sib_raw			ib_u.uib_addr8
#define sib_subnet_prefix	ib_u.uib_addr64[0]
#define sib_interface_id	ib_u.uib_addr64[1]
};

static inline int ib_addr_any(const struct ib_addr *a)
{
	return ((a->sib_addr64[0] | a->sib_addr64[1]) == 0);
}

static inline int ib_addr_loopback(const struct ib_addr *a)
{
	return ((a->sib_addr32[0] | a->sib_addr32[1] |
		 a->sib_addr32[2] | (a->sib_addr32[3] ^ htobe32(1))) == 0);
}

static inline void ib_addr_set(struct ib_addr *addr,
			       __be32 w1, __be32 w2, __be32 w3, __be32 w4)
{
	addr->sib_addr32[0] = w1;
	addr->sib_addr32[1] = w2;
	addr->sib_addr32[2] = w3;
	addr->sib_addr32[3] = w4;
}

static inline int ib_addr_cmp(const struct ib_addr *a1, const struct ib_addr *a2)
{
	return memcmp(a1, a2, sizeof(struct ib_addr));
}

struct sockaddr_ib {
	unsigned short int	sib_family;	/* AF_IB */
	__be16			sib_pkey;
	__be32			sib_flowinfo;
	struct ib_addr		sib_addr;
	__be64			sib_sid;
	__be64			sib_sid_mask;
	__u64			sib_scope_id;
};

#endif /* _RDMA_IB_H */
