/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: rdma service ccu interface
 * Create: 2024-04-29
 */

#include "user_log.h"
#include "dl_ccu_function.h"
#include "ra_rs_ctx.h"
#include "rs_ccu.h"

int rs_ctx_ccu_custom_channel(const struct channel_info_in *in, struct channel_info_out *out)
{
    return rs_ccu_custom_channel(in, out);
}

STATIC int rs_ccu_mission_exec(unsigned int udie_id, ccu_u_opcode_t ccu_op)
{
    struct channel_info_out chan_out = {0};
    struct channel_info_in chan_in = {0};
    int ret = 0;

    chan_in.data.data_info.udie_idx = udie_id;
    chan_in.op = ccu_op;
    ret = rs_ctx_ccu_custom_channel(&chan_in, &chan_out);
    CHK_PRT_RETURN(ret != 0, hccp_run_warn("ccu_custom_channel unsuccessful, ccu_op[%u], ret[%d] udie_id[%u]",
        ccu_op, ret, udie_id), ret);
    CHK_PRT_RETURN(chan_out.op_ret != 0, hccp_run_warn("ccu_u_op unsuccessful, ccu_op[%u], op_ret[%d] udie_id[%u]",
        ccu_op, chan_out.op_ret, udie_id), chan_out.op_ret);

    return 0;
}

int rs_ctx_ccu_mission_kill(unsigned int die_id)
{
    return rs_ccu_mission_exec(die_id, CCU_U_OP_SET_TASKKILL);
}

int rs_ctx_ccu_mission_done(unsigned int die_id)
{
    return rs_ccu_mission_exec(die_id, CCU_U_OP_CLEAN_TASKKILL_STATE);
}
