/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _TM_TYPES_H
#define _TM_TYPES_H

#include <linux/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ibv_tmh_op {
	IBV_TMH_NO_TAG	      = 0,
	IBV_TMH_RNDV	      = 1,
	IBV_TMH_FIN	      = 2,
	IBV_TMH_EAGER	      = 3,
};

struct ibv_tmh {
	uint8_t		opcode;      /* from enum ibv_tmh_op */
	uint8_t		reserved[3]; /* must be zero */
	__be32		app_ctx;     /* opaque user data */
	__be64		tag;
};

struct ibv_rvh {
	__be64		va;
	__be32		rkey;
	__be32		len;
};

#ifdef __cplusplus
}
#endif
#endif				/* _TM_TYPES_H */
