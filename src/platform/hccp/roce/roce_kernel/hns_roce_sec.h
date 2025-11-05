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

#include <linux/device.h>

#define HNS_ROCE_SEC_CHECK_RET_INT(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);return ret;}}

#define HNS_ROCE_SEC_CHECK_NEG_RET_INT(dev, ret) \
{if (ret < 0) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);return ret;}}

#define HNS_ROCE_SEC_CHECK_RET_VOID(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);return;}}

#define HNS_ROCE_SEC_CHECK_RET_NULL(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);return NULL;}}

#define HNS_ROCE_SEC_CHECK_GOTO_OUT(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);goto out;}}

#define HNS_ROCE_SEC_CHECK_GOTO_MEM_ERR(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);goto mem_err;}}

#define HNS_ROCE_SEC_CHECK_GOTO_CMD_ERR(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);goto err_cmd;}}

#define HNS_ROCE_SEC_CHECK_GOTO_MAILBOX_ERR(dev, ret) \
{if (ret) {dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);goto err_mailbox;}}

#define HNS_ROCE_SEC_CHECK_GOTO_LP_QP_ERR(dev, ret) \
{if (ret) dev_err(dev, "memerr func %s line %d\n", __func__, __LINE__);goto create_lp_qp_failed;}}

#define HNS_ROCE_NOTIFY_SEC_CHECK_RET_NULL(ret) \
{if (ret) {printk("memerr func %s line %d\n", __func__, __LINE__);return NULL;}}

#define HNS_ROCE_SPRINTF_CHECK_GOTO_KZ_FAILED(dev, ret) \
{if (ret == -1) {dev_err(dev, "sprintf err func %s line %d\n", __func__, __LINE__);goto err_kzalloc_failed;}}

#define HNS_ROCE_CHECK_NULL_POINT(p, ret) do { \
    if (p == NULL) {                    \
        pr_err("hns3: point is NULL! func %s line %d\n", __func__, __LINE__); \
        return ret;                   \
    }               \
} while (0)

#define HNS_ROCE_SEC_CHECK_RET_INT_WITHOUT_DEV(ret) \
{if (ret) {pr_err("func %s line %d check ret[%d] err.\n", __func__, __LINE__, ret); return ret;}}
#define HNS_ROCE_SEC_CHECK_RET_VOID_WITHOUT_DEV(ret) \
{if (ret) {pr_err("func %s line %d check ret[%d] err.\n", __func__, __LINE__, ret); return;}}

extern int memset_s(void *dest, size_t destMax, int c, size_t count);
extern int memcpy_s(void *dest, size_t destMax, const void *src, size_t count);
extern int strncat_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
extern int strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count);
extern int sscanf_s(const char *buffer, const char *format, ...);
extern int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...);

#endif
