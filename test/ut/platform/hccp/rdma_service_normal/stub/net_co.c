#include <stdlib.h>
#include "netco_api.h"

#define STUB_NET_CO_PROCED (1987)

void *Net_CoInitFactory(int epollfd, NetCoIpPortArg ipPortArg)
{
    return NULL;
}

void NET_CoDestruct(void *co)
{
    co = NULL;
    return;
}

unsigned int NET_CoFdEventDispatch(void *co, int fd, unsigned int curEvents)
{
    return STUB_NET_CO_PROCED;
}

int NET_CoTblAddUpd(void *netco_handle, unsigned int type, char *data, unsigned int data_len)
{
    return 0;
}