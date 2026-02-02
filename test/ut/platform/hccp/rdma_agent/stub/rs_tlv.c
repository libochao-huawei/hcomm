#include <stdlib.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include "hccp_tlv.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

int RsTlvInit(unsigned int moduleType, unsigned int phyId, unsigned int *bufferSize)
{
    return 0;
}

int RsTlvDeinit(unsigned int moduleType, unsigned int phyId)
{
    return 0;
}

int RsTlvRequest(struct TlvRequestMsgHead *head, char *data)
{
    return 0;
}