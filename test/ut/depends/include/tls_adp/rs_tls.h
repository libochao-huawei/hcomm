/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RS_SSL_H
#define RS_SSL_H
#include "rs_inner.h"
#include "tls_common.h"

TLS_ATTRI_VISI_DEF void rs_ssl_err_string(int fd, int err);

TLS_ATTRI_VISI_DEF int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb);

TLS_ATTRI_VISI_DEF int rs_ssl_init(struct rs_cb *rscb);

TLS_ATTRI_VISI_DEF void rs_ssl_deinit(struct rs_cb *rscb);
#endif // RS_SSL_H
