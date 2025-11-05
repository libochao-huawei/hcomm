/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: encrypt private key
 * Author:
 * Create: 2025-5-19
 */

#include "encrypt.h"
#include <errno.h>
#include "securec.h"
#include "user_log.h"
#include "tls.h"

__attribute__ ((visibility ("default"))) int crypto_encrypt_with_aes_gcm(crypt_info *info, unsigned char *out_buf,
    unsigned int *out_len)
{
    return 0;
}

