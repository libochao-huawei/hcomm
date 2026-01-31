/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: CCU user interface
 * Create: 2024-05-08
 */

#ifndef CCU_U_API_H
#define CCU_U_API_H

#include "ccu_u_comm.h"

#define MAX_IO_DIE 2
#define CCU_DIE_ATTACHED 1
#define CCU_INIT_OK 1
#define CCU_INIT_NO 0

#define CCU_ATTRI_VISI_DEF __attribute__ ((visibility ("default")))

struct ccu_u_op_handle {
    unsigned int opcode;
    int (*op_handle)(const struct channel_info_in *, struct channel_info_out *);
};

CCU_ATTRI_VISI_DEF int ccu_init(void);
CCU_ATTRI_VISI_DEF int ccu_uninit(void);
CCU_ATTRI_VISI_DEF unsigned long long ccu_get_cqe_base_addr(unsigned int die_id);
CCU_ATTRI_VISI_DEF int ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out);
int get_ccu_u_info(unsigned int die_id, struct ccu_u_info *info);
int get_region_by_op(ccu_u_opcode_t op, struct ccu_region **region);

#endif /* CCU_U_API_H */
