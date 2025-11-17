/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _IBDIAG_SA_H_
#define _IBDIAG_SA_H_

#include <infiniband/mad.h>
#include <util/iba_types.h>

/* define an SA query structure to be common
 * This is by no means optimal but it moves the saquery functionality out of
 * the saquery tool and provides it to other utilities.
 */
struct sa_handle {
	int fd, agent;
	ib_portid_t dport;
	struct ibmad_port *srcport;
};

struct sa_query_result {
	uint32_t status;
	unsigned result_cnt;
	void *p_result_madw;
};

/* NOTE: umad_init must be called prior to sa_get_handle */
struct sa_handle * sa_get_handle(void);
void sa_free_handle(struct sa_handle * h);

int sa_query(struct sa_handle *h, uint8_t method,
	     uint16_t attr, uint32_t mod, uint64_t comp_mask, uint64_t sm_key,
	     void *data, size_t datasz, struct sa_query_result *result);
void sa_free_result_mad(struct sa_query_result *result);
void *sa_get_query_rec(void *mad, unsigned i);
void sa_report_err(int status);

/* Macros for setting query values and ComponentMasks */
static inline uint8_t htobe8(uint8_t val)
{
	return val;
}
#define CHECK_AND_SET_VAL(val, size, comp_with, target, name, mask) \
	if ((int##size##_t) val != (int##size##_t) comp_with) { \
		target = htobe##size((uint##size##_t) val); \
		comp_mask |= IB_##name##_COMPMASK_##mask; \
	}

#define CHECK_AND_SET_GID(val, target, name, mask) \
	if (valid_gid(&(val))) { \
		memcpy(&(target), &(val), sizeof(val)); \
		comp_mask |= IB_##name##_COMPMASK_##mask; \
	}

#define CHECK_AND_SET_VAL_AND_SEL(val, target, name, mask, sel) \
	if (val) { \
		target = val; \
		comp_mask |= IB_##name##_COMPMASK_##mask##sel; \
		comp_mask |= IB_##name##_COMPMASK_##mask; \
	}

#endif /* _IBDIAG_SA_H_ */
