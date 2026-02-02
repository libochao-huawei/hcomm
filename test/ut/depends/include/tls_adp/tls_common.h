/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef TLS_COMMON_H
#define TLS_COMMON_H

#define TLS_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

int set_tls_enable_to_hcdev(unsigned int chip_id, unsigned int tls_enable);
void tls_get_enable_info(unsigned int save_mode, unsigned int chip_id, unsigned char *buf, unsigned int buf_size);
int tls_get_user_config(unsigned int save_mode, unsigned int chip_id, const char *name, unsigned char *buf,
    unsigned int *buf_size);
int tls_set_user_config(unsigned int save_mode, unsigned int chip_id, const char *name, unsigned char *buf,
    unsigned int buf_size);
int tls_clear_user_config(unsigned int save_mode, unsigned int chip_id, const char *name);
int tls_get_lock_file_path(char *lock_file_path, unsigned int file_path_len);

#endif