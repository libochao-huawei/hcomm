#include <stddef.h>
#include <errno.h>
#include "ccu_u_api.h"

int ccu_init(void)
{
    return 0;
}

int ccu_uninit(void)
{
    return 0;
}

int ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    if (in == NULL || out == NULL) {
        return -EINVAL;
    }
    return 0;
}

unsigned long long ccu_get_cqe_base_addr(unsigned int die_id)
{
    return 0;
}
