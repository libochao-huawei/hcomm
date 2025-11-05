/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma service tls interface
 * Create: 2025-9-24
 */

#ifndef RS_SSL_H
#define RS_SSL_H
#include "rs_inner.h"

__attribute__ ((visibility ("default"))) void rs_ssl_err_string(int fd, int err);

__attribute__ ((visibility ("default"))) int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb);

__attribute__ ((visibility ("default"))) int rs_ssl_init(struct rs_cb *rscb);

__attribute__ ((visibility ("default"))) void rs_ssl_deinit(struct rs_cb *rscb);
#endif // RS_SSL_H
