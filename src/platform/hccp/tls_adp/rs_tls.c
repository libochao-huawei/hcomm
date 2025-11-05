/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: rdma service interface
 * Author:
 * Create: 2025-9-24
 */

#include "rs_tls.h"
#include "rs.h"
#include "ra_rs_err.h"


__attribute__ ((visibility ("default"))) void rs_ssl_err_string(int fd, int err)
{
    return;
};

__attribute__ ((visibility ("default"))) int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb)
{
    return 0;
};

__attribute__ ((visibility ("default"))) int rs_ssl_init(struct rs_cb *rscb)
{
    return 0;
};

__attribute__ ((visibility ("default"))) void rs_ssl_deinit(struct rs_cb *rscb)
{
    return;
};