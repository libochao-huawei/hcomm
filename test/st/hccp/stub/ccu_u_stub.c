#include "ccu_u_comm.h"

struct ccu_mem_rsp {
    unsigned int die_id;
    unsigned int num;
    struct ccu_mem_info list[64U];
};

int ccu_init(void)
{
    return 0;
};

int ccu_uninit(void)
{
    return 0;
};

unsigned long long ccu_get_cqe_base_addr(unsigned int die_id)
{
    return 0;
};

int ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return 0;
};

int ccu_get_mem_info(unsigned int die_id, unsigned long long mem_type_bitmap,
    struct ccu_mem_rsp *rsp)
    {
        return 0;
    };
    
int get_ccu_u_info(unsigned int die_id, struct ccu_u_info *info)
{
    return 0;
};

int get_region_by_op(ccu_u_opcode_t op, struct ccu_region **region)
{
    return 0;
};
