/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * Description: network ssl adaptation function realization
 * Create: 2025-10-23
 */
#include "ssl_adp.h"

__attribute__ ((visibility ("default"))) long ssl_adp_set_mode(SSL *s, long op){
    return 0;
};

__attribute__ ((visibility ("default"))) SSL *ssl_adp_new(SSL_CTX *ctx){
    return NULL;
};

__attribute__ ((visibility ("default"))) void ssl_adp_free(SSL *s){
    return;
};

__attribute__ ((visibility ("default"))) int ssl_adp_read(SSL *s, void *buf, int num){
    return 0;
};

__attribute__ ((visibility ("default"))) int ssl_adp_write(SSL *s, const void *buf, int num){
    return 0;
};

__attribute__ ((visibility ("default"))) int ssl_adp_set_fd(SSL *s, int fd){
    return 0;
};

__attribute__ ((visibility ("default"))) void ssl_adp_ctx_free(SSL_CTX *a){
    return;
};

__attribute__ ((visibility ("default"))) int ssl_adp_shutdown(SSL *s){
    return 0;
};

__attribute__ ((visibility ("default"))) int ssl_adp_get_error(const SSL *s, int i){
    return 0;
};

__attribute__ ((visibility ("default"))) int ssl_adp_do_handshake(SSL *s){
    return 0;
};

__attribute__ ((visibility ("default"))) void ssl_adp_set_accept_state(SSL *s){
    return;
};

__attribute__ ((visibility ("default"))) void ssl_adp_set_connect_state(SSL *s){
    return;
};

__attribute__ ((visibility ("default"))) void ssl_adp_clear_error(void){
    return;
};