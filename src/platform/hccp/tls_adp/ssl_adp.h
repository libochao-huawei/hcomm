/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: network ssl adaptation function declaration
 * Create: 2025-10-23
 */
#ifndef SSL_ADP_H
#define SSL_ADP_H

#if defined (CA_CONFIG_LLT) || defined (CONFIG_LLT)
#else
#include <openssl/err.h>
#endif
#include "tls.h"

__attribute__ ((visibility ("default"))) long ssl_adp_set_mode(SSL *s, long op);
__attribute__ ((visibility ("default"))) long ssl_adp_ctrl(SSL *s, int cmd, long larg, void *parg);
__attribute__ ((visibility ("default"))) SSL *ssl_adp_new(SSL_CTX *ctx);
__attribute__ ((visibility ("default"))) void ssl_adp_free(SSL *s);
__attribute__ ((visibility ("default"))) int ssl_adp_read(SSL *s, void *buf, int num);
__attribute__ ((visibility ("default"))) int ssl_adp_write(SSL *s, const void *buf, int num);
__attribute__ ((visibility ("default"))) int ssl_adp_set_fd(SSL *s, int fd);
__attribute__ ((visibility ("default"))) void ssl_adp_ctx_free(SSL_CTX *a);
__attribute__ ((visibility ("default"))) int ssl_adp_shutdown(SSL *s);
__attribute__ ((visibility ("default"))) int ssl_adp_get_error(const SSL *s, int i);
__attribute__ ((visibility ("default"))) int ssl_adp_do_handshake(SSL *s);
__attribute__ ((visibility ("default"))) void ssl_adp_set_accept_state(SSL *s);
__attribute__ ((visibility ("default"))) void ssl_adp_set_connect_state(SSL *s);
__attribute__ ((visibility ("default"))) void ssl_adp_clear_error(void);

#endif