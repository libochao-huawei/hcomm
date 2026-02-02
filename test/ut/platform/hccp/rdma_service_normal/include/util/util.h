/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/* GPLv2 or OpenIB.org BSD (MIT) See COPYING file */
#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <stdbool.h>
#include <sys/types.h>

/* Return true if the snprintf succeeded, false if there was truncation or
 * error */
static inline bool __good_snprintf(size_t len, int rc)
{
	return (rc < len && rc >= 0);
}

#define check_snprintf(buf, len, fmt, ...)                                     \
	__good_snprintf(len, snprintf(buf, len, fmt, ##__VA_ARGS__))

/* a CMP b. See also the BSD macro timercmp(). */
#define ts_cmp(a, b, CMP)			\
	(((a)->tv_sec == (b)->tv_sec) ?		\
	 ((a)->tv_nsec CMP (b)->tv_nsec) :	\
	 ((a)->tv_sec CMP (b)->tv_sec))

int set_fd_nonblock(int fd, bool nonblock);

#endif
