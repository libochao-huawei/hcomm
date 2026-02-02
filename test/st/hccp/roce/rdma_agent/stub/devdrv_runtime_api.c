#include <string.h>
#include "ascend_hal.h"
#include "dsmi_common_interface.h"

int dsmi_get_device_ip_address(int device_id, int port_type, int port_id, ip_addr_t *ip_address,
                               ip_addr_t *mask_address)
{
	return 0;
}

int dev_read_flash(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int *buf_len)
{
   return 0;
}

int halSetUserConfig(unsigned int dev_id, const char *name, unsigned char *buf, unsigned int buf_size)
{
    return 0;
}

int halClearUserConfig(unsigned int devid, const char *name)
{
    return 0;
}