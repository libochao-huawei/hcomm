#include <stdlib.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>
#include "hccp_tlv.h"
#include "ra_rs_comm.h"
#include "ra_comm.h"

int rs_tlv_init(unsigned int module_type, unsigned int phy_id, unsigned int *buffer_size)
{
    return 0;
}

int rs_tlv_deinit(unsigned int module_type, unsigned int phy_id)
{
    return 0;
}

int rs_tlv_request(struct tlv_request_msg_head *head, char *data)
{
    return 0;
}
