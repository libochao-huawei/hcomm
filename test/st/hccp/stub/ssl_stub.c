#include "ssl_adp.h"
#include "rs_tls.h"

long ssl_adp_set_mode(SSL *s, long op)
{
    return 0;
}

SSL *ssl_adp_new(SSL_CTX *ctx)
{
    return NULL;
}

void ssl_adp_free(SSL *s)
{
    return;
}

int ssl_adp_read(SSL *s, void *buf, int num)
{
    return 0;
}

int ssl_adp_write(SSL *s, const void *buf, int num)
{
    return 0;
}

int ssl_adp_set_fd(SSL *s, int fd)
{
    return 0;
}

void ssl_adp_ctx_free(SSL_CTX *a)
{
    return;
}

int ssl_adp_shutdown(SSL *s)
{
    return 0;
}

int ssl_adp_get_error(const SSL *s, int i)
{
    return 0;
}

int ssl_adp_do_handshake(SSL *s)
{
    return 0;
}

void ssl_adp_set_accept_state(SSL *s)
{
    return;
}

void ssl_adp_set_connect_state(SSL *s)
{
    return;
}

void ssl_adp_clear_error(void)
{
    return;
}

void rs_ssl_err_string(int fd, int err)
{
    return;
}

int rs_tls_peer_cert_verify(SSL *ssl, struct rs_cb *rscb)
{
    return 0;
}

int rs_ssl_init(struct rs_cb *rscb)
{
    return 0;
}

void rs_ssl_deinit(struct rs_cb *rscb)
{
    return;
}