#include <stddef.h>
#include <errno.h>
#include "net_adapt_u_api.h"

int net_adapt_init(void)
{
    return 0;
}

void net_adapt_uninit(void)
{
}

int net_alloc_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int *jfc_id)
{
    return 0;
}

int net_free_jfc_id(const char *udev_name, unsigned int jfc_mode, unsigned int jfc_id)
{
    return 0;
}

int net_alloc_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int *jetty_id)
{
    return 0;
}

int net_free_jetty_id(const char *udev_name, unsigned int jetty_mode, unsigned int jetty_id)
{
    return 0;
}

unsigned long long net_get_cqe_base_addr(unsigned int die_id)
{
    return 0;
}