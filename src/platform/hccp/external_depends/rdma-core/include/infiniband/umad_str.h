/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _UMAD_STR_H
#define _UMAD_STR_H

#include <infiniband/umad.h>

#ifdef __cplusplus
extern "C" {
#endif

const char * umad_class_str(uint8_t mgmt_class);
const char * umad_method_str(uint8_t mgmt_class, uint8_t method);
const char * umad_attribute_str(uint8_t mgmt_class, __be16 attr_id);

const char * umad_common_mad_status_str(__be16 status);
const char * umad_sa_mad_status_str(__be16 status);

#ifdef __cplusplus
}
#endif
#endif /* _UMAD_STR_H */
