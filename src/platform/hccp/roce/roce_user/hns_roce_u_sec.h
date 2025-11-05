/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __HNS_ROCE_SEC_H
#define __HNS_ROCE_SEC_H

#include "securec.h"
#include "hns_roce_u.h"

#define HNS_ROCE_U_SEC_CHECK_RET_INT(ret) \
{if (ret) {roce_err("execute failed, return [%d], expect 0", ret);return ret;}}

#define HNS_ROCE_U_SEC_CHECK_RET_VOID(ret) \
{if (ret) {roce_err("execute failed, return [%d], expect 0", ret);return;}}

#define HNS_ROCE_U_MEM_CHECK_RET_GOTO_WRIT_ERR(p, ret) \
{if (ret) {roce_err(" memset failed, return [%d], expect 0", ret);free(p);p = NULL;goto malloc_rq_wrid_err;}}

#define HNS_ROCE_U_MEM_CHECK_RET_ERR(p, ret) \
{if (ret) {roce_err(" memset failed, return [%d], expect 0", ret);free(p);p = NULL;return (-ENOMEM);}}

#define HNS_ROCE_U_MEM_CHECK_RET_NULL(p, ret) \
{if (ret) {roce_err(" memset failed, return [%d], expect 0", ret);free(p);p = NULL;return  NULL;}}

#define HNS_ROCE_U_PARA_CHECK_GOTO_ERR(num) \
{if ((num) <= 0) {roce_err("parameter is less than or equal to 0");goto err_map;}}

#define HNS_ROCE_U_SEC_CHECK_GOTO_RQDB_ERR(ret) \
{if (ret) {roce_err("execute failed, return [%d], expect 0", ret);goto err_rq_db;}}

#define HNS_ROCE_U_SPRINTF_CHECK_RET_INT(ret) \
{if (ret == -1) {roce_err("snprintf err");return ret;}}

#define HNS_ROCE_U_NULL_POINT_RETURN_NULL(p) \
{if (p == NULL) {roce_err("point is NULL");return NULL;}}

#define HNS_ROCE_U_NULL_POINT_RETURN_ERR(p) \
{if (p == NULL) {roce_err("point is NULL");return (-ENOMEM);}}

#define HNS_ROCE_U_NULL_POINT_RETURN_VOID(p) \
{if (p == NULL) {roce_err("point is NULL");return;}}

#define HNS_ROCE_U_PARA_CHECK_RETURN_INT(num) \
{if ((num) <= 0) {roce_err("parameter is less than or equal to 0");return (-1);}}

#endif

