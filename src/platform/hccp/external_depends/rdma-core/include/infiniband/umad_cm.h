/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef _UMAD_CM_H
#define _UMAD_CM_H

#include <infiniband/umad_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Communication management attributes */
enum {
	UMAD_CM_ATTR_REQ		= 0x0010,
	UMAD_CM_ATTR_MRA		= 0x0011,
	UMAD_CM_ATTR_REJ		= 0x0012,
	UMAD_CM_ATTR_REP		= 0x0013,
	UMAD_CM_ATTR_RTU		= 0x0014,
	UMAD_CM_ATTR_DREQ		= 0x0015,
	UMAD_CM_ATTR_DREP		= 0x0016,
	UMAD_CM_ATTR_SIDR_REQ		= 0x0017,
	UMAD_CM_ATTR_SIDR_REP		= 0x0018,
	UMAD_CM_ATTR_LAP		= 0x0019,
	UMAD_CM_ATTR_APR		= 0x001A,
	UMAD_CM_ATTR_SAP		= 0x001B,
	UMAD_CM_ATTR_SPR		= 0x001C,
};

#ifdef __cplusplus
}
#endif
#endif				/* _UMAD_CM_H */
